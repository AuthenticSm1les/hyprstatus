// Status icons: network, audio, bluetooth, battery, idle, nightlight. Each is
// one symbolic icon, hidden while there is nothing to show (radio off, no
// battery...). Usable on their own, and as the indicators of a group.

#include "Module.hpp"
#include "../core/Config.hpp"
#include "../core/Shared.hpp"
#include "../core/StatusIcons.hpp"
#include "../services/Audio.hpp"
#include "../services/Battery.hpp"
#include "../services/Bluetooth.hpp"
#include "../services/IdleInhibit.hpp"
#include "../services/Network.hpp"
#include "../services/NightLight.hpp"

namespace {
    // one icon from one shared service; icon() returns "" to hide
    template <class TService>
    class CStatusModule : public IModule {
      public:
        using FIcon = std::function<std::string(const TService&, SBarItem&)>;

        explicit CStatusModule(FIcon icon) : m_svc(sharedInstance<TService>()), m_icon(std::move(icon)) {
            m_listener = m_svc->m_changed.listen([this] { changed(); });
        }

        void build(PHLMONITOR, std::vector<SBarItem>& out) override {
            SBarItem item{.fg = configColor(g_config.fgColor)};
            item.symbolic = m_icon(*m_svc, item);
            if (!item.symbolic.empty())
                out.push_back(std::move(item));
        }

        SP<TService> m_svc;

      private:
        FIcon               m_icon;
        CHyprSignalListener m_listener;
    };

    std::string networkIcon(const CNetworkService& n, SBarItem&) {
        if (!n.available())
            return "";
        const auto& TYPE = n.connectionType();
        if (n.connection().empty())
            return n.wifiEnabled() ? "network-wireless-offline-symbolic" : ""; // searching; Wi-Fi off and offline: nothing
        if (TYPE == "802-11-wireless")
            return "network-wireless-signal-excellent-symbolic";
        if (TYPE == "vpn" || TYPE == "wireguard")
            return "network-vpn-symbolic";
        return "network-wired-symbolic"; // ethernet, bridges, modems...
    }

    std::string audioIcon(const CAudioService& a, SBarItem&) {
        const auto& OUT = a.output();
        return OUT.known ? StatusIcons::volume(OUT.volume, OUT.muted) : "";
    }

    std::string bluetoothIcon(const CBluetoothService& b, SBarItem&) {
        if (!b.available() || !b.powered())
            return ""; // off: hidden, like a disabled radio
        return b.connectedDevices().empty() ? "bluetooth-active-symbolic" : "bluetooth-symbolic";
    }

    std::string batteryIcon(const CBatteryService& b, SBarItem& item) {
        if (!b.percent())
            return "";
        const int CAP = *b.percent();
        if (CAP <= 15 && !b.charging()) { // low: urgent pill
            item.fg   = onAccentColor();
            item.pill = configColor(g_config.urgentColor);
        }
        return StatusIcons::battery(CAP, b.charging(), b.status() == "Full");
    }

    std::string nightLightIcon(const CNightLightService& n, SBarItem&) {
        return n.on() ? "night-light-symbolic" : "";
    }

    // idle: shown while the session is kept awake
    class CIdleModule : public IModule {
      public:
        CIdleModule() {
            if (g_pIdleInhibit)
                m_listener = g_pIdleInhibit->m_changed.listen([this] { changed(); });
        }

        void build(PHLMONITOR, std::vector<SBarItem>& out) override {
            if (g_pIdleInhibit && g_pIdleInhibit->active())
                out.push_back({.fg = configColor(g_config.fgColor), .symbolic = "view-reveal-symbolic"});
        }

      private:
        CHyprSignalListener m_listener;
    };
}

UP<IModule> makeStatusModule(const std::string& name) {
    if (name == "network")
        return makeUnique<CStatusModule<CNetworkService>>(networkIcon);
    if (name == "audio")
        return makeUnique<CStatusModule<CAudioService>>(audioIcon);
    if (name == "bluetooth")
        return makeUnique<CStatusModule<CBluetoothService>>(bluetoothIcon);
    if (name == "battery")
        return makeUnique<CStatusModule<CBatteryService>>(batteryIcon);
    if (name == "nightlight") {
        auto m = makeUnique<CStatusModule<CNightLightService>>(nightLightIcon);
        m->m_svc->refresh(); // hyprsunset may already run
        return m;
    }
    if (name == "idle")
        return makeUnique<CIdleModule>();
    return nullptr;
}
