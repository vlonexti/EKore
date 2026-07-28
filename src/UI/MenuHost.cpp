#include <EKore/UI/MenuHost.hpp>

#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>

#if __has_include(<backends/imgui_impl_dx11.h>)
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#else
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#endif

#include <exception>
#include <iterator>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

namespace EKore::UI {
namespace {

constexpr wchar_t kWindowClass[] = L"EKoreMenuHostWindow";

template <typename T>
void Release(T*& object) {
    if (object)
        object->Release();
    object = nullptr;
}

} // namespace

struct MenuHost::State {
    HWND window = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;
    ImGuiContext* previousContext = nullptr;
    ImGuiContext* contextOwned = nullptr;
    bool win32Ready = false;
    bool dx11Ready = false;
    bool running = false;

    void DestroyRenderTarget() {
        Release(renderTarget);
    }

    bool CreateRenderTarget() {
        ID3D11Texture2D* backBuffer = nullptr;
        if (!swapChain ||
            FAILED(swapChain->GetBuffer(
                0, IID_PPV_ARGS(&backBuffer)))) {
            return false;
        }
        const HRESULT result = device->CreateRenderTargetView(
            backBuffer, nullptr, &renderTarget);
        backBuffer->Release();
        return SUCCEEDED(result);
    }

    void Shutdown() {
        running = false;
        if (dx11Ready)
            ImGui_ImplDX11_Shutdown();
        if (win32Ready)
            ImGui_ImplWin32_Shutdown();
        dx11Ready = false;
        win32Ready = false;

        if (contextOwned) {
            ImGui::DestroyContext(contextOwned);
            contextOwned = nullptr;
            ImGui::SetCurrentContext(previousContext);
        }

        DestroyRenderTarget();
        Release(swapChain);
        Release(context);
        Release(device);

        if (window) {
            ::DestroyWindow(window);
            window = nullptr;
        }
        ::UnregisterClassW(kWindowClass, ::GetModuleHandleW(nullptr));
    }
};

namespace {

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(
            window, message, wParam, lParam)) {
        return 1;
    }

    auto* state = reinterpret_cast<MenuHost::State*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto create =
            reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<MenuHost::State*>(
            create->lpCreateParams);
        ::SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
        case WM_SIZE:
            if (state && state->swapChain &&
                wParam != SIZE_MINIMIZED) {
                state->DestroyRenderTarget();
                if (SUCCEEDED(state->swapChain->ResizeBuffers(
                        0,
                        static_cast<UINT>(LOWORD(lParam)),
                        static_cast<UINT>(HIWORD(lParam)),
                        DXGI_FORMAT_UNKNOWN,
                        0))) {
                    state->CreateRenderTarget();
                }
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0u) == SC_KEYMENU)
                return 0;
            break;

        case WM_CLOSE:
            ::DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            if (state && state->running)
                ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(window, message, wParam, lParam);
}

bool CreateDevice(
    MenuHost::State& state,
    bool allowSoftwareRenderer) {
    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferCount = 2;
    swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.OutputWindow = state.window;
    swap.SampleDesc.Count = 1;
    swap.Windowed = TRUE;
    swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selected{};
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT result = ::D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        static_cast<UINT>(std::size(levels)),
        D3D11_SDK_VERSION,
        &swap,
        &state.swapChain,
        &state.device,
        &selected,
        &state.context);
    if (FAILED(result) && allowSoftwareRenderer) {
        result = ::D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags & ~D3D11_CREATE_DEVICE_DEBUG,
            levels,
            static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION,
            &swap,
            &state.swapChain,
            &state.device,
            &selected,
            &state.context);
    }
    return SUCCEEDED(result) && state.CreateRenderTarget();
}

} // namespace

MenuHost::MenuHost() = default;

MenuHost::~MenuHost() {
    RequestClose();
    if (m_state)
        m_state->Shutdown();
}

int MenuHost::Run(
    std::function<void()> frame,
    const MenuHostOptions& options) {
    if (!frame || Running() || options.width < 320 ||
        options.height < 240) {
        return -1;
    }

    m_closeRequested = false;
    m_state = std::make_unique<State>();
    auto& state = *m_state;

    if (options.dpiAware)
        ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW windowClass{
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        WindowProc,
        0,
        0,
        ::GetModuleHandleW(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        kWindowClass,
        nullptr,
    };
    if (!::RegisterClassExW(&windowClass) &&
        ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        m_state.reset();
        return -2;
    }

    const DWORD style = options.resizable
        ? WS_OVERLAPPEDWINDOW
        : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT bounds{0, 0, options.width, options.height};
    ::AdjustWindowRect(&bounds, style, FALSE);
    const int windowWidth = bounds.right - bounds.left;
    const int windowHeight = bounds.bottom - bounds.top;
    const int x = options.centered
        ? (::GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2
        : CW_USEDEFAULT;
    const int y = options.centered
        ? (::GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2
        : CW_USEDEFAULT;
    state.window = ::CreateWindowExW(
        options.topmost ? WS_EX_TOPMOST : 0,
        kWindowClass,
        options.title.c_str(),
        style,
        x,
        y,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        ::GetModuleHandleW(nullptr),
        &state);
    if (!state.window ||
        !CreateDevice(state, options.allowSoftwareRenderer)) {
        state.Shutdown();
        m_state.reset();
        return -3;
    }

    state.previousContext = ImGui::GetCurrentContext();
    IMGUI_CHECKVERSION();
    state.contextOwned = ImGui::CreateContext();
    ImGui::SetCurrentContext(state.contextOwned);
    auto& io = ImGui::GetIO();
    if (options.keyboardNavigation)
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename =
        options.iniFilename.empty() ? nullptr : options.iniFilename.c_str();
    ImGui::StyleColorsDark();
    try {
        if (options.windowReady)
            options.windowReady(state.window);
        if (options.configureImGui)
            options.configureImGui();
    } catch (...) {
        state.Shutdown();
        m_state.reset();
        return -4;
    }

    state.win32Ready = ImGui_ImplWin32_Init(state.window);
    state.dx11Ready = ImGui_ImplDX11_Init(
        state.device, state.context);
    if (!state.win32Ready || !state.dx11Ready) {
        state.Shutdown();
        m_state.reset();
        return -4;
    }

    state.running = true;
    ::ShowWindow(
        state.window,
        options.startMaximized ? SW_MAXIMIZE : SW_SHOWDEFAULT);
    ::UpdateWindow(state.window);

    int exitCode = 0;
    bool done = false;
    while (!done) {
        MSG message{};
        while (::PeekMessageW(
            &message, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
            if (message.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if (m_closeRequested.exchange(false))
            ::PostMessageW(state.window, WM_CLOSE, 0, 0);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        try {
            frame();
        } catch (...) {
            exitCode = -5;
            done = true;
        }
        ImGui::Render();

        const float clear[4] = {
            options.clearColor[0],
            options.clearColor[1],
            options.clearColor[2],
            options.clearColor[3],
        };
        state.context->OMSetRenderTargets(
            1, &state.renderTarget, nullptr);
        state.context->ClearRenderTargetView(
            state.renderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(
            ImGui::GetDrawData());
        state.swapChain->Present(options.vsync ? 1 : 0, 0);
    }

    state.Shutdown();
    m_state.reset();
    return exitCode;
}

void MenuHost::RequestClose() {
    m_closeRequested = true;
}

HWND MenuHost::Window() const {
    return m_state ? m_state->window : nullptr;
}

bool MenuHost::Running() const {
    return m_state && m_state->running;
}

} // namespace EKore::UI
