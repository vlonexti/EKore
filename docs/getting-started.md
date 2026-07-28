# Getting started

## Requirements

- Windows
- CMake 3.21 or newer
- A C++20 compiler
- A 64-bit controller when inspecting arbitrary 32-bit and 64-bit targets

EKore has no third-party dependencies.

## Build and test

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Link

```cmake
add_subdirectory(external/EKore)

add_executable(MyTool src/Main.cpp)
target_compile_features(MyTool PRIVATE cxx_std_20)
target_link_libraries(MyTool PRIVATE EKore::Library)
```

## Open a target

```cpp
#include <EKore/EKore.hpp>

EKore::Process process =
    EKore::Process::OpenByName(L"game.exe");

if (!process) {
    std::cerr << "Open failed: "
              << process.LastErrorMessage() << '\n';
    return 1;
}
```

Alternatives:

```cpp
auto byPid = EKore::Process::Open(processId);
auto readOnly = EKore::Process::Open(
    processId, EKore::Process::ReadAccess);
auto byWindow = EKore::Process::OpenByWindow(hwnd);
auto visible = EKore::Process::List();
```

`Process` is move-only and closes its Windows handle automatically.

## Find a module and read

```cpp
const auto module = process.FindModule(L"client.dll");
if (!module)
    return 1;

const EKore::Address address = module->base + 0x123456;
const auto health = process.Read<int>(address);
if (!health)
    return 1;
```

Never treat a failed optional as a real zero. Use `ReadOr` only when the
fallback is genuinely acceptable.

## Required access

`ReadAccess` requests query, read, and synchronize rights.
`ReadWriteAccess` additionally requests write and virtual-memory-operation
rights and is the default.

Opening a higher-integrity target from a lower-integrity controller can fail
with access denied. Run with only the privileges and access rights needed.

## Architecture

```cpp
const EKore::Architecture architecture =
    process.TargetArchitecture();
const std::size_t pointerBytes = process.PointerSize();
```

`ReadPointer` and `ResolvePointer` use the target width automatically. Compile
the controller as x64 when it must inspect x64 targets.

