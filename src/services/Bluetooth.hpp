#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../core/Bus.hpp"

// BlueZ over the system bus: first adapter's power state and connected
// devices. Re-read on BlueZ's PropertiesChanged / InterfacesAdded / Removed.
class CBluetoothService {
  public:
    CBluetoothService();

    bool                            available() const { return !m_adapter.empty(); }
    bool                            powered() const { return m_powered; }
    const std::vector<std::string>& connectedDevices() const { return m_connected; }

    void                            togglePower();

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    void                     refresh();
    void                     changed();
    SP<CBus>                 m_bus;
    SP<bool>                 m_alive = makeShared<bool>(true);
    CSlot                    m_props, m_added, m_removed;
    std::string              m_adapter;
    bool                     m_powered = false;
    std::vector<std::string> m_connected;
    bool                     m_inFlight = false, m_again = false;
};
