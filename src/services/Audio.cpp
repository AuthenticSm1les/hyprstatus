#include "Audio.hpp"

#include <wp/wp.h>

#include <dlfcn.h>
#include <link.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <string_view>

// The plugin can be unloaded and loaded again, but libwireplumber registers
// GObject types, which GLib (resident: Hyprland links it) can never forget.
// Keep the library, and its modules, loaded for the life of the process.
static void pinResident(const char* lib) {
    if (void* h = dlopen(lib, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE))
        dlclose(h); // drops our reference; NODELETE keeps it mapped
}

static constexpr guint ID_NONE = 0xffffffff; // SPA_ID_INVALID

static const char* const MODULES[] = {"libwireplumber-module-default-nodes-api", "libwireplumber-module-mixer-api"};

// ---------------------------------------------------------------- compositor thread

CAudioService::CAudioService() {
    pinResident("libwireplumber-0.5.so.0");
    pinResident("libpipewire-0.3.so.0");
    // wp_init must run once per process; a quark outlives our unload
    if (!g_quark_try_string("hyprstatus-wp-init")) {
        g_quark_from_string("hyprstatus-wp-init");
        wp_init(WpInitFlags(WP_INIT_PIPEWIRE | WP_INIT_SPA_TYPES)); // leave GLib's and PipeWire's logging alone
    }

    m_wakeFd    = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    m_wakeWatch = makeUnique<CFdWatch>(m_wakeFd, [this] { onWake(); });

    m_ctx    = g_main_context_new();
    m_loop   = g_main_loop_new(m_ctx, FALSE);
    m_thread = std::thread([this] { run(); });
}

CAudioService::~CAudioService() {
    g_main_context_invoke(m_ctx, quitCb, this);
    m_thread.join();
    g_main_loop_unref(m_loop);
    g_main_context_unref(m_ctx);
    m_wakeWatch.reset();
    close(m_wakeFd);
}

void CAudioService::onWake() {
    uint64_t n;
    while (read(m_wakeFd, &n, sizeof n) > 0) {}

    bool changed = false;
    {
    std::lock_guard lock(m_mutex);
    for (int i = 0; i < 2; ++i) {
        auto next = m_published[i];
        // the slider is ahead of WirePlumber while dragging: keep its value
        if (m_appliedSeq[i] < m_volumeSeq[i])
            next.volume = m_state[i].volume;
        changed |= next.volume != m_state[i].volume || next.muted != m_state[i].muted || next.known != m_state[i].known;
        m_state[i] = next;
    }
    }
    if (changed) // unlocked: listeners may call back into setVolume
        m_changed.emit();
}

// requests coalesce: while one is queued, later ones just update it
void CAudioService::schedule() {
    if (!m_requests.scheduled) {
        m_requests.scheduled = true;
        g_main_context_invoke(m_ctx, applyCb, this);
    }
}

void CAudioService::setVolume(bool input, float v) {
    const int I       = input;
    v                 = std::clamp(v, 0.F, 1.F);
    m_state[I].volume = v; // optimistic, so the slider follows the pointer
    {
        std::lock_guard lock(m_mutex);
        m_requests.volume[I]    = v;
        m_requests.volumeSeq[I] = ++m_volumeSeq[I];
        schedule();
    }
    m_changed.emit();
}

void CAudioService::toggleMute(bool input) {
    std::lock_guard lock(m_mutex);
    m_requests.toggleMute[input] = !m_requests.toggleMute[input]; // two quick toggles cancel out
    schedule();
}

// ---------------------------------------------------------------- WirePlumber thread

void CAudioService::run() {
    g_main_context_push_thread_default(m_ctx);

    m_core = wp_core_new(m_ctx, nullptr, wp_properties_new("application.name", "hyprstatus", nullptr));
    g_signal_connect_swapped(m_core, "disconnected", G_CALLBACK(onDisconnectedCb), this);
    m_pendingLoads = std::size(MODULES);
    for (const char* m : MODULES)
        wp_core_load_component(m_core, m, "module", nullptr, nullptr, nullptr, onComponentLoaded, this);
    if (!wp_core_connect(m_core))
        reconnectLater(); // PipeWire not up yet

    g_main_loop_run(m_loop);

    if (m_mixer)
        g_signal_handlers_disconnect_by_data(m_mixer, this);
    if (m_defaults)
        g_signal_handlers_disconnect_by_data(m_defaults, this);
    g_signal_handlers_disconnect_by_data(m_core, this);
    g_clear_object(&m_mixer);
    g_clear_object(&m_defaults);
    wp_core_disconnect(m_core);
    g_clear_object(&m_core);
    while (g_main_context_iteration(m_ctx, FALSE)) {} // let finalizers' idle work run

    g_main_context_pop_thread_default(m_ctx);
}

void CAudioService::onComponentLoaded(GObject* core, GAsyncResult* res, void* data) {
    auto*   self  = static_cast<CAudioService*>(data);
    GError* error = nullptr;
    if (!wp_core_load_component_finish(WP_CORE(core), res, &error)) {
        g_warning("[hyprstatus] audio: %s", error->message);
        g_error_free(error);
    }
    if (--self->m_pendingLoads == 0)
        self->onLoaded();
}

void CAudioService::onLoaded() {
    m_mixer    = wp_plugin_find(m_core, "mixer-api");
    m_defaults = wp_plugin_find(m_core, "default-nodes-api");
    if (!m_mixer || !m_defaults) {
        g_warning("[hyprstatus] audio: WirePlumber's mixer / default-nodes API not available");
        return;
    }
    // the modules register GObject types too
    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void*) {
            if (info->dlpi_name && std::string_view{info->dlpi_name}.contains("libwireplumber-module-"))
                pinResident(info->dlpi_name);
            return 0;
        },
        nullptr);

    g_object_set(m_mixer, "scale", 1 /* cubic, as wpctl and pavucontrol show it */, nullptr);
    // mixer "changed" (node id) and default-nodes "changed": both just mean re-read
    g_signal_connect_swapped(m_mixer, "changed", G_CALLBACK(onChangedCb), this);
    g_signal_connect_swapped(m_defaults, "changed", G_CALLBACK(onChangedCb), this);
    publish();
}

unsigned CAudioService::defaultNode(bool input) {
    guint id = ID_NONE;
    if (m_defaults)
        g_signal_emit_by_name(m_defaults, "get-default-node", input ? "Audio/Source" : "Audio/Sink", &id);
    return id;
}

// read both sides and hand them to the compositor thread
void CAudioService::publish() {
    SChannel next[2];
    for (int i = 0; i < 2; ++i) {
        const guint ID = defaultNode(i);
        if (!m_mixer || ID == ID_NONE || ID == 0)
            continue;
        GVariant* v = nullptr;
        g_signal_emit_by_name(m_mixer, "get-volume", ID, &v);
        if (!v)
            continue;
        double   vol  = 0;
        gboolean mute = FALSE;
        if (g_variant_lookup(v, "volume", "d", &vol)) {
            next[i].volume = (float)vol;
            next[i].known  = true;
        }
        g_variant_lookup(v, "mute", "b", &mute);
        next[i].muted = mute;
        g_variant_unref(v);
    }

    {
        std::lock_guard lock(m_mutex);
        m_published[0] = next[0];
        m_published[1] = next[1];
    }
    const uint64_t ONE = 1;
    (void)!write(m_wakeFd, &ONE, sizeof ONE);
}

void CAudioService::applyRequests() {
    SRequests req;
    {
        std::lock_guard lock(m_mutex);
        req        = m_requests;
        m_requests = {};
    }

    for (int i = 0; i < 2; ++i) {
        const guint ID = defaultNode(i);
        if (m_mixer && ID != ID_NONE && ID != 0) {
            GVariantBuilder b;
            g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
            bool any = false;
            if (req.volume[i]) {
                g_variant_builder_add(&b, "{sv}", "volume", g_variant_new_double(*req.volume[i]));
                any = true;
            }
            if (req.toggleMute[i]) {
                GVariant* cur  = nullptr;
                gboolean  mute = FALSE;
                g_signal_emit_by_name(m_mixer, "get-volume", ID, &cur);
                if (cur) {
                    g_variant_lookup(cur, "mute", "b", &mute);
                    g_variant_unref(cur);
                }
                g_variant_builder_add(&b, "{sv}", "mute", g_variant_new_boolean(!mute));
                any = true;
            }
            gboolean ok = FALSE;
            if (any)
                g_signal_emit_by_name(m_mixer, "set-volume", ID, g_variant_builder_end(&b), &ok);
            else
                g_variant_builder_clear(&b);
        }
        if (req.volume[i]) {
            std::lock_guard lock(m_mutex);
            m_appliedSeq[i] = std::max(m_appliedSeq[i], req.volumeSeq[i]);
        }
    }
    publish(); // also releases the slider's hold once applied
}

void CAudioService::reconnectLater() {
    GSource* s = g_timeout_source_new_seconds(2);
    g_source_set_callback(s, reconnectCb, this, nullptr);
    g_source_attach(s, m_ctx);
    g_source_unref(s);
}

void CAudioService::onChangedCb(void* self) {
    static_cast<CAudioService*>(self)->publish();
}

// PipeWire restarted (or went away): show nothing, try again
void CAudioService::onDisconnectedCb(void* data) {
    auto* self = static_cast<CAudioService*>(data);
    self->publish();
    self->reconnectLater();
}

int CAudioService::applyCb(void* self) {
    static_cast<CAudioService*>(self)->applyRequests();
    return G_SOURCE_REMOVE;
}

int CAudioService::quitCb(void* self) {
    g_main_loop_quit(static_cast<CAudioService*>(self)->m_loop);
    return G_SOURCE_REMOVE;
}

int CAudioService::reconnectCb(void* data) {
    auto* self = static_cast<CAudioService*>(data);
    if (!wp_core_is_connected(self->m_core) && !wp_core_connect(self->m_core))
        return G_SOURCE_CONTINUE; // every 2s until PipeWire is back
    return G_SOURCE_REMOVE;
}
