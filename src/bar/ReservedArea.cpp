#include "ReservedArea.hpp"

#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/state/MonitorState.hpp>

static CFunctionHook*                   g_hook = nullptr;
static std::function<void(PHLMONITOR)> g_after;
using origArrange                      = void (*)(void*, const MONITORID&);

static void hkArrangeLayersForMonitor(void* thisptr, const MONITORID& id) {
    (*(origArrange)g_hook->m_original)(thisptr, id);
    if (!g_after)
        return;
    if (const auto MON = State::monitorState()->query().id(id).run())
        g_after(MON);
}

bool installReservedAreaHook(HANDLE handle, std::function<void(PHLMONITOR)> after) {
    g_after = std::move(after);
    for (const auto& fn : HyprlandAPI::findFunctionsByName(handle, "arrangeLayersForMonitor")) {
        if (!fn.demangled.contains("IHyprRenderer::arrangeLayersForMonitor"))
            continue;
        g_hook = HyprlandAPI::createFunctionHook(handle, fn.address, (void*)::hkArrangeLayersForMonitor);
        break;
    }
    return g_hook && g_hook->hook();
}

// hooks are removed by Hyprland on unload; this just stops the callback
void clearReservedAreaHook() {
    g_after = nullptr;
}
