# hyprstatus

A status bar for [Hyprland](https://hyprland.org), implemented as a compositor
plugin. The bar, its control center and its tray menus are drawn by Hyprland
itself rather than by a separate layer-shell client such as Waybar.

> **Compatibility:** built and tested against Hyprland **0.57.x**. Hyprland's
> plugin API is not stable between releases; rebuild the plugin after every
> Hyprland update, and expect source changes to be needed across minor versions.

## Features

- Workspaces, window title, clock, keyboard layout, submap and a system tray
- Status indicators for network, audio, Bluetooth, battery, keep-awake and night light
- A control center panel with volume, microphone, brightness and night-light
  sliders, Wi-Fi / Bluetooth / night-light / keep-awake toggles, battery
  status, power profiles and session actions
- Native rendering of tray (dbusmenu) menus
- Top, bottom, left or right placement, with optional autohide
- Event driven: no polling loops, no helper processes for status updates
- Configured through Hyprland's own config; changes apply immediately

## Requirements

- Hyprland 0.57.x and its plugin headers (`hyprpm update`)
- `systemd-libs` (libudev, sd-bus)
- `wireplumber` 0.5 (libwireplumber, for audio)
- `pkg-config` and a C++23 compiler (GCC or Clang)

Optional runtime services; the related modules hide themselves when absent:

| Service                | Used for                         |
|------------------------|----------------------------------|
| NetworkManager         | network indicator, Wi-Fi tile    |
| BlueZ                  | Bluetooth indicator and tile     |
| power-profiles-daemon  | power profile selector           |
| hyprsunset             | night light                      |
| systemd-logind         | brightness, lock and power actions |

## Installation

### hyprpm

```sh
hyprpm add https://github.com/AuthenticSm1les/hyprstatus
hyprpm enable hyprstatus
```

### Manual

```sh
make
hyprctl plugin load "$PWD/hyprstatus.so"
```

`make clean` removes build output.

## Configuration

All options live under `plugin:hyprstatus:`. With the Lua config:

```lua
hl.config({ plugin = { hyprstatus = { position = "top", height = 28 } } })
```

[`example/hyprstatus.lua`](example/hyprstatus.lua) lists every option with its
default value.

| Option                     | Default                          | Description |
|----------------------------|----------------------------------|-------------|
| `position`                 | `top`                            | `top`, `bottom`, `left` or `right` |
| `height`                   | `28`                             | Bar thickness in logical pixels (width when vertical) |
| `autohide`                 | `false`                          | Reserve no space; reveal the bar when the cursor touches its edge |
| `show_on_empty`            | `false`                          | With `autohide`, keep the bar shown on an empty workspace |
| `passthrough`              | `false`                          | Let windows under the bar receive pointer motion (hover, focus follows mouse); clicks still go to the bar |
| `modules_left`             | `workspaces submap`              | Modules at the start of the bar |
| `modules_center`           | `title`                          | Modules in the middle of the bar |
| `modules_right`            | `tray layout clock controlcenter`| Modules at the end of the bar |
| `controlcenter_indicators` | `network audio bluetooth battery`| Bar modules shown on the control center button |
| `controlcenter_modules`    | see [Panel modules](#panel-modules) | Sections of the control center, top to bottom |
| `persistent_workspaces`    | `0`                              | Always show workspaces 1..N |
| `clock_format`             | `%a %d %b  %H:%M`                | `strftime(3)` format |
| `font`                     | `Sans`                           | Font family |
| `font_size`                | `11`                             | Font size in points |
| `icon_theme`               | `Adwaita`                        | Icon theme for indicators, panel and tray (falls back to `hicolor`) |
| `rounding`                 | `6`                              | Corner radius of pills and panels |
| `col.background`           | `0xE01E1E2E`                     | Bar and panel background |
| `col.text`                 | `0xFFCDD6F4`                     | Text; dimmed text is derived from it |
| `col.accent`               | `0xFF89B4FA`                     | Active workspace, enabled toggles, sliders |
| `col.urgent`               | `0xFFF38BA8`                     | Urgent workspace, low battery, confirmation buttons |

The default colors are from the Catppuccin Mocha palette. Text drawn on a
colored pill uses the background color.

### Keybinding

The plugin exposes one Lua function, which opens or closes the control center
on the focused monitor:

```lua
hl.bind("SUPER + A", function() hl.plugin.hyprstatus.toggle_control_center() end)
```

## Bar modules

| Module          | Shows                                                         | Mouse |
|-----------------|---------------------------------------------------------------|-------|
| `workspaces`    | Workspaces on the bar's monitor; active and urgent are highlighted | Click: switch; scroll: previous / next |
| `title`         | Title of the focused window (on its own monitor only)         | |
| `clock`         | Time and date formatted with `clock_format`                   | |
| `tray`          | StatusNotifierItem icons                                      | Left: activate; middle: secondary activate; right: menu; scroll |
| `layout`        | Layout of the main keyboard                                   | |
| `submap`        | Active submap, if any                                         | |
| `network`       | Wired, Wi-Fi, VPN or disconnected (NetworkManager primary connection); hidden when offline with Wi-Fi off | |
| `audio`         | Output volume level, or muted                                 | |
| `bluetooth`     | Powered on, or a device connected; hidden when off            | |
| `battery`       | Charge level or charging; urgent at 15% or below while discharging | |
| `idle`          | Shown while keep-awake is enabled                             | |
| `nightlight`    | Shown while night light is enabled                            | |
| `controlcenter` | The control center button (see below)                         | Click: open / close |

Status icons are symbolic icons from `icon_theme`, tinted with the bar colors.

### Tray menus

Right-clicking a tray item (or left-clicking one that sets `ItemIsMenu`) opens
its menu. Items that export a dbusmenu (Qt, Electron and libappindicator
applications) have the menu drawn by hyprstatus as a popup beside the bar;
submenus open in place with a back row at the top. Items without a dbusmenu
receive a `ContextMenu` call and show their own.

A menu closes when an entry is chosen, on a click outside it, on clicking the
icon again, on Escape (which first leaves a submenu), when the application
exits, or when the session locks.

hyprstatus acts as the `org.kde.StatusNotifierWatcher`. If another tray host
already owns that name, hyprstatus shows a notification and its tray stays
empty, so it cannot share the tray with another bar.

## Control center

The `controlcenter` module is a single button made from the bar modules in
`controlcenter_indicators`. Its pill uses the accent color while the panel is
open and the urgent color when an indicator requests it (low battery). When no
indicator has anything to show, the button displays a gear icon. Clicking
anywhere on it toggles the panel; indicators keep their own scroll actions.

The panel opens next to the button, or on the focused monitor when triggered by
the keybinding. It closes on Escape, on a click outside it, or when the session
locks.

### Panel modules

`controlcenter_modules` lists the panel's sections from top to bottom. Two
adjacent tiles share a row; a single tile spans the full width. Sections whose
backend is unavailable hide themselves.

| Module              | Shows                                                    | Backend |
|---------------------|----------------------------------------------------------|---------|
| `battery`           | Charge level and state (charging, full, ...)             | sysfs + udev |
| `volume`            | Output volume slider and mute button                     | WirePlumber |
| `mic`               | Input volume slider and mute button                      | WirePlumber |
| `brightness`        | Backlight slider                                         | sysfs + udev; set through logind, no root needed |
| `nightlight_slider` | Night light temperature, 1700–4700 K (only while enabled) | `hyprctl hyprsunset` |
| `wifi`              | Tile: Wi-Fi radio on/off and network name                | NetworkManager |
| `bluetooth`         | Tile: power on/off and connected device                  | BlueZ |
| `nightlight`        | Tile: night light on/off                                 | Starts / stops `hyprsunset` |
| `idle`              | Tile: keep-awake (prevents idle lock, DPMS and suspend)  | Hyprland idle inhibition |
| `powerprofile`      | Power saver / balanced / performance                     | power-profiles-daemon |
| `power`             | Lock, suspend, log out, reboot, power off; the last three require a second click to confirm | logind |

Default: `battery volume mic brightness nightlight_slider wifi bluetooth nightlight idle powerprofile power`

Sliders and the power profile selector also respond to the scroll wheel. Panel
modules and their backends exist only while the panel is open. Keep-awake is
the exception: it stays active until it is switched off or the plugin is
unloaded. The night light temperature is saved in
`$XDG_STATE_HOME/hyprstatus/` (default `~/.local/state/hyprstatus/`).

## Placement and autohide

On a vertical bar (`left` or `right`), modules stack from `modules_left` at the
top to `modules_right` at the bottom, and text wider than the bar is
ellipsized. Short formats work best, for example `clock_format = "%H\n%M"`.
The bar starts after space reserved by other bars on the perpendicular edges,
so a left bar sits below a top layer-shell bar rather than underneath it.

With `autohide = true` the bar reserves no space. It slides in over windows
when the cursor touches its screen edge and slides out 400 ms after the cursor
leaves. It stays shown while the control center or a tray menu is open.

The bar is hidden while a fullscreen window covers its monitor.

## Updates

All state is event driven:

| Source                         | Mechanism |
|--------------------------------|-----------|
| Workspaces, title, submap, layout | Hyprland events |
| Battery, AC adapter            | udev `power_supply` events |
| Brightness                     | udev `backlight` events |
| Tray                           | StatusNotifierItem D-Bus signals; open menus refetch on dbusmenu `LayoutUpdated` / `ItemsPropertiesUpdated` |
| Audio                          | libwireplumber mixer and default-nodes APIs, on a dedicated thread |
| Network, Bluetooth, power profiles | NetworkManager, BlueZ and power-profiles-daemon D-Bus signals |
| Clock                          | A single timer armed for the next minute (or second, if the format shows seconds) |
| Configuration                  | `config.props_refreshed` / `config.reloaded`, so `hl.config()` changes apply immediately |
| Autohide                       | Pointer motion events and a one-shot timer for the hide delay |

Toggles reflect the new state once the corresponding daemon reports it;
sliders follow the pointer while dragging. Night light state is read when the
module is created and updated when toggled from hyprstatus; a `hyprsunset`
started or stopped elsewhere is picked up the next time the module is
rebuilt.

## How it works

- The bar is drawn in the `RENDER_POST_WINDOWS` render stage: above windows and
  below top and overlay layer-shell surfaces such as launchers and
  notifications.
- Space is reserved by hooking `IHyprRenderer::arrangeLayersForMonitor` and
  adding the bar's thickness to the monitor's reserved area after layer
  surfaces are arranged. Exclusive layer-shell bars on the same edge are
  therefore not pushed aside and will overlap it.
- Keep-awake hooks `CInputManager::recheckIdleInhibitorStatus`. If the hook
  cannot be installed, a notification is shown and the `idle` tile is hidden.
- Pointer input over the bar, control center and tray menus is consumed, so
  windows underneath do not receive it (pointer motion over the bar is passed
  through when `passthrough` is enabled).

## Source layout

```
src/main.cpp        plugin entry and exit
src/core/           configuration, event loop timers and fds, D-Bus, child processes, helpers
src/bar/            the bar: layout, drawing, input, autohide, reserved space
src/modules/        bar modules, one file each (registered in Registry.cpp)
src/services/       system backends (battery, tray, audio, network, ...); no drawing
src/panel/          control center and tray menu: container, layout, drawing, input
src/panel/modules/  panel modules, grouped by kind (registered in Registry.cpp)
```
