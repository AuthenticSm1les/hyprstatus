#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <hyprutils/signal/Signal.hpp>

#include <string>
#include <unordered_set>
#include <vector>

#include "../core/Bus.hpp"
#include "../core/EventLoop.hpp"
#include "Painter.hpp"

class CDbusMenu;
struct SMenuNode;
namespace Render {
    class CRenderContext;
}

// A tray item's dbusmenu, drawn as a popup next to the bar where it was
// clicked. Submenus open in place, with a back row on top. Closes on a click
// outside it (the click goes through), Escape, choosing an entry, the app
// quitting, or the session locking.
class CTrayMenu {
  public:
    CTrayMenu();
    ~CTrayMenu();

    // register the button listener after the bar's, so a click on the bar
    // reaches the tray icon (toggling this menu) before we look at it
    void                          setupInput();

    // owner: the tray item's id; the same owner again closes the menu
    void                          toggle(PHLMONITOR mon, const Vector2D& anchor, SP<CBus> bus, const std::string& service, const std::string& path, const std::string& owner);
    void                          close();
    bool                          isOpen() const { return m_open; }
    PHLMONITOR                    monitor() const { return m_open ? m_monitor.lock() : nullptr; }
    bool                          covers(const Vector2D& global) const { return shown() && boxGlobal().containsPoint(global); }

    Hyprutils::Signal::CSignalT<> m_openChanged;

  private:
    enum eRowKind : uint8_t {
        ROW_ITEM,
        ROW_SEPARATOR,
        ROW_BACK,
    };
    struct SRow {
        eRowKind    kind = ROW_ITEM;
        int         id   = 0;
        std::string label;
        bool        enabled = true, submenu = false, check = false, radio = false, checked = false;
        CBox        box; // logical, relative to the content area
    };

    bool             shown() const; // open and the tree has arrived
    const SMenuNode* current() const;
    void             layout();
    void             changed();
    void             damage();
    CBox             boxGlobal() const;
    Vector2D         toContent(const Vector2D& global) const;
    const SRow*      rowAt(const Vector2D& content) const;
    void             activate(const SRow& row);
    void             back();
    void             updateHover();
    void             render(Render::CRenderContext& ctx, PHLMONITOR mon);

    bool                             m_open = false;
    bool                             m_toggledInPress = false; // toggle() ran for the press being handled
    PHLMONITORREF                    m_monitor;
    Vector2D                         m_anchor;
    std::string                      m_owner;
    UP<CDbusMenu>                    m_menu;
    std::vector<UP<CDbusMenu>>       m_retired; // closed from inside its own callback: freed on the next tick
    UP<CWakeTimer>                   m_reapTimer;
    std::vector<int>                 m_stack; // open submenus, innermost last

    std::vector<SRow>                m_rows;
    Vector2D                         m_size;
    int                              m_hover = -1; // index into m_rows
    std::unordered_set<uint32_t>     m_pressedButtons;

    float                            m_lastScale = 0.F;
    CTextureCache                    m_tex;
    std::vector<CHyprSignalListener> m_listeners;
};

inline UP<CTrayMenu> g_pTrayMenu;
