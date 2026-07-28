# MenuKit

EKore includes a ready-to-run Win32 and Direct3D 11 menu host. The host owns
the window, graphics device, ImGui context, resize handling, and frame loop.

```cpp
EKore::UI::MenuKit menu;
EKore::UI::MenuHost host;

bool monitoring = true;
int refreshRate = 60;

menu.SetTitle("External controller");
menu.ApplyTheme(EKore::UI::ThemePreset::Midnight);
menu.AddPage({
    "process",
    "Process",
    "Authorized target status and controller settings.",
    {"status", "refresh"},
    [&] {
        EKore::UI::Widgets::Toggle("Monitoring", monitoring);
        EKore::UI::Widgets::Slider(
            "Refresh rate", refreshRate, 1, 240, "%d Hz");
    },
});

return host.Run([&] {
    menu.Draw();
});
```

## Included capabilities

- resizable, centered, DPI-aware Windows 10/11 D3D11 host with configurable
  topmost/maximized behavior and optional WARP fallback
- sidebar and tab layouts
- searchable pages and Ctrl+P command palette
- five themes, custom `ImGuiStyle`, window flags, position, size constraints,
  clear color, layout persistence, and setup/frame callbacks
- configurable visibility hotkey
- thread-safe notifications
- header/footer callbacks
- buttons, text input, toggles, sliders, drag controls, combos, colors,
  progress bars, badges, metrics, help text, sections, and keybinds
- custom callbacks for any ImGui widget or drawing

Dear ImGui v1.92.9 and its Win32/D3D11 backends are bundled with EKore. Use
`MenuHostOptions::configureImGui` to add fonts or change `ImGuiIO`, and use
`MenuKit::SetStyleCustomizer` for complete style/color customization.

The menu runs in the controller process, not inside the target process.
