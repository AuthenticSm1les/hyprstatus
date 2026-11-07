#include "Battery.hpp"
#include "../core/Util.hpp"

#include <libudev.h>

#include <filesystem>

CBatteryService::CBatteryService() {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator("/sys/class/power_supply", ec)) {
        if (readFirstLine(e.path().string() + "/type") == "Battery") {
            m_path = e.path().string();
            break;
        }
    }
    read();

    m_udev = udev_new();
    if (!m_udev)
        return;
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor)
        return;
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "power_supply", nullptr);
    if (udev_monitor_enable_receiving(m_monitor) < 0)
        return;
    m_watch = makeUnique<CFdWatch>(udev_monitor_get_fd(m_monitor), [this] { onUdev(); });
}

CBatteryService::~CBatteryService() {
    m_watch.reset();
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

bool CBatteryService::read() {
    if (m_path.empty())
        return false;

    const auto         OLD_P = m_percent;
    const auto         OLD_S = m_status;
    std::optional<int> p;
    try {
        p = std::stoi(readFirstLine(m_path + "/capacity"));
    } catch (...) {}
    m_percent = p;
    m_status  = readFirstLine(m_path + "/status");
    return OLD_P != m_percent || OLD_S != m_status;
}

void CBatteryService::onUdev() {
    while (auto* dev = udev_monitor_receive_device(m_monitor))
        udev_device_unref(dev);
    if (read())
        m_changed.emit();
}
