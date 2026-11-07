#include "Bar.hpp"
#include "Geometry.hpp"
#include "../core/Config.hpp"
#include "../services/Icons.hpp"
#include "../core/Util.hpp"
#include "../modules/Module.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>

#include <format>

// ---------------------------------------------------------------- lifecycle

CStatusBar::CStatusBar() {
    auto& ev = Event::bus()->m_events;

    std::function<void()> dirty = [this] { markDirty(); };
    m_listeners.emplace_back(ev.monitor.added.listen(dirty));
    m_listeners.emplace_back(ev.monitor.removed.listen(dirty));
    m_listeners.emplace_back(ev.monitor.layoutChanged.listen(dirty));
    m_listeners.emplace_back(ev.window.fullscreen.listen(dirty)); // hide_on_fullscreen
    m_listeners.emplace_back(ev.workspace.active.listen(dirty));  // ditto, per workspace

    // hl.config()/eval changes fire props_refreshed; a full reload fires reloaded
    m_listeners.emplace_back(ev.config.props_refreshed.listen([this](bool) { onConfigChanged(); }));
    m_listeners.emplace_back(ev.config.reloaded.listen([this] { onConfigChanged(); }));

    m_listeners.emplace_back(ev.render.stage.listen([this](const Event::SRenderStageEvent& e) {
        if (e.stage == RENDER_POST_WINDOWS && e.context && e.monitor)
            render(e.context->get(), e.monitor);
    }));

    m_relayoutTimer = makeUnique<CWakeTimer>([this] { relayout(); });
    setupInput();
    syncModules();
}

CStatusBar::~CStatusBar() {
    m_listeners.clear();
    m_bars.clear();
    m_modules.clear();
}

// instantiate the modules named in the config, drop the rest
void CStatusBar::syncModules() {
    const std::string LISTS[3] = {g_config.modulesLeft->value(), g_config.modulesCenter->value(), g_config.modulesRight->value()};

    std::map<std::string, UP<IModule>> next;
    for (int i = 0; i < 3; ++i) {
        m_moduleOrder[i] = splitWords(LISTS[i]);
        for (const auto& name : m_moduleOrder[i]) {
            if (next.contains(name))
                continue;
            if (auto it = m_modules.find(name); it != m_modules.end())
                next[name] = std::move(it->second);
            else if (auto mod = createModule(name)) {
                mod->m_onChanged = [this] { markDirty(); };
                next[name]       = std::move(mod);
            }
        }
    }

    // items may hold callbacks into modules about to be destroyed
    for (auto& b : m_bars)
        b.items.clear();
    m_modules = std::move(next);
    markDirty();
}

void CStatusBar::onConfigChanged() {
    if (setIconTheme(g_config.iconTheme->value()))
        for (auto& b : m_bars)
            b.texCache.clear(); // icons from the old theme
    syncModules();
    for (auto& [_, m] : m_modules)
        m->onConfigChanged();
    checkGeometryChanged();
    markDirty();
}

void CStatusBar::checkGeometryChanged() {
    const auto POS = g_config.position->value();
    const auto H   = g_config.height->value();
    const auto AH  = g_config.autohide->value();
    if (POS == m_appliedPosition && H == m_appliedHeight && AH == m_appliedAutohide)
        return;

    // damage where the bar used to be before moving it
    for (auto& m : State::monitorState()->monitors())
        g_pHyprRenderer->damageMonitor(m);
    for (auto& b : m_bars)
        b.texCache.clear();
    rearrangeAll();
}

// modules changing coalesce into one relayout on the next loop iteration
void CStatusBar::markDirty() {
    for (auto& b : m_bars)
        b.needsLayout = true;
    m_relayoutTimer->arm(std::chrono::milliseconds(1));
}

// Lay out now, outside the render pass, so the damage can be just the items
// whose look or place changed: a ticking clock redraws the clock, not the bar.
void CStatusBar::relayout() {
    if (!g_pHyprRenderer || m_unloading)
        return;
    for (auto& mon : State::monitorState()->monitors()) {
        auto& bar = barFor(mon);
        if (!bar.needsLayout && bar.lastScale == mon->m_scale && !bar.items.empty())
            continue;
        // not fully out (sliding, hidden, fullscreen) or never drawn: the render pass lays out
        if (!shouldDraw(mon) || visibility(bar) < 1.F || bar.lastScale != mon->m_scale || !Render::GL::g_pHyprOpenGL) {
            g_pHyprRenderer->damageBox(barBoxGlobal(mon));
            continue;
        }

        struct SOld {
            CBox                      box;
            Render::ITexture*         tex;
            std::optional<CHyprColor> pill;
        };
        std::vector<SOld> old;
        for (const auto& it : bar.items)
            old.push_back({it.box, it.tex.get(), it.pill});

        Render::GL::g_pHyprOpenGL->makeEGLCurrent(); // layout makes textures
        layout(bar, mon);

        if (old.size() != bar.items.size()) {
            g_pHyprRenderer->damageBox(barBoxGlobal(mon));
            continue;
        }
        const double SCALE  = mon->m_scale;
        auto         global = [&](const CBox& b) { return CBox{mon->m_position + b.pos() / SCALE, b.size() / SCALE}.expand(1); };
        for (size_t i = 0; i < old.size(); ++i) {
            const auto& NOW  = bar.items[i];
            const bool  SAME = old[i].box == NOW.box && old[i].tex == NOW.tex.get() && old[i].pill.has_value() == NOW.pill.has_value() &&
                (!NOW.pill || old[i].pill->getAsHex() == NOW.pill->getAsHex());
            if (SAME)
                continue;
            g_pHyprRenderer->damageBox(global(old[i].box));
            g_pHyprRenderer->damageBox(global(NOW.box));
        }
    }
}

void CStatusBar::damageAll() {
    if (!g_pHyprRenderer)
        return;
    for (auto& m : State::monitorState()->monitors())
        g_pHyprRenderer->damageBox(barBoxGlobal(m));
}

// ---------------------------------------------------------------- reserved area

int CStatusBar::reservedThickness() const {
    if (m_unloading || g_config.autohide->value())
        return 0;
    return std::max<int64_t>(1, g_config.height->value());
}

void CStatusBar::onMonitorArranged(PHLMONITOR mon) {
    // other bars' reservations decide where we start/end, so re-layout
    for (auto& b : m_bars) {
        if (b.monitor.lock() == mon)
            b.needsLayout = true;
    }
    g_pHyprRenderer->damageMonitor(mon);

    const double D = reservedThickness();
    if (D <= 0)
        return;
    const auto E = barEdge();
    mon->m_reservedArea.addType(Desktop::RESERVED_DYNAMIC_TYPE_LS, Vector2D{E == EDGE_LEFT ? D : 0.0, E == EDGE_TOP ? D : 0.0},
                                Vector2D{E == EDGE_RIGHT ? D : 0.0, E == EDGE_BOTTOM ? D : 0.0});
    g_layoutManager->invalidateMonitorGeometries(mon);
}

void CStatusBar::rearrangeAll() {
    m_appliedPosition = g_config.position->value();
    m_appliedHeight   = g_config.height->value();
    m_appliedAutohide = g_config.autohide->value();
    for (auto& m : State::monitorState()->monitors())
        g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
}

// ---------------------------------------------------------------- state

bool CStatusBar::shouldDraw(PHLMONITOR mon) const {
    if (m_unloading || !mon)
        return false;
    if (mon->m_activeWorkspace && Fullscreen::controller()->hasFullscreen(mon->m_activeWorkspace))
        return false;
    return true;
}


SMonitorBar& CStatusBar::barFor(PHLMONITOR mon) {
    std::erase_if(m_bars, [](const SMonitorBar& b) { return b.monitor.expired(); });
    for (auto& b : m_bars) {
        if (b.monitor.lock() == mon)
            return b;
    }
    auto& b   = m_bars.emplace_back();
    b.monitor = mon;
    return b;
}

// ---------------------------------------------------------------- layout

void CStatusBar::layout(SMonitorBar& bar, PHLMONITOR mon) {
    if (bar.lastScale != mon->m_scale) {
        bar.texCache.clear();
        bar.lastScale = mon->m_scale;
    }
    bar.needsLayout = false;
    bar.items.clear();

    const double SCALE   = mon->m_scale;
    const double PAD     = Style::PADDING * SCALE;
    const double SPACING = Style::SPACING * SCALE;
    const double IPAD    = 6.0 * SCALE; // padding inside each item, along the bar
    const double GAP     = 2.0 * SCALE; // gap between items of one module
    const bool   VERT    = barVertical();
    const CBox   BAR     = barBoxLocal(mon);
    const int    PT      = std::round(g_config.fontSize->value() * SCALE);
    const int    ICON    = std::round(std::min(BAR.w, BAR.h) * 0.6);
    // vertical bars are narrow: let pango ellipsize text to the bar's width
    const int MAXW = VERT ? (int)std::max(1.0, BAR.w - 4 * SCALE) : 0;

    std::unordered_map<std::string, SP<Render::ITexture>> newCache;

    const int SYM = std::round(ICON * 2.0 / 3.0); // status icons sit smaller, as in GNOME

    auto texture = [&](SBarItem& it) {
        std::string key;
        if (!it.symbolic.empty())
            key = std::format("sym|{}|{:x}|{}", it.symbolic, it.fg.getAsHex(), SYM);
        else if (it.icon)
            key = std::format("icon|{}|{}", it.iconKey, ICON);
        else
            key = std::format("{}|{:x}|{}|{}|{}", it.text, it.fg.getAsHex(), PT, g_config.font->value(), MAXW);
        if (auto c = bar.texCache.find(key); c != bar.texCache.end() && c->second)
            it.tex = c->second;
        else if (!it.symbolic.empty())
            it.tex = symbolicIcons({it.symbolic}, SYM, it.fg);
        else if (it.icon)
            it.tex = it.icon(ICON);
        else
            it.tex = g_pHyprRenderer->renderText(it.text, it.fg, PT, false, g_config.font->value(), MAXW);
        newCache[key] = it.tex;
    };

    using CGroups = std::vector<std::vector<SBarItem>>;
    auto buildSection = [&](const std::vector<std::string>& names) {
        CGroups groups;
        for (const auto& name : names) {
            auto it = m_modules.find(name);
            if (it == m_modules.end())
                continue;
            std::vector<SBarItem> items;
            it->second->build(mon, items);
            if (items.empty())
                continue;
            for (auto& i : items)
                texture(i);
            groups.push_back(std::move(items));
        }
        return groups;
    };

    // a group's items sit JPAD apart inside, IPAD from its pill's ends
    const double JPAD   = std::round(SYM / 4.0);
    auto         joined = [](const std::vector<SBarItem>& g, size_t a, size_t b) { return b < g.size() && g[a].join && g[a].join == g[b].join; };
    auto         len    = [&](const std::vector<SBarItem>& g, size_t i) {
        const auto&  it      = g[i];
        const double CONTENT = it.icon && !it.iconNative && it.symbolic.empty() ? ICON : (it.tex ? (VERT ? it.tex->m_size.y : it.tex->m_size.x) : 0);
        const double BEFORE  = i > 0 && joined(g, i - 1, i) ? JPAD : IPAD;
        const double AFTER   = joined(g, i, i + 1) ? JPAD : IPAD;
        return CONTENT + BEFORE + AFTER;
    };
    auto gapAfter = [&](const std::vector<SBarItem>& g, size_t i) { return i + 1 >= g.size() || joined(g, i, i + 1) ? 0.0 : GAP; };

    auto total = [&](const CGroups& groups) {
        double w = 0;
        for (size_t g = 0; g < groups.size(); ++g) {
            for (size_t i = 0; i < groups[g].size(); ++i)
                w += len(groups[g], i) + gapAfter(groups[g], i);
            if (g + 1 < groups.size())
                w += SPACING;
        }
        return w;
    };

    auto place = [&](CGroups& groups, double x) {
        for (auto& group : groups) {
            for (size_t i = 0; i < group.size(); ++i) {
                const double W   = len(group, i);
                const double GAP_ = gapAfter(group, i);
                auto&        it  = group[i];
                it.box           = VERT ? CBox{BAR.x, x, BAR.w, W} : CBox{x, BAR.y, W, BAR.h};
                x += W + GAP_;
                bar.items.push_back(std::move(it));
            }
            x += SPACING;
        }
    };

    auto start  = buildSection(m_moduleOrder[0]);
    auto center = buildSection(m_moduleOrder[1]);
    auto end    = buildSection(m_moduleOrder[2]);

    // "left" = start of the bar (top when vertical), "right" = end
    const double S0  = VERT ? BAR.y : BAR.x;
    const double LEN = VERT ? BAR.h : BAR.w;
    place(start, S0 + PAD);
    place(center, S0 + std::round((LEN - total(center)) / 2.0));
    place(end, S0 + LEN - PAD - total(end));

    bar.texCache = std::move(newCache);
}

// ---------------------------------------------------------------- render

void CStatusBar::render(Render::CRenderContext& ctx, PHLMONITOR mon) {
    if (!shouldDraw(mon))
        return;

    auto& bar = barFor(mon);
    for (auto& [_, m] : m_modules)
        m->onFrame();

    animate(bar, mon);
    if (visibility(bar) <= 0.F)
        return;

    if (bar.needsLayout || bar.lastScale != mon->m_scale)
        layout(bar, mon); // GL context is current here, so textures can be made

    const double   SCALE = mon->m_scale;
    const auto     E     = barEdge();
    const bool     VERT  = barVertical();
    const CBox     FULL  = barBoxLocal(mon);
    // slide out of the screen edge, ease-out
    const double   EASED = 1.0 - std::pow(1.0 - visibility(bar), 3.0);
    const double   DIST  = std::round((1.0 - EASED) * (VERT ? FULL.w : FULL.h)) * (E == EDGE_TOP || E == EDGE_LEFT ? -1.0 : 1.0);
    const Vector2D OFF   = VERT ? Vector2D{DIST, 0.0} : Vector2D{0.0, DIST};
    const CBox     BAR   = FULL.copy().translate(OFF);
    const int      ROUND = std::round(g_config.rounding->value() * SCALE);

    CRectPassElement::SRectData bg;
    bg.box   = BAR;
    bg.color = configColor(g_config.bgColor);
    ctx.m_pass.add(makeUnique<CRectPassElement>(bg));

    for (auto& it : bar.items) {
        const CBox IB = it.box.copy().translate(OFF);

        // a group's pill spans all its items: drawn once, at the first
        const bool CONT = it.join && &it != &bar.items.front() && (&it - 1)->join == it.join;
        if (it.pill && !CONT) {
            CBox span = IB;
            for (auto* n = &it + 1; n != bar.items.data() + bar.items.size() && n->join && n->join == it.join; ++n) {
                const CBox NB = n->box.copy().translate(OFF);
                span          = VERT ? CBox{span.x, span.y, span.w, NB.y + NB.h - span.y} : CBox{span.x, span.y, NB.x + NB.w - span.x, span.h};
            }
            // inset across the bar only, so pills read as separate chips
            const double                INSET = std::round((VERT ? BAR.w : BAR.h) * 0.15);
            CRectPassElement::SRectData pill;
            pill.box   = (VERT ? CBox{span.x + INSET, span.y, span.w - 2 * INSET, span.h} : CBox{span.x, span.y + INSET, span.w, span.h - 2 * INSET}).round();
            pill.color = *it.pill;
            pill.round = std::min<int>(ROUND, std::min(pill.box.w, pill.box.h) / 2);
            ctx.m_pass.add(makeUnique<CRectPassElement>(pill));
        }

        if (!it.tex || it.tex->m_texID == 0)
            continue;

        // icon images are scaled into a square; text and symbolic icons are drawn at their native size
        Vector2D size = it.tex->m_size;
        if (it.icon && !it.iconNative && it.symbolic.empty()) {
            const double ICON = std::round(std::min(BAR.w, BAR.h) * 0.6);
            const double K    = ICON / std::max(size.x, size.y);
            size              = (size * K).round();
        }

        CTexPassElement::SRenderData tex;
        tex.tex        = it.tex;
        tex.box        = CBox{Vector2D{IB.x + (IB.w - size.x) / 2.0, IB.y + (IB.h - size.y) / 2.0}.round(), size};
        tex.clipRegion = CRegion(FULL);
        ctx.m_pass.add(makeUnique<CTexPassElement>(std::move(tex)));
    }
}
