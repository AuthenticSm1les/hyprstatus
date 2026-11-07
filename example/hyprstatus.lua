-------------------
---- HYPRSTATUS ---
-------------------

-- Status bar drawn by the compositor itself (no layer-shell client).
-- Values below are the plugin defaults.

if not PluginLoaded("hyprstatus") then return end

hl.config({
    plugin = {
        hyprstatus = {
            position                 = "top",   -- top / bottom / left / right
            height                   = 28,      -- thickness (width when vertical)
            autohide                 = false,   -- reserve no space; reveal when the cursor hits the edge
            show_on_empty            = false,   -- with autohide: stay shown on an empty workspace
            passthrough              = false,   -- windows under the bar see the pointer (hover effects)

            -- bar modules: workspaces title clock layout submap tray controlcenter
            --              network audio bluetooth battery idle nightlight (status icons)
            modules_left             = "workspaces submap",
            modules_center           = "title",
            modules_right            = "tray layout clock controlcenter",

            -- the control center is a group: these bar modules form its button...
            controlcenter_indicators = "network audio bluetooth battery",

            -- ...and these panel modules its menu, top to bottom (tiles pair up two per row):
            -- battery volume mic brightness nightlight_slider wifi bluetooth nightlight idle powerprofile power
            controlcenter_modules    = "battery volume mic brightness nightlight_slider wifi bluetooth nightlight idle powerprofile power",

            persistent_workspaces    = 0,       -- always show 1..N
            clock_format             = "%a %d %b  %H:%M",

            font                     = "Sans",
            font_size                = 11,
            icon_theme               = "Adwaita",
            rounding                 = 6,

            ["col.background"]       = 0xE01E1E2E,
            ["col.text"]             = 0xFFCDD6F4,
            ["col.accent"]           = 0xFF89B4FA,
            ["col.urgent"]           = 0xFFF38BA8,
        },
        -- vertical example:
        -- hyprstatus = { position = "left", height = 40, modules_center = "clock",
        --                modules_right = "tray controlcenter", clock_format = "%H\n%M" },
    },
})

-- open the control center from the keyboard
-- hl.bind("SUPER + A", function() hl.plugin.hyprstatus.toggle_control_center() end)
