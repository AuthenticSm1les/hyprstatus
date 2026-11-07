#include "PointerHold.hpp"

#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

// Created after the bar and the panel, so their button listeners have already
// claimed (cancelled) presses on our UI when ours runs.
CPointerHold::CPointerHold(std::function<bool(const Vector2D&)> covers) : m_covers(std::move(covers)) {
    auto& ev = Event::bus()->m_events;

    m_listeners.emplace_back(ev.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
        if (e.state == WL_POINTER_BUTTON_STATE_RELEASED)
            m_foreignPresses.erase(e.button);
        else if (!info.cancelled)
            m_foreignPresses.insert(e.button);
    }));

    m_listeners.emplace_back(ev.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo& info) {
        if (!m_foreignPresses.empty() || !m_covers(g_pInputManager->getMouseCoordsInternal())) {
            m_holding = false; // Hyprland refocuses whatever is under the cursor
            return;
        }
        if (!m_holding) {
            m_holding = true;
            g_pSeatManager->setPointerFocus(nullptr, {});
            g_pHyprRenderer->setCursorFromName("left_ptr");
        }
        info.cancelled = true;
    }));
}
