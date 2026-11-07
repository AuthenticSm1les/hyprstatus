#include "DbusMenu.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

static constexpr const char* MENU_IFACE = "com.canonical.dbusmenu";

// "_Open" -> "Open", "__" -> "_"
static std::string stripMnemonic(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '_') {
            if (i + 1 < s.size() && s[i + 1] == '_')
                out += s[++i];
            continue;
        }
        out += s[i];
    }
    return out;
}

static int readVariantInt(sd_bus_message* m) {
    int v = 0;
    if (sd_bus_message_read(m, "v", "i", &v) < 0)
        sd_bus_message_skip(m, "v");
    return v;
}

// (ia{sv}av) at the current read position
static bool readNode(sd_bus_message* m, SMenuNode& n) {
    if (sd_bus_message_enter_container(m, 'r', "ia{sv}av") <= 0)
        return false;
    sd_bus_message_read(m, "i", &n.id);

    readProperties(m, [&](std::string_view key, std::string_view sig) {
        if (key == "label" && sig == "s")
            n.label = stripMnemonic(readVariantString(m));
        else if (key == "type" && sig == "s")
            n.separator = readVariantString(m) == "separator";
        else if (key == "enabled" && sig == "b")
            n.enabled = readVariantBool(m);
        else if (key == "visible" && sig == "b")
            n.visible = readVariantBool(m);
        else if (key == "children-display" && sig == "s")
            n.submenu = readVariantString(m) == "submenu";
        else if (key == "toggle-type" && sig == "s") {
            const auto T = readVariantString(m);
            n.toggle     = T == "checkmark" ? SMenuNode::TOGGLE_CHECK : T == "radio" ? SMenuNode::TOGGLE_RADIO : SMenuNode::TOGGLE_NONE;
        } else if (key == "toggle-state" && sig == "i")
            n.checked = readVariantInt(m) == 1;
        else
            return false;
        return true;
    });

    if (sd_bus_message_enter_container(m, 'a', "v") >= 0) {
        while (sd_bus_message_enter_container(m, 'v', "(ia{sv}av)") > 0) {
            SMenuNode child;
            if (readNode(m, child))
                n.children.push_back(std::move(child));
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }
    // some apps leave children-display out but still send children
    if (!n.children.empty())
        n.submenu = true;

    sd_bus_message_exit_container(m);
    return true;
}

static const SMenuNode* findIn(const SMenuNode& n, int id) {
    if (n.id == id)
        return &n;
    for (const auto& c : n.children)
        if (const auto* F = findIn(c, id))
            return F;
    return nullptr;
}

CDbusMenu::CDbusMenu(SP<CBus> bus, std::string service, std::string path, std::function<void()> onChange, std::function<void()> onGone) :
    m_bus(std::move(bus)), m_service(std::move(service)), m_path(std::move(path)), m_onChange(std::move(onChange)), m_onGone(std::move(onGone)) {
    // LayoutUpdated, ItemsPropertiesUpdated: refetch the tree
    sd_bus_match_signal_async(
        m_bus->get(), m_signals.out(), m_service.c_str(), m_path.c_str(), MENU_IFACE, nullptr,
        [](sd_bus_message* m, void* data, sd_bus_error*) -> int {
            auto*            self = sc<CDbusMenu*>(data);
            const std::string_view MEMBER = sd_bus_message_get_member(m) ? sd_bus_message_get_member(m) : "";
            if (MEMBER == "LayoutUpdated" || MEMBER == "ItemsPropertiesUpdated")
                self->fetch();
            return 0;
        },
        nullptr, this);

    // the app quits while its menu is open
    sd_bus_match_signal_async(
        m_bus->get(), m_nameOwner.out(), "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged",
        [](sd_bus_message* m, void* data, sd_bus_error*) -> int {
            auto*       self = sc<CDbusMenu*>(data);
            const char *name = nullptr, *oldOwner = nullptr, *newOwner = nullptr;
            if (sd_bus_message_read(m, "sss", &name, &oldOwner, &newOwner) >= 0 && name && newOwner && !*newOwner && self->m_service == name && self->m_onGone)
                self->m_onGone();
            return 0;
        },
        nullptr, this);

    aboutToShow(0);
    fetch();
}

CDbusMenu::~CDbusMenu() {
    m_signals.reset();
    m_nameOwner.reset();
}

const SMenuNode* CDbusMenu::find(int id) const {
    return m_loaded ? findIn(m_root, id) : nullptr;
}

// one GetLayout in flight at a time; changes arriving meanwhile fetch once more
void CDbusMenu::fetch() {
    if (m_fetching) {
        m_refetch = true;
        return;
    }
    m_fetching = true;
    m_bus->call(m_service.c_str(), m_path.c_str(), MENU_IFACE, "GetLayout", m_alive, [this](sd_bus_message* m) { onLayout(m); }, "iias", 0, -1, 0);
}

void CDbusMenu::onLayout(sd_bus_message* m) {
    m_fetching = false;
    if (sd_bus_message_is_method_error(m, nullptr)) {
        if (!m_loaded && m_onGone) // no usable menu at all
            m_onGone();
        return;
    }

    uint32_t  revision = 0;
    SMenuNode root;
    if (sd_bus_message_read(m, "u", &revision) >= 0 && readNode(m, root)) {
        m_root   = std::move(root);
        m_loaded = true;
        if (m_onChange)
            m_onChange();
    }

    if (std::exchange(m_refetch, false))
        fetch();
}

void CDbusMenu::aboutToShow(int id) {
    // a true reply means the app changed the submenu; it usually signals
    // LayoutUpdated too, but not always
    m_bus->call(m_service.c_str(), m_path.c_str(), MENU_IFACE, "AboutToShow", m_alive,
                [this](sd_bus_message* m) {
                    int needUpdate = 0;
                    if (!sd_bus_message_is_method_error(m, nullptr) && sd_bus_message_read(m, "b", &needUpdate) >= 0 && needUpdate)
                        fetch();
                },
                "i", id);
}

void CDbusMenu::clicked(int id) {
    m_bus->call(m_service.c_str(), m_path.c_str(), MENU_IFACE, "Event", m_alive, {}, "isvu", id, "clicked", "i", 0, (uint32_t)0);
}
