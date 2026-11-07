// A group: bar modules (its indicators) drawn as one button, with a menu (a
// panel of panel modules) that the button opens. The control center is the
// group "controlcenter": indicators from controlcenter_indicators, menu
// modules from controlcenter_modules.

#include "Module.hpp"
#include "../core/Config.hpp"
#include "../core/Util.hpp"
#include "../panel/ControlCenter.hpp"

namespace {
    class CGroupModule : public IModule {
      public:
        CGroupModule(SP<Config::Values::CStringValue> indicators, CControlCenter* menu, int id) : m_indicators(std::move(indicators)), m_menu(menu), m_id(id) {
            if (m_menu)
                m_openListener = m_menu->m_openChanged.listen([this] { changed(); });
            onConfigChanged();
        }

        void onConfigChanged() override {
            if (m_indicators->value() != m_indicatorsConf) {
                m_indicatorsConf = m_indicators->value();
                m_children.clear();
                for (const auto& name : splitWords(m_indicatorsConf)) {
                    auto m = name == "controlcenter" ? nullptr : createModule(name); // no group inside itself
                    if (!m) {
                        HyprlandAPI::addNotification(PHANDLE, "[hyprstatus] unknown module in a group: " + name, CHyprColor{1.0, 0.6, 0.2, 1.0}, 5000);
                        continue;
                    }
                    m->m_onChanged = [this] { changed(); };
                    m_children.push_back(std::move(m));
                }
                changed();
            } else
                for (auto& c : m_children)
                    c->onConfigChanged();
        }

        void onFrame() override {
            for (auto& c : m_children)
                c->onFrame();
        }

        void build(PHLMONITOR mon, std::vector<SBarItem>& out) override {
            std::vector<SBarItem> items;
            for (auto& c : m_children)
                c->build(mon, items);
            if (items.empty()) // nothing to show (yet): still offer the button
                items.push_back({.fg = configColor(g_config.fgColor), .symbolic = "emblem-system-symbolic"});

            // open: accent pill; else an indicator's pill (low battery) colors the whole group
            std::optional<CHyprColor> pill;
            if (m_menu && m_menu->isOpen())
                pill = configColor(g_config.accentColor);
            for (const auto& it : items)
                if (!pill && it.pill)
                    pill = it.pill;

            PHLMONITORREF monRef = mon;
            for (auto& it : items) {
                it.join = m_id;
                it.pill = pill;
                if (pill)
                    it.fg = onAccentColor();
                it.onClick = [this, monRef](uint32_t, const Vector2D& pos) { // indicators' own scroll stays
                    if (m_menu)
                        m_menu->toggle(monRef.lock(), pos);
                };
                out.push_back(std::move(it));
            }
        }

      private:
        SP<Config::Values::CStringValue> m_indicators;
        std::string                      m_indicatorsConf = "\x01"; // forces the first sync
        CControlCenter*                  m_menu;
        int                              m_id;
        std::vector<UP<IModule>>         m_children;
        CHyprSignalListener              m_openListener;
    };
}

UP<IModule> makeControlCenterModule() {
    return makeUnique<CGroupModule>(g_config.ccIndicators, g_pControlCenter.get(), 1);
}
