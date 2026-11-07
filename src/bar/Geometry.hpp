#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/math/Math.hpp>

#include <optional>

// Where the bar sits, derived from config + monitor. Shared by the bar and by
// anything anchored to it (control center panel).

enum eEdge : uint8_t {
    EDGE_TOP,
    EDGE_BOTTOM,
    EDGE_LEFT,
    EDGE_RIGHT,
};

eEdge barEdge();
bool  barVertical();

// The bar starts after space other bars reserve on the perpendicular edges,
// so e.g. a left bar sits below a top layer-shell bar instead of under it.
CBox barBoxGlobal(PHLMONITOR mon);           // logical, global
CBox barBoxLocal(PHLMONITOR mon);            // physical, monitor-local
CBox barEdgeStripGlobal(PHLMONITOR mon, double depth); // thin strip at the screen edge

// A popup of `size` hanging off the bar, `gap` away from it, centered on
// `anchor` along the bar (kept within it); no anchor: at the bar's end.
CBox popupBoxGlobal(PHLMONITOR mon, const Vector2D& size, std::optional<Vector2D> anchor, double gap);
