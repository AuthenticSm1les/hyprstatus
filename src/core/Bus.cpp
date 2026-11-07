#include "Bus.hpp"

#include <cstdarg>

CBus::CBus(sd_bus* bus) : m_bus(bus) {
    m_watch = makeUnique<CFdWatch>(sd_bus_get_fd(m_bus), [this] { process(); });
}

CBus::~CBus() {
    m_watch.reset();
    if (m_bus)
        sd_bus_flush_close_unref(m_bus);
}

static SP<CBus> shared(WP<CBus>& weak, int (*open)(sd_bus**), auto make) {
    if (auto s = weak.lock())
        return s;
    sd_bus* bus = nullptr;
    if (open(&bus) < 0 || !bus)
        return nullptr;
    SP<CBus> s = make(bus);
    weak       = s;
    return s;
}

SP<CBus> CBus::user() {
    static WP<CBus> weak;
    return shared(weak, sd_bus_open_user, [](sd_bus* b) { return SP<CBus>(new CBus(b)); });
}

SP<CBus> CBus::system() {
    static WP<CBus> weak;
    return shared(weak, sd_bus_open_system, [](sd_bus* b) { return SP<CBus>(new CBus(b)); });
}

namespace {
    struct SCallCtx {
        WP<bool>     alive;
        CBus::CReply cb;
    };
}

void CBus::call(const char* dest, const char* path, const char* iface, const char* member, WP<bool> alive, CReply cb, const char* types, ...) {
    sd_bus_message* m = nullptr;
    if (sd_bus_message_new_method_call(m_bus, &m, dest, path, iface, member) < 0)
        return;
    if (types) {
        va_list ap;
        va_start(ap, types);
        const int R = sd_bus_message_appendv(m, types, ap);
        va_end(ap);
        if (R < 0) {
            sd_bus_message_unref(m);
            return;
        }
    }

    auto*        ctx  = new SCallCtx{std::move(alive), std::move(cb)};
    sd_bus_slot* slot = nullptr;
    const int    R    = sd_bus_call_async(
        m_bus, &slot, m,
        [](sd_bus_message* reply, void* data, sd_bus_error*) -> int {
            auto* c = sc<SCallCtx*>(data);
            if (c->cb && !c->alive.expired())
                c->cb(reply);
            return 0;
        },
        ctx, 0);
    sd_bus_message_unref(m);
    if (R < 0) {
        delete ctx;
        return;
    }
    // floating slot: lives until the reply (or bus close), then frees ctx
    sd_bus_slot_set_destroy_callback(slot, [](void* data) { delete sc<SCallCtx*>(data); });
    sd_bus_slot_set_floating(slot, 1);
    sd_bus_slot_unref(slot);
    sd_bus_flush(m_bus);
}

void CBus::process() {
    // sd_bus_process handles one message per call
    while (sd_bus_process(m_bus, nullptr) > 0) {
        ;
    }
    sd_bus_flush(m_bus);
}

void readProperties(sd_bus_message* m, const std::function<bool(std::string_view key, std::string_view sig)>& handler) {
    if (sd_bus_message_enter_container(m, 'a', "{sv}") < 0)
        return;
    while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
        const char* key      = nullptr;
        char        type     = 0;
        const char* contents = nullptr;
        sd_bus_message_read(m, "s", &key);
        sd_bus_message_peek_type(m, &type, &contents);
        if (!handler(key ? key : "", contents ? contents : ""))
            sd_bus_message_skip(m, "v");
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);
}

std::string readVariantString(sd_bus_message* m) {
    const char* v = nullptr;
    char        type = 0;
    const char* contents = nullptr;
    sd_bus_message_peek_type(m, &type, &contents);
    const std::string_view SIG = contents ? contents : "";
    if (SIG != "s" && SIG != "o") {
        sd_bus_message_skip(m, "v");
        return "";
    }
    sd_bus_message_read(m, "v", SIG == "s" ? "s" : "o", &v);
    return v ? v : "";
}

bool readVariantBool(sd_bus_message* m) {
    int b = 0;
    if (sd_bus_message_read(m, "v", "b", &b) < 0)
        sd_bus_message_skip(m, "v");
    return b;
}
