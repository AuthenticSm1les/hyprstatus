#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

#include "../core/EventLoop.hpp"

typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop    GMainLoop;
typedef struct _WpCore       WpCore;
typedef struct _WpPlugin     WpPlugin;
typedef struct _GObject      GObject;
typedef struct _GAsyncResult GAsyncResult;

// Default sink/source volume + mute, talking to WirePlumber directly
// (libwireplumber's default-nodes and mixer APIs, the same ones wpctl uses).
// WirePlumber runs on a GLib main loop, kept on a thread of its own; changes
// come back to the compositor's thread through an eventfd. No polling and
// no processes.
class CAudioService {
  public:
    CAudioService();
    ~CAudioService();
    CAudioService(const CAudioService&)            = delete;
    CAudioService& operator=(const CAudioService&) = delete;

    struct SChannel {
        float volume = 0.F; // 0..1, cubic like wpctl (more is possible; the slider caps at 1)
        bool  muted  = false;
        bool  known  = false;
    };

    const SChannel& output() const { return m_state[0]; }
    const SChannel& input() const { return m_state[1]; }

    void            setVolume(bool input, float v);
    void            toggleMute(bool input);

    Hyprutils::Signal::CSignalT<> m_changed; // any value above changed

  private:
    // ---- compositor thread
    void     onWake();
    void     schedule(); // run applyRequests() on the WirePlumber thread
    SChannel m_state[2];
    uint64_t m_volumeSeq[2] = {0, 0}; // last setVolume() per side
    int      m_wakeFd       = -1;
    UP<CFdWatch> m_wakeWatch;

    // ---- WirePlumber thread
    void          run();
    void          onLoaded();
    void          publish();
    void          applyRequests();
    void          reconnectLater();
    static void   onComponentLoaded(GObject* core, GAsyncResult* res, void* self);
    static void   onChangedCb(void* self);
    static void   onDisconnectedCb(void* self);
    static int    applyCb(void* self);
    static int    quitCb(void* self);
    static int    reconnectCb(void* self);
    unsigned      defaultNode(bool input);

    GMainContext* m_ctx  = nullptr;
    GMainLoop*    m_loop = nullptr;
    WpCore*       m_core = nullptr;
    WpPlugin*     m_mixer = nullptr;
    WpPlugin*     m_defaults = nullptr;
    int           m_pendingLoads = 0;
    std::thread   m_thread;

    // ---- shared, under m_mutex
    struct SRequests {
        std::optional<float> volume[2];
        uint64_t             volumeSeq[2] = {0, 0};
        bool                 toggleMute[2] = {false, false};
        bool                 scheduled = false;
    };
    std::mutex m_mutex;
    SRequests  m_requests;
    SChannel   m_published[2];
    uint64_t   m_appliedSeq[2] = {0, 0};
};
