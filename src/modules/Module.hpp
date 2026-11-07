#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../bar/Item.hpp"

// A bar module. Owns its own event subscriptions / timers and calls changed()
// when its content may differ; the bar then rebuilds items via build().
// Modules exist only while listed in the config.
class IModule {
  public:
    virtual ~IModule() = default;

    // append this module's items for one monitor
    virtual void build(PHLMONITOR monitor, std::vector<SBarItem>& out) = 0;

    // plugin options changed
    virtual void onConfigChanged() {}

    // called before each drawn bar frame; cheap checks only
    virtual void onFrame() {}

    std::function<void()> m_onChanged; // set by the bar

  protected:
    void changed() {
        if (m_onChanged)
            m_onChanged();
    }
};

// "workspaces", "clock", "text:foo", ... -> module, or nullptr if unknown
UP<IModule> createModule(const std::string& name);
