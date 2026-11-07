// title: focused window's title, on the monitor that window is on

#include "Module.hpp"
#include "../core/Config.hpp"
#include "../core/Util.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>

namespace {
    class CTitleModule : public IModule {
      public:
        CTitleModule() {
            std::function<void()> dirty = [this] { changed(); };
            auto&                 ev    = Event::bus()->m_events;
            m_listeners.emplace_back(ev.window.active.listen(dirty));
            m_listeners.emplace_back(ev.window.title.listen(dirty));
            m_listeners.emplace_back(ev.window.close.listen(dirty));
            m_listeners.emplace_back(ev.window.moveToWorkspace.listen(dirty));
            m_listeners.emplace_back(ev.monitor.focused.listen(dirty));
        }

        void build(PHLMONITOR mon, std::vector<SBarItem>& out) override {
            const auto W = Desktop::focusState()->window();
            if (!W || W->m_monitor.lock() != mon || W->metadata().title().empty())
                return;
            out.push_back({.text = truncateUtf8(W->metadata().title(), Style::TITLE_MAX), .fg = configColor(g_config.fgColor)});
        }

      private:
        std::vector<CHyprSignalListener> m_listeners;
    };
}

UP<IModule> makeTitleModule() {
    return makeUnique<CTitleModule>();
}
