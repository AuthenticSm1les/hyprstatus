// clock: strftime(clock_format). One timer armed for the exact moment the
// text can change (next minute, or next second if the format shows seconds).

#include "Module.hpp"
#include "../core/Config.hpp"
#include "../core/EventLoop.hpp"

#include <algorithm>
#include <array>
#include <ctime>

namespace {
    class CClockModule : public IModule {
      public:
        CClockModule() : m_timer([this] { tick(); }) {
            schedule();
        }

        void build(PHLMONITOR, std::vector<SBarItem>& out) override {
            const auto NOW = std::time(nullptr);
            std::tm    tm{};
            char       buf[256] = {0};
            localtime_r(&NOW, &tm);
            std::strftime(buf, sizeof(buf), g_config.clockFormat->value().c_str(), &tm);
            out.push_back({.text = buf, .fg = configColor(g_config.fgColor)});
        }

        void onConfigChanged() override {
            schedule();
        }

        // The timer runs on the monotonic clock, which stops during suspend.
        // Checking the wall clock on each drawn frame (one time() call) makes
        // the clock right on the first frame after resume.
        void onFrame() override {
            if (bucket() != m_bucket) {
                schedule();
                changed();
            }
        }

      private:
        bool hasSeconds() const {
            static const std::array SPECS = {"%S", "%T", "%r", "%c", "%s", "%X", "%+"};
            const auto&             FMT   = g_config.clockFormat->value();
            return std::ranges::any_of(SPECS, [&](const char* s) { return FMT.contains(s); });
        }

        int64_t bucket() const {
            const auto NOW = std::time(nullptr);
            return hasSeconds() ? NOW : NOW / 60;
        }

        void schedule() {
            using namespace std::chrono;
            m_bucket        = bucket();
            const auto NOW  = system_clock::now();
            const auto NEXT = hasSeconds() ? ceil<seconds>(NOW + milliseconds(1)) : ceil<minutes>(NOW + milliseconds(1));
            m_timer.arm(duration_cast<milliseconds>(NEXT - NOW) + milliseconds(5));
        }

        void tick() {
            schedule();
            changed();
        }

        CWakeTimer  m_timer;
        int64_t m_bucket = -1;
    };
}

UP<IModule> makeClockModule() {
    return makeUnique<CClockModule>();
}
