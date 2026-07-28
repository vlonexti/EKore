# EKore

EKore (ExternalKore) is a dependency-free Windows C++20 library for inspecting
and modifying another process. It covers the typed memory operations provided
by Swed32/Swed64 and adds automatic target-architecture handling, safer return
values, modules and memory regions, pointer chains, signature/value scanning,
remote allocation/protection, reversible patches, and value freezing.

The library supports both 32-bit and 64-bit targets. Build your controller as
64-bit when you need to inspect both; Windows does not allow a 32-bit
controller to enumerate or address every part of a 64-bit target.

## Why EKore instead of a direct Swed port?

Swed exposes many type-specific C# methods. C++ templates let EKore provide one
checked operation for every trivially copyable type:

| Swed-style operation | EKore |
|---|---|
| Get process | `Process::OpenByName(L"game.exe")` |
| Get module base | `process.FindModule(L"client.dll")` |
| Read int/float/vector/matrix | `process.Read<T>(address)` |
| Write int/float/vector/matrix | `process.Write<T>(address, value)` |
| Read/write byte arrays | `ReadBytes`, `ReadArray`, `WriteArray` |
| Read/write strings | `ReadString`, `ReadWideString`, `WriteString`, `WriteWideString` |
| Read pointer | `ReadPointer` |
| Follow pointer chain | `ResolvePointer` or `RemoteValue<T>` |

EKore reports failure with invalid objects, `std::optional`, or `bool` instead
of silently returning zero-filled buffers. The originating Win32 error remains
available through `Process::LastError()` and `LastErrorMessage()`.

## Capabilities

- Open by PID, executable name, or window
- Enumerate processes, modules, and virtual-memory regions
- Detect x86, x64, and ARM64 target architecture
- Read target-width pointers from one x64 build
- Typed reads/writes for primitives and custom trivially copyable structures
- Arrays, byte buffers, narrow strings, and UTF-16 strings
- IDA-style signature scanning with wildcards and region filters
- Exact value scanning
- Remote `VirtualAllocEx`, `VirtualFreeEx`, and `VirtualProtectEx`
- Automatic temporary page-protection changes for writes
- Reversible patches with optional destructor auto-revert
- Caller-scheduled value freezing
- Move-only RAII process handles

EKore deliberately does not include an overlay, DLL injector, anti-cheat bypass,
driver, or evasion system. Use [IKore](https://github.com/vlonexti/IKore) when
code needs to run inside the target.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The output is `build/Release/EKore.lib`.

To install a CMake package:

```powershell
cmake --install build --config Release --prefix C:/Libraries/EKore
```

An overlay port is also available under
[`packaging/vcpkg`](packaging/vcpkg/README.md).

## Add it to a project

From a source checkout:

```cmake
add_subdirectory(external/EKore)
target_link_libraries(MyTool PRIVATE EKore::Library)
```

From an installed package:

```cmake
find_package(EKore CONFIG REQUIRED)
target_link_libraries(MyTool PRIVATE EKore::Library)
```

Include the umbrella header:

```cpp
#include <EKore/EKore.hpp>
```

## Basic use

```cpp
#include <EKore/EKore.hpp>

#include <iostream>

int main() {
    EKore::Process process =
        EKore::Process::OpenByName(L"game.exe");

    if (!process) {
        std::cerr << process.LastErrorMessage() << '\n';
        return 1;
    }

    const auto module = process.FindModule(L"client.dll");
    if (!module)
        return 1;

    const EKore::Address healthAddress =
        module->base + 0x123456;

    if (const auto health = process.Read<int>(healthAddress))
        std::cout << "Health: " << *health << '\n';

    process.Write<int>(healthAddress, 100);
}
```

## Pointer chains

EKore automatically reads 4-byte pointers from x86 targets and 8-byte pointers
from x64/ARM64 targets:

```cpp
const std::array<EKore::Offset, 3> offsets{
    0x20, 0x18, 0x4
};

const auto healthAddress = process.ResolvePointer(
    module->base + 0x123456,
    offsets);

if (healthAddress)
    process.Write<int>(*healthAddress, 100);
```

For a reusable typed chain:

```cpp
EKore::RemoteValue<int> health(
    process,
    module->base + 0x123456,
    {0x20, 0x18, 0x4});

std::cout << health.GetOr(0) << '\n';
health.Set(100);
```

## Signature scanning

```cpp
EKore::Pattern pattern(
    "48 8B 05 ? ? ? ? 48 85 C0 74 ?");

if (const auto hit = pattern.ScanModule(process, *module))
    std::cout << std::hex << *hit << '\n';
```

Use `ScanModuleAll` while developing and require one result before trusting a
signature. `ScanOptions` can restrict scanning to executable or writable
regions and cap the result count.

## Reversible patches

```cpp
constexpr EKore::u8 replacement[]{0x90, 0x90, 0x90};
EKore::Patch patch(process, address, replacement);

if (patch.Apply()) {
    // ...
    patch.Revert();
}
```

An applied patch reverts in its destructor by default. Call
`SetAutoRevert(false)` only when leaving the replacement behind is intentional.

## Documentation

- [Documentation overview](docs/README.md)
- [Getting started](docs/getting-started.md)
- [API reference](docs/api-reference.md)
- [Scanning and pointer chains](docs/scanning.md)
- [Safety and troubleshooting](docs/troubleshooting.md)

## Scope

EKore is intended for authorized debugging, inspection, accessibility tools,
modding, trainers, and automation. Opening protected or elevated processes may
require matching privileges. Confirm that your use is allowed by the target
software and environment.

## License

MIT
