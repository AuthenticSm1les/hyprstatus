// power: lock, suspend, log out, reboot, power off. The last three ask for a
// second click within 3 s.

#include "../PanelModule.hpp"
#include "../Widgets.hpp"
#include "../../core/EventLoop.hpp"
#include "../../services/Power.hpp"

#include <array>
#include <optional>

using namespace Widgets;

namespace {
    struct SAction {
        const char* icon;
        bool        destructive;
        void (CPowerService::*run)();
    };
    const std::array<SAction, 5> ACTIONS = {{
        {"system-lock-screen-symbolic", false, &CPowerService::lock},
        {"media-playback-pause-symbolic", false, &CPowerService::suspend}, // GNOME's suspend icon
        {"system-log-out-symbolic", true, &CPowerService::logout},
        {"system-reboot-symbolic", true, &CPowerService::reboot},
        {"system-shutdown-symbolic", true, &CPowerService::powerOff},
    }};

    class CPowerRow : public IPanelModule {
      public:
        CPowerRow() : m_power(makeShared<CPowerService>()) {
            m_confirmTimer = makeUnique<CWakeTimer>([this] {
                m_confirm.reset();
                changed();
            });
        }

        double height() const override {
            return BUTTON_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            for (size_t i = 0; i < ACTIONS.size(); ++i) {
                const bool CONFIRM = m_confirm == i;
                // waiting for the confirming click: same icon, urgent color
                button(p, cell(box, i, ACTIONS.size()), ACTIONS[i].icon, false, CONFIRM);
            }
        }

        bool onPress(const Vector2D& pos, const CBox& box) override {
            const int I = cellAt(box, pos.x, ACTIONS.size());
            if (I < 0)
                return false;
            const auto& A = ACTIONS[I];
            if (A.destructive && m_confirm != (size_t)I) {
                m_confirm = I;
                m_confirmTimer->arm(std::chrono::seconds(3));
                changed();
                return false;
            }
            // the panel destroys its modules after this returns, not during
            auto power = m_power;
            closePanel();
            ((*power).*A.run)();
            return false;
        }

      private:
        SP<CPowerService>     m_power;
        std::optional<size_t> m_confirm;
        UP<CWakeTimer>        m_confirmTimer;
    };
}

UP<IPanelModule> makePowerPanelModule() {
    return makeUnique<CPowerRow>();
}
