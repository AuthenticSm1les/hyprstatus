// workspaces: numbered workspaces of the monitor; click to switch, scroll to cycle

#include "Module.hpp"
#include "../core/Config.hpp"

#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <algorithm>

namespace {
    class CWorkspacesModule : public IModule {
      public:
        CWorkspacesModule() {
            std::function<void()> dirty = [this] { changed(); };
            auto&                 ev    = Event::bus()->m_events;
            for (auto* sig : {&ev.workspace.active})
                m_listeners.emplace_back(sig->listen(dirty));
            m_listeners.emplace_back(ev.workspace.createdLate.listen(dirty));
            m_listeners.emplace_back(ev.workspace.removed.listen(dirty));
            m_listeners.emplace_back(ev.workspace.renamed.listen(dirty));
            m_listeners.emplace_back(ev.workspace.moveToMonitor.listen(dirty));
            // occupied / urgent state
            m_listeners.emplace_back(ev.window.open.listen(dirty));
            m_listeners.emplace_back(ev.window.close.listen(dirty));
            m_listeners.emplace_back(ev.window.urgent.listen(dirty));
            m_listeners.emplace_back(ev.window.moveToWorkspace.listen(dirty));
        }

        void build(PHLMONITOR mon, std::vector<SBarItem>& out) override {
            std::vector<PHLWORKSPACE> list;
            for (auto& ref : State::workspaceState()->workspaceRefs()) {
                const auto WS = ref.lock();
                if (!WS || WS->type() == Workspace::eWorkspaceType::SPECIAL || WS->m_monitor.lock() != mon || !WS->numberedID())
                    continue;
                list.push_back(WS);
            }
            auto num = [](const PHLWORKSPACE& w) { return (int64_t)w->numberedID().value_or(0); };
            std::ranges::sort(list, {}, num);

            // persistent slots 1..N are shown even if the workspace doesn't exist yet
            std::vector<std::pair<int64_t, PHLWORKSPACE>> slots;
            const int64_t                                 PERSIST = g_config.persistentWorkspaces->value();
            for (int64_t i = 1; i <= PERSIST; ++i) {
                auto it = std::ranges::find_if(list, [&](const auto& w) { return num(w) == i; });
                slots.emplace_back(i, it == list.end() ? nullptr : *it);
            }
            for (auto& w : list) {
                if (num(w) > PERSIST)
                    slots.emplace_back(num(w), w);
            }

            const auto ACTIVE = mon->m_activeWorkspace;
            for (auto& [id, ws] : slots) {
                SBarItem item;
                item.text = ws && !ws->displayName().empty() ? ws->displayName() : std::to_string(id);

                if (ws && ws == ACTIVE) {
                    item.fg   = onAccentColor();
                    item.pill = configColor(g_config.accentColor);
                } else if (ws && ws->hasUrgentWindow()) {
                    item.fg   = onAccentColor();
                    item.pill = configColor(g_config.urgentColor);
                } else
                    item.fg = ws && ws->getWindowCount() > 0 ? configColor(g_config.fgColor) : dimColor();

                const std::string TARGET = std::to_string(id);
                item.onClick             = [TARGET](uint32_t, const Vector2D&) { Config::Actions::changeWorkspace(TARGET); };
                item.onScroll            = [](int dir) { Config::Actions::changeWorkspace(dir > 0 ? "m+1" : "m-1"); };
                out.push_back(std::move(item));
            }
        }

      private:
        std::vector<CHyprSignalListener> m_listeners;
    };
}

UP<IModule> makeWorkspacesModule() {
    return makeUnique<CWorkspacesModule>();
}
