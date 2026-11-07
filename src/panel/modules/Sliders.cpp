// volume, mic, brightness, nightlight_slider: a slider with an icon (a mute button for audio)
// and the percentage

#include "../PanelModule.hpp"
#include "../Widgets.hpp"
#include "../../core/StatusIcons.hpp"
#include "../../services/Audio.hpp"
#include "../../services/Brightness.hpp"
#include "../../services/NightLight.hpp"

#include <cmath>
#include <format>

using namespace Widgets;

namespace {
    constexpr double PCT_W = 52;

    class CSliderRow : public IPanelModule {
      public:
        double height() const override {
            return ROW_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const auto P = parts(box);
            if (hasButton())
                button(p, P.icon, icon(), false, muted());
            else
                p.icon(icon(), P.icon, p.theme().fg);
            slider(p, P.track, value(), muted());
            p.text(known() ? label() : "–", P.pct, p.theme().fg, {.sizeDelta = label().size() > 4 ? -1 : 0});
        }

        bool onPress(const Vector2D& pos, const CBox& box) override {
            const auto P = parts(box);
            if (P.icon.containsPoint(pos)) {
                if (hasButton())
                    onButton();
                return false;
            }
            // the whole height of the row grabs the slider, not just the thin track
            if (pos.x >= P.track.x - 8 && pos.x <= P.track.x + P.track.w + 8) {
                setValue(sliderValueAt(P.track, pos.x));
                return true;
            }
            return false;
        }

        void onDrag(const Vector2D& pos, const CBox& box) override {
            setValue(sliderValueAt(parts(box).track, pos.x));
        }

        void onScroll(int dir, const Vector2D&, const CBox&) override {
            setValue(std::clamp(value() - dir * 0.05F, 0.F, 1.F));
        }

      protected:
        virtual float       value() const = 0; // 0..1
        virtual void        setValue(float v) = 0;
        virtual std::string icon() const = 0;
        virtual std::string label() const {
            return std::format("{}%", (int)std::round(value() * 100));
        }
        virtual bool known() const {
            return true;
        }
        virtual bool muted() const {
            return false;
        }
        virtual bool hasButton() const {
            return false;
        }
        virtual void onButton() {}

      private:
        struct SParts {
            CBox icon, track, pct;
        };
        static SParts parts(const CBox& b) {
            return {{b.x, b.y, ROW_H, ROW_H}, {b.x + ROW_H + GAP, b.y, b.w - ROW_H - GAP - PCT_W, b.h}, {b.x + b.w - PCT_W, b.y, PCT_W, b.h}};
        }
    };

    class CAudioRow : public CSliderRow {
      public:
        explicit CAudioRow(bool input) : m_input(input), m_audio(use<CAudioService>()) {}

      protected:
        const CAudioService::SChannel& ch() const {
            return m_input ? m_audio->input() : m_audio->output();
        }
        float value() const override {
            return std::min(1.F, ch().volume);
        }
        void setValue(float v) override {
            m_audio->setVolume(m_input, v);
        }
        bool known() const override {
            return ch().known;
        }
        bool muted() const override {
            return ch().muted;
        }
        std::string icon() const override {
            return m_input ? StatusIcons::microphone(ch().volume, ch().muted) : StatusIcons::volume(ch().volume, ch().muted);
        }
        bool hasButton() const override {
            return true;
        }
        void onButton() override {
            m_audio->toggleMute(m_input);
        }

      private:
        bool              m_input;
        SP<CAudioService> m_audio;
    };

    class CBrightnessRow : public CSliderRow {
      public:
        CBrightnessRow() : m_brightness(use<CBrightnessService>()) {}

        bool visible() const override {
            return m_brightness->available(); // no backlight (desktop): no row
        }

      protected:
        float value() const override {
            return m_brightness->value();
        }
        void setValue(float v) override {
            m_brightness->set(v);
        }
        std::string icon() const override {
            return "display-brightness-symbolic";
        }

      private:
        SP<CBrightnessService> m_brightness;
    };
}

namespace {
    // shown only while night light is on; warmer to the left
    class CNightLightRow : public CSliderRow {
      public:
        CNightLightRow() : m_night(use<CNightLightService>()) {}

        bool visible() const override {
            return m_night->on();
        }

      protected:
        static constexpr float RANGE = CNightLightService::MAX_K - CNightLightService::MIN_K;
        float value() const override {
            return (m_night->temperature() - CNightLightService::MIN_K) / RANGE;
        }
        void setValue(float v) override {
            // 100 K steps, like GNOME
            m_night->setTemperature(CNightLightService::MIN_K + (int)std::round(v * RANGE / 100) * 100);
        }
        std::string label() const override {
            return std::format("{}K", m_night->temperature());
        }
        std::string icon() const override {
            return "night-light-symbolic";
        }

      private:
        SP<CNightLightService> m_night;
    };
}

UP<IPanelModule> makeNightLightSliderPanelModule() {
    return makeUnique<CNightLightRow>();
}

UP<IPanelModule> makeVolumePanelModule() {
    return makeUnique<CAudioRow>(false);
}
UP<IPanelModule> makeMicPanelModule() {
    return makeUnique<CAudioRow>(true);
}
UP<IPanelModule> makeBrightnessPanelModule() {
    return makeUnique<CBrightnessRow>();
}
