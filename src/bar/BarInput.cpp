// Bar input routing (click, scroll) and autohide (edge hover, slide animation)

#include "Bar.hpp"
#include "Geometry.hpp"
#include "../core/Config.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>

void CStatusBar::setupInput() {
    auto& ev = Event::bus()->m_events;

    m_listeners.emplace_back(ev.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
        if (info.cancelled)
            return; // something above us (e.g. the control center) took it

        // a release belongs to whoever got the press: swallowing the release of a
        // window drag that ends over the bar would leave the drag stuck
        if (e.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            if (m_pressedButtons.erase(e.button))
                info.cancelled = true;
            return;
        }

        PHLMONITOR mon;
        auto*      item = itemAtCursor(mon);
        if (!mon)
            return;

        // the bar owns its strip: swallow presses there so nothing below reacts
        info.cancelled = true;
        m_pressedButtons.insert(e.button);

        if (!item || !item->onClick)
            return;

        auto fn = item->onClick; // the item may be rebuilt while fn runs
        fn(e.button, g_pInputManager->getMouseCoordsInternal());
        markDirty();
    }));

    m_listeners.emplace_back(ev.input.mouse.axis.listen([this](IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
        if (info.cancelled || e.axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
            return;
        PHLMONITOR mon;
        auto*      item = itemAtCursor(mon);
        if (!mon)
            return;
        info.cancelled = true;
        if (!item || !item->onScroll || e.delta == 0)
            return;
        auto fn = item->onScroll;
        fn(e.delta > 0 ? 1 : -1);
        markDirty();
    }));

    m_listeners.emplace_back(ev.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo&) { updateHover(); }));

    m_hideTimer = makeUnique<CWakeTimer>([this] { onHideTimer(); });

    // show_on_empty: workspaces gain or lose windows. Checked a moment later,
    // since a closing window still counts while its close event runs.
    m_emptyCheckTimer = makeUnique<CWakeTimer>([this] { updateHover(); });
    auto recheck      = [this] { m_emptyCheckTimer->arm(std::chrono::milliseconds(50)); };
    m_listeners.emplace_back(ev.window.open.listen([recheck](PHLWINDOW) { recheck(); }));
    m_listeners.emplace_back(ev.window.close.listen([recheck](PHLWINDOW) { recheck(); }));
    m_listeners.emplace_back(ev.window.moveToWorkspace.listen([recheck](PHLWINDOW, PHLWORKSPACE) { recheck(); }));
    m_listeners.emplace_back(ev.workspace.active.listen([recheck](PHLWORKSPACE) { recheck(); }));
}

bool CStatusBar::covers(const Vector2D& global) {
    for (auto& b : m_bars) {
        const auto MON = b.monitor.lock();
        if (MON && shouldDraw(MON) && visibility(b) >= 0.5F && barBoxGlobal(MON).containsPoint(global))
            return true;
    }
    return false;
}

SBarItem* CStatusBar::itemAtCursor(PHLMONITOR& outMon) {
    outMon           = nullptr;
    const auto MOUSE = g_pInputManager->getMouseCoordsInternal();

    for (auto& b : m_bars) {
        const auto MON = b.monitor.lock();
        if (!MON || !shouldDraw(MON) || visibility(b) < 0.5F || !barBoxGlobal(MON).containsPoint(MOUSE))
            continue;

        outMon           = MON;
        const auto LOCAL = (MOUSE - MON->m_position) * MON->m_scale;
        for (auto& it : b.items) {
            if (it.box.containsPoint(LOCAL))
                return &it;
        }
        return nullptr;
    }
    return nullptr;
}

// ---------------------------------------------------------------- autohide

float CStatusBar::visibility(const SMonitorBar& bar) const {
    return g_config.autohide->value() ? bar.shown : 1.F;
}

void CStatusBar::keepShown(PHLMONITOR mon) {
    m_keepShown = mon;
    updateHover(); // reveal now, or start the hide delay once released
}

void CStatusBar::updateHover() {
    if (!g_config.autohide->value() || m_unloading)
        return;

    const auto MOUSE   = g_pInputManager->getMouseCoordsInternal();
    const auto TRIGGER = Style::AUTOHIDE_TRIGGER;
    const auto NOW     = std::chrono::steady_clock::now();
    const auto DELAY   = std::chrono::milliseconds(Style::AUTOHIDE_DELAY);

    for (auto& m : State::monitorState()->monitors()) {
        auto&      bar = barFor(m);
        const auto BOX = barBoxGlobal(m);

        // reveal only from the edge; once shown, the whole bar keeps it open
        const bool EMPTY = g_config.showOnEmpty->value() && m->m_activeWorkspace && m->m_activeWorkspace->getWindowCount() == 0;
        const bool HOVER = barEdgeStripGlobal(m, TRIGGER).containsPoint(MOUSE) || (bar.target && BOX.containsPoint(MOUSE)) || m_keepShown.lock() == m || EMPTY;

        if (HOVER) {
            bar.hideAt.reset();
            if (!bar.target) {
                bar.target    = true;
                bar.lastFrame = NOW;
                g_pHyprRenderer->damageBox(BOX);
            }
        } else if (bar.target && !bar.hideAt) {
            bar.hideAt = NOW + DELAY;
            m_hideTimer->arm(DELAY);
        }
    }
}

void CStatusBar::onHideTimer() {
    const auto                                           NOW = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> next;

    for (auto& b : m_bars) {
        if (!b.hideAt)
            continue;
        if (*b.hideAt <= NOW) {
            b.hideAt.reset();
            b.target    = false;
            b.lastFrame = NOW;
            if (const auto MON = b.monitor.lock())
                g_pHyprRenderer->damageBox(barBoxGlobal(MON));
        } else if (!next || *b.hideAt < *next)
            next = b.hideAt;
    }

    if (next)
        m_hideTimer->arm(std::chrono::duration_cast<std::chrono::milliseconds>(*next - NOW) + std::chrono::milliseconds(1));
}

// advance the slide toward its target; keeps frames coming until it settles
void CStatusBar::animate(SMonitorBar& bar, PHLMONITOR mon) {
    if (!g_config.autohide->value())
        return;

    const auto  NOW  = std::chrono::steady_clock::now();
    const float DUR  = Style::AUTOHIDE_DURATION;
    const float GOAL = bar.target ? 1.F : 0.F;
    const float STEP = DUR <= 0 ? 1.F : std::chrono::duration<float, std::milli>(NOW - bar.lastFrame).count() / DUR;
    bar.lastFrame    = NOW;

    if (bar.shown != GOAL) {
        bar.shown = GOAL > bar.shown ? std::min(GOAL, bar.shown + STEP) : std::max(GOAL, bar.shown - STEP);
        g_pHyprRenderer->damageBox(barBoxGlobal(mon));
    }
}
