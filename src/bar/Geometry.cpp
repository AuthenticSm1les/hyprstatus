#include "Geometry.hpp"
#include "../core/Config.hpp"

#include <hyprland/src/output/Monitor.hpp>

#include <algorithm>

eEdge barEdge() {
    const auto& P = g_config.position->value();
    return P == "bottom" ? EDGE_BOTTOM : P == "left" ? EDGE_LEFT : P == "right" ? EDGE_RIGHT : EDGE_TOP;
}

bool barVertical() {
    const auto E = barEdge();
    return E == EDGE_LEFT || E == EDGE_RIGHT;
}

static CBox edgeBox(const CBox& area, double thick, double insetA, double insetB) {
    switch (barEdge()) {
        case EDGE_TOP: return {area.x + insetA, area.y, area.w - insetA - insetB, thick};
        case EDGE_BOTTOM: return {area.x + insetA, area.y + area.h - thick, area.w - insetA - insetB, thick};
        case EDGE_LEFT: return {area.x, area.y + insetA, thick, area.h - insetA - insetB};
        case EDGE_RIGHT: return {area.x + area.w - thick, area.y + insetA, thick, area.h - insetA - insetB};
    }
    return {};
}

// reservations on the two edges perpendicular to the bar, logical px
static std::pair<double, double> perpendicularInsets(PHLMONITOR mon) {
    const auto& R = mon->m_reservedArea;
    return barVertical() ? std::pair{R.top(), R.bottom()} : std::pair{R.left(), R.right()};
}

CBox barBoxGlobal(PHLMONITOR mon) {
    const auto [A, B] = perpendicularInsets(mon);
    return edgeBox({mon->m_position, mon->m_size}, g_config.height->value(), A, B);
}

CBox barBoxLocal(PHLMONITOR mon) {
    const auto [A, B] = perpendicularInsets(mon);
    const double S    = mon->m_scale;
    return edgeBox({{0, 0}, mon->m_transformedSize}, std::round(g_config.height->value() * S), std::round(A * S), std::round(B * S));
}

CBox barEdgeStripGlobal(PHLMONITOR mon, double depth) {
    const auto [A, B] = perpendicularInsets(mon);
    return edgeBox({mon->m_position, mon->m_size}, depth, A, B);
}

CBox popupBoxGlobal(PHLMONITOR mon, const Vector2D& size, std::optional<Vector2D> anchor, double gap) {
    const CBox   BAR = barBoxGlobal(mon);
    const double W = size.x, H = size.y;

    auto along = [&](double start, double len, double sz, std::optional<double> center) {
        const double V = center ? *center - sz / 2 : start + len - sz; // default: the bar's end
        return std::clamp(V, start, std::max(start, start + len - sz));
    };
    const auto AX = anchor ? std::optional(anchor->x) : std::nullopt;
    const auto AY = anchor ? std::optional(anchor->y) : std::nullopt;

    switch (barEdge()) {
        case EDGE_LEFT: return {BAR.x + BAR.w + gap, along(BAR.y, BAR.h, H, AY), W, H};
        case EDGE_RIGHT: return {BAR.x - gap - W, along(BAR.y, BAR.h, H, AY), W, H};
        case EDGE_TOP: return {along(BAR.x, BAR.w, W, AX), BAR.y + BAR.h + gap, W, H};
        case EDGE_BOTTOM: return {along(BAR.x, BAR.w, W, AX), BAR.y - gap - H, W, H};
    }
    return {};
}
