#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

#include <functional>

// Hyprland recomputes each monitor's layer-shell reserved area in
// IHyprRenderer::arrangeLayersForMonitor (resetting it first). This hooks that
// function and runs `after` once the original finished, so space can be added.
bool installReservedAreaHook(HANDLE handle, std::function<void(PHLMONITOR)> after);
void clearReservedAreaHook();
