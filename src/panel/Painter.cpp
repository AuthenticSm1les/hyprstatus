#include "Painter.hpp"
#include "../core/Config.hpp"
#include "../services/Icons.hpp"

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/output/Monitor.hpp>

#include <cmath>
#include <format>

SP<Render::ITexture> CTextureCache::get(const std::string& key, const std::function<SP<Render::ITexture>()>& make) {
    if (auto it = m_next.find(key); it != m_next.end())
        return it->second;
    auto it  = m_cur.find(key);
    auto tex = it != m_cur.end() ? it->second : make();
    return m_next[key] = tex;
}

void CTextureCache::endFrame() {
    m_cur = std::move(m_next);
    m_next.clear();
}

void CTextureCache::clear() {
    m_cur.clear();
    m_next.clear();
}

CPanelPainter::CPanelPainter(Render::CRenderContext& ctx, PHLMONITOR mon, const CBox& panel, double pad, const STheme& theme, CTextureCache& cache) :
    m_ctx(ctx), m_mon(mon), m_panel(panel), m_origin(panel.pos() + Vector2D{pad, pad}), m_theme(theme), m_cache(cache) {}

CBox CPanelPainter::toLocal(const CBox& global) const {
    return global.copy().translate(-m_mon->m_position).scale(m_mon->m_scale).round();
}

void CPanelPainter::fill(const CBox& global, const CHyprColor& col, double rounding) {
    CRectPassElement::SRectData d;
    d.box   = toLocal(global);
    d.color = col;
    d.round = std::min<int>(std::round(rounding * m_mon->m_scale), std::min(d.box.w, d.box.h) / 2);
    m_ctx.m_pass.add(makeUnique<CRectPassElement>(d));
}

void CPanelPainter::background(const CHyprColor& col, double rounding) {
    fill(m_panel, col, rounding);
}

void CPanelPainter::rect(const CBox& box, const CHyprColor& col, double rounding) {
    fill(box.copy().translate(m_origin), col, rounding);
}

void CPanelPainter::text(const std::string& s, const CBox& box, const CHyprColor& col, const SText& opts) {
    if (s.empty())
        return;
    const int   PT   = std::round(std::max(1.0, (double)g_config.fontSize->value() + opts.sizeDelta) * m_mon->m_scale);
    const auto& FONT = g_config.font->value();
    const int   MAXW = opts.ellipsize ? std::max(1, (int)std::floor(box.w * m_mon->m_scale)) : 0;
    place(m_cache.get(std::format("t|{}|{}|{:x}|{}|{}", s, FONT, col.getAsHex(), PT, MAXW), [&] { return g_pHyprRenderer->renderText(s, col, PT, false, FONT, MAXW); }), box, opts.align);
}

void CPanelPainter::icon(const std::string& name, const CBox& box, const CHyprColor& col, double size) {
    const int PX = std::round(size * m_mon->m_scale);
    place(m_cache.get(std::format("i|{}|{:x}|{}", name, col.getAsHex(), PX), [&] { return symbolicIcons({name}, PX, col); }), box, ALIGN_CENTER);
}

void CPanelPainter::place(const SP<Render::ITexture>& tex, const CBox& box, eAlign align) {
    if (!tex || tex->m_texID == 0)
        return;
    const CBox   L = toLocal(box.copy().translate(m_origin));
    const double X = align == ALIGN_LEFT ? L.x : align == ALIGN_RIGHT ? L.x + L.w - tex->m_size.x : L.x + (L.w - tex->m_size.x) / 2;
    CTexPassElement::SRenderData d;
    d.tex        = tex;
    d.box        = CBox{Vector2D{X, L.y + (L.h - tex->m_size.y) / 2}.round(), tex->m_size};
    d.a          = 1.F;
    d.clipRegion = CRegion(toLocal(m_panel));
    m_ctx.m_pass.add(makeUnique<CTexPassElement>(std::move(d)));
}
