#pragma once

// Building blocks shared by panel modules

#include "Painter.hpp"

#include <string>

namespace Widgets {
    inline constexpr double ROW_H    = 36; // slider rows
    inline constexpr double TILE_H   = 48; // toggle tiles
    inline constexpr double BUTTON_H = 40; // button rows
    inline constexpr double GAP      = 10;

    // track + knob, value 0..1
    void   slider(CPanelPainter& p, const CBox& box, float value, bool muted);
    float  sliderValueAt(const CBox& box, double x);

    // button with a symbolic icon
    void   button(CPanelPainter& p, const CBox& box, const std::string& icon, bool active, bool urgent = false);

    // toggle tile: symbolic icon, title, and a dim state line below
    void   tile(CPanelPainter& p, const CBox& box, const std::string& icon, const std::string& title, const std::string& state, bool on);

    // split a box into n equal cells along x, GAP apart
    CBox   cell(const CBox& box, int i, int n);
    int    cellAt(const CBox& box, double x, int n); // -1 if in a gap
}
