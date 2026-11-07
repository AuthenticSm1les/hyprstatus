#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>

// All plugin options (plugin:hyprstatus:*). Read live; never cached.
struct SConfig {
    SP<Config::Values::CBoolValue>   autohide, showOnEmpty, passthrough;
    SP<Config::Values::CIntValue>    height, fontSize, rounding, persistentWorkspaces;
    SP<Config::Values::CStringValue> position, font, iconTheme, modulesLeft, modulesCenter, modulesRight;
    SP<Config::Values::CStringValue> clockFormat, ccIndicators, ccModules;
    SP<Config::Values::CColorValue>  bgColor, fgColor, accentColor, urgentColor;
};

// fixed layout / behavior (logical px, ms)
namespace Style {
    inline constexpr int PADDING           = 8;   // at both ends of the bar
    inline constexpr int SPACING           = 12;  // between modules
    inline constexpr int TITLE_MAX         = 80;  // characters
    inline constexpr int AUTOHIDE_TRIGGER  = 2;   // edge strip that reveals the bar
    inline constexpr int AUTOHIDE_DELAY    = 400; // the bar stays this long after the cursor leaves
    inline constexpr int AUTOHIDE_DURATION = 150; // slide
}

inline SConfig g_config;
inline HANDLE  PHANDLE = nullptr;

void           registerConfig(HANDLE handle);

CHyprColor     configColor(const SP<Config::Values::CColorValue>& v);
CHyprColor     dimColor();      // col.text, faded: empty workspaces, inactive states
CHyprColor     onAccentColor(); // text on accent / urgent pills: the opaque background
