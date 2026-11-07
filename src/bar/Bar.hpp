#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Item.hpp"
#include "../core/EventLoop.hpp"

class IModule;
namespace Render {
    class CRenderContext;
}

struct SMonitorBar {
    PHLMONITORREF                                         monitor;
    std::vector<SBarItem>                                 items;
    std::unordered_map<std::string, SP<Render::ITexture>> texCache;
    bool                                                  needsLayout = true;
    float                                                 lastScale   = 0.F;

    // autohide
    float                                                 shown  = 0.F; // 0 hidden .. 1 shown
    bool                                                  target = false;
    std::optional<std::chrono::steady_clock::time_point>  hideAt;
    std::chrono::steady_clock::time_point                 lastFrame;
};

// The bar: owns the configured modules, lays out their items per monitor,
// draws them, routes input, handles autohide and the reserved strip.
// It knows nothing about what any module shows.
class CStatusBar {
  public:
    CStatusBar();
    ~CStatusBar();

    void markDirty();

    // from the reserved-area hook, after Hyprland arranged a monitor
    void onMonitorArranged(PHLMONITOR mon);
    void rearrangeAll();

    // the bar is drawn (and visible) under this global point
    bool covers(const Vector2D& global);

    // autohide: keep the bar out on this monitor (e.g. while a panel attached
    // to it is open), regardless of the cursor; nullptr releases it
    void keepShown(PHLMONITOR mon);

    bool m_unloading = false;

  private:
    // Bar.cpp
    void                  syncModules();
    void                  onConfigChanged();
    void                  checkGeometryChanged();
    SMonitorBar&          barFor(PHLMONITOR mon);
    void                  layout(SMonitorBar& bar, PHLMONITOR mon);
    void                  render(Render::CRenderContext& ctx, PHLMONITOR mon);
    bool                  shouldDraw(PHLMONITOR mon) const;
    int                   reservedThickness() const;
    void                  damageAll();
    void                  relayout(); // re-lay out dirty bars now, damaging only what changed

    // BarInput.cpp
    void                  setupInput();
    SBarItem*             itemAtCursor(PHLMONITOR& outMon);
    void                  updateHover();
    void                  onHideTimer();
    float                 visibility(const SMonitorBar& bar) const;
    void                  animate(SMonitorBar& bar, PHLMONITOR mon);

    std::vector<std::string>                 m_moduleOrder[3]; // left, center, right
    std::map<std::string, UP<IModule>>       m_modules;
    std::vector<SMonitorBar>                 m_bars;
    std::vector<CHyprSignalListener>         m_listeners;
    UP<CWakeTimer>                           m_hideTimer, m_emptyCheckTimer, m_relayoutTimer;
    PHLMONITORREF                            m_keepShown;
    std::unordered_set<uint32_t>             m_pressedButtons; // presses we swallowed

    // geometry last applied to the reserved area
    std::string                              m_appliedPosition;
    int64_t                                  m_appliedHeight  = -1;
    bool                                     m_appliedAutohide = false;
};

inline UP<CStatusBar> g_pStatusBar;
