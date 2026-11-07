#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <functional>
#include <optional>
#include <string>

#include "../core/Bus.hpp"
#include "../core/EventLoop.hpp"

struct udev;
struct udev_monitor;

// First /sys/class/backlight device. The kernel emits a udev change event on
// every brightness write (keys, brightnessctl, us), so no polling. Writes go
// through logind's Session.SetBrightness, which needs no root or udev rules.
class CBrightnessService {
  public:
    CBrightnessService();
    ~CBrightnessService();

    bool  available() const { return m_max > 0; }
    float value() const { return m_max > 0 ? (float)m_cur / m_max : 0.F; } // 0..1
    void  set(float v);

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    bool                  read();
    void                  onUdev();
    void                  push();
    std::string           m_name, m_path;
    int                   m_cur = 0, m_max = 0;
    std::optional<int>    m_pending;
    bool                  m_inFlight = false;
    SP<CBus>              m_bus;
    SP<bool>              m_alive = makeShared<bool>(true);
    udev*                 m_udev    = nullptr;
    udev_monitor*         m_monitor = nullptr;
    UP<CFdWatch>          m_watch;
};
