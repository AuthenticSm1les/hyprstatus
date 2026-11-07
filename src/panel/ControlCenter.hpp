#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <hyprutils/signal/Signal.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/EventLoop.hpp"
#include "Painter.hpp"
#include "../core/Config.hpp"

class IPanelModule;
namespace Render {
    class CRenderContext;
    class ITexture;
}

// A group's menu: a panel anchored next to the bar. A container: what it shows
// is the list of panel modules in its option (see modules/),
// stacked as rows, with consecutive tiles paired side by side. Drawn above
// windows (and the bar), below top/overlay layer surfaces. Its modules, and
// so their backends, exist only while it is open.
class CControlCenter {
  public:
    explicit CControlCenter(SP<Config::Values::CStringValue> modules);
    ~CControlCenter();

    // anchor: global logical point on the bar to center on (e.g. the click);
    // empty = the end of the bar on the focused monitor (keybind)
    void                          toggle(PHLMONITOR mon = nullptr, std::optional<Vector2D> anchor = {});
    void                          open(PHLMONITOR mon, std::optional<Vector2D> anchor);
    void                          close();
    bool                          isOpen() const { return m_open; }
    PHLMONITOR                    monitor() const { return m_open ? m_monitor.lock() : nullptr; }
    bool                          covers(const Vector2D& global) const { return m_open && panelBoxGlobal().containsPoint(global); }

    Hyprutils::Signal::CSignalT<> m_openChanged;

  private:
    struct SPlaced {
        IPanelModule* module = nullptr;
        CBox          box; // logical, relative to the content area
    };

    void                 createModules();
    void                 retireModules();
    void                 layout();
    void                 changed();
    void                 damage();
    void                 render(Render::CRenderContext& ctx, PHLMONITOR mon);
    void                 setupInput();
    CBox                 panelBoxGlobal() const;
    Vector2D             toContent(const Vector2D& global) const;
    const SPlaced*       placedAt(const Vector2D& content) const;
    const SPlaced*       placedFor(const IPanelModule* m) const;

    bool                                                  m_open = false;
    PHLMONITORREF                                         m_monitor;
    std::optional<Vector2D>                               m_anchor;
    Vector2D                                              m_size;

    SP<Config::Values::CStringValue>                      m_modulesOption;
    std::string                                           m_modulesConf;
    std::vector<UP<IPanelModule>>                         m_modules;
    std::vector<UP<IPanelModule>>                         m_retired; // closed from inside a module: freed on the next tick
    UP<CWakeTimer>                                        m_reapTimer;
    std::vector<SPlaced>                                  m_layout;
    IPanelModule*                                         m_dragging = nullptr;
    std::unordered_set<uint32_t>                          m_pressedButtons;

    float                                                 m_lastScale = 0.F;
    CTextureCache                                         m_tex;

    std::vector<CHyprSignalListener>                      m_listeners;
};

inline UP<CControlCenter> g_pControlCenter;

// hl.plugin.hyprstatus.toggle_control_center()
void registerControlCenterLua(HANDLE handle);
