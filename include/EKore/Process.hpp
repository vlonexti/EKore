#pragma once

#include <EKore/Types.hpp>

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace EKore {

class Process {
public:
    static constexpr DWORD ReadAccess =
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE;
    static constexpr DWORD ReadWriteAccess =
        ReadAccess | PROCESS_VM_WRITE | PROCESS_VM_OPERATION;

    Process() = default;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    /// Open a process by PID. An invalid Process carries the Win32 error.
    [[nodiscard]] static Process Open(DWORD processId,
                                      DWORD access = ReadWriteAccess);

    /// Open the first process whose executable name matches case-insensitively.
    /// The ".exe" suffix is optional.
    [[nodiscard]] static Process OpenByName(std::wstring_view executable,
                                            DWORD access = ReadWriteAccess);

    /// Resolve a window's owning PID and open it.
    [[nodiscard]] static Process OpenByWindow(HWND window,
                                              DWORD access = ReadWriteAccess);

    /// Snapshot all visible processes.
    [[nodiscard]] static std::vector<ProcessInfo> List();

    void Close();

    [[nodiscard]] bool Valid() const { return m_handle != nullptr; }
    [[nodiscard]] explicit operator bool() const { return Valid(); }
    [[nodiscard]] bool Alive() const;

    [[nodiscard]] HANDLE Handle() const { return m_handle; }
    [[nodiscard]] DWORD Id() const { return m_processId; }
    [[nodiscard]] DWORD Access() const { return m_access; }
    [[nodiscard]] DWORD LastError() const { return m_lastError; }
    [[nodiscard]] std::string LastErrorMessage() const;

    [[nodiscard]] Architecture TargetArchitecture() const;
    [[nodiscard]] std::size_t PointerSize() const;

    [[nodiscard]] std::vector<Module> Modules() const;
    [[nodiscard]] std::optional<Module> MainModule() const;
    [[nodiscard]] std::optional<Module> FindModule(std::wstring_view name) const;
    [[nodiscard]] std::optional<Module> WaitForModule(
        std::wstring_view name,
        std::chrono::milliseconds timeout = std::chrono::seconds(30),
        std::chrono::milliseconds pollInterval = std::chrono::milliseconds(50)) const;

    [[nodiscard]] std::optional<MemoryRegion> Query(Address address) const;
    [[nodiscard]] std::vector<MemoryRegion> Regions(
        Address begin = 0,
        Address end = kMaxAddress) const;

    /// Exact I/O: returns false unless the full span was transferred.
    bool ReadRaw(Address address, std::span<u8> output) const;
    bool WriteRaw(Address address, std::span<const u8> input,
                  bool changeProtection = true) const;

    /// Best-effort read used by scanners and strings. Returns transferred bytes.
    [[nodiscard]] std::size_t ReadPartial(Address address,
                                          std::span<u8> output) const;

    template <typename T>
    [[nodiscard]] std::optional<T> Read(Address address) const {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Process::Read<T> requires a trivially copyable T");
        T value{};
        auto bytes = std::span<u8>(reinterpret_cast<u8*>(&value), sizeof(T));
        if (!ReadRaw(address, bytes))
            return std::nullopt;
        return value;
    }

    template <typename T>
    [[nodiscard]] T ReadOr(Address address, T fallback = {}) const {
        return Read<T>(address).value_or(fallback);
    }

    template <typename T>
    bool Write(Address address, const T& value,
               bool changeProtection = true) const {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Process::Write<T> requires a trivially copyable T");
        const auto* data = reinterpret_cast<const u8*>(&value);
        return WriteRaw(address, std::span<const u8>(data, sizeof(T)),
                        changeProtection);
    }

    template <typename T>
    [[nodiscard]] std::optional<std::vector<T>> ReadArray(
        Address address, std::size_t count) const {
        static_assert(std::is_trivially_copyable_v<T>);
        if (count == 0)
            return std::vector<T>{};
        if (count > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            SetError(ERROR_ARITHMETIC_OVERFLOW);
            return std::nullopt;
        }
        std::vector<T> values(count);
        auto bytes = std::span<u8>(reinterpret_cast<u8*>(values.data()),
                                   values.size() * sizeof(T));
        if (!ReadRaw(address, bytes))
            return std::nullopt;
        return values;
    }

    template <typename T>
    bool WriteArray(Address address, std::span<const T> values,
                    bool changeProtection = true) const {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* data = reinterpret_cast<const u8*>(values.data());
        return WriteRaw(address,
                        std::span<const u8>(data, values.size_bytes()),
                        changeProtection);
    }

    [[nodiscard]] std::optional<std::vector<u8>> ReadBytes(
        Address address, std::size_t count) const;
    [[nodiscard]] std::optional<std::string> ReadString(
        Address address, std::size_t maxLength = 4096) const;
    [[nodiscard]] std::optional<std::wstring> ReadWideString(
        Address address, std::size_t maxCharacters = 4096) const;

    bool WriteString(Address address, std::string_view value,
                     bool includeTerminator = true) const;
    bool WriteWideString(Address address, std::wstring_view value,
                         bool includeTerminator = true) const;

    /// Read one pointer using the target process's pointer width.
    [[nodiscard]] std::optional<Address> ReadPointer(Address address) const;

    /// For offsets {a,b,c}: *base+a -> *cursor+b -> *cursor+c.
    [[nodiscard]] std::optional<Address> ResolvePointer(
        Address base, std::span<const Offset> offsets) const;

    [[nodiscard]] std::optional<DWORD> Protect(
        Address address, std::size_t size, DWORD protection) const;
    [[nodiscard]] std::optional<Address> Allocate(
        std::size_t size,
        DWORD protection = PAGE_READWRITE,
        DWORD allocationType = MEM_COMMIT | MEM_RESERVE) const;
    bool Free(Address address, std::size_t size = 0,
              DWORD freeType = MEM_RELEASE) const;

private:
    explicit Process(HANDLE handle, DWORD processId, DWORD access)
        : m_handle(handle), m_processId(processId), m_access(access) {}

    void SetError(DWORD error) const { m_lastError = error; }

    HANDLE        m_handle = nullptr;
    DWORD         m_processId = 0;
    DWORD         m_access = 0;
    mutable DWORD m_lastError = ERROR_SUCCESS;
};

template <typename T>
class RemoteValue {
public:
    static_assert(std::is_trivially_copyable_v<T>);

    RemoteValue() = default;
    RemoteValue(const Process& process, Address base,
                std::vector<Offset> offsets = {})
        : m_process(&process), m_base(base), m_offsets(std::move(offsets)) {}

    [[nodiscard]] std::optional<Address> Resolve() const {
        if (!m_process)
            return std::nullopt;
        if (m_offsets.empty())
            return m_base;
        return m_process->ResolvePointer(m_base, m_offsets);
    }

    [[nodiscard]] std::optional<T> Get() const {
        const auto address = Resolve();
        return address ? m_process->Read<T>(*address) : std::nullopt;
    }

    [[nodiscard]] T GetOr(T fallback = {}) const {
        return Get().value_or(fallback);
    }

    bool Set(const T& value, bool changeProtection = true) const {
        const auto address = Resolve();
        return address &&
               m_process->Write<T>(*address, value, changeProtection);
    }

    [[nodiscard]] Address Base() const { return m_base; }
    void SetBase(Address base) { m_base = base; }

private:
    const Process*       m_process = nullptr;
    Address              m_base = 0;
    std::vector<Offset>  m_offsets;
};

} // namespace EKore

