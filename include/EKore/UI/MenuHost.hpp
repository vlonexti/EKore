#pragma once

#include <Windows.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace EKore::UI {

struct MenuHostOptions {
    std::wstring title = L"EKore";
    int width = 900;
    int height = 620;
    bool resizable = true;
    bool vsync = true;
    bool topmost = false;
    bool centered = true;
    bool startMaximized = false;
    bool dpiAware = true;
    bool keyboardNavigation = true;
    bool allowSoftwareRenderer = true;
    float clearColor[4] = {0.055f, 0.06f, 0.075f, 1.0f};
    std::string iniFilename;
    std::function<void(HWND)> windowReady;
    std::function<void()> configureImGui;
};

/// Owns a Win32 window, D3D11 device/swap chain, and ImGui frame loop.
///
/// Run blocks until the window closes. The callback is invoked once per frame
/// between ImGui::NewFrame() and ImGui::Render().
class MenuHost {
public:
    struct State;

    MenuHost();
    ~MenuHost();

    MenuHost(const MenuHost&) = delete;
    MenuHost& operator=(const MenuHost&) = delete;

    int Run(
        std::function<void()> frame,
        const MenuHostOptions& options = {});
    void RequestClose();

    [[nodiscard]] HWND Window() const;
    [[nodiscard]] bool Running() const;

private:
    std::unique_ptr<State> m_state;
    std::atomic_bool m_closeRequested = false;
};

} // namespace EKore::UI
