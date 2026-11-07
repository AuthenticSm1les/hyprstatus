#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>

#include <functional>
#include <string>
#include <vector>

#include "EventLoop.hpp"

// A child process whose stdout is read from the event loop (no blocking).
// onLine fires per output line; onExit fires once with everything read when
// stdout closes. Destroying the object stops reading and terminates the child,
// so callbacks never outlive their owner. (Hyprland reaps children itself.)
class CProcess {
  public:
    CProcess(const std::vector<std::string>& argv, std::function<void(const std::string&)> onLine, std::function<void(const std::string&)> onExit);
    ~CProcess();
    CProcess(const CProcess&)            = delete;
    CProcess& operator=(const CProcess&) = delete;

    bool running() const { return m_fd >= 0; }

  private:
    void                                    onReadable();

    int                                     m_fd  = -1;
    pid_t                                   m_pid = -1;
    std::string                             m_buf, m_all;
    std::function<void(const std::string&)> m_onLine, m_onExit;
    UP<CFdWatch>                            m_watch;
};

// fire-and-forget: run a command, ignore its output
void spawnDetached(const std::vector<std::string>& argv);
