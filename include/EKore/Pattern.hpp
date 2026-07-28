#pragma once

#include <EKore/Process.hpp>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace EKore {

struct ScanOptions {
    std::size_t chunkSize = 1024 * 1024;
    std::size_t maxResults = 0;
    bool executableOnly = false;
    bool writableOnly = false;
};

class Pattern {
public:
    /// IDA syntax: "48 8B 05 ? ? ? ? 48 85 C0". Both ? and ?? are wildcards.
    explicit Pattern(std::string_view signature);

    [[nodiscard]] static Pattern Exact(std::span<const u8> bytes);

    [[nodiscard]] bool Valid() const;
    [[nodiscard]] std::size_t Size() const { return m_bytes.size(); }

    [[nodiscard]] std::optional<Address> Scan(
        const Process& process, Address base, std::size_t size,
        ScanOptions options = {}) const;

    [[nodiscard]] std::vector<Address> ScanAll(
        const Process& process, Address base, std::size_t size,
        ScanOptions options = {}) const;

    [[nodiscard]] std::optional<Address> ScanModule(
        const Process& process, const Module& module,
        ScanOptions options = {}) const {
        return Scan(process, module.base, module.size, options);
    }

    [[nodiscard]] std::vector<Address> ScanModuleAll(
        const Process& process, const Module& module,
        ScanOptions options = {}) const {
        return ScanAll(process, module.base, module.size, options);
    }

private:
    Pattern(std::vector<u8> bytes, std::vector<bool> mask)
        : m_bytes(std::move(bytes)), m_mask(std::move(mask)) {}

    [[nodiscard]] bool Matches(std::span<const u8> data,
                               std::size_t offset) const;

    std::vector<u8>   m_bytes;
    std::vector<bool> m_mask;
};

template <typename T>
[[nodiscard]] Pattern ValuePattern(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* data = reinterpret_cast<const u8*>(&value);
    return Pattern::Exact(std::span<const u8>(data, sizeof(T)));
}

} // namespace EKore
