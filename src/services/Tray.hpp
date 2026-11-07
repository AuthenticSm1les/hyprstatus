#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/render/Texture.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../core/Bus.hpp"

struct STrayPixmap {
    int                  w = 0, h = 0;
    std::vector<uint8_t> argb; // network byte order, as sent over D-Bus
};

class CTrayService;

struct STrayItem {
    CTrayService*            owner = nullptr;
    std::string              id; // "service/path"
    std::string              service, path;
    std::string              iconName, iconThemePath, attentionIconName, title, status;
    std::string              menuPath; // com.canonical.dbusmenu object, "" if none
    std::vector<STrayPixmap> pixmaps, attentionPixmaps;
    bool                     itemIsMenu = false;
    bool                     loaded     = false; // first GetAll arrived
    uint64_t                 generation = 0;     // bumps whenever the icon may have changed
    CSlot                    signals, pending;
};

// System tray: the StatusNotifierWatcher (and its host). Apps register with
// it over D-Bus; another tray running at the same time leaves this one empty.
// Fully signal driven.
class CTrayService {
  public:
    explicit CTrayService(std::function<void()> onChange);
    ~CTrayService();

    bool                          ok() const { return (bool)m_bus; }
    SP<CBus>                      bus() const { return m_bus; }

    // shown items, in registration order (Passive ones hidden, like other hosts)
    std::vector<const STrayItem*> items() const;

    // GL context must be current. Pixmap if the app sent one, else themed icon.
    SP<Render::ITexture>          texture(const STrayItem& item, int size);

    void                          activate(const STrayItem& item, int x, int y);
    void                          secondaryActivate(const STrayItem& item, int x, int y);
    void                          contextMenu(const STrayItem& item, int x, int y);
    void                          scroll(const STrayItem& item, int delta);

    // --- internal, called from sd-bus callbacks
    void                     addItem(const std::string& id);
    void                     removeItem(const std::string& id);
    void                     refresh(STrayItem& item);
    void                     onGetAll(STrayItem& item, sd_bus_message* m);
    void                     onServiceGone(const std::string& name);
    void                     emitWatcherSignal(const char* member, const char* arg);
    std::vector<std::string> registeredIds() const;
    STrayItem*               find(const std::string& id);

  private:
    void                                 call(const STrayItem& item, const char* method, int a, int b);
    void                                 changed();

    SP<CBus>                             m_bus;
    std::function<void()>                m_onChange;
    std::vector<std::string>             m_order;
    std::map<std::string, UP<STrayItem>> m_items;
    CSlot                                m_nameOwnerSlot, m_vtableSlot;
};
