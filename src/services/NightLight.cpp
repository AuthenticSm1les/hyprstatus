#include "NightLight.hpp"
#include "../core/Util.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static fs::path stateFile() {
    const char* xdg  = getenv("XDG_STATE_HOME");
    const char* home = getenv("HOME");
    const fs::path BASE = xdg && *xdg ? fs::path(xdg) : fs::path(home ? home : "/tmp") / ".local/state";
    return BASE / "hyprstatus" / "nightlight-temperature";
}

static bool inPath(const std::string& exe) {
    const char* path = getenv("PATH");
    std::stringstream ss(path ? path : "/usr/bin");
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::error_code ec;
        if (!dir.empty() && fs::exists(fs::path(dir) / exe, ec))
            return true;
    }
    return false;
}

static bool processRunning(const std::string& comm) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("/proc", ec)) {
        const auto NAME = e.path().filename().string();
        if (NAME.empty() || !std::isdigit((unsigned char)NAME[0]))
            continue;
        if (readFirstLine(e.path().string() + "/comm") == comm)
            return true;
    }
    return false;
}

CNightLightService::CNightLightService() {
    m_installed = inPath("hyprsunset");
    if (const auto SAVED = readFirstLine(stateFile().string()); !SAVED.empty()) {
        try {
            m_temperature = std::clamp(std::stoi(SAVED), MIN_K, MAX_K);
        } catch (...) {}
    }
    refresh();
}

void CNightLightService::refresh() {
    const bool ON = m_installed && processRunning("hyprsunset");
    if (ON != m_on) {
        m_on = ON;
        m_changed.emit();
    }
}

void CNightLightService::toggle() {
    if (!m_installed)
        return;
    if (m_on)
        spawnDetached({"pkill", "-x", "hyprsunset"});
    else
        spawnDetached({"hyprsunset", "-t", std::to_string(m_temperature)});
    m_on = !m_on; // we just started / stopped it; refresh() catches outside changes
    m_changed.emit();
}

void CNightLightService::setTemperature(int kelvin) {
    kelvin = std::clamp(kelvin, MIN_K, MAX_K);
    if (kelvin == m_temperature)
        return;
    m_temperature = kelvin;
    std::error_code ec;
    fs::create_directories(stateFile().parent_path(), ec);
    std::ofstream(stateFile()) << kelvin << '\n';
    if (m_on) {
        m_pending = kelvin;
        push();
    }
    m_changed.emit();
}

// one hyprctl call at a time; while dragging, only the latest value is sent
void CNightLightService::push() {
    if (!m_pending || (m_set && m_set->running()))
        return;
    const int K = *m_pending;
    m_pending.reset();
    m_set = makeUnique<CProcess>(std::vector<std::string>{"hyprctl", "hyprsunset", "temperature", std::to_string(K)}, nullptr, [this](const std::string&) { push(); });
}
