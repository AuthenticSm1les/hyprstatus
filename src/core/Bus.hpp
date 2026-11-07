#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>

#include <systemd/sd-bus.h>

#include <functional>
#include <string>
#include <string_view>

#include "EventLoop.hpp"

// An sd-bus connection driven by Hyprland's event loop: its fd is watched for
// readability and pending messages are dispatched from there. No threads.
// One shared connection per bus; services hold an SP to keep it alive.
class CBus {
  public:
    static SP<CBus> user();
    static SP<CBus> system();
    ~CBus();

    sd_bus* get() const { return m_bus; }
    void    process();

    // Async method call. `cb` (may be empty) gets the reply; it is skipped if
    // `alive` expired meanwhile, so owners never need to cancel calls.
    // `types` + varargs as for sd_bus_message_append.
    using CReply = std::function<void(sd_bus_message* reply)>;
    void call(const char* dest, const char* path, const char* iface, const char* member, WP<bool> alive, CReply cb, const char* types = nullptr, ...);

  private:
    explicit CBus(sd_bus* bus);
    sd_bus*      m_bus = nullptr;
    UP<CFdWatch> m_watch;
};

// Walk an a{sv} property dict at the current read position. `handler` gets
// each key and the variant's signature; it either reads the variant itself
// (return true) or returns false to have it skipped.
void readProperties(sd_bus_message* m, const std::function<bool(std::string_view key, std::string_view sig)>& handler);

// read a variant holding a string / bool (call from a readProperties handler)
std::string readVariantString(sd_bus_message* m);
bool        readVariantBool(sd_bus_message* m);

// RAII for sd_bus_slot (match rules, async calls, vtables)
class CSlot {
  public:
    CSlot() = default;
    CSlot(CSlot&& o) noexcept : m_slot(o.m_slot) { o.m_slot = nullptr; }
    CSlot& operator=(CSlot&& o) noexcept {
        reset();
        m_slot   = o.m_slot;
        o.m_slot = nullptr;
        return *this;
    }
    CSlot(const CSlot&)            = delete;
    CSlot& operator=(const CSlot&) = delete;
    ~CSlot() { reset(); }

    sd_bus_slot** out() {
        reset();
        return &m_slot;
    }
    void reset() {
        if (m_slot)
            sd_bus_slot_unref(m_slot);
        m_slot = nullptr;
    }

  private:
    sd_bus_slot* m_slot = nullptr;
};
