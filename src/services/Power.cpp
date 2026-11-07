#include "Power.hpp"

#include <hyprland/src/config/shared/actions/ConfigActions.hpp>

static constexpr const char* LOGIN1 = "org.freedesktop.login1";

CPowerService::CPowerService() : m_bus(CBus::system()) {}

void CPowerService::manager(const char* method) {
    if (m_bus) // (b interactive): false, never block on a polkit prompt
        m_bus->call(LOGIN1, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", method, m_alive, nullptr, "b", 0);
}

void CPowerService::lock() {
    if (m_bus)
        m_bus->call(LOGIN1, "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "Lock", m_alive, nullptr);
}

void CPowerService::suspend() {
    manager("Suspend");
}

void CPowerService::reboot() {
    manager("Reboot");
}

void CPowerService::powerOff() {
    manager("PowerOff");
}

void CPowerService::logout() {
    (void)Config::Actions::exit();
}
