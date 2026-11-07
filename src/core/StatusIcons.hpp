#pragma once

// Symbolic icon names (freedesktop / Adwaita) for states shown in several places

#include <algorithm>
#include <string>

namespace StatusIcons {
    inline std::string battery(int percent, bool charging, bool full = false) {
        const int LEVEL = std::clamp((percent + 5) / 10 * 10, 0, 100);
        if (full || (charging && LEVEL == 100))
            return "battery-level-100-charged-symbolic";
        return "battery-level-" + std::to_string(LEVEL) + (charging ? "-charging" : "") + "-symbolic";
    }

    inline std::string volume(float v, bool muted) {
        if (muted || v <= 0.F)
            return "audio-volume-muted-symbolic";
        return v < 0.34F ? "audio-volume-low-symbolic" : v < 0.67F ? "audio-volume-medium-symbolic" : "audio-volume-high-symbolic";
    }

    inline std::string microphone(float v, bool muted) {
        if (muted || v <= 0.F)
            return "microphone-sensitivity-muted-symbolic";
        return v < 0.34F ? "microphone-sensitivity-low-symbolic" : v < 0.67F ? "microphone-sensitivity-medium-symbolic" : "microphone-sensitivity-high-symbolic";
    }
}
