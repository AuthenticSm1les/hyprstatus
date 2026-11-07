// Small event-fed modules: layout, submap

#include "Module.hpp"
#include "../core/Config.hpp"

#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/event/EventBus.hpp>

namespace {
    // layout: active keyboard layout of the main keyboard
    class CLayoutModule : public IModule {
      public:
        CLayoutModule() {
            if (g_pSeatManager && g_pSeatManager->m_keyboard)
                m_layout = g_pSeatManager->m_keyboard->getActiveLayout();
            m_listener = Event::bus()->m_events.input.keyboard.layout.listen([this](SP<IKeyboard>, const std::string& layout) {
                m_layout = layout;
                changed();
            });
        }

        void build(PHLMONITOR, std::vector<SBarItem>& out) override {
            if (!m_layout.empty())
                out.push_back({.text = m_layout, .fg = configColor(g_config.fgColor)});
        }

      private:
        std::string         m_layout;
        CHyprSignalListener m_listener;
    };

    // submap: name of the active keybind submap, hidden when none
    class CSubmapModule : public IModule {
      public:
        CSubmapModule() {
            m_listener = Event::bus()->m_events.keybinds.submap.listen([this](const std::string& name) {
                m_submap = name;
                changed();
            });
        }

        void build(PHLMONITOR, std::vector<SBarItem>& out) override {
            if (!m_submap.empty())
                out.push_back({.text = m_submap, .fg = onAccentColor(), .pill = configColor(g_config.accentColor)});
        }

      private:
        std::string         m_submap;
        CHyprSignalListener m_listener;
    };
}

UP<IModule> makeLayoutModule() {
    return makeUnique<CLayoutModule>();
}
UP<IModule> makeSubmapModule() {
    return makeUnique<CSubmapModule>();
}
