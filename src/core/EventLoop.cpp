#include "EventLoop.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>

CWakeTimer::CWakeTimer(std::function<void()> fn) : m_fn(std::move(fn)) {
    m_timer = makeShared<CEventLoopTimer>(std::nullopt, [this](SP<CEventLoopTimer>, void*) { m_fn(); }, nullptr);
    g_pEventLoopManager->addTimer(m_timer);
}

CWakeTimer::~CWakeTimer() {
    m_timer->cancel();
    g_pEventLoopManager->removeTimer(m_timer);
}

void CWakeTimer::arm(std::chrono::milliseconds in) {
    m_timer->updateTimeout(std::max(in, std::chrono::milliseconds(1)));
}

void CWakeTimer::disarm() {
    m_timer->updateTimeout(std::nullopt);
}

CFdWatch::CFdWatch(int fd, std::function<void()> fn) : m_fn(std::move(fn)) {
    if (fd >= 0)
        m_source = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, fd, WL_EVENT_READABLE, onReadable, this);
}

CFdWatch::~CFdWatch() {
    if (m_source)
        wl_event_source_remove(m_source);
}

int CFdWatch::onReadable(int, uint32_t, void* data) {
    sc<CFdWatch*>(data)->m_fn();
    return 0;
}
