#include "TrayMenu.hpp"
#include "ControlCenter.hpp"
#include "../bar/Geometry.hpp"
#include "../core/Config.hpp"
#include "../services/DbusMenu.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <hyprgraphics/resource/resources/TextResource.hpp>

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cmath>

// logical px
static constexpr double PAD     = 6;
static constexpr double ROW_H   = 30;
static constexpr double SEP_H   = 9;
static constexpr double MARK_W  = 26; // check / radio / back arrow column
static constexpr double ARROW_W = 22; // submenu arrow column
static constexpr double TEXT_IN = 10; // text inset when there's no mark column
static constexpr double MIN_W   = 160;
static constexpr double MAX_W   = 420;
static constexpr double BAR_GAP = 8;

// width of a label in logical px, the way the painter will draw it
static double textWidth(const std::string& s) {
    if (s.empty())
        return 0;
    Hyprgraphics::CTextResource res({
        .text     = s,
        .font     = g_config.font->value(),
        .fontSize = (size_t)std::max<int64_t>(1, g_config.fontSize->value()),
        .wrap     = false,
    });
    res.render();
    return res.m_asset.pixelSize.x;
}

// ---------------------------------------------------------------- lifecycle

CTrayMenu::CTrayMenu() {
    m_reapTimer = makeUnique<CWakeTimer>([this] { m_retired.clear(); });

    auto& ev = Event::bus()->m_events;
    m_listeners.emplace_back(ev.render.stage.listen([this](const Event::SRenderStageEvent& e) {
        if (shown() && e.stage == RENDER_POST_WINDOWS && e.context && e.monitor && e.monitor == m_monitor.lock())
            render(e.context->get(), e.monitor);
    }));
    m_listeners.emplace_back(ev.monitor.removed.listen([this](PHLMONITOR m) {
        if (m == m_monitor.lock())
            close();
    }));
}

CTrayMenu::~CTrayMenu() {
    m_listeners.clear();
    close();
    m_retired.clear();
}

void CTrayMenu::toggle(PHLMONITOR mon, const Vector2D& anchor, SP<CBus> bus, const std::string& service, const std::string& path, const std::string& owner) {
    m_toggledInPress = true;
    const bool SAME  = m_open && m_owner == owner;
    close();
    if (SAME || !mon || !bus)
        return;

    if (g_pControlCenter && g_pControlCenter->isOpen())
        g_pControlCenter->close();

    m_open    = true;
    m_monitor = mon;
    m_anchor  = anchor;
    m_owner   = owner;
    m_stack.clear();
    m_hover = -1;
    // shown once the tree arrives
    m_menu = makeUnique<CDbusMenu>(bus, service, path, [this] { changed(); }, [this] { close(); });
    m_openChanged.emit();
}

void CTrayMenu::close() {
    if (!m_open)
        return;
    damage();
    m_open = false;
    m_owner.clear();
    m_rows.clear();
    m_stack.clear();
    m_hover = -1;
    m_tex.clear();
    // may be called from one of the menu's own D-Bus callbacks
    if (m_menu) {
        m_retired.push_back(std::move(m_menu));
        m_reapTimer->arm(std::chrono::milliseconds(1));
    }
    m_openChanged.emit();
}

bool CTrayMenu::shown() const {
    return m_open && m_menu && m_menu->loaded() && !m_rows.empty();
}

void CTrayMenu::changed() {
    if (!m_open)
        return;
    damage(); // old size
    layout();
    updateHover();
    damage(); // new size
}

void CTrayMenu::damage() {
    if (shown() && g_pHyprRenderer && m_monitor)
        g_pHyprRenderer->damageBox(boxGlobal().expand(2));
}

// ---------------------------------------------------------------- layout

// the submenu being shown; falls back out of submenus that went away
const SMenuNode* CTrayMenu::current() const {
    if (!m_menu || !m_menu->loaded())
        return nullptr;
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it)
        if (const auto* N = m_menu->find(*it))
            return N;
    return &m_menu->root();
}

void CTrayMenu::layout() {
    m_rows.clear();
    const auto* NODE = current();
    if (!NODE)
        return;

    // the back row names the submenu
    if (NODE != &m_menu->root())
        m_rows.push_back({.kind = ROW_BACK, .id = NODE->id, .label = NODE->label});

    for (const auto& c : NODE->children) {
        if (!c.visible)
            continue;
        if (c.separator) {
            // no leading, trailing or doubled separators
            if (!m_rows.empty() && m_rows.back().kind != ROW_SEPARATOR)
                m_rows.push_back({.kind = ROW_SEPARATOR, .id = c.id});
            continue;
        }
        m_rows.push_back({
            .kind    = ROW_ITEM,
            .id      = c.id,
            .label   = c.label,
            .enabled = c.enabled,
            .submenu = c.submenu,
            .check   = c.toggle == SMenuNode::TOGGLE_CHECK,
            .radio   = c.toggle == SMenuNode::TOGGLE_RADIO,
            .checked = c.checked,
        });
    }
    while (!m_rows.empty() && m_rows.back().kind == ROW_SEPARATOR)
        m_rows.pop_back();

    bool   marks = false, arrows = false;
    double textW = 0;
    for (const auto& r : m_rows) {
        marks  = marks || r.check || r.radio || r.kind == ROW_BACK;
        arrows = arrows || r.submenu;
        if (r.kind != ROW_SEPARATOR)
            textW = std::max(textW, textWidth(r.label));
    }
    const double W = std::clamp((marks ? MARK_W : TEXT_IN) + textW + (arrows ? ARROW_W : TEXT_IN) + 4, MIN_W, MAX_W);

    double y = 0;
    for (auto& r : m_rows) {
        const double H = r.kind == ROW_SEPARATOR ? SEP_H : ROW_H;
        r.box          = {0, y, W, H};
        y += H;
    }
    m_size = {W + 2 * PAD, y + 2 * PAD};
}

CBox CTrayMenu::boxGlobal() const {
    const auto MON = m_monitor.lock();
    return MON ? popupBoxGlobal(MON, m_size, m_anchor, BAR_GAP) : CBox{};
}

Vector2D CTrayMenu::toContent(const Vector2D& global) const {
    return global - boxGlobal().pos() - Vector2D{PAD, PAD};
}

const CTrayMenu::SRow* CTrayMenu::rowAt(const Vector2D& content) const {
    for (const auto& r : m_rows)
        if (r.box.containsPoint(content))
            return &r;
    return nullptr;
}

// ---------------------------------------------------------------- actions

void CTrayMenu::activate(const SRow& row) {
    if (row.kind == ROW_BACK) {
        back();
        return;
    }
    if (row.kind != ROW_ITEM || !row.enabled)
        return;
    if (row.submenu) {
        const int ID = row.id;
        m_menu->aboutToShow(ID);
        m_stack.push_back(ID);
        changed();
        return;
    }
    m_menu->clicked(row.id);
    close();
}

void CTrayMenu::back() {
    if (m_stack.empty())
        return;
    m_stack.pop_back();
    changed();
}

void CTrayMenu::updateHover() {
    int idx = -1;
    if (shown()) {
        const auto POS = toContent(g_pInputManager->getMouseCoordsInternal());
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const auto& R = m_rows[i];
            if (R.box.containsPoint(POS) && R.kind != ROW_SEPARATOR && R.enabled)
                idx = (int)i;
        }
    }
    if (idx != m_hover) {
        m_hover = idx;
        damage();
    }
}

// ---------------------------------------------------------------- input

void CTrayMenu::setupInput() {
    auto& ev = Event::bus()->m_events;

    m_listeners.emplace_back(ev.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
        if (e.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            if (m_pressedButtons.erase(e.button))
                info.cancelled = true;
            return;
        }

        // the bar ran first: a click on our own tray icon toggled us already
        const bool TOGGLED = std::exchange(m_toggledInPress, false);
        if (!m_open || TOGGLED)
            return;

        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        if (info.cancelled || !shown() || !boxGlobal().containsPoint(MOUSE)) {
            close(); // click elsewhere (the bar included): dismiss, and let the click through
            return;
        }

        info.cancelled = true;
        m_pressedButtons.insert(e.button);
        if (e.button == BTN_LEFT || e.button == BTN_RIGHT) {
            if (const auto* R = rowAt(toContent(MOUSE))) {
                const SRow ROW = *R; // activate() may rebuild m_rows
                activate(ROW);
            }
        } else if (e.button == BTN_SIDE)
            back();
    }));

    m_listeners.emplace_back(ev.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo&) {
        if (m_open)
            updateHover();
    }));

    m_listeners.emplace_back(ev.input.mouse.axis.listen([this](IPointer::SAxisEvent, Event::SCallbackInfo& info) {
        if (!info.cancelled && covers(g_pInputManager->getMouseCoordsInternal()))
            info.cancelled = true;
    }));

    m_listeners.emplace_back(ev.input.keyboard.key.listen([this](IKeyboard::SKeyEvent e, Event::SCallbackInfo& info) {
        if (!m_open || e.state != WL_KEYBOARD_KEY_STATE_PRESSED)
            return;
        if (e.keycode == KEY_ESC) {
            info.cancelled = true;
            if (m_stack.empty())
                close();
            else
                back();
        }
    }));
}

// ---------------------------------------------------------------- render

void CTrayMenu::render(Render::CRenderContext& ctx, PHLMONITOR mon) {
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
    CPanelPainter p(ctx, mon, boxGlobal(), PAD, THEME, m_tex);

    auto bg = configColor(g_config.bgColor);
    bg.a    = 1.F; // over content: keep it readable
    p.background(bg, THEME.rounding * 1.5);

    bool marks = false, arrows = false;
    for (const auto& r : m_rows) {
        marks  = marks || r.check || r.radio || r.kind == ROW_BACK;
        arrows = arrows || r.submenu;
    }
    const double TEXT_X = marks ? MARK_W : TEXT_IN;
    const double TEXT_R = arrows ? ARROW_W : TEXT_IN;

    for (size_t i = 0; i < m_rows.size(); ++i) {
        const auto& R = m_rows[i];
        const auto& B = R.box;

        if (R.kind == ROW_SEPARATOR) {
            p.rect({B.x + 4, B.y + std::floor(B.h / 2), B.w - 8, 1}, CHyprColor(FG.r, FG.g, FG.b, 0.15), 0);
            continue;
        }

        const bool HOVER = (int)i == m_hover;
        if (HOVER)
            p.rect(B, THEME.accent, THEME.rounding);
        const auto COL = HOVER ? THEME.onAccent : R.enabled ? FG : THEME.dim;

        const CBox MARK = {B.x, B.y, MARK_W, B.h};
        if (R.kind == ROW_BACK)
            p.icon("go-previous-symbolic", MARK, COL);
        else if (R.checked && R.check)
            p.icon("object-select-symbolic", MARK, COL);
        else if (R.checked && R.radio)
            p.icon("media-record-symbolic", MARK, COL, 10);

        p.text(R.label, {B.x + TEXT_X, B.y, B.w - TEXT_X - TEXT_R, B.h}, COL, {.align = CPanelPainter::ALIGN_LEFT, .ellipsize = true});

        if (R.submenu)
            p.icon("go-next-symbolic", {B.x + B.w - ARROW_W, B.y, ARROW_W, B.h}, COL);
        // the back row's title sits over a line, like a header
        if (R.kind == ROW_BACK)
            p.rect({B.x + 4, B.y + B.h - 1, B.w - 8, 1}, CHyprColor(FG.r, FG.g, FG.b, 0.15), 0);
    }

    m_tex.endFrame();
}
