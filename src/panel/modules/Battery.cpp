// battery: charge level and state (charging, full...)

#include "../PanelModule.hpp"
#include "../Widgets.hpp"
#include "../../core/StatusIcons.hpp"
#include "../../services/Battery.hpp"

#include <format>

namespace {
    class CBatteryRow : public IPanelModule {
      public:
        CBatteryRow() : m_battery(use<CBatteryService>()) {}

        bool visible() const override {
            return m_battery->percent().has_value();
        }
        double height() const override {
            return 28;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const auto& T        = p.theme();
            const int   CAP      = m_battery->percent().value_or(0);
            const bool  CHARGING = m_battery->charging();
            const bool  LOW      = CAP <= 15 && !CHARGING;
            const auto  COL      = LOW ? T.urgent : T.fg;

            p.icon(StatusIcons::battery(CAP, CHARGING, m_battery->status() == "Full"), {box.x, box.y, Widgets::ROW_H, box.h}, COL);
            p.text(std::format("{}%", CAP), {box.x + Widgets::ROW_H + Widgets::GAP, box.y, box.w / 2, box.h}, COL,
                   {.sizeDelta = 2, .align = CPanelPainter::ALIGN_LEFT});
            const auto& STATUS = m_battery->status();
            p.text(STATUS, {box.x + box.w / 2, box.y, box.w / 2, box.h}, T.dim,
                   {.align = CPanelPainter::ALIGN_RIGHT});
        }

      private:
        SP<CBatteryService> m_battery;
    };
}

UP<IPanelModule> makeBatteryPanelModule() {
    return makeUnique<CBatteryRow>();
}
