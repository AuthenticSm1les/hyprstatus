#pragma once

#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/render/Texture.hpp>

#include <functional>
#include <optional>
#include <string>

// One clickable cell on the bar, produced by a module: text, a symbolic icon
// (tinted with fg), or an icon image.
struct SBarItem {
    std::string               text;
    CHyprColor                fg;
    std::optional<CHyprColor> pill; // highlight behind the item

    // symbolic icon name instead of text ("audio-volume-high-symbolic"),
    // drawn status-icon sized and tinted with fg
    std::string symbolic;

    // icon image instead of text: loader is called (GL context current) with the
    // wanted size in px; iconKey identifies the image for caching
    std::string                                   iconKey;
    std::function<SP<Render::ITexture>(int size)> icon;
    bool                                          iconNative = false; // draw the icon texture at its own size, not fitted into a square

    // nonzero: consecutive items with the same id form one button (a group):
    // packed closer together and sharing one pill
    int join = 0;

    // button: linux input code (BTN_LEFT...), pos: cursor in global logical coords
    std::function<void(uint32_t button, const Vector2D& pos)> onClick;
    std::function<void(int dir)>                              onScroll; // +1 down, -1 up

    // filled by the bar during layout (monitor-local physical px)
    SP<Render::ITexture> tex;
    CBox                 box;
};
