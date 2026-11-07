#pragma once

#include <string>

// Icon file for a name: from `extraDir` (an app's own icon dir) if given,
// else icon_theme, else hicolor; svg preferred, else the largest png.
// "" if not found. The theme is indexed once.
std::string findIcon(const std::string& name, const std::string& extraDir = "");

// Icon theme to use ("" = Adwaita). Returns true if it changed (cached
// textures are stale).
bool setIconTheme(const std::string& name);

#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>

#include <vector>

namespace Render {
    class ITexture;
}

// Symbolic icons ("audio-volume-high-symbolic"...) tinted with `col`, the way
// GTK colors them: the icon's alpha is a mask for the color. Several names
// make one strip, `gap` px apart, in a row or a column. Unknown names are
// skipped; nullptr if none was found. Call with the GL context current.
SP<Render::ITexture> symbolicIcons(const std::vector<std::string>& names, int size, const CHyprColor& col, int gap = 0, bool vertical = false);
