#include <EKore/Patch.hpp>

#include <algorithm>

namespace EKore {

Patch::Patch(const Process& process, Address address,
             std::span<const u8> replacement)
    : m_address(address),
      m_replacement(replacement.begin(), replacement.end()) {
    if (!process || address == 0 || replacement.empty()) {
        m_lastError = ERROR_INVALID_PARAMETER;
        return;
    }

    if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            process.Handle(),
            ::GetCurrentProcess(),
            &m_process,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS)) {
        m_lastError = ::GetLastError();
        return;
    }

    m_original.resize(replacement.size());
    SIZE_T transferred = 0;
    if (!::ReadProcessMemory(
            m_process,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(address)),
            m_original.data(),
            m_original.size(),
            &transferred) ||
        transferred != m_original.size()) {
        m_lastError =
            transferred == m_original.size() ? ::GetLastError()
                                             : ERROR_PARTIAL_COPY;
        m_original.clear();
        Close();
        return;
    }
}

Patch::~Patch() {
    if (m_autoRevert && m_applied)
        Revert();
    Close();
}

Patch::Patch(Patch&& other) noexcept
    : m_process(other.m_process),
      m_address(other.m_address),
      m_original(std::move(other.m_original)),
      m_replacement(std::move(other.m_replacement)),
      m_applied(other.m_applied),
      m_autoRevert(other.m_autoRevert),
      m_lastError(other.m_lastError) {
    other.m_process = nullptr;
    other.m_address = 0;
    other.m_applied = false;
}

Patch& Patch::operator=(Patch&& other) noexcept {
    if (this == &other)
        return *this;

    if (m_autoRevert && m_applied)
        Revert();
    Close();

    m_process = other.m_process;
    m_address = other.m_address;
    m_original = std::move(other.m_original);
    m_replacement = std::move(other.m_replacement);
    m_applied = other.m_applied;
    m_autoRevert = other.m_autoRevert;
    m_lastError = other.m_lastError;

    other.m_process = nullptr;
    other.m_address = 0;
    other.m_applied = false;
    return *this;
}

void Patch::Close() {
    if (m_process)
        ::CloseHandle(m_process);
    m_process = nullptr;
}

bool Patch::Write(std::span<const u8> bytes) {
    if (!m_process || m_address == 0 || bytes.empty()) {
        m_lastError = ERROR_INVALID_HANDLE;
        return false;
    }

    void* target = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(m_address));
    SIZE_T transferred = 0;
    if (::WriteProcessMemory(
            m_process, target, bytes.data(), bytes.size(), &transferred) &&
        transferred == bytes.size()) {
        ::FlushInstructionCache(m_process, target, bytes.size());
        m_lastError = ERROR_SUCCESS;
        return true;
    }

    DWORD oldProtection = 0;
    if (!::VirtualProtectEx(
            m_process,
            target,
            bytes.size(),
            PAGE_EXECUTE_READWRITE,
            &oldProtection)) {
        m_lastError = ::GetLastError();
        return false;
    }

    transferred = 0;
    const BOOL wrote = ::WriteProcessMemory(
        m_process, target, bytes.data(), bytes.size(), &transferred);
    const DWORD writeError = wrote ? ERROR_SUCCESS : ::GetLastError();

    DWORD ignored = 0;
    const BOOL restored = ::VirtualProtectEx(
        m_process,
        target,
        bytes.size(),
        oldProtection,
        &ignored);
    const DWORD restoreError = restored ? ERROR_SUCCESS : ::GetLastError();
    ::FlushInstructionCache(m_process, target, bytes.size());

    if (!wrote || transferred != bytes.size()) {
        m_lastError = wrote ? ERROR_PARTIAL_COPY : writeError;
        return false;
    }
    if (!restored) {
        m_lastError = restoreError;
        return false;
    }

    m_lastError = ERROR_SUCCESS;
    return true;
}

bool Patch::Apply() {
    if (!Valid() || m_applied) {
        m_lastError = m_applied ? ERROR_ALREADY_EXISTS
                                : ERROR_INVALID_DATA;
        return false;
    }

    m_applied = Write(m_replacement);
    return m_applied;
}

bool Patch::Revert() {
    if (!Valid() || !m_applied) {
        m_lastError = !m_applied ? ERROR_INVALID_STATE
                                 : ERROR_INVALID_DATA;
        return false;
    }

    if (!Write(m_original))
        return false;
    m_applied = false;
    return true;
}

void Freezer::HoldBytes(std::string tag, Address address,
                        std::vector<u8> value) {
    if (tag.empty() || address == 0 || value.empty())
        return;

    const auto entry = std::find_if(
        m_entries.begin(),
        m_entries.end(),
        [&](const Entry& current) { return current.tag == tag; });

    if (entry != m_entries.end()) {
        entry->address = address;
        entry->bytes = std::move(value);
        return;
    }

    m_entries.push_back(
        Entry{std::move(tag), address, std::move(value)});
}

void Freezer::Release(std::string_view tag) {
    std::erase_if(
        m_entries,
        [&](const Entry& entry) { return entry.tag == tag; });
}

void Freezer::ReleaseAll() {
    m_entries.clear();
}

bool Freezer::Held(std::string_view tag) const {
    return std::any_of(
        m_entries.begin(),
        m_entries.end(),
        [&](const Entry& entry) { return entry.tag == tag; });
}

std::size_t Freezer::Tick() const {
    if (!m_process)
        return 0;

    std::size_t written = 0;
    for (const Entry& entry : m_entries) {
        if (m_process->WriteRaw(entry.address, entry.bytes))
            ++written;
    }
    return written;
}

} // namespace EKore

