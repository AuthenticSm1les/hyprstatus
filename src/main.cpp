#define WLR_USE_UNSTABLE

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include "core/Config.hpp"
#include "bar/Bar.hpp"
#include "bar/ReservedArea.hpp"
#include "panel/ControlCenter.hpp"
#include "panel/TrayMenu.hpp"
#include "services/IdleInhibit.hpp"
#include "core/PointerHold.hpp"

// the bar stays out (autohide) while the control center or a tray menu hangs off it
static CHyprSignalListener s_panelHoldsBar, s_trayMenuHoldsBar;

static void holdBar() {
    if (!g_pStatusBar)
        return;
    PHLMONITOR mon = g_pControlCenter ? g_pControlCenter->monitor() : nullptr;
    if (!mon && g_pTrayMenu)
        mon = g_pTrayMenu->monitor();
    g_pStatusBar->keepShown(mon);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

[[noreturn]] static void fail(const std::string& why) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprstatus] " + why, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error("[hyprstatus] " + why);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    if (std::string{__hyprland_api_get_hash()} != __hyprland_api_get_client_hash())
        fail("Version mismatch (headers ver is not equal to running hyprland ver)");

    registerConfig(handle);

    // tiled windows leave room for the bar
    if (!installReservedAreaHook(handle, [](PHLMONITOR mon) {
            if (g_pStatusBar)
                g_pStatusBar->onMonitorArranged(mon);
        }))
        fail("Could not hook arrangeLayersForMonitor");

    // optional: without the hook the idle tile just doesn't show
    if (!CIdleInhibitService::install(handle))
        HyprlandAPI::addNotification(handle, "[hyprstatus] idle inhibitor unavailable (hook not found)", CHyprColor{1.0, 0.6, 0.2, 1.0}, 5000);
    g_pIdleInhibit = makeUnique<CIdleInhibitService>();

    // the menu first: the bar's controlcenter group subscribes to it
    g_pControlCenter = makeUnique<CControlCenter>(g_config.ccModules);
    g_pTrayMenu      = makeUnique<CTrayMenu>();
    g_pStatusBar     = makeUnique<CStatusBar>();
    g_pTrayMenu->setupInput(); // after the bar: see there
    registerControlCenterLua(handle);
    // windows under the panel, and under the bar unless passthrough, don't see the pointer
    g_pPointerHold = makeUnique<CPointerHold>([](const Vector2D& p) {
        return (g_pControlCenter && g_pControlCenter->covers(p)) || (g_pTrayMenu && g_pTrayMenu->covers(p)) || (!g_config.passthrough->value() && g_pStatusBar && g_pStatusBar->covers(p));
    });
    s_panelHoldsBar = g_pControlCenter->m_openChanged.listen([] {
        // one popup at a time
        if (g_pControlCenter->isOpen() && g_pTrayMenu)
            g_pTrayMenu->close();
        holdBar();
    });
    s_trayMenuHoldsBar = g_pTrayMenu->m_openChanged.listen(holdBar);

    HyprlandAPI::reloadConfig();
    g_pStatusBar->rearrangeAll();

    return {"hyprstatus", "A status bar drawn by the compositor.", "smiley", "0.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    s_panelHoldsBar.reset();
    s_trayMenuHoldsBar.reset();
    g_pPointerHold.reset();
    if (g_pStatusBar) {
        // drop our reserved strip while the hook can still run, then let windows reclaim it
        g_pStatusBar->m_unloading = true;
        g_pStatusBar->rearrangeAll();
        g_pStatusBar.reset();
    }
    g_pTrayMenu.reset();
    g_pControlCenter.reset();
    g_pIdleInhibit.reset(); // lets the session idle again
    clearReservedAreaHook();

    for (auto& m : State::monitorState()->monitors())
        g_pHyprRenderer->damageMonitor(m);
}
