#include "Icons.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
    std::string                                  g_theme = "Adwaita";
    std::unordered_map<std::string, std::string> g_index; // icon name -> file, for g_theme; empty = not built

    std::vector<std::string> iconBaseDirs() {
        std::vector<std::string> out;
        if (const char* home = getenv("HOME")) {
            out.push_back(std::string(home) + "/.local/share/icons");
            out.push_back(std::string(home) + "/.icons");
        }
        const char*        xdg = getenv("XDG_DATA_DIRS");
        std::istringstream ss(xdg && *xdg ? xdg : "/usr/local/share:/usr/share");
        std::string        d;
        while (std::getline(ss, d, ':'))
            if (!d.empty())
                out.push_back(d + "/icons");
        return out;
    }

    // pixel size from a "48x48" path component; svg counts as the largest
    int sizeOf(const fs::path& p) {
        if (p.extension() == ".svg")
            return 1 << 20;
        for (const auto& part : p) {
            const auto S = part.string();
            if (const auto X = S.find('x'); X != std::string::npos && X > 0 && std::isdigit((unsigned char)S[0]))
                return std::atoi(S.c_str());
        }
        return 0;
    }

    // name -> largest file (svg first) under dir; names already in idx win
    void indexDir(std::unordered_map<std::string, std::string>& idx, const fs::path& dir) {
        std::unordered_map<std::string, std::string> local;
        std::error_code                              ec;
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::follow_directory_symlink, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            const auto& P = it->path();
            if (P.extension() != ".svg" && P.extension() != ".png")
                continue;
            auto& slot = local[P.stem().string()];
            if (slot.empty() || sizeOf(P) > sizeOf(slot))
                slot = P.string();
        }
        idx.merge(local);
    }

    // the theme, then hicolor (where apps install their icons)
    const std::unordered_map<std::string, std::string>& index() {
        if (g_index.empty()) {
            for (const auto& theme : {g_theme, std::string{"hicolor"}})
                for (const auto& base : iconBaseDirs())
                    indexDir(g_index, fs::path(base) / theme);
        }
        return g_index;
    }
}

std::string findIcon(const std::string& name, const std::string& extraDir) {
    if (name.empty())
        return "";
    if (name.starts_with("/"))
        return fs::exists(name) ? name : "";
    if (!extraDir.empty()) {
        std::unordered_map<std::string, std::string> idx;
        indexDir(idx, extraDir);
        if (auto it = idx.find(name); it != idx.end())
            return it->second;
    }
    const auto& IDX = index();
    auto        it  = IDX.find(name);
    return it == IDX.end() ? "" : it->second;
}

bool setIconTheme(const std::string& name) {
    const auto THEME = name.empty() ? std::string{"Adwaita"} : name;
    if (THEME == g_theme)
        return false;
    g_theme = THEME;
    g_index.clear(); // rebuilt on the next lookup
    return true;
}

// ---------------------------------------------------------------- symbolic icons

#include <hyprland/src/render/Renderer.hpp>
#include <hyprgraphics/image/Image.hpp>
#include <cairo/cairo.h>

SP<Render::ITexture> symbolicIcons(const std::vector<std::string>& names, int size, const CHyprColor& col, int gap, bool vertical) {
    if (size <= 0)
        return nullptr;

    std::vector<UP<Hyprgraphics::CImage>> images;
    for (const auto& name : names) {
        const auto PATH = findIcon(name);
        if (PATH.empty())
            continue;
        auto img = makeUnique<Hyprgraphics::CImage>(PATH, Vector2D{(double)size, (double)size});
        if (img->success())
            images.push_back(std::move(img));
    }
    if (images.empty())
        return nullptr;

    const int N    = images.size();
    const int LONG = N * size + (N - 1) * gap;
    auto*     surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, vertical ? size : LONG, vertical ? LONG : size);
    auto*     cr   = cairo_create(surf);
    cairo_set_source_rgba(cr, col.r, col.g, col.b, col.a);
    for (int i = 0; i < N; ++i) {
        auto*        src = images[i]->cairoSurface()->cairo();
        const double OFF = i * (size + gap);
        // center icons whose image isn't exactly size x size
        const double DX = (size - cairo_image_surface_get_width(src)) / 2.0, DY = (size - cairo_image_surface_get_height(src)) / 2.0;
        cairo_mask_surface(cr, src, (vertical ? 0 : OFF) + DX, (vertical ? OFF : 0) + DY);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surf);
    auto tex = g_pHyprRenderer->createTexture(surf);
    cairo_surface_destroy(surf);
    return tex;
}
