#include "PowerProfiles.hpp"

#include <algorithm>

static constexpr const char* PPD      = "org.freedesktop.UPower.PowerProfiles";
static constexpr const char* PPD_PATH = "/org/freedesktop/UPower/PowerProfiles";
static constexpr const char* PROPS    = "org.freedesktop.DBus.Properties";

CPowerProfilesService::CPowerProfilesService() {
    m_bus = CBus::system();
    if (!m_bus)
        return;

    sd_bus_match_signal_async(
        m_bus->get(), m_signal.out(), PPD, PPD_PATH, PROPS, "PropertiesChanged",
        [](sd_bus_message*, void* data, sd_bus_error*) -> int {
            sc<CPowerProfilesService*>(data)->refresh();
            return 0;
        },
        nullptr, this);
    refresh();
}

bool CPowerProfilesService::has(const std::string& profile) const {
    return std::ranges::find(m_profiles, profile) != m_profiles.end();
}

void CPowerProfilesService::refresh() {
    m_bus->call(
        PPD, PPD_PATH, PROPS, "GetAll", m_alive,
        [this](sd_bus_message* m) {
            if (sd_bus_message_is_method_error(m, nullptr)) {
                m_available = false;
                m_changed.emit();
                return;
            }
            m_profiles.clear();
            readProperties(m, [&](std::string_view key, std::string_view sig) {
                if (key == "ActiveProfile" && sig == "s") {
                    m_active = readVariantString(m);
                    return true;
                }
                if (key == "Profiles" && sig == "aa{sv}") {
                    // one a{sv} per profile; we only need its "Profile" name
                    sd_bus_message_enter_container(m, 'v', "aa{sv}");
                    sd_bus_message_enter_container(m, 'a', "a{sv}");
                    while (sd_bus_message_at_end(m, false) == 0) {
                        readProperties(m, [&](std::string_view k, std::string_view s) {
                            if (k == "Profile" && s == "s") {
                                m_profiles.push_back(readVariantString(m));
                                return true;
                            }
                            return false;
                        });
                    }
                    sd_bus_message_exit_container(m);
                    sd_bus_message_exit_container(m);
                    return true;
                }
                return false;
            });
            m_available = true;
            m_changed.emit();
        },
        "s", PPD);
}

void CPowerProfilesService::set(const std::string& profile) {
    if (!m_available || profile == m_active)
        return;
    // the daemon's PropertiesChanged updates the state
    m_bus->call(PPD, PPD_PATH, PROPS, "Set", m_alive, nullptr, "ssv", PPD, "ActiveProfile", "s", profile.c_str());
}
