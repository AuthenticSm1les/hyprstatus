// wifi, bluetooth, nightlight, idle: toggle tiles with their state below the name

#include "../PanelModule.hpp"
#include "../Widgets.hpp"
#include "../../services/Bluetooth.hpp"
#include "../../services/Network.hpp"
#include "../../services/NightLight.hpp"
#include "../../services/IdleInhibit.hpp"

using namespace Widgets;

namespace {
    class CWifiTile : public IPanelModule {
      public:
        CWifiTile() : m_net(use<CNetworkService>()) {}

        bool tile() const override {
            return true;
        }
        double height() const override {
            return TILE_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const bool  ON    = m_net->available() && m_net->wifiEnabled();
            std::string state = !m_net->available() ? "Unavailable" : !ON ? "Off" : !m_net->wifiConnection().empty() ? m_net->wifiConnection() : "Not connected";
            Widgets::tile(p, box, ON ? "network-wireless-signal-excellent-symbolic" : "network-wireless-disabled-symbolic", "Wi-Fi", state, ON);
        }

        bool onPress(const Vector2D&, const CBox&) override {
            m_net->toggleWifi();
            return false;
        }

      private:
        SP<CNetworkService> m_net;
    };

    class CBluetoothTile : public IPanelModule {
      public:
        CBluetoothTile() : m_bt(use<CBluetoothService>()) {}

        bool tile() const override {
            return true;
        }
        double height() const override {
            return TILE_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const bool  ON    = m_bt->available() && m_bt->powered();
            std::string state = !m_bt->available() ? "Unavailable" : !ON ? "Off" : !m_bt->connectedDevices().empty() ? m_bt->connectedDevices().front() : "On";
            Widgets::tile(p, box, ON ? "bluetooth-active-symbolic" : "bluetooth-disabled-symbolic", "Bluetooth", state, ON);
        }

        bool onPress(const Vector2D&, const CBox&) override {
            m_bt->togglePower();
            return false;
        }

      private:
        SP<CBluetoothService> m_bt;
    };
}

namespace {
    class CNightLightTile : public IPanelModule {
      public:
        CNightLightTile() : m_night(use<CNightLightService>()) {
            m_night->refresh(); // it may have been started or stopped elsewhere
        }

        bool visible() const override {
            return m_night->installed();
        }
        bool tile() const override {
            return true;
        }
        double height() const override {
            return TILE_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const bool ON = m_night->on();
            Widgets::tile(p, box, ON ? "night-light-symbolic" : "night-light-disabled-symbolic", "Night Light", ON ? std::to_string(m_night->temperature()) + "K" : "Off", ON);
        }

        bool onPress(const Vector2D&, const CBox&) override {
            m_night->toggle();
            return false;
        }

      private:
        SP<CNightLightService> m_night;
    };

    // the service is the plugin's (it must outlive the panel); only listen here
    class CIdleTile : public IPanelModule {
      public:
        CIdleTile() {
            if (g_pIdleInhibit)
                m_listener = g_pIdleInhibit->m_changed.listen([this] { changed(); });
        }

        bool visible() const override {
            return g_pIdleInhibit && g_pIdleInhibit->available();
        }
        bool tile() const override {
            return true;
        }
        double height() const override {
            return TILE_H;
        }

        void draw(CPanelPainter& p, const CBox& box) override {
            const bool ON = g_pIdleInhibit->active();
            Widgets::tile(p, box, ON ? "view-reveal-symbolic" : "view-conceal-symbolic", "Keep Awake", ON ? "On" : "Off", ON);
        }

        bool onPress(const Vector2D&, const CBox&) override {
            g_pIdleInhibit->toggle();
            return false;
        }

      private:
        CHyprSignalListener m_listener;
    };
}

UP<IPanelModule> makeNightLightPanelModule() {
    return makeUnique<CNightLightTile>();
}
UP<IPanelModule> makeIdlePanelModule() {
    return makeUnique<CIdleTile>();
}

UP<IPanelModule> makeWifiPanelModule() {
    return makeUnique<CWifiTile>();
}
UP<IPanelModule> makeBluetoothPanelModule() {
    return makeUnique<CBluetoothTile>();
}
