#include "../PanelModule.hpp"

UP<IPanelModule> makeBatteryPanelModule();
UP<IPanelModule> makeVolumePanelModule();
UP<IPanelModule> makeMicPanelModule();
UP<IPanelModule> makeBrightnessPanelModule();
UP<IPanelModule> makeWifiPanelModule();
UP<IPanelModule> makeBluetoothPanelModule();
UP<IPanelModule> makePowerProfilePanelModule();
UP<IPanelModule> makePowerPanelModule();
UP<IPanelModule> makeNightLightPanelModule();
UP<IPanelModule> makeNightLightSliderPanelModule();
UP<IPanelModule> makeIdlePanelModule();

UP<IPanelModule> createPanelModule(const std::string& name) {
    if (name == "battery")
        return makeBatteryPanelModule();
    if (name == "volume")
        return makeVolumePanelModule();
    if (name == "mic")
        return makeMicPanelModule();
    if (name == "brightness")
        return makeBrightnessPanelModule();
    if (name == "wifi")
        return makeWifiPanelModule();
    if (name == "bluetooth")
        return makeBluetoothPanelModule();
    if (name == "powerprofile")
        return makePowerProfilePanelModule();
    if (name == "nightlight")
        return makeNightLightPanelModule();
    if (name == "nightlight_slider")
        return makeNightLightSliderPanelModule();
    if (name == "idle")
        return makeIdlePanelModule();
    if (name == "power")
        return makePowerPanelModule();
    return nullptr;
}
