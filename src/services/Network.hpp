#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../core/Bus.hpp"

// NetworkManager over the system bus: Wi-Fi radio state and the primary
// connection's name. Updated from NM's PropertiesChanged signals.
class CNetworkService {
  public:
    CNetworkService();

    bool               available() const { return m_available; }
    bool               wifiEnabled() const { return m_wifiEnabled; }
    const std::string& connection() const { return m_connection; } // "" if offline
    const std::string& connectionType() const { return m_connectionType; } // NM type, e.g. "802-3-ethernet"
    const std::string& wifiConnection() const { return m_wifiConnection; } // active Wi-Fi network, even if not primary

    void               toggleWifi();

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    struct SRound { // one refresh's results, applied together
        bool        wifiEnabled = false;
        std::string primary, connection, connectionType, wifiConnection;
        size_t      left = 0; // active connections still to answer
    };

    void                  refresh();
    void                  apply(const SRound& r);

    SP<CBus>              m_bus;
    SP<bool>              m_alive = makeShared<bool>(true);
    CSlot                 m_signal;
    SP<SRound>            m_round; // the newest refresh; older replies are ignored
    bool                  m_available = false, m_wifiEnabled = false;
    std::string           m_connection, m_connectionType, m_wifiConnection;
};
