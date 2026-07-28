#pragma once

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <exception>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace EKore::UI {

enum class MenuLayout {
    Sidebar,
    Tabs,
};

enum class ThemePreset {
    Dark,
    Light,
    Classic,
    Midnight,
    Crimson,
};

enum class NoticeLevel {
    Information,
    Success,
    Warning,
    Error,
};

struct MenuPage {
    std::string id;
    std::string title;
    std::string description;
    std::vector<std::string> keywords;
    std::function<void()> draw;
};

class MenuKit {
public:
    MenuKit() = default;
    MenuKit(const MenuKit&) = delete;
    MenuKit& operator=(const MenuKit&) = delete;

    bool AddPage(MenuPage page) {
        if (page.id.empty() || page.title.empty() || !page.draw ||
            FindPage(page.id) != m_pages.end()) {
            return false;
        }
        if (m_activePage.empty())
            m_activePage = page.id;
        m_pages.push_back(std::move(page));
        return true;
    }

    bool RemovePage(std::string_view id) {
        const auto page = FindPage(id);
        if (page == m_pages.end())
            return false;
        const bool wasActive = page->id == m_activePage;
        m_pages.erase(page);
        if (wasActive)
            m_activePage = m_pages.empty() ? std::string{} : m_pages.front().id;
        return true;
    }

    void ClearPages() {
        m_pages.clear();
        m_activePage.clear();
    }

    [[nodiscard]] std::size_t PageCount() const { return m_pages.size(); }

    bool SetActivePage(std::string_view id) {
        const auto page = FindPage(id);
        if (page == m_pages.end())
            return false;
        m_activePage = page->id;
        return true;
    }

    [[nodiscard]] const std::string& ActivePage() const {
        return m_activePage;
    }

    void SetTitle(std::string title) { m_title = std::move(title); }
    void SetSubtitle(std::string subtitle) {
        m_subtitle = std::move(subtitle);
    }
    void SetLayout(MenuLayout layout) { m_layout = layout; }
    void SetToggleKey(int virtualKey) { m_toggleKey = virtualKey; }
    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool Visible() const { return m_visible; }
    void SetSize(ImVec2 size) { m_size = size; }
    void SetPosition(ImVec2 position) {
        m_position = position;
        m_hasPosition = true;
    }
    void ClearPosition() { m_hasPosition = false; }
    void SetSizeConstraints(ImVec2 minimum, ImVec2 maximum) {
        m_minimumSize = minimum;
        m_maximumSize = maximum;
    }
    void SetWindowFlags(ImGuiWindowFlags flags) { m_windowFlags = flags; }
    void SetHeader(std::function<void()> callback) {
        m_header = std::move(callback);
    }
    void SetFooter(std::function<void()> callback) {
        m_footer = std::move(callback);
    }
    void SetBeforeDraw(std::function<void()> callback) {
        m_beforeDraw = std::move(callback);
    }
    void SetAfterDraw(std::function<void()> callback) {
        m_afterDraw = std::move(callback);
    }
    void SetStyleCustomizer(
        std::function<void(ImGuiStyle&)> callback) {
        m_styleCustomizer = std::move(callback);
        m_themeDirty = true;
    }

    void Notify(
        std::string message,
        NoticeLevel level = NoticeLevel::Information,
        std::chrono::milliseconds lifetime =
            std::chrono::milliseconds(3500)) {
        if (message.empty())
            return;
        std::scoped_lock lock(m_noticeMutex);
        m_notices.push_back({
            std::move(message),
            level,
            std::chrono::steady_clock::now() + lifetime,
        });
    }

    void ApplyTheme(ThemePreset preset) {
        m_theme = preset;
        m_themeDirty = true;
        if (!ImGui::GetCurrentContext())
            return;
        switch (preset) {
            case ThemePreset::Light:
                ImGui::StyleColorsLight();
                break;
            case ThemePreset::Classic:
                ImGui::StyleColorsClassic();
                break;
            default:
                ImGui::StyleColorsDark();
                break;
        }

        auto& style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 5.0f;
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.FramePadding = ImVec2(9.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);

        if (preset == ThemePreset::Midnight ||
            preset == ThemePreset::Crimson) {
            const ImVec4 accent = preset == ThemePreset::Midnight
                ? ImVec4(0.16f, 0.55f, 0.95f, 1.0f)
                : ImVec4(0.88f, 0.16f, 0.25f, 1.0f);
            style.Colors[ImGuiCol_Button] =
                ImVec4(accent.x, accent.y, accent.z, 0.65f);
            style.Colors[ImGuiCol_ButtonHovered] = accent;
            style.Colors[ImGuiCol_ButtonActive] =
                ImVec4(accent.x * 0.8f, accent.y * 0.8f,
                       accent.z * 0.8f, 1.0f);
            style.Colors[ImGuiCol_CheckMark] = accent;
            style.Colors[ImGuiCol_SliderGrab] = accent;
            style.Colors[ImGuiCol_Header] =
                ImVec4(accent.x, accent.y, accent.z, 0.45f);
            style.Colors[ImGuiCol_HeaderHovered] =
                ImVec4(accent.x, accent.y, accent.z, 0.7f);
            style.Colors[ImGuiCol_TabSelected] = accent;
        }
        if (m_styleCustomizer)
            m_styleCustomizer(style);
        m_themeDirty = false;
    }

    /// Draw inside an active ImGui frame. The host owns NewFrame/Render.
    void Draw(bool* externalOpen = nullptr) {
        if (!ImGui::GetCurrentContext())
            return;
        if (m_themeDirty)
            ApplyTheme(m_theme);

        auto& io = ImGui::GetIO();
        if (m_toggleKey != 0 && !io.WantTextInput &&
            (::GetAsyncKeyState(m_toggleKey) & 1)) {
            m_visible = !m_visible;
            if (externalOpen)
                *externalOpen = m_visible;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
            ImGui::OpenPopup("MenuKit command palette");

        bool open = externalOpen ? *externalOpen : m_visible;
        if (m_beforeDraw)
            m_beforeDraw();
        if (open) {
            if (m_hasPosition)
                ImGui::SetNextWindowPos(
                    m_position, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(m_size, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(
                m_minimumSize, m_maximumSize);
            if (ImGui::Begin(
                    m_title.c_str(),
                    &open,
                    m_windowFlags)) {
                DrawMenuBar();
                if (!m_subtitle.empty())
                    ImGui::TextDisabled("%s", m_subtitle.c_str());
                if (m_header)
                    m_header();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint(
                    "##menukit_search",
                    "Search pages (Ctrl+P for palette)",
                    m_search.data(),
                    m_search.size());
                ImGui::Separator();
                if (m_layout == MenuLayout::Sidebar)
                    DrawSidebar();
                else
                    DrawTabs();
                if (m_footer) {
                    ImGui::Separator();
                    m_footer();
                }
            }
            ImGui::End();
        }

        m_visible = open;
        if (externalOpen)
            *externalOpen = open;
        DrawCommandPalette();
        DrawNotifications();
        if (m_afterDraw)
            m_afterDraw();
    }

private:
    struct Notice {
        std::string message;
        NoticeLevel level;
        std::chrono::steady_clock::time_point expires;
    };

    using PageIterator = std::vector<MenuPage>::iterator;

    PageIterator FindPage(std::string_view id) {
        return std::find_if(
            m_pages.begin(), m_pages.end(),
            [id](const MenuPage& page) { return page.id == id; });
    }

    [[nodiscard]] bool Matches(const MenuPage& page) const {
        std::string needle(m_search.data());
        if (needle.empty())
            return true;
        auto lower = [](std::string value) {
            std::transform(
                value.begin(), value.end(), value.begin(),
                [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
            return value;
        };
        needle = lower(std::move(needle));
        if (lower(page.title).find(needle) != std::string::npos ||
            lower(page.description).find(needle) != std::string::npos) {
            return true;
        }
        return std::any_of(
            page.keywords.begin(), page.keywords.end(),
            [&](const std::string& keyword) {
                return lower(keyword).find(needle) != std::string::npos;
            });
    }

    void DrawMenuBar() {
        if (!ImGui::BeginMenuBar())
            return;
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Sidebar", nullptr,
                                m_layout == MenuLayout::Sidebar))
                m_layout = MenuLayout::Sidebar;
            if (ImGui::MenuItem("Tabs", nullptr,
                                m_layout == MenuLayout::Tabs))
                m_layout = MenuLayout::Tabs;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme")) {
            if (ImGui::MenuItem("Dark"))
                ApplyTheme(ThemePreset::Dark);
            if (ImGui::MenuItem("Light"))
                ApplyTheme(ThemePreset::Light);
            if (ImGui::MenuItem("Classic"))
                ApplyTheme(ThemePreset::Classic);
            if (ImGui::MenuItem("Midnight"))
                ApplyTheme(ThemePreset::Midnight);
            if (ImGui::MenuItem("Crimson"))
                ApplyTheme(ThemePreset::Crimson);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    void DrawSidebar() {
        ImGui::BeginChild(
            "##menukit_sidebar", ImVec2(170.0f, 0.0f), true);
        for (const auto& page : m_pages) {
            if (!Matches(page))
                continue;
            if (ImGui::Selectable(
                    page.title.c_str(), page.id == m_activePage))
                m_activePage = page.id;
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##menukit_content", ImVec2(0.0f, 0.0f));
        DrawActivePage();
        ImGui::EndChild();
    }

    void DrawTabs() {
        if (!ImGui::BeginTabBar(
                "##menukit_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
            return;
        for (const auto& page : m_pages) {
            if (!Matches(page) || !ImGui::BeginTabItem(page.title.c_str()))
                continue;
            m_activePage = page.id;
            DrawPage(page);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    void DrawActivePage() {
        const auto page = std::find_if(
            m_pages.begin(), m_pages.end(),
            [&](const MenuPage& candidate) {
                return candidate.id == m_activePage;
            });
        if (page == m_pages.end()) {
            ImGui::TextDisabled("No page selected.");
            return;
        }
        DrawPage(*page);
    }

    static void DrawPage(const MenuPage& page) {
        if (!page.description.empty())
            ImGui::TextWrapped("%s", page.description.c_str());
        if (!page.description.empty())
            ImGui::Separator();
        try {
            page.draw();
        } catch (const std::exception& error) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "Page callback failed: %s",
                error.what());
        } catch (...) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "Page callback failed with an unknown exception.");
        }
    }

    void DrawCommandPalette() {
        if (!ImGui::BeginPopup("MenuKit command palette"))
            return;
        ImGui::TextUnformatted("Jump to page");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputTextWithHint(
            "##palette_search",
            "Type a page name...",
            m_paletteSearch.data(),
            m_paletteSearch.size());
        const std::string filter(m_paletteSearch.data());
        for (const auto& page : m_pages) {
            if (!filter.empty()) {
                std::string title = page.title;
                std::string query = filter;
                std::transform(title.begin(), title.end(), title.begin(),
                               [](unsigned char c) {
                                   return static_cast<char>(std::tolower(c));
                               });
                std::transform(query.begin(), query.end(), query.begin(),
                               [](unsigned char c) {
                                   return static_cast<char>(std::tolower(c));
                               });
                if (title.find(query) == std::string::npos)
                    continue;
            }
            if (ImGui::Selectable(page.title.c_str())) {
                m_activePage = page.id;
                m_visible = true;
                m_paletteSearch.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    void DrawNotifications() {
        std::scoped_lock lock(m_noticeMutex);
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(m_notices, [&](const Notice& notice) {
            return notice.expires <= now;
        });
        if (m_notices.empty())
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        float y = viewport->WorkPos.y + 12.0f;
        for (std::size_t index = 0; index < m_notices.size(); ++index) {
            const auto& notice = m_notices[index];
            ImVec4 color = ImVec4(0.2f, 0.55f, 0.95f, 1.0f);
            if (notice.level == NoticeLevel::Success)
                color = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
            else if (notice.level == NoticeLevel::Warning)
                color = ImVec4(1.0f, 0.7f, 0.15f, 1.0f);
            else if (notice.level == NoticeLevel::Error)
                color = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);

            ImGui::SetNextWindowPos(
                ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, y),
                ImGuiCond_Always,
                ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.94f);
            char id[48]{};
            std::snprintf(id, sizeof(id), "##menukit_notice_%zu", index);
            if (ImGui::Begin(
                    id,
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoDecoration |
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoInputs)) {
                ImGui::TextColored(color, "%s", notice.message.c_str());
                y += ImGui::GetWindowSize().y + 8.0f;
            }
            ImGui::End();
        }
    }

    std::string m_title = "Kore Menu";
    std::string m_subtitle;
    std::vector<MenuPage> m_pages;
    std::string m_activePage;
    MenuLayout m_layout = MenuLayout::Sidebar;
    ThemePreset m_theme = ThemePreset::Dark;
    bool m_themeDirty = true;
    int m_toggleKey = VK_INSERT;
    bool m_visible = true;
    ImVec2 m_size = ImVec2(760.0f, 520.0f);
    ImVec2 m_position{};
    ImVec2 m_minimumSize = ImVec2(420.0f, 300.0f);
    ImVec2 m_maximumSize = ImVec2(FLT_MAX, FLT_MAX);
    ImGuiWindowFlags m_windowFlags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
    bool m_hasPosition = false;
    std::array<char, 128> m_search{};
    std::array<char, 128> m_paletteSearch{};
    std::function<void()> m_header;
    std::function<void()> m_footer;
    std::function<void()> m_beforeDraw;
    std::function<void()> m_afterDraw;
    std::function<void(ImGuiStyle&)> m_styleCustomizer;
    std::mutex m_noticeMutex;
    std::vector<Notice> m_notices;
};

namespace Widgets {

inline void Section(std::string_view title) {
    ImGui::Spacing();
    ImGui::TextDisabled("%.*s", static_cast<int>(title.size()), title.data());
    ImGui::Separator();
}

inline void Help(std::string_view text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (!ImGui::IsItemHovered())
        return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

inline bool Toggle(std::string_view label, bool& value) {
    return ImGui::Checkbox(
        std::string(label).c_str(), &value);
}

inline bool Button(
    std::string_view label,
    ImVec2 size = ImVec2(0.0f, 0.0f)) {
    return ImGui::Button(std::string(label).c_str(), size);
}

inline bool Input(
    std::string_view label,
    std::span<char> buffer,
    std::string_view hint = {}) {
    if (buffer.empty())
        return false;
    const std::string id(label);
    if (hint.empty())
        return ImGui::InputText(id.c_str(), buffer.data(), buffer.size());
    const std::string hintText(hint);
    return ImGui::InputTextWithHint(
        id.c_str(), hintText.c_str(), buffer.data(), buffer.size());
}

inline bool InputMultiline(
    std::string_view label,
    std::span<char> buffer,
    ImVec2 size = ImVec2(-1.0f, 100.0f)) {
    if (buffer.empty())
        return false;
    return ImGui::InputTextMultiline(
        std::string(label).c_str(), buffer.data(), buffer.size(), size);
}

inline bool Slider(
    std::string_view label,
    float& value,
    float minimum,
    float maximum,
    const char* format = "%.3f") {
    return ImGui::SliderFloat(
        std::string(label).c_str(), &value, minimum, maximum, format);
}

inline bool Drag(
    std::string_view label,
    float& value,
    float speed = 1.0f,
    float minimum = 0.0f,
    float maximum = 0.0f,
    const char* format = "%.3f") {
    return ImGui::DragFloat(
        std::string(label).c_str(),
        &value,
        speed,
        minimum,
        maximum,
        format);
}

inline bool Drag(
    std::string_view label,
    int& value,
    float speed = 1.0f,
    int minimum = 0,
    int maximum = 0,
    const char* format = "%d") {
    return ImGui::DragInt(
        std::string(label).c_str(),
        &value,
        speed,
        minimum,
        maximum,
        format);
}

inline bool Slider(
    std::string_view label,
    int& value,
    int minimum,
    int maximum,
    const char* format = "%d") {
    return ImGui::SliderInt(
        std::string(label).c_str(), &value, minimum, maximum, format);
}

inline bool Combo(
    std::string_view label,
    int& selected,
    std::span<const char* const> items) {
    if (items.empty())
        return false;
    if (selected < 0 || selected >= static_cast<int>(items.size()))
        selected = 0;
    bool changed = false;
    if (ImGui::BeginCombo(
            std::string(label).c_str(), items[selected])) {
        for (int index = 0;
             index < static_cast<int>(items.size());
             ++index) {
            const bool active = selected == index;
            if (ImGui::Selectable(items[index], active)) {
                selected = index;
                changed = true;
            }
            if (active)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

inline bool Color(
    std::string_view label,
    std::array<float, 4>& value) {
    return ImGui::ColorEdit4(
        std::string(label).c_str(), value.data());
}

inline void Metric(
    std::string_view label,
    std::string_view value) {
    ImGui::TextDisabled(
        "%.*s", static_cast<int>(label.size()), label.data());
    ImGui::SameLine();
    ImGui::Text(
        "%.*s", static_cast<int>(value.size()), value.data());
}

inline void Progress(
    std::string_view label,
    float fraction,
    ImVec2 size = ImVec2(-1.0f, 0.0f)) {
    const float value = std::clamp(fraction, 0.0f, 1.0f);
    const std::string overlay(label);
    ImGui::ProgressBar(value, size, overlay.c_str());
}

inline void Badge(
    std::string_view text,
    ImVec4 color = ImVec4(0.2f, 0.55f, 0.95f, 1.0f)) {
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::SmallButton(std::string(text).c_str());
    ImGui::PopStyleColor(3);
}

inline bool Keybind(std::string_view label, int& virtualKey) {
    static ImGuiID capture = 0;
    ImGui::PushID(label.data(), label.data() + label.size());
    const ImGuiID id = ImGui::GetID("##keybind");
    std::string button = "Unbound";
    if (capture == id) {
        button = "Press a key...";
    } else if (virtualKey != 0) {
        button = "VK " + std::to_string(virtualKey);
    }

    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::SameLine();
    if (ImGui::Button(button.c_str()))
        capture = id;

    bool changed = false;
    if (capture == id) {
        for (int key = 1; key < 256; ++key) {
            if (!(::GetAsyncKeyState(key) & 1))
                continue;
            virtualKey = key == VK_ESCAPE ? 0 : key;
            capture = 0;
            changed = true;
            break;
        }
    }
    ImGui::PopID();
    return changed;
}

} // namespace Widgets

} // namespace EKore::UI
