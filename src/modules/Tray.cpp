// tray: StatusNotifierItem icons. Left click activates, middle click is the
// secondary action, right click opens the item's menu, scroll scrolls. Items
// that export a dbusmenu get it drawn by us (panel/TrayMenu); the others are
// asked to show their own context menu.

#include "Module.hpp"
#include "../services/Tray.hpp"
#include "../panel/TrayMenu.hpp"

#include <linux/input-event-codes.h>

#include <format>

namespace {
    class CTrayModule : public IModule {
      public:
        CTrayModule() : m_tray([this] { changed(); }) {}

        void build(PHLMONITOR mon, std::vector<SBarItem>& out) override {
            PHLMONITORREF monRef = mon;
            for (const auto* it : m_tray.items()) {
                const std::string ID = it->id;
                SBarItem          item;
                item.iconKey = std::format("tray|{}|{}", ID, it->generation);
                item.icon    = [this, ID](int size) -> SP<Render::ITexture> {
                    const auto* ITEM = m_tray.find(ID);
                    return ITEM ? m_tray.texture(*ITEM, size) : nullptr;
                };
                item.onClick = [this, ID, monRef](uint32_t button, const Vector2D& pos) {
                    const auto* ITEM = m_tray.find(ID);
                    if (!ITEM)
                        return;
                    const int  X    = pos.x, Y = pos.y;
                    const bool MENU = button == BTN_RIGHT || (button == BTN_LEFT && ITEM->itemIsMenu);
                    if (MENU && !ITEM->menuPath.empty() && g_pTrayMenu)
                        g_pTrayMenu->toggle(monRef.lock(), pos, m_tray.bus(), ITEM->service, ITEM->menuPath, ID);
                    else if (button == BTN_RIGHT)
                        m_tray.contextMenu(*ITEM, X, Y);
                    else if (button == BTN_MIDDLE)
                        m_tray.secondaryActivate(*ITEM, X, Y);
                    else
                        m_tray.activate(*ITEM, X, Y);
                };
                item.onScroll = [this, ID](int dir) {
                    if (const auto* ITEM = m_tray.find(ID))
                        m_tray.scroll(*ITEM, dir * 120);
                };
                out.push_back(std::move(item));
            }
        }

      private:
        CTrayService m_tray;
    };
}

UP<IModule> makeTrayModule() {
    return makeUnique<CTrayModule>();
}
