#include "IdleInhibit.hpp"

#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/protocols/IdleNotify.hpp>

static CFunctionHook* g_hook = nullptr;
using origRecheck            = void (*)(void*);

// Hyprland recomputes "inhibited" from windows on every change; add ours on top
static void hkRecheckIdleInhibitorStatus(void* thisptr) {
    (*(origRecheck)g_hook->m_original)(thisptr);
    if (g_pIdleInhibit && g_pIdleInhibit->active() && PROTO::idle)
        PROTO::idle->setInhibit(true);
}

bool CIdleInhibitService::install(HANDLE handle) {
    for (const auto& fn : HyprlandAPI::findFunctionsByName(handle, "recheckIdleInhibitorStatus")) {
        if (!fn.demangled.contains("CInputManager::recheckIdleInhibitorStatus"))
            continue;
        g_hook = HyprlandAPI::createFunctionHook(handle, fn.address, (void*)::hkRecheckIdleInhibitorStatus);
        break;
    }
    return g_hook && g_hook->hook();
}

CIdleInhibitService::~CIdleInhibitService() {
    if (m_active) {
        m_active = false;
        if (g_pInputManager)
            g_pInputManager->recheckIdleInhibitorStatus();
    }
}

bool CIdleInhibitService::available() const {
    return g_hook != nullptr;
}

void CIdleInhibitService::set(bool on) {
    if (on == m_active || !available())
        return;
    m_active = on;
    g_pInputManager->recheckIdleInhibitorStatus(); // runs our hook
    m_changed.emit();
}
