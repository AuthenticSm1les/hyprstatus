#include "Tray.hpp"
#include "Icons.hpp"
#include "../core/Config.hpp"

#include <hyprland/src/render/Renderer.hpp>

#include <hyprgraphics/image/Image.hpp>
#include <drm_fourcc.h>

#include <algorithm>
#include <format>

static constexpr const char* WATCHER_NAME  = "org.kde.StatusNotifierWatcher";
static constexpr const char* WATCHER_PATH  = "/StatusNotifierWatcher";
static constexpr const char* WATCHER_IFACE = "org.kde.StatusNotifierWatcher";
static constexpr const char* ITEM_IFACE    = "org.kde.StatusNotifierItem";

// ---------------------------------------------------------------- watcher object

static int methodRegisterItem(sd_bus_message* m, void* data, sd_bus_error*) {
    auto*       self = sc<CTrayService*>(data);
    const char* arg  = nullptr;
    if (sd_bus_message_read(m, "s", &arg) < 0 || !arg)
        return sd_bus_reply_method_return(m, "");

    // items register either a bus name or (Ayatana) an object path on their own connection
    const std::string SENDER = sd_bus_message_get_sender(m) ? sd_bus_message_get_sender(m) : "";
    std::string       id;
    if (arg[0] == '/')
        id = SENDER + arg;
    else
        id = std::string(arg) + "/StatusNotifierItem";

    self->addItem(id);
    self->emitWatcherSignal("StatusNotifierItemRegistered", id.c_str());
    return sd_bus_reply_method_return(m, "");
}

static int methodRegisterHost(sd_bus_message* m, void* data, sd_bus_error*) {
    sc<CTrayService*>(data)->emitWatcherSignal("StatusNotifierHostRegistered", nullptr);
    return sd_bus_reply_method_return(m, "");
}

static int propItems(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* data, sd_bus_error*) {
    const auto IDS = sc<CTrayService*>(data)->registeredIds();
    sd_bus_message_open_container(reply, 'a', "s");
    for (const auto& id : IDS)
        sd_bus_message_append(reply, "s", id.c_str());
    return sd_bus_message_close_container(reply);
}

static int propHostRegistered(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "b", 1);
}

static int propProtocol(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "i", 0);
}

static const sd_bus_vtable WATCHER_VTABLE[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("RegisterStatusNotifierItem", "s", "", methodRegisterItem, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RegisterStatusNotifierHost", "s", "", methodRegisterHost, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("RegisteredStatusNotifierItems", "as", propItems, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("IsStatusNotifierHostRegistered", "b", propHostRegistered, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ProtocolVersion", "i", propProtocol, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_SIGNAL("StatusNotifierItemRegistered", "s", 0),
    SD_BUS_SIGNAL("StatusNotifierItemUnregistered", "s", 0),
    SD_BUS_SIGNAL("StatusNotifierHostRegistered", "", 0),
    SD_BUS_SIGNAL("StatusNotifierHostUnregistered", "", 0),
    SD_BUS_VTABLE_END,
};

// ---------------------------------------------------------------- lifecycle

CTrayService::CTrayService(std::function<void()> onChange) : m_onChange(std::move(onChange)) {
    m_bus = CBus::user();
    if (!m_bus)
        return;
    auto* bus = m_bus->get();

    if (sd_bus_request_name(bus, WATCHER_NAME, 0) < 0) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprstatus] another tray (StatusNotifierWatcher) is running; the tray stays empty", CHyprColor{1.0, 0.6, 0.2, 1.0}, 5000);
        m_bus.reset();
        return;
    }

    // items whose app exits are dropped
    sd_bus_match_signal_async(
        bus, m_nameOwnerSlot.out(), "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged",
        [](sd_bus_message* m, void* data, sd_bus_error*) -> int {
            const char *name = nullptr, *oldOwner = nullptr, *newOwner = nullptr;
            if (sd_bus_message_read(m, "sss", &name, &oldOwner, &newOwner) >= 0 && newOwner && !*newOwner && name)
                sc<CTrayService*>(data)->onServiceGone(name);
            return 0;
        },
        nullptr, this);

    sd_bus_add_object_vtable(bus, m_vtableSlot.out(), WATCHER_PATH, WATCHER_IFACE, WATCHER_VTABLE, this);
    // running apps watch the watcher name and register with us on their own
    emitWatcherSignal("StatusNotifierHostRegistered", nullptr);
    m_bus->process();
}

CTrayService::~CTrayService() {
    m_items.clear();
    m_vtableSlot.reset();
    m_nameOwnerSlot.reset();
    if (m_bus)
        sd_bus_release_name(m_bus->get(), WATCHER_NAME);
}

void CTrayService::emitWatcherSignal(const char* member, const char* arg) {
    if (!m_bus)
        return;
    if (arg)
        sd_bus_emit_signal(m_bus->get(), WATCHER_PATH, WATCHER_IFACE, member, "s", arg);
    else
        sd_bus_emit_signal(m_bus->get(), WATCHER_PATH, WATCHER_IFACE, member, "");
}

void CTrayService::onServiceGone(const std::string& name) {
    std::vector<std::string> gone;
    for (const auto& [id, item] : m_items) {
        if (item->service == name)
            gone.push_back(id);
    }
    for (const auto& id : gone) {
        removeItem(id);
        emitWatcherSignal("StatusNotifierItemUnregistered", id.c_str());
    }
}

// ---------------------------------------------------------------- items

std::vector<std::string> CTrayService::registeredIds() const {
    return m_order;
}

STrayItem* CTrayService::find(const std::string& id) {
    auto it = m_items.find(id);
    return it == m_items.end() ? nullptr : it->second.get();
}

// The signal match and the pending GetAll live inside the item, so they are
// cancelled when the item is destroyed and callbacks can point at the item.
void CTrayService::addItem(const std::string& id) {
    if (m_items.contains(id))
        return;

    auto item   = makeUnique<STrayItem>();
    item->owner = this;
    item->id    = id;
    if (const auto SLASH = id.find('/'); SLASH != std::string::npos) {
        item->service = id.substr(0, SLASH);
        item->path    = id.substr(SLASH);
    } else {
        item->service = id;
        item->path    = "/StatusNotifierItem";
    }

    // NewIcon / NewStatus / NewTitle / ... -> re-read properties
    sd_bus_match_signal_async(
        m_bus->get(), item->signals.out(), item->service.c_str(), item->path.c_str(), ITEM_IFACE, nullptr,
        [](sd_bus_message*, void* data, sd_bus_error*) -> int {
            auto* it = sc<STrayItem*>(data);
            it->owner->refresh(*it);
            return 0;
        },
        nullptr, item.get());

    m_order.push_back(id);
    auto& ref = *item;
    m_items.emplace(id, std::move(item));
    refresh(ref);
}

void CTrayService::removeItem(const std::string& id) {
    if (!m_items.erase(id))
        return;
    std::erase(m_order, id);
    changed();
}

void CTrayService::refresh(STrayItem& item) {
    sd_bus_call_method_async(
        m_bus->get(), item.pending.out(), item.service.c_str(), item.path.c_str(), "org.freedesktop.DBus.Properties", "GetAll",
        [](sd_bus_message* m, void* data, sd_bus_error*) -> int {
            auto* it = sc<STrayItem*>(data);
            it->owner->onGetAll(*it, m);
            return 0;
        },
        &item, "s", ITEM_IFACE);
}

static std::vector<STrayPixmap> readPixmaps(sd_bus_message* m) {
    std::vector<STrayPixmap> out;
    if (sd_bus_message_enter_container(m, 'v', "a(iiay)") < 0)
        return out;
    if (sd_bus_message_enter_container(m, 'a', "(iiay)") >= 0) {
        while (sd_bus_message_enter_container(m, 'r', "iiay") > 0) {
            int         w = 0, h = 0;
            const void* data = nullptr;
            size_t      n    = 0;
            if (sd_bus_message_read(m, "ii", &w, &h) >= 0 && sd_bus_message_read_array(m, 'y', &data, &n) >= 0 && w > 0 && h > 0 && n == (size_t)w * h * 4)
                out.push_back({w, h, std::vector<uint8_t>((const uint8_t*)data, (const uint8_t*)data + n)});
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);
    return out;
}

void CTrayService::onGetAll(STrayItem& item, sd_bus_message* m) {
    if (sd_bus_message_is_method_error(m, nullptr))
        return;
    if (sd_bus_message_enter_container(m, 'a', "{sv}") < 0)
        return;

    while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
        const char* key      = nullptr;
        char        type     = 0;
        const char* contents = nullptr;
        sd_bus_message_read(m, "s", &key);
        sd_bus_message_peek_type(m, &type, &contents);
        const std::string_view K   = key ? key : "";
        const std::string_view SIG = contents ? contents : "";

        auto readString = [&](std::string& dst) {
            const char* v = nullptr;
            if (sd_bus_message_read(m, "v", "s", &v) >= 0)
                dst = v ? v : "";
        };

        if (SIG == "s" && K == "IconName")
            readString(item.iconName);
        else if (SIG == "s" && K == "IconThemePath")
            readString(item.iconThemePath);
        else if (SIG == "s" && K == "AttentionIconName")
            readString(item.attentionIconName);
        else if (SIG == "s" && K == "Title")
            readString(item.title);
        else if (SIG == "s" && K == "Status")
            readString(item.status);
        else if (SIG == "b" && K == "ItemIsMenu") {
            int b = 0;
            if (sd_bus_message_read(m, "v", "b", &b) >= 0)
                item.itemIsMenu = b;
        } else if (SIG == "o" && K == "Menu") {
            const char* v = nullptr;
            if (sd_bus_message_read(m, "v", "o", &v) >= 0)
                item.menuPath = v && std::string_view{v} != "/" && std::string_view{v} != "/NO_DBUSMENU" ? v : "";
        } else if (SIG == "a(iiay)" && K == "IconPixmap")
            item.pixmaps = readPixmaps(m);
        else if (SIG == "a(iiay)" && K == "AttentionIconPixmap")
            item.attentionPixmaps = readPixmaps(m);
        else
            sd_bus_message_skip(m, "v");

        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);

    item.loaded = true;
    item.generation++;
    changed();
}

// ---------------------------------------------------------------- rendering

// SNI pixmaps are ARGB32 in network byte order, straight alpha; Hyprland
// textures want little-endian ARGB8888 (B,G,R,A in memory), premultiplied.
static SP<Render::ITexture> pixmapTexture(const STrayPixmap& px) {
    std::vector<uint8_t> buf(px.argb.size());
    for (size_t i = 0; i + 3 < px.argb.size(); i += 4) {
        const uint32_t A = px.argb[i], R = px.argb[i + 1], G = px.argb[i + 2], B = px.argb[i + 3];
        buf[i]           = B * A / 255;
        buf[i + 1]       = G * A / 255;
        buf[i + 2]       = R * A / 255;
        buf[i + 3]       = A;
    }
    return g_pHyprRenderer->createTexture(DRM_FORMAT_ARGB8888, buf.data(), px.w * 4, Vector2D{(double)px.w, (double)px.h});
}

SP<Render::ITexture> CTrayService::texture(const STrayItem& item, int size) {
    const bool  ATTENTION = item.status == "NeedsAttention";
    const auto& PIX       = ATTENTION && !item.attentionPixmaps.empty() ? item.attentionPixmaps : item.pixmaps;

    if (!PIX.empty()) {
        // smallest pixmap >= size, else the largest
        const STrayPixmap* best = nullptr;
        for (const auto& p : PIX) {
            if (p.w >= size && (!best || p.w < best->w))
                best = &p;
        }
        if (!best)
            best = &*std::ranges::max_element(PIX, {}, &STrayPixmap::w);
        return pixmapTexture(*best);
    }

    const auto& NAME = ATTENTION && !item.attentionIconName.empty() ? item.attentionIconName : item.iconName;
    const auto  PATH = findIcon(NAME, item.iconThemePath);
    if (PATH.empty())
        return nullptr;
    Hyprgraphics::CImage img(PATH, Vector2D{(double)size, (double)size});
    if (!img.success())
        return nullptr;
    return g_pHyprRenderer->createTexture(img.cairoSurface()->cairo());
}

// ---------------------------------------------------------------- actions

void CTrayService::call(const STrayItem& item, const char* method, int a, int b) {
    sd_bus_call_method_async(m_bus->get(), nullptr, item.service.c_str(), item.path.c_str(), ITEM_IFACE, method, nullptr, nullptr, "ii", a, b);
    m_bus->process();
}

void CTrayService::activate(const STrayItem& item, int x, int y) {
    call(item, item.itemIsMenu ? "ContextMenu" : "Activate", x, y);
}

void CTrayService::secondaryActivate(const STrayItem& item, int x, int y) {
    call(item, "SecondaryActivate", x, y);
}

void CTrayService::contextMenu(const STrayItem& item, int x, int y) {
    call(item, "ContextMenu", x, y);
}

void CTrayService::scroll(const STrayItem& item, int delta) {
    sd_bus_call_method_async(m_bus->get(), nullptr, item.service.c_str(), item.path.c_str(), ITEM_IFACE, "Scroll", nullptr, nullptr, "is", delta, "vertical");
    m_bus->process();
}

void CTrayService::changed() {
    if (m_onChange)
        m_onChange();
}

std::vector<const STrayItem*> CTrayService::items() const {
    std::vector<const STrayItem*> out;
    for (const auto& id : m_order) {
        auto it = m_items.find(id);
        if (it != m_items.end() && it->second->loaded && it->second->status != "Passive")
            out.push_back(it->second.get());
    }
    return out;
}
