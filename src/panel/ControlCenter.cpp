#include "ControlCenter.hpp"
#include "PanelModule.hpp"
#include "Widgets.hpp"
#include "../bar/Geometry.hpp"
#include "../core/Config.hpp"
#include "../core/Util.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cmath>
#include <format>

// logical px
static constexpr double PANEL_W = 320;
static constexpr double PAD     = 14;
static constexpr double BAR_GAP = 8; // distance from the bar

// ---------------------------------------------------------------- lifecycle

CControlCenter::CControlCenter(SP<Config::Values::CStringValue> modules) : m_modulesOption(std::move(modules)) {
    m_reapTimer = makeUnique<CWakeTimer>([this] { m_retired.clear(); });

    auto& ev = Event::bus()->m_events;
    m_listeners.emplace_back(ev.render.stage.listen([this](const Event::SRenderStageEvent& e) {
        if (m_open && e.stage == RENDER_POST_WINDOWS && e.context && e.monitor && e.monitor == m_monitor.lock())
            render(e.context->get(), e.monitor);
    }));
    m_listeners.emplace_back(ev.monitor.removed.listen([this](PHLMONITOR m) {
        if (m == m_monitor.lock())
            close();
    }));
    // the module list edited while open: rebuild in place
    auto onConfig = [this] {
        if (m_open && m_modulesOption->value() != m_modulesConf) {
            damage();
            retireModules();
            createModules();
            changed();
        }
    };
    m_listeners.emplace_back(ev.config.props_refreshed.listen([onConfig](bool) { onConfig(); }));
    m_listeners.emplace_back(ev.config.reloaded.listen(onConfig));

    setupInput();
}

CControlCenter::~CControlCenter() {
    m_listeners.clear();
    close();
    m_retired.clear();
}

void CControlCenter::toggle(PHLMONITOR mon, std::optional<Vector2D> anchor) {
    if (m_open)
        close();
    else
        open(mon, anchor);
}

void CControlCenter::open(PHLMONITOR mon, std::optional<Vector2D> anchor) {
    if (!mon)
        mon = Desktop::focusState()->monitor();
    if (!mon)
        return;

    if (m_open)
        damage();
    m_monitor = mon;
    m_anchor  = anchor;
    if (!m_open)
        createModules();
    m_open = true;

    layout();
    damage();
    m_openChanged.emit();
}

void CControlCenter::close() {
    if (!m_open)
        return;
    damage();
    m_open = false;
    retireModules();
    m_tex.clear();
    m_openChanged.emit();
}

void CControlCenter::createModules() {
    m_modulesConf = m_modulesOption->value();
    for (const auto& name : splitWords(m_modulesConf)) {
        auto m = createPanelModule(name);
        if (!m) {
            HyprlandAPI::addNotification(PHANDLE, "[hyprstatus] unknown control center module: " + name, CHyprColor{1.0, 0.6, 0.2, 1.0}, 5000);
            continue;
        }
        m->m_onChanged = [this] { changed(); };
        m->m_close     = [this] { close(); };
        m_modules.push_back(std::move(m));
    }
}

// modules may be the caller (a power button closing the panel): free them later
void CControlCenter::retireModules() {
    for (auto& m : m_modules) {
        m->m_onChanged = nullptr;
        m->m_close     = nullptr;
        m_retired.push_back(std::move(m));
    }
    m_modules.clear();
    m_layout.clear();
    m_dragging = nullptr;
    m_reapTimer->arm(std::chrono::milliseconds(1));
}

void CControlCenter::changed() {
    if (!m_open)
        return;
    damage(); // old size
    layout();
    damage(); // new size
}

void CControlCenter::damage() {
    if (m_open && g_pHyprRenderer && m_monitor)
        g_pHyprRenderer->damageBox(panelBoxGlobal().expand(2));
}

// ---------------------------------------------------------------- layout

// rows top to bottom; two consecutive tiles share a row, a lone tile is full width
void CControlCenter::layout() {
    using Widgets::GAP;
    const double  W    = PANEL_W - 2 * PAD;
    const double  HALF = (W - GAP) / 2;
    double        y    = 0;
    IPanelModule* lone = nullptr;

    m_layout.clear();
    auto flushTile = [&] {
        if (!lone)
            return;
        m_layout.push_back({lone, {0, y, W, lone->height()}});
        y += lone->height() + GAP;
        lone = nullptr;
    };

    for (auto& m : m_modules) {
        if (!m->visible())
            continue;
        if (!m->tile()) {
            flushTile();
            m_layout.push_back({m.get(), {0, y, W, m->height()}});
            y += m->height() + GAP;
        } else if (!lone)
            lone = m.get();
        else {
            const double H = std::max(lone->height(), m->height());
            m_layout.push_back({lone, {0, y, HALF, H}});
            m_layout.push_back({m.get(), {HALF + GAP, y, HALF, H}});
            y += H + GAP;
            lone = nullptr;
        }
    }
    flushTile();

    m_size = {PANEL_W, std::max(0.0, y - GAP) + 2 * PAD};
}

CBox CControlCenter::panelBoxGlobal() const {
    const auto MON = m_monitor.lock();
    return MON ? popupBoxGlobal(MON, m_size, m_anchor, BAR_GAP) : CBox{};
}

Vector2D CControlCenter::toContent(const Vector2D& global) const {
    return global - panelBoxGlobal().pos() - Vector2D{PAD, PAD};
}

const CControlCenter::SPlaced* CControlCenter::placedAt(const Vector2D& content) const {
    for (const auto& p : m_layout)
        if (p.box.containsPoint(content))
            return &p;
    return nullptr;
}

const CControlCenter::SPlaced* CControlCenter::placedFor(const IPanelModule* m) const {
    for (const auto& p : m_layout)
        if (p.module == m)
            return &p;
    return nullptr;
}

// ---------------------------------------------------------------- input

void CControlCenter::setupInput() {
    auto& ev = Event::bus()->m_events;

    m_listeners.emplace_back(ev.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
        if (e.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            if (m_pressedButtons.erase(e.button)) {
                info.cancelled = true;
                m_dragging     = nullptr;
            }
            return;
        }
        if (!m_open || info.cancelled) // the bar handled it (e.g. our own button)
            return;

        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        if (!panelBoxGlobal().containsPoint(MOUSE)) {
            close(); // click outside: dismiss, and let the click through
            return;
        }

        info.cancelled = true;
        m_pressedButtons.insert(e.button);
        if (e.button != BTN_LEFT)
            return;

        const auto  POS = toContent(MOUSE);
        const auto* P   = placedAt(POS);
        if (!P)
            return;
        auto* module = P->module;
        if (module->onPress(POS, P->box) && m_open)
            m_dragging = module;
        changed();
    }));

    m_listeners.emplace_back(ev.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo&) {
        if (!m_open || !m_dragging)
            return;
        if (const auto* P = placedFor(m_dragging))
            m_dragging->onDrag(toContent(g_pInputManager->getMouseCoordsInternal()), P->box);
    }));

    m_listeners.emplace_back(ev.input.mouse.axis.listen([this](IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
        if (!m_open || info.cancelled)
            return;
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        if (!panelBoxGlobal().containsPoint(MOUSE))
            return;
        info.cancelled = true;
        if (e.axis != WL_POINTER_AXIS_VERTICAL_SCROLL || e.delta == 0)
            return;
        const auto POS = toContent(MOUSE);
        if (const auto* P = placedAt(POS))
            P->module->onScroll(e.delta > 0 ? 1 : -1, POS, P->box);
    }));

    m_listeners.emplace_back(ev.input.keyboard.key.listen([this](IKeyboard::SKeyEvent e, Event::SCallbackInfo& info) {
        if (m_open && e.keycode == KEY_ESC && e.state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            info.cancelled = true;
            close();
        }
    }));
}

// ---------------------------------------------------------------- render

void CControlCenter::render(Render::CRenderContext& ctx, PHLMONITOR mon) {
    if (g_pSessionLockManager && g_pSessionLockManager->isSessionLocked()) {
        close();
        return;
    }

    if (mon->m_scale != m_lastScale) {
        m_tex.clear();
        m_lastScale = mon->m_scale;
    }

    const auto                  FG = configColor(g_config.fgColor);
    const CPanelPainter::STheme THEME{
        .fg       = FG,
        .dim      = dimColor(),
        .accent   = configColor(g_config.accentColor),
        .onAccent = onAccentColor(),
        .urgent   = configColor(g_config.urgentColor),
        .surface  = CHyprColor(FG.r, FG.g, FG.b, 0.08),
        .rounding = (double)g_config.rounding->value(),
    };
    CPanelPainter painter(ctx, mon, panelBoxGlobal(), PAD, THEME, m_tex);

    auto bg = configColor(g_config.bgColor);
    bg.a    = 1.F; // the panel sits over content: keep it readable
    painter.background(bg, THEME.rounding * 2);

    for (const auto& p : m_layout)
        p.module->draw(painter, p.box);

    m_tex.endFrame();
}

// ---------------------------------------------------------------- lua

static int luaToggleControlCenter(lua_State*) {
    if (g_pControlCenter)
        g_pControlCenter->toggle();
    return 0;
}

void registerControlCenterLua(HANDLE handle) {
    HyprlandAPI::addLuaFunction(handle, "hyprstatus", "toggle_control_center", luaToggleControlCenter);
}
