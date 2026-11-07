#pragma once

#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <functional>
#include <string>
#include <unordered_map>

namespace Render {
    class CRenderContext;
    class ITexture;
}

// Text / icon textures by key. Whatever wasn't used during a frame is dropped
// at endFrame(), so the cache holds exactly what's on screen.
class CTextureCache {
  public:
    SP<Render::ITexture> get(const std::string& key, const std::function<SP<Render::ITexture>()>& make);
    void                 endFrame();
    void                 clear();

  private:
    std::unordered_map<std::string, SP<Render::ITexture>> m_cur, m_next;
};

// Draws one frame of the control center onto Hyprland's render pass. Boxes
// are logical px relative to the panel's content area.
class CPanelPainter {
  public:
    struct STheme {
        CHyprColor fg, dim, accent, onAccent, urgent;
        CHyprColor surface; // button / tile background when off
        double     rounding = 6;
    };

    enum eAlign : uint8_t {
        ALIGN_CENTER,
        ALIGN_LEFT,
        ALIGN_RIGHT,
    };
    struct SText {
        int    sizeDelta = 0; // pt relative to font_size
        eAlign align     = ALIGN_CENTER;
        bool   ellipsize = false; // cut to the box's width
    };

    static constexpr double ICON_SIZE = 16;

    // panel: the whole panel (global logical); content starts `pad` inside it
    CPanelPainter(Render::CRenderContext& ctx, PHLMONITOR mon, const CBox& panel, double pad, const STheme& theme, CTextureCache& cache);

    const STheme& theme() const {
        return m_theme;
    }

    void background(const CHyprColor& col, double rounding); // the whole panel
    void rect(const CBox& box, const CHyprColor& col, double rounding);
    void text(const std::string& s, const CBox& box, const CHyprColor& col, const SText& opts);
    void text(const std::string& s, const CBox& box, const CHyprColor& col) {
        text(s, box, col, SText{});
    }
    void icon(const std::string& name, const CBox& box, const CHyprColor& col, double size = ICON_SIZE); // symbolic icon, centered

  private:
    CBox                    toLocal(const CBox& global) const; // -> monitor-local physical px
    void                    fill(const CBox& global, const CHyprColor& col, double rounding);
    void                    place(const SP<Render::ITexture>& tex, const CBox& box, eAlign align);

    Render::CRenderContext& m_ctx;
    PHLMONITOR              m_mon;
    CBox                    m_panel;
    Vector2D                m_origin; // content area, global logical
    STheme                  m_theme;
    CTextureCache&          m_cache;
};
