#include "Module.hpp"

UP<IModule> makeWorkspacesModule();
UP<IModule> makeTitleModule();
UP<IModule> makeClockModule();
UP<IModule> makeLayoutModule();
UP<IModule> makeSubmapModule();
UP<IModule> makeTrayModule();
UP<IModule> makeControlCenterModule();
UP<IModule> makeStatusModule(const std::string& name); // network audio bluetooth battery idle nightlight

UP<IModule> createModule(const std::string& name) {
    if (name == "workspaces")
        return makeWorkspacesModule();
    if (name == "title")
        return makeTitleModule();
    if (name == "clock")
        return makeClockModule();
    if (name == "layout")
        return makeLayoutModule();
    if (name == "submap")
        return makeSubmapModule();
    if (name == "tray")
        return makeTrayModule();
    if (name == "controlcenter")
        return makeControlCenterModule();
    return makeStatusModule(name);
}
