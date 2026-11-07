#include "Config.hpp"

CHyprColor configColor(const SP<Config::Values::CColorValue>& v) {
    return CHyprColor{sc<uint64_t>(v->value())};
}

CHyprColor dimColor() {
    auto c = configColor(g_config.fgColor);
    c.a *= 0.45;
    return c;
}

CHyprColor onAccentColor() {
    auto c = configColor(g_config.bgColor);
    c.a    = 1.0;
    return c;
}

void registerConfig(HANDLE handle) {
    using namespace Config::Values;
    auto& c = g_config;
    // clang-format off
    c.position             = makeShared<CStringValue>("plugin:hyprstatus:position", "top, bottom, left or right", "top");
    c.height               = makeShared<CIntValue>("plugin:hyprstatus:height", "Bar thickness (logical px)", 28);

    c.autohide             = makeShared<CBoolValue>("plugin:hyprstatus:autohide", "Hide the bar until the cursor touches its screen edge (reserves no space)", false);
    c.showOnEmpty          = makeShared<CBoolValue>("plugin:hyprstatus:show_on_empty", "With autohide: keep the bar shown on an empty workspace", false);
    c.passthrough          = makeShared<CBoolValue>("plugin:hyprstatus:passthrough", "Let windows under the bar see the pointer (hover effects)", false);

    c.font                 = makeShared<CStringValue>("plugin:hyprstatus:font", "Font family", "Sans");
    c.iconTheme            = makeShared<CStringValue>("plugin:hyprstatus:icon_theme", "Icon theme", "Adwaita");
    c.fontSize             = makeShared<CIntValue>("plugin:hyprstatus:font_size", "Text size (pt)", 11);
    c.rounding             = makeShared<CIntValue>("plugin:hyprstatus:rounding", "Rounding of pills and panels", 6);

    c.modulesLeft          = makeShared<CStringValue>("plugin:hyprstatus:modules_left", "Modules at the start of the bar", "workspaces submap");
    c.modulesCenter        = makeShared<CStringValue>("plugin:hyprstatus:modules_center", "Modules in the middle of the bar", "title");
    c.modulesRight         = makeShared<CStringValue>("plugin:hyprstatus:modules_right", "Modules at the end of the bar", "tray layout clock controlcenter");

    c.persistentWorkspaces = makeShared<CIntValue>("plugin:hyprstatus:persistent_workspaces", "Always show workspaces 1..N", 0);
    c.clockFormat          = makeShared<CStringValue>("plugin:hyprstatus:clock_format", "strftime format", "%a %d %b  %H:%M");
    c.ccIndicators         = makeShared<CStringValue>("plugin:hyprstatus:controlcenter_indicators",
                                                      "Bar modules shown together as the control center button (e.g. network audio bluetooth battery idle nightlight)", "network audio bluetooth battery");
    c.ccModules            = makeShared<CStringValue>("plugin:hyprstatus:controlcenter_modules",
                                                      "Panel modules in the control center menu: battery volume mic brightness nightlight_slider wifi bluetooth nightlight idle powerprofile power",
                                                      "battery volume mic brightness nightlight_slider wifi bluetooth nightlight idle powerprofile power");

    c.bgColor              = makeShared<CColorValue>("plugin:hyprstatus:col.background", "Bar and panel background", 0xE01E1E2E);
    c.fgColor              = makeShared<CColorValue>("plugin:hyprstatus:col.text", "Text color", 0xFFCDD6F4);
    c.accentColor          = makeShared<CColorValue>("plugin:hyprstatus:col.accent", "Active workspace, enabled toggles, sliders", 0xFF89B4FA);
    c.urgentColor          = makeShared<CColorValue>("plugin:hyprstatus:col.urgent", "Urgent workspace, low battery, confirm buttons", 0xFFF38BA8);
    // clang-format on

    for (const SP<IValue>& v : std::initializer_list<SP<IValue>>{
             c.position,    c.height,        c.autohide,     c.showOnEmpty, c.passthrough, c.font,         c.iconTheme,    c.fontSize,   c.rounding,
             c.modulesLeft, c.modulesCenter, c.modulesRight, c.persistentWorkspaces, c.clockFormat, c.ccIndicators, c.ccModules,
             c.bgColor,     c.fgColor,       c.accentColor,  c.urgentColor})
        HyprlandAPI::addConfigValueV2(handle, v);
}
