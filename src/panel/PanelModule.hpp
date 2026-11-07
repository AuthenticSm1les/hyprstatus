#pragma once

#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Painter.hpp"
#include "../core/Shared.hpp"

// A section of the control center (controlcenter_modules). Modules exist only
// while the panel is open, so their backends do too. Positions and boxes are
// logical px relative to the panel's content area.
class IPanelModule {
  public:
    virtual ~IPanelModule() = default;

    // half width; consecutive tiles share a row
    virtual bool   tile() const { return false; }
    virtual double height() const = 0;
    virtual bool   visible() const { return true; }

    virtual void   draw(CPanelPainter& p, const CBox& box) = 0;

    // left button pressed inside box; return true to receive onDrag until release
    virtual bool onPress(const Vector2D& pos, const CBox& box) {
        return false;
    }
    virtual void onDrag(const Vector2D& pos, const CBox& box) {}
    virtual void onScroll(int dir, const Vector2D& pos, const CBox& box) {} // +1 down

    // set by the panel
    std::function<void()> m_onChanged; // redraw, re-layout
    std::function<void()> m_close;     // close the panel

  protected:
    void changed() {
        if (m_onChanged)
            m_onChanged();
    }
    void closePanel() {
        if (m_close)
            m_close();
    }

    // the shared backend T, redrawing whenever it changes
    template <class T>
    SP<T> use() {
        auto svc = sharedInstance<T>();
        m_serviceListeners.emplace_back(svc->m_changed.listen([this] { changed(); }));
        return svc;
    }

  private:
    std::vector<CHyprSignalListener> m_serviceListeners;
};

// "volume", "wifi", ... -> module, or nullptr if unknown
UP<IPanelModule> createPanelModule(const std::string& name);
