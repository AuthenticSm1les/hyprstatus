#pragma once

#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>

#include <functional>
#include <unordered_set>
#include <vector>

// Keeps the pointer from windows underneath our UI: while `covers(cursor)` is
// true, pointer motion isn't delivered below (no hover effects, no focus
// follows mouse) and the window gets a leave, as over empty space. Stays out
// of the way while a button pressed elsewhere is held (window drags, text
// selections crossing the bar).
class CPointerHold {
  public:
    explicit CPointerHold(std::function<bool(const Vector2D&)> covers);

  private:
    std::function<bool(const Vector2D&)> m_covers;
    std::unordered_set<uint32_t>         m_foreignPresses;
    bool                                 m_holding = false;
    std::vector<CHyprSignalListener>     m_listeners;
};

inline UP<CPointerHold> g_pPointerHold;
