#include "Bluetooth.hpp"

static constexpr const char* BLUEZ = "org.bluez";

CBluetoothService::CBluetoothService() {
    m_bus = CBus::system();
    if (!m_bus)
        return;

    auto refreshCb = [](sd_bus_message*, void* data, sd_bus_error*) -> int {
        sc<CBluetoothService*>(data)->refresh();
        return 0;
    };
    sd_bus_match_signal_async(m_bus->get(), m_props.out(), BLUEZ, nullptr, "org.freedesktop.DBus.Properties", "PropertiesChanged", refreshCb, nullptr, this);
    sd_bus_match_signal_async(m_bus->get(), m_added.out(), BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", refreshCb, nullptr, this);
    sd_bus_match_signal_async(m_bus->get(), m_removed.out(), BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved", refreshCb, nullptr, this);
    refresh();
}

void CBluetoothService::changed() {
    m_changed.emit();
}

// one GetManagedObjects in flight; signals during it schedule exactly one more
void CBluetoothService::refresh() {
    if (m_inFlight) {
        m_again = true;
        return;
    }
    m_inFlight = true;

    m_bus->call(BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", m_alive, [this](sd_bus_message* m) {
        m_inFlight = false;

        std::string              adapter;
        bool                     powered = false;
        std::vector<std::string> connected;

        // a{oa{sa{sv}}}
        if (!sd_bus_message_is_method_error(m, nullptr) && sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}") >= 0) {
            while (sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}") > 0) {
                const char* path = nullptr;
                sd_bus_message_read(m, "o", &path);
                if (sd_bus_message_enter_container(m, 'a', "{sa{sv}}") >= 0) {
                    while (sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0) {
                        const char* iface = nullptr;
                        sd_bus_message_read(m, "s", &iface);
                        const std::string_view IFACE = iface ? iface : "";

                        if (IFACE == "org.bluez.Adapter1" && adapter.empty()) {
                            adapter = path ? path : "";
                            readProperties(m, [&](std::string_view key, std::string_view sig) {
                                if (key == "Powered" && sig == "b") {
                                    powered = readVariantBool(m);
                                    return true;
                                }
                                return false;
                            });
                        } else if (IFACE == "org.bluez.Device1") {
                            bool        isConnected = false;
                            std::string alias;
                            readProperties(m, [&](std::string_view key, std::string_view sig) {
                                if (key == "Connected" && sig == "b") {
                                    isConnected = readVariantBool(m);
                                    return true;
                                }
                                if (key == "Alias" && sig == "s") {
                                    alias = readVariantString(m);
                                    return true;
                                }
                                return false;
                            });
                            if (isConnected)
                                connected.push_back(alias);
                        } else
                            sd_bus_message_skip(m, "a{sv}");

                        sd_bus_message_exit_container(m);
                    }
                    sd_bus_message_exit_container(m);
                }
                sd_bus_message_exit_container(m);
            }
            sd_bus_message_exit_container(m);
        }

        const bool CHANGED = adapter != m_adapter || powered != m_powered || connected != m_connected;
        m_adapter          = adapter;
        m_powered          = powered;
        m_connected        = connected;
        if (CHANGED)
            changed();

        if (m_again) {
            m_again = false;
            refresh();
        }
    });
}

void CBluetoothService::togglePower() {
    if (m_adapter.empty())
        return;
    // BlueZ's PropertiesChanged updates the state
    m_bus->call(BLUEZ, m_adapter.c_str(), "org.freedesktop.DBus.Properties", "Set", m_alive, nullptr, "ssv", "org.bluez.Adapter1", "Powered", "b", (int)!m_powered);
}
