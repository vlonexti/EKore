#include <EKore/Process.hpp>

#include <TlHelp32.h>

#include <algorithm>
#include <cwchar>
#include <limits>
#include <thread>

namespace EKore {
namespace {

DWORD BaseProtection(DWORD protection) {
    return protection & 0xFFu;
}

std::wstring NormalizeExecutableName(std::wstring_view name) {
    std::wstring normalized(name);
    if (normalized.size() < 4 ||
        _wcsicmp(normalized.c_str() + normalized.size() - 4, L".exe") != 0) {
        normalized += L".exe";
    }
    return normalized;
}

bool EqualInsensitive(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size())
        return false;
    return _wcsnicmp(left.data(), right.data(), left.size()) == 0;
}

bool FitsLocalPointer(Address address) {
    return address <=
           static_cast<Address>(std::numeric_limits<std::uintptr_t>::max());
}

std::optional<Address> AddOffset(Address address, Offset offset) {
    if (offset >= 0) {
        const Address amount = static_cast<Address>(offset);
        if (address > kMaxAddress - amount)
            return std::nullopt;
        return address + amount;
    }

    // Avoid negating INT64_MIN.
    const Address amount =
        static_cast<Address>(-(offset + 1)) + static_cast<Address>(1);
    if (address < amount)
        return std::nullopt;
    return address - amount;
}

Architecture FromMachine(USHORT machine) {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:
            return Architecture::X86;
        case IMAGE_FILE_MACHINE_AMD64:
            return Architecture::X64;
#ifdef IMAGE_FILE_MACHINE_ARM64
        case IMAGE_FILE_MACHINE_ARM64:
            return Architecture::Arm64;
#endif
        default:
            return Architecture::Unknown;
    }
}

} // namespace

bool MemoryRegion::Readable() const {
    if (!Committed() || Guarded() || BaseProtection(protection) == PAGE_NOACCESS)
        return false;

    switch (BaseProtection(protection)) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool MemoryRegion::Writable() const {
    if (!Committed() || Guarded())
        return false;

    switch (BaseProtection(protection)) {
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool MemoryRegion::Executable() const {
    if (!Committed() || Guarded())
        return false;

    switch (BaseProtection(protection)) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

Process::~Process() {
    Close();
}

Process::Process(Process&& other) noexcept
    : m_handle(other.m_handle),
      m_processId(other.m_processId),
      m_access(other.m_access),
      m_lastError(other.m_lastError) {
    other.m_handle = nullptr;
    other.m_processId = 0;
    other.m_access = 0;
    other.m_lastError = ERROR_SUCCESS;
}

Process& Process::operator=(Process&& other) noexcept {
    if (this == &other)
        return *this;

    Close();
    m_handle = other.m_handle;
    m_processId = other.m_processId;
    m_access = other.m_access;
    m_lastError = other.m_lastError;
    other.m_handle = nullptr;
    other.m_processId = 0;
    other.m_access = 0;
    other.m_lastError = ERROR_SUCCESS;
    return *this;
}

Process Process::Open(DWORD processId, DWORD access) {
    if (processId == 0) {
        Process invalid;
        invalid.SetError(ERROR_INVALID_PARAMETER);
        return invalid;
    }

    HANDLE handle = ::OpenProcess(access, FALSE, processId);
    if (!handle) {
        Process invalid;
        invalid.m_processId = processId;
        invalid.m_access = access;
        invalid.SetError(::GetLastError());
        return invalid;
    }

    Process process(handle, processId, access);
    process.SetError(ERROR_SUCCESS);
    return process;
}

Process Process::OpenByName(std::wstring_view executable, DWORD access) {
    if (executable.empty()) {
        Process invalid;
        invalid.SetError(ERROR_INVALID_PARAMETER);
        return invalid;
    }

    const std::wstring wanted = NormalizeExecutableName(executable);
    for (const ProcessInfo& candidate : List()) {
        if (EqualInsensitive(candidate.name, wanted))
            return Open(candidate.id, access);
    }

    Process invalid;
    invalid.SetError(ERROR_NOT_FOUND);
    return invalid;
}

Process Process::OpenByWindow(HWND window, DWORD access) {
    if (!::IsWindow(window)) {
        Process invalid;
        invalid.SetError(ERROR_INVALID_WINDOW_HANDLE);
        return invalid;
    }

    DWORD processId = 0;
    ::GetWindowThreadProcessId(window, &processId);
    return Open(processId, access);
}

std::vector<ProcessInfo> Process::List() {
    std::vector<ProcessInfo> processes;
    const HANDLE snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return processes;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Process32FirstW(snapshot, &entry)) {
        do {
            processes.push_back(ProcessInfo{
                entry.th32ProcessID,
                entry.szExeFile,
            });
        } while (::Process32NextW(snapshot, &entry));
    }

    ::CloseHandle(snapshot);
    return processes;
}

void Process::Close() {
    if (m_handle)
        ::CloseHandle(m_handle);
    m_handle = nullptr;
    m_processId = 0;
    m_access = 0;
}

bool Process::Alive() const {
    if (!m_handle)
        return false;
    const DWORD wait = ::WaitForSingleObject(m_handle, 0);
    if (wait == WAIT_TIMEOUT)
        return true;
    if (wait == WAIT_OBJECT_0)
        return false;
    SetError(::GetLastError());
    return false;
}

std::string Process::LastErrorMessage() const {
    if (m_lastError == ERROR_SUCCESS)
        return {};

    char* raw = nullptr;
    const DWORD length = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        m_lastError,
        0,
        reinterpret_cast<char*>(&raw),
        0,
        nullptr);

    if (length == 0 || !raw)
        return "Win32 error " + std::to_string(m_lastError);

    std::string message(raw, length);
    ::LocalFree(raw);
    while (!message.empty() &&
           (message.back() == '\r' || message.back() == '\n' ||
            message.back() == ' ')) {
        message.pop_back();
    }
    return message;
}

Architecture Process::TargetArchitecture() const {
    if (!m_handle) {
        SetError(ERROR_INVALID_HANDLE);
        return Architecture::Unknown;
    }

    using IsWow64Process2Fn =
        BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    const HMODULE kernel = ::GetModuleHandleW(L"kernel32.dll");
    const auto isWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
        ::GetProcAddress(kernel, "IsWow64Process2"));

    if (isWow64Process2) {
        USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        if (!isWow64Process2(m_handle, &processMachine, &nativeMachine)) {
            SetError(::GetLastError());
            return Architecture::Unknown;
        }

        SetError(ERROR_SUCCESS);
        return FromMachine(processMachine == IMAGE_FILE_MACHINE_UNKNOWN
                               ? nativeMachine
                               : processMachine);
    }

    BOOL wow64 = FALSE;
    if (!::IsWow64Process(m_handle, &wow64)) {
        SetError(::GetLastError());
        return Architecture::Unknown;
    }

    SetError(ERROR_SUCCESS);
    if (wow64)
        return Architecture::X86;
    return sizeof(void*) == 8 ? Architecture::X64 : Architecture::X86;
}

std::size_t Process::PointerSize() const {
    switch (TargetArchitecture()) {
        case Architecture::X86:
            return 4;
        case Architecture::X64:
        case Architecture::Arm64:
            return 8;
        default:
            return 0;
    }
}

std::vector<Module> Process::Modules() const {
    std::vector<Module> modules;
    if (m_processId == 0) {
        SetError(ERROR_INVALID_HANDLE);
        return modules;
    }

    const HANDLE snapshot = ::CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        SetError(::GetLastError());
        return modules;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Module32FirstW(snapshot, &entry)) {
        do {
            modules.push_back(Module{
                entry.szModule,
                entry.szExePath,
                static_cast<Address>(
                    reinterpret_cast<std::uintptr_t>(entry.modBaseAddr)),
                static_cast<std::size_t>(entry.modBaseSize),
            });
        } while (::Module32NextW(snapshot, &entry));
        SetError(ERROR_SUCCESS);
    } else {
        SetError(::GetLastError());
    }

    ::CloseHandle(snapshot);
    return modules;
}

std::optional<Module> Process::MainModule() const {
    auto modules = Modules();
    if (modules.empty())
        return std::nullopt;
    return modules.front();
}

std::optional<Module> Process::FindModule(std::wstring_view name) const {
    for (Module& module : Modules()) {
        if (EqualInsensitive(module.name, name))
            return module;
    }
    SetError(ERROR_NOT_FOUND);
    return std::nullopt;
}

std::optional<Module> Process::WaitForModule(
    std::wstring_view name,
    std::chrono::milliseconds timeout,
    std::chrono::milliseconds pollInterval) const {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (auto module = FindModule(name))
            return module;
        if (timeout.count() == 0)
            break;
        std::this_thread::sleep_for(
            std::max(pollInterval, std::chrono::milliseconds(1)));
    } while (std::chrono::steady_clock::now() < deadline);

    SetError(WAIT_TIMEOUT);
    return std::nullopt;
}

std::optional<MemoryRegion> Process::Query(Address address) const {
    if (!m_handle || !FitsLocalPointer(address)) {
        SetError(m_handle ? ERROR_INVALID_ADDRESS : ERROR_INVALID_HANDLE);
        return std::nullopt;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (::VirtualQueryEx(
            m_handle,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(address)),
            &info,
            sizeof(info)) == 0) {
        SetError(::GetLastError());
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return MemoryRegion{
        static_cast<Address>(
            reinterpret_cast<std::uintptr_t>(info.BaseAddress)),
        info.RegionSize,
        info.State,
        info.Protect,
        info.Type,
    };
}

std::vector<MemoryRegion> Process::Regions(Address begin,
                                           Address end) const {
    std::vector<MemoryRegion> regions;
    if (!m_handle || begin >= end) {
        SetError(!m_handle ? ERROR_INVALID_HANDLE : ERROR_INVALID_PARAMETER);
        return regions;
    }

    SYSTEM_INFO system{};
    ::GetNativeSystemInfo(&system);
    const Address systemEnd = static_cast<Address>(
        reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress));
    end = std::min(end, systemEnd);

    Address cursor = begin;
    while (cursor < end) {
        const auto region = Query(cursor);
        if (!region)
            break;

        regions.push_back(*region);
        const Address next = region->End();
        if (next <= cursor)
            break;
        cursor = next;
    }
    return regions;
}

bool Process::ReadRaw(Address address, std::span<u8> output) const {
    if (output.empty()) {
        SetError(ERROR_SUCCESS);
        return true;
    }
    if (!m_handle || !FitsLocalPointer(address)) {
        SetError(m_handle ? ERROR_INVALID_ADDRESS : ERROR_INVALID_HANDLE);
        return false;
    }

    SIZE_T transferred = 0;
    const BOOL ok = ::ReadProcessMemory(
        m_handle,
        reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address)),
        output.data(),
        output.size(),
        &transferred);
    if (!ok || transferred != output.size()) {
        SetError(ok ? ERROR_PARTIAL_COPY : ::GetLastError());
        return false;
    }

    SetError(ERROR_SUCCESS);
    return true;
}

std::size_t Process::ReadPartial(Address address,
                                 std::span<u8> output) const {
    if (output.empty()) {
        SetError(ERROR_SUCCESS);
        return 0;
    }
    if (!m_handle || !FitsLocalPointer(address)) {
        SetError(m_handle ? ERROR_INVALID_ADDRESS : ERROR_INVALID_HANDLE);
        return 0;
    }

    SIZE_T transferred = 0;
    const BOOL ok = ::ReadProcessMemory(
        m_handle,
        reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address)),
        output.data(),
        output.size(),
        &transferred);
    if (!ok && transferred == 0)
        SetError(::GetLastError());
    else
        SetError(ERROR_SUCCESS);
    return static_cast<std::size_t>(transferred);
}

bool Process::WriteRaw(Address address, std::span<const u8> input,
                       bool changeProtection) const {
    if (input.empty()) {
        SetError(ERROR_SUCCESS);
        return true;
    }
    if (!m_handle || !FitsLocalPointer(address)) {
        SetError(m_handle ? ERROR_INVALID_ADDRESS : ERROR_INVALID_HANDLE);
        return false;
    }

    void* target = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(address));
    SIZE_T transferred = 0;
    if (::WriteProcessMemory(m_handle, target, input.data(), input.size(),
                             &transferred) &&
        transferred == input.size()) {
        ::FlushInstructionCache(m_handle, target, input.size());
        SetError(ERROR_SUCCESS);
        return true;
    }

    const DWORD firstError = ::GetLastError();
    if (!changeProtection) {
        SetError(firstError);
        return false;
    }

    DWORD oldProtection = 0;
    if (!::VirtualProtectEx(m_handle, target, input.size(),
                            PAGE_EXECUTE_READWRITE, &oldProtection)) {
        SetError(::GetLastError());
        return false;
    }

    transferred = 0;
    const BOOL wrote = ::WriteProcessMemory(
        m_handle, target, input.data(), input.size(), &transferred);
    const DWORD writeError = wrote ? ERROR_SUCCESS : ::GetLastError();

    DWORD ignored = 0;
    const BOOL restored = ::VirtualProtectEx(
        m_handle, target, input.size(), oldProtection, &ignored);
    const DWORD restoreError = restored ? ERROR_SUCCESS : ::GetLastError();
    ::FlushInstructionCache(m_handle, target, input.size());

    if (!wrote || transferred != input.size()) {
        SetError(wrote ? ERROR_PARTIAL_COPY : writeError);
        return false;
    }
    if (!restored) {
        SetError(restoreError);
        return false;
    }

    SetError(ERROR_SUCCESS);
    return true;
}

std::optional<std::vector<u8>> Process::ReadBytes(
    Address address, std::size_t count) const {
    std::vector<u8> bytes(count);
    if (!ReadRaw(address, bytes))
        return std::nullopt;
    return bytes;
}

std::optional<std::string> Process::ReadString(
    Address address, std::size_t maxLength) const {
    if (maxLength == 0)
        return std::string{};

    std::vector<u8> bytes(maxLength);
    const std::size_t read = ReadPartial(address, bytes);
    if (read == 0)
        return std::nullopt;

    const auto end = std::find(bytes.begin(), bytes.begin() + read, u8{0});
    return std::string(bytes.begin(), end);
}

std::optional<std::wstring> Process::ReadWideString(
    Address address, std::size_t maxCharacters) const {
    if (maxCharacters == 0)
        return std::wstring{};
    if (maxCharacters >
        std::numeric_limits<std::size_t>::max() / sizeof(wchar_t)) {
        SetError(ERROR_ARITHMETIC_OVERFLOW);
        return std::nullopt;
    }

    std::vector<wchar_t> characters(maxCharacters);
    auto bytes = std::span<u8>(
        reinterpret_cast<u8*>(characters.data()),
        characters.size() * sizeof(wchar_t));
    const std::size_t readBytes = ReadPartial(address, bytes);
    const std::size_t readCharacters = readBytes / sizeof(wchar_t);
    if (readCharacters == 0)
        return std::nullopt;

    const auto end = std::find(
        characters.begin(),
        characters.begin() + readCharacters,
        L'\0');
    return std::wstring(characters.begin(), end);
}

bool Process::WriteString(Address address, std::string_view value,
                          bool includeTerminator) const {
    std::vector<u8> bytes(value.begin(), value.end());
    if (includeTerminator)
        bytes.push_back(0);
    return WriteRaw(address, bytes);
}

bool Process::WriteWideString(Address address, std::wstring_view value,
                              bool includeTerminator) const {
    std::vector<wchar_t> characters(value.begin(), value.end());
    if (includeTerminator)
        characters.push_back(L'\0');
    const auto* data = reinterpret_cast<const u8*>(characters.data());
    return WriteRaw(
        address,
        std::span<const u8>(data, characters.size() * sizeof(wchar_t)));
}

std::optional<Address> Process::ReadPointer(Address address) const {
    switch (PointerSize()) {
        case 4: {
            const auto value = Read<u32>(address);
            return value ? std::optional<Address>(*value) : std::nullopt;
        }
        case 8:
            return Read<u64>(address);
        default:
            SetError(ERROR_NOT_SUPPORTED);
            return std::nullopt;
    }
}

std::optional<Address> Process::ResolvePointer(
    Address base, std::span<const Offset> offsets) const {
    if (offsets.empty())
        return base;

    Address cursor = base;
    for (Offset offset : offsets) {
        const auto pointer = ReadPointer(cursor);
        if (!pointer || *pointer == 0)
            return std::nullopt;
        const auto next = AddOffset(*pointer, offset);
        if (!next) {
            SetError(ERROR_ARITHMETIC_OVERFLOW);
            return std::nullopt;
        }
        cursor = *next;
    }
    return cursor;
}

std::optional<DWORD> Process::Protect(
    Address address, std::size_t size, DWORD protection) const {
    if (!m_handle || !FitsLocalPointer(address) || size == 0) {
        SetError(!m_handle ? ERROR_INVALID_HANDLE : ERROR_INVALID_PARAMETER);
        return std::nullopt;
    }

    DWORD oldProtection = 0;
    if (!::VirtualProtectEx(
            m_handle,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            size,
            protection,
            &oldProtection)) {
        SetError(::GetLastError());
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return oldProtection;
}

std::optional<Address> Process::Allocate(
    std::size_t size, DWORD protection, DWORD allocationType) const {
    if (!m_handle || size == 0) {
        SetError(!m_handle ? ERROR_INVALID_HANDLE : ERROR_INVALID_PARAMETER);
        return std::nullopt;
    }

    void* allocation = ::VirtualAllocEx(
        m_handle, nullptr, size, allocationType, protection);
    if (!allocation) {
        SetError(::GetLastError());
        return std::nullopt;
    }

    SetError(ERROR_SUCCESS);
    return static_cast<Address>(
        reinterpret_cast<std::uintptr_t>(allocation));
}

bool Process::Free(Address address, std::size_t size,
                   DWORD freeType) const {
    if (!m_handle || !FitsLocalPointer(address) || address == 0) {
        SetError(!m_handle ? ERROR_INVALID_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }

    if (!::VirtualFreeEx(
            m_handle,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            size,
            freeType)) {
        SetError(::GetLastError());
        return false;
    }

    SetError(ERROR_SUCCESS);
    return true;
}

} // namespace EKore

