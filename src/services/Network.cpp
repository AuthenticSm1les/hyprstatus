#include "Network.hpp"

static constexpr const char* NM        = "org.freedesktop.NetworkManager";
static constexpr const char* NM_PATH   = "/org/freedesktop/NetworkManager";
static constexpr const char* NM_ACTIVE = "org.freedesktop.NetworkManager.Connection.Active";
static constexpr const char* PROPS     = "org.freedesktop.DBus.Properties";

CNetworkService::CNetworkService() {
    m_bus = CBus::system();
    if (!m_bus)
        return;

    // the manager object announces WirelessEnabled / PrimaryConnection / ActiveConnections changes
    sd_bus_match_signal_async(
        m_bus->get(), m_signal.out(), NM, NM_PATH, PROPS, "PropertiesChanged",
        [](sd_bus_message*, void* data, sd_bus_error*) -> int {
            sc<CNetworkService*>(data)->refresh();
            return 0;
        },
        nullptr, this);
    refresh();
}

// manager properties, then one GetAll per active connection; applied once all replied
void CNetworkService::refresh() {
    m_bus->call(
        NM, NM_PATH, PROPS, "GetAll", m_alive,
        [this](sd_bus_message* m) {
            if (sd_bus_message_is_method_error(m, nullptr)) {
                m_available = false;
                m_changed.emit();
                return;
            }
            auto                     round = makeShared<SRound>();
            std::vector<std::string> active;
            readProperties(m, [&](std::string_view key, std::string_view sig) {
                if (key == "WirelessEnabled" && sig == "b")
                    round->wifiEnabled = readVariantBool(m);
                else if (key == "PrimaryConnection" && sig == "o")
                    round->primary = readVariantString(m);
                else if (key == "ActiveConnections" && sig == "ao") {
                    sd_bus_message_enter_container(m, 'v', "ao");
                    sd_bus_message_enter_container(m, 'a', "o");
                    const char* path = nullptr;
                    while (sd_bus_message_read(m, "o", &path) > 0)
                        active.emplace_back(path);
                    sd_bus_message_exit_container(m);
                    sd_bus_message_exit_container(m);
                } else
                    return false;
                return true;
            });
            m_round       = round;
            round->left   = active.size();
            if (active.empty())
                apply(*round);
            for (const auto& path : active) {
                m_bus->call(
                    NM, path.c_str(), PROPS, "GetAll", m_alive,
                    [this, round, path](sd_bus_message* r) {
                        if (!sd_bus_message_is_method_error(r, nullptr)) {
                            std::string id, type;
                            readProperties(r, [&](std::string_view key, std::string_view sig) {
                                if (sig != "s" || (key != "Id" && key != "Type"))
                                    return false;
                                (key == "Id" ? id : type) = readVariantString(r);
                                return true;
                            });
                            if (path == round->primary) {
                                round->connection     = id;
                                round->connectionType = type;
                            }
                            if (type == "802-11-wireless" && round->wifiConnection.empty())
                                round->wifiConnection = id;
                        }
                        if (--round->left == 0 && round == m_round)
                            apply(*round);
                    },
                    "s", NM_ACTIVE);
            }
        },
        "s", NM);
}

void CNetworkService::apply(const SRound& r) {
    m_available      = true;
    m_wifiEnabled    = r.wifiEnabled;
    m_connection     = r.connection;
    m_connectionType = r.connectionType;
    m_wifiConnection = r.wifiConnection;
    m_changed.emit();
}

void CNetworkService::toggleWifi() {
    if (m_available) // NM's PropertiesChanged updates the state
        m_bus->call(NM, NM_PATH, PROPS, "Set", m_alive, nullptr, "ssv", NM, "WirelessEnabled", "b", (int)!m_wifiEnabled);
}
