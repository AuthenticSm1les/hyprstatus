#include "Brightness.hpp"
#include "../core/Util.hpp"

#include <libudev.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

CBrightnessService::CBrightnessService() {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator("/sys/class/backlight", ec)) {
        m_name = e.path().filename().string();
        m_path = e.path().string();
        break;
    }
    if (m_path.empty())
        return;

    try {
        m_max = std::stoi(readFirstLine(m_path + "/max_brightness"));
    } catch (...) { m_max = 0; }
    read();

    m_bus  = CBus::system();
    m_udev = udev_new();
    if (!m_udev)
        return;
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor)
        return;
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "backlight", nullptr);
    if (udev_monitor_enable_receiving(m_monitor) < 0)
        return;
    m_watch = makeUnique<CFdWatch>(udev_monitor_get_fd(m_monitor), [this] { onUdev(); });
}

CBrightnessService::~CBrightnessService() {
    m_watch.reset();
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

bool CBrightnessService::read() {
    int v = m_cur;
    try {
        v = std::stoi(readFirstLine(m_path + "/brightness"));
    } catch (...) {}
    const bool CHANGED = v != m_cur;
    m_cur              = v;
    return CHANGED;
}

void CBrightnessService::onUdev() {
    while (auto* dev = udev_monitor_receive_device(m_monitor))
        udev_device_unref(dev);
    // while the user drags, the slider shows the pending value
    if (!m_pending && !m_inFlight && read())
        m_changed.emit();
}

void CBrightnessService::set(float v) {
    if (!available() || !m_bus)
        return;
    // keep at least 1 so the screen never goes fully black from the slider
    m_pending = std::clamp((int)std::round(v * m_max), 1, m_max);
    m_cur     = *m_pending;
    push();
    m_changed.emit();
}

// one SetBrightness call in flight; the latest value wins
void CBrightnessService::push() {
    if (!m_pending || m_inFlight)
        return;
    const uint32_t V = *m_pending;
    m_pending.reset();
    m_inFlight = true;
    m_bus->call(
        "org.freedesktop.login1", "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "SetBrightness", m_alive,
        [this](sd_bus_message*) {
            m_inFlight = false;
            push();
        },
        "ssu", "backlight", m_name.c_str(), V);
}
