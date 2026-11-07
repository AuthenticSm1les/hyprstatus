#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <string>
#include <vector>

#include "../core/Bus.hpp"

// power-profiles-daemon (or anything implementing its D-Bus API, e.g.
// tuned-ppd): the active profile and the ones this machine offers.
// Updated from its PropertiesChanged signal.
class CPowerProfilesService {
  public:
    CPowerProfilesService();

    bool                            available() const { return m_available; }
    const std::string&              active() const { return m_active; }   // "power-saver", "balanced", "performance"
    const std::vector<std::string>& profiles() const { return m_profiles; }
    bool                            has(const std::string& profile) const;

    void                            set(const std::string& profile);

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    void                     refresh();

    SP<CBus>                 m_bus;
    SP<bool>                 m_alive = makeShared<bool>(true);
    CSlot                    m_signal;
    bool                     m_available = false;
    std::string              m_active;
    std::vector<std::string> m_profiles;
};
