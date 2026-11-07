#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <functional>
#include <optional>
#include <string>

#include "../core/EventLoop.hpp"

struct udev;
struct udev_monitor;

// First battery under /sys/class/power_supply. Updated on udev power_supply
// events (charge level, AC plug/unplug).
class CBatteryService {
  public:
    CBatteryService();
    ~CBatteryService();

    std::optional<int> percent() const { return m_percent; }
    bool               charging() const { return m_status == "Charging"; }
    const std::string& status() const { return m_status; } // sysfs: Charging, Discharging, Full, Not charging

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    bool                  read(); // true if anything changed
    void                  onUdev();
    std::optional<int>    m_percent;
    std::string           m_status, m_path;
    udev*                 m_udev    = nullptr;
    udev_monitor*         m_monitor = nullptr;
    UP<CFdWatch>          m_watch;
};
