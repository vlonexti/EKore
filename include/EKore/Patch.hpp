#pragma once

#include <EKore/Process.hpp>

#include <span>
#include <vector>

namespace EKore {

/// Reversible remote-memory patch. It duplicates the process handle, snapshots
/// the original bytes, and reverts an applied patch on destruction by default.
class Patch {
public:
    Patch() = default;
    Patch(const Process& process, Address address,
          std::span<const u8> replacement);
    ~Patch();

    Patch(const Patch&) = delete;
    Patch& operator=(const Patch&) = delete;
    Patch(Patch&& other) noexcept;
    Patch& operator=(Patch&& other) noexcept;

    bool Apply();
    bool Revert();

    [[nodiscard]] bool Valid() const {
        return m_process != nullptr && m_address != 0 &&
               !m_replacement.empty() &&
               m_original.size() == m_replacement.size();
    }
    [[nodiscard]] bool Applied() const { return m_applied; }
    [[nodiscard]] Address Target() const { return m_address; }
    [[nodiscard]] DWORD LastError() const { return m_lastError; }

    void SetAutoRevert(bool enabled) { m_autoRevert = enabled; }
    [[nodiscard]] bool AutoRevert() const { return m_autoRevert; }

private:
    void Close();
    bool Write(std::span<const u8> bytes);

    HANDLE          m_process = nullptr;
    Address         m_address = 0;
    std::vector<u8> m_original;
    std::vector<u8> m_replacement;
    bool            m_applied = false;
    bool            m_autoRevert = true;
    DWORD           m_lastError = ERROR_SUCCESS;
};

class Freezer {
public:
    explicit Freezer(const Process& process) : m_process(&process) {}

    template <typename T>
    void Hold(std::string tag, Address address, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* data = reinterpret_cast<const u8*>(&value);
        HoldBytes(std::move(tag), address,
                  std::vector<u8>(data, data + sizeof(T)));
    }

    void HoldBytes(std::string tag, Address address, std::vector<u8> value);
    void Release(std::string_view tag);
    void ReleaseAll();
    [[nodiscard]] bool Held(std::string_view tag) const;
    [[nodiscard]] std::size_t Count() const { return m_entries.size(); }

    /// Rewrite every held entry once. The caller controls scheduling.
    [[nodiscard]] std::size_t Tick() const;

private:
    struct Entry {
        std::string     tag;
        Address         address = 0;
        std::vector<u8> bytes;
    };

    const Process*     m_process = nullptr;
    std::vector<Entry> m_entries;
};

} // namespace EKore

