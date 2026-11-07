#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>

#include "../core/Bus.hpp"

// Session / power actions through logind; logout exits Hyprland.
// Lock asks logind to lock the session, so the user's idle daemon
// (hypridle lock_cmd) decides which locker runs.
class CPowerService {
  public:
    CPowerService();

    void lock();
    void suspend();
    void logout();
    void reboot();
    void powerOff();

  private:
    void     manager(const char* method);
    SP<CBus> m_bus;
    SP<bool> m_alive = makeShared<bool>(true);
};
