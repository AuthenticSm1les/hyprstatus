// powerprofile: power saver / balanced / performance (power-profiles-daemon)

#include "../PanelModule.hpp"
#include "../Widgets.hpp"
#include "../../services/PowerProfiles.hpp"

#include <array>

using namespace Widgets;

namespace {
    struct SProfile {
        const char* id;
        const char* icon;
        const char* label;
    };
    const std::array<SProfile, 3> PROFILES = {{
        {"power-saver", "power-profile-power-saver-symbolic", "Power Saver"},
        {"balanced", "power-profile-balanced-symbolic", "Balanced"},
        {"performance", "power-profile-performance-symbolic", "Performance"},
    }};

    class CPowerProfileRow : public IPanelModule {
      public:
        CPowerProfileRow() : m_ppd(use<CPowerProfilesService>()) {}

        bool visible() const override {
            return m_ppd->available();
        }
        double height() const override {
            return TILE_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const auto& T = p.theme();
            for (size_t i = 0; i < PROFILES.size(); ++i) {
                const auto& PR     = PROFILES[i];
                const CBox  C      = cell(box, i, PROFILES.size());
                const bool  ACTIVE = m_ppd->active() == PR.id;
                const bool  OFFERS = m_ppd->has(PR.id);
                const auto  COL    = ACTIVE ? T.onAccent : OFFERS ? T.fg : T.dim;
                p.rect(C, ACTIVE ? T.accent : T.surface, T.rounding);
                p.icon(PR.icon, {C.x, C.y + 6, C.w, C.h * 0.5 - 6}, COL);
                p.text(PR.label, {C.x, C.y + C.h * 0.5, C.w, C.h * 0.5 - 6}, COL, {.sizeDelta = -2});
            }
        }

        bool onPress(const Vector2D& pos, const CBox& box) override {
            const int I = cellAt(box, pos.x, PROFILES.size());
            if (I >= 0 && m_ppd->has(PROFILES[I].id))
                m_ppd->set(PROFILES[I].id);
            return false;
        }

        // scroll steps through the offered profiles
        void onScroll(int dir, const Vector2D&, const CBox&) override {
            int cur = -1;
            for (size_t i = 0; i < PROFILES.size(); ++i)
                if (m_ppd->active() == PROFILES[i].id)
                    cur = i;
            for (int i = cur - dir; i >= 0 && i < (int)PROFILES.size(); i -= dir) {
                if (m_ppd->has(PROFILES[i].id)) {
                    m_ppd->set(PROFILES[i].id);
                    return;
                }
            }
        }

      private:
        SP<CPowerProfilesService> m_ppd;
    };
}

UP<IPanelModule> makePowerProfilePanelModule() {
    return makeUnique<CPowerProfileRow>();
}
