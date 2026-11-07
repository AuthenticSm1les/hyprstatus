#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../core/Bus.hpp"

// One entry of a com.canonical.dbusmenu tree (what Qt, Electron and
// libappindicator tray items export instead of drawing a menu themselves).
struct SMenuNode {
    int                    id = 0;
    std::string            label; // mnemonics stripped
    bool                   separator = false, enabled = true, visible = true, submenu = false;
    enum eToggle : uint8_t {
        TOGGLE_NONE,
        TOGGLE_CHECK,
        TOGGLE_RADIO,
    } toggle    = TOGGLE_NONE;
    bool                   checked = false;
    std::vector<SMenuNode> children;
};

// Client for one exported menu. Fetches the whole tree and refetches it
// whenever the app says it changed.
class CDbusMenu {
  public:
    // onChange: the tree changed (or arrived); onGone: the app left or has no menu
    CDbusMenu(SP<CBus> bus, std::string service, std::string path, std::function<void()> onChange, std::function<void()> onGone);
    ~CDbusMenu();

    bool             loaded() const { return m_loaded; }
    const SMenuNode& root() const { return m_root; }
    const SMenuNode* find(int id) const; // nullptr if it went away

    // before showing (sub)menu `id`: apps fill it in lazily
    void             aboutToShow(int id);
    void             clicked(int id);

  private:
    void             fetch();
    void             onLayout(sd_bus_message* m);

    SP<CBus>              m_bus;
    std::string           m_service, m_path;
    std::function<void()> m_onChange, m_onGone;
    SP<bool>              m_alive = makeShared<bool>(true);

    SMenuNode             m_root;
    bool                  m_loaded = false;
    bool                  m_fetching = false, m_refetch = false;
    CSlot                 m_signals, m_nameOwner;
};
