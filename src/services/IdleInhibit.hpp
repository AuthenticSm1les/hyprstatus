#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/signal/Signal.hpp>

// Keeps the session from going idle (no hypridle lock / dpms / suspend),
// like a window with an idle inhibitor would, until turned off or the plugin
// unloads. Lives for the plugin's lifetime, not just while the panel is open.
class CIdleInhibitService {
  public:
    // hooks Hyprland's inhibitor recheck so ours survives it; false if not found
    static bool install(HANDLE handle);

    CIdleInhibitService() = default;
    ~CIdleInhibitService();

    bool                          available() const;
    bool                          active() const { return m_active; }
    void                          set(bool on);
    void                          toggle() { set(!m_active); }

    Hyprutils::Signal::CSignalT<> m_changed;

  private:
    bool m_active = false;
};

inline UP<CIdleInhibitService> g_pIdleInhibit;
