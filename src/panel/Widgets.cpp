#include "Widgets.hpp"

#include <algorithm>
#include <cmath>

namespace Widgets {
    void slider(CPanelPainter& p, const CBox& box, float value, bool muted) {
        const auto&  T     = p.theme();
        const double TH    = 6, KNOB = 14;
        const CBox   TRACK = {box.x, box.y + (box.h - TH) / 2, box.w, TH};
        const auto   COL   = muted ? T.dim : T.accent;
        value              = std::clamp(value, 0.F, 1.F);
        p.rect(TRACK, CHyprColor(T.dim.r, T.dim.g, T.dim.b, 0.4), 99);
        p.rect({TRACK.x, TRACK.y, std::max(TH, TRACK.w * value), TH}, COL, 99);
        p.rect({TRACK.x + TRACK.w * value - KNOB / 2, box.y + (box.h - KNOB) / 2, KNOB, KNOB}, COL, 99);
    }

    float sliderValueAt(const CBox& box, double x) {
        return box.w <= 0 ? 0.F : std::clamp((float)((x - box.x) / box.w), 0.F, 1.F);
    }

    void button(CPanelPainter& p, const CBox& box, const std::string& icon, bool active, bool urgent) {
        const auto& T = p.theme();
        p.rect(box, urgent ? T.urgent : active ? T.accent : T.surface, T.rounding);
        p.icon(icon, box, urgent || active ? T.onAccent : T.fg);
    }

    void tile(CPanelPainter& p, const CBox& box, const std::string& icon, const std::string& title, const std::string& state, bool on) {
        const auto&  T     = p.theme();
        const auto   COL   = on ? T.onAccent : T.fg;
        const double ICONW = 40;
        p.rect(box, on ? T.accent : T.surface, T.rounding);
        p.icon(icon, {box.x, box.y, ICONW, box.h}, COL);
        const CBox TEXT = {box.x + ICONW, box.y + 6, box.w - ICONW - 8, (box.h - 12) / 2};
        p.text(title, TEXT, COL, {.align = CPanelPainter::ALIGN_LEFT});
        p.text(state, {TEXT.x, TEXT.y + TEXT.h, TEXT.w, TEXT.h}, on ? T.onAccent : T.dim, {.sizeDelta = -1, .align = CPanelPainter::ALIGN_LEFT});
    }

    CBox cell(const CBox& box, int i, int n) {
        const double W = (box.w - (n - 1) * GAP) / n;
        return {box.x + i * (W + GAP), box.y, W, box.h};
    }

    int cellAt(const CBox& box, double x, int n) {
        for (int i = 0; i < n; ++i) {
            const auto C = cell(box, i, n);
            if (x >= C.x && x < C.x + C.w)
                return i;
        }
        return -1;
    }
}
