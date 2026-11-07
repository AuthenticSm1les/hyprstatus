#pragma once

// Small RAII adapters over Hyprland's event loop, so components can own their
// wakeup sources and have them removed automatically when they go away.

#include <hyprland/src/helpers/memory/Memory.hpp>

#include <chrono>
#include <functional>

class CEventLoopTimer;
struct wl_event_source;

// One-shot timer; re-arm from the callback for repeating behaviour.
class CWakeTimer {
  public:
    explicit CWakeTimer(std::function<void()> fn);
    ~CWakeTimer();
    CWakeTimer(const CWakeTimer&)            = delete;
    CWakeTimer& operator=(const CWakeTimer&) = delete;

    void arm(std::chrono::milliseconds in);
    void disarm();

  private:
    SP<CEventLoopTimer>   m_timer;
    std::function<void()> m_fn;
};

// Calls fn whenever fd becomes readable.
class CFdWatch {
  public:
    CFdWatch(int fd, std::function<void()> fn);
    ~CFdWatch();
    CFdWatch(const CFdWatch&)            = delete;
    CFdWatch& operator=(const CFdWatch&) = delete;

  private:
    static int            onReadable(int, uint32_t, void* data);
    wl_event_source*      m_source = nullptr;
    std::function<void()> m_fn;
};
