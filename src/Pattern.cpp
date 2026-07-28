#include <EKore/Pattern.hpp>

#include <algorithm>
#include <cctype>
#include <limits>

namespace EKore {
namespace {

int HexValue(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

Address SaturatingEnd(Address base, std::size_t size) {
    if (size > kMaxAddress - base)
        return kMaxAddress;
    return base + static_cast<Address>(size);
}

} // namespace

Pattern::Pattern(std::string_view signature) {
    std::size_t index = 0;
    while (index < signature.size()) {
        const char current = signature[index];
        if (std::isspace(static_cast<unsigned char>(current))) {
            ++index;
            continue;
        }

        if (current == '?') {
            m_bytes.push_back(0);
            m_mask.push_back(false);
            ++index;
            if (index < signature.size() && signature[index] == '?')
                ++index;
            continue;
        }

        if (index + 1 >= signature.size()) {
            m_bytes.clear();
            m_mask.clear();
            return;
        }

        const int high = HexValue(current);
        const int low = HexValue(signature[index + 1]);
        if (high < 0 || low < 0) {
            m_bytes.clear();
            m_mask.clear();
            return;
        }

        m_bytes.push_back(static_cast<u8>((high << 4) | low));
        m_mask.push_back(true);
        index += 2;
    }
}

Pattern Pattern::Exact(std::span<const u8> bytes) {
    return Pattern(
        std::vector<u8>(bytes.begin(), bytes.end()),
        std::vector<bool>(bytes.size(), true));
}

bool Pattern::Valid() const {
    return !m_bytes.empty() && m_bytes.size() == m_mask.size() &&
           std::any_of(m_mask.begin(), m_mask.end(),
                       [](bool exact) { return exact; });
}

bool Pattern::Matches(std::span<const u8> data,
                      std::size_t offset) const {
    if (offset > data.size() || m_bytes.size() > data.size() - offset)
        return false;

    for (std::size_t index = 0; index < m_bytes.size(); ++index) {
        if (m_mask[index] &&
            data[offset + index] != m_bytes[index]) {
            return false;
        }
    }
    return true;
}

std::optional<Address> Pattern::Scan(
    const Process& process, Address base, std::size_t size,
    ScanOptions options) const {
    options.maxResults = 1;
    auto results = ScanAll(process, base, size, options);
    if (results.empty())
        return std::nullopt;
    return results.front();
}

std::vector<Address> Pattern::ScanAll(
    const Process& process, Address base, std::size_t size,
    ScanOptions options) const {
    std::vector<Address> results;
    if (!Valid() || !process || size < m_bytes.size())
        return results;

    const Address scanEnd = SaturatingEnd(base, size);
    if (scanEnd <= base)
        return results;

    const std::size_t chunkSize =
        std::max(options.chunkSize, m_bytes.size());
    const std::size_t overlap = m_bytes.size() - 1;

    for (const MemoryRegion& region : process.Regions(base, scanEnd)) {
        if (!region.Readable())
            continue;
        if (options.executableOnly && !region.Executable())
            continue;
        if (options.writableOnly && !region.Writable())
            continue;

        const Address regionBegin = std::max(base, region.base);
        const Address regionEnd = std::min(scanEnd, region.End());
        if (regionEnd <= regionBegin ||
            regionEnd - regionBegin < m_bytes.size()) {
            continue;
        }

        Address cursor = regionBegin;
        while (cursor < regionEnd) {
            const Address remainingAddress = regionEnd - cursor;
            const std::size_t remaining =
                remainingAddress >
                        static_cast<Address>(
                            std::numeric_limits<std::size_t>::max())
                    ? std::numeric_limits<std::size_t>::max()
                    : static_cast<std::size_t>(remainingAddress);
            const std::size_t primary = std::min(chunkSize, remaining);

            std::size_t request = primary;
            if (request <= std::numeric_limits<std::size_t>::max() - overlap)
                request = std::min(remaining, request + overlap);

            std::vector<u8> buffer(request);
            const std::size_t received =
                process.ReadPartial(cursor, buffer);
            buffer.resize(received);

            if (buffer.size() >= m_bytes.size()) {
                const std::size_t possible =
                    buffer.size() - m_bytes.size() + 1;
                const std::size_t starts = std::min(primary, possible);

                for (std::size_t offset = 0; offset < starts; ++offset) {
                    if (!Matches(buffer, offset))
                        continue;

                    results.push_back(cursor + offset);
                    if (options.maxResults != 0 &&
                        results.size() >= options.maxResults) {
                        return results;
                    }
                }
            }

            if (primary == 0 ||
                cursor > kMaxAddress - static_cast<Address>(primary)) {
                break;
            }
            cursor += static_cast<Address>(primary);
        }
    }

    return results;
}

} // namespace EKore

