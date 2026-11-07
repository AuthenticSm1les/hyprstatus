#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <optional>

#include "../core/Process.hpp"

// Night light through hyprsunset: on = a hyprsunset process is running.
// Turning it on starts `hyprsunset -t <temperature>`, off stops it; the
// temperature is changed live over its IPC (hyprctl hyprsunset temperature)
// and remembered across restarts in $XDG_STATE_HOME/hyprstatus/nightlight-temperature.
class CNightLightService {
  public:
    CNightLightService();

    static constexpr int MIN_K = 1700, MAX_K = 4700; // GNOME's range; warmer = lower

    bool                 installed() const { return m_installed; }
    bool                 on() const { return m_on; }
    int                  temperature() const { return m_temperature; }

    void                 toggle();
    void                 setTemperature(int kelvin);
    void                 refresh(); // re-check whether hyprsunset runs (started or stopped elsewhere)

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    void               push();

    bool               m_installed = false, m_on = false;
    int                m_temperature = 3500;
    std::optional<int> m_pending;
    UP<CProcess>       m_set;
};
