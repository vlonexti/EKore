# API reference

Include everything with:

```cpp
#include <EKore/EKore.hpp>
```

## Types

`Address` is an unsigned 64-bit remote address. `Offset` is signed 64-bit.
Integer aliases `u8` through `u64` and `i8` through `i64` are provided.

`Vec2`, `Vec3`, `Vec4`, and `Matrix4x4` are contiguous float structures for
typed reads. `Architecture` is `Unknown`, `X86`, `X64`, or `Arm64`.

`ProcessInfo` contains `id` and executable `name`.

`Module` contains `name`, full `path`, `base`, and `size`. `End()` returns the
first address after the image.

`MemoryRegion` contains `base`, `size`, `state`, `protection`, and `type`, plus
`Readable`, `Writable`, `Executable`, `Committed`, `Guarded`, and `End`.

## Process

### Creation and state

| Member | Result |
|---|---|
| `Open(pid, access)` | Process handle or invalid object with an error |
| `OpenByName(name, access)` | First case-insensitive executable match |
| `OpenByWindow(hwnd, access)` | Process owning a valid window |
| `List()` | Snapshot of visible processes |
| `Close()` | Close early |
| `Valid()` / `operator bool` | Whether a handle is open |
| `Alive()` | Whether the process handle is unsignaled |
| `Handle()` / `Id()` / `Access()` | Native state |
| `LastError()` | Most recent Win32 error code |
| `LastErrorMessage()` | Formatted error text |

### Architecture and modules

| Member | Result |
|---|---|
| `TargetArchitecture()` | x86/x64/ARM64 detection |
| `PointerSize()` | 4, 8, or 0 when unknown |
| `Modules()` | Module snapshot |
| `MainModule()` | First/main module |
| `FindModule(name)` | Case-insensitive module lookup |
| `WaitForModule(name, timeout, poll)` | Poll for a late module |

### Memory maps

| Member | Result |
|---|---|
| `Query(address)` | Region containing an address |
| `Regions(begin, end)` | Regions intersecting an address interval |
| `Protect(address, size, protection)` | Previous protection |
| `Allocate(size, protection, type)` | New remote address |
| `Free(address, size, type)` | Release/decommit result |

`Free` defaults to `MEM_RELEASE`, which requires `size == 0`.

### Reads and writes

| Member | Result |
|---|---|
| `Read<T>(address)` | `optional<T>` exact read |
| `ReadOr<T>(address, fallback)` | Value or fallback |
| `Write<T>(address, value)` | Exact typed write |
| `ReadArray<T>(address, count)` | Optional vector |
| `WriteArray<T>(address, span)` | Exact array write |
| `ReadBytes(address, count)` | Optional byte vector |
| `ReadRaw(address, output)` | Exact span read |
| `ReadPartial(address, output)` | Number of bytes transferred |
| `WriteRaw(address, input, changeProtection)` | Exact span write |

Typed operations require trivially copyable types. Writes temporarily use
`PAGE_EXECUTE_READWRITE` only if the first write fails and
`changeProtection == true`; the old protection is restored afterward.

### Strings

`ReadString` returns bytes through NUL or the maximum length.
`ReadWideString` returns UTF-16 `wchar_t` data.
`WriteString` and `WriteWideString` include a terminator by default.

The destination allocation size cannot be inferred. Do not write a string
larger than its remote buffer.

### Pointers

`ReadPointer` reads four or eight bytes based on the target.
`ResolvePointer(base, offsets)` dereferences before each offset.
`RemoteValue<T>` stores a process, base, and offsets and exposes `Resolve`,
`Get`, `GetOr`, and `Set`.

The referenced `Process` must outlive a `RemoteValue`.

## Pattern

`Pattern(signature)` parses IDA-style bytes with `?`/`??` wildcards.
`Pattern::Exact(bytes)` creates an exact byte pattern.
`ValuePattern(value)` creates an exact pattern for any trivially copyable value.

| Member | Result |
|---|---|
| `Valid()` / `Size()` | Parsed-pattern state |
| `Scan(process, base, size, options)` | First result |
| `ScanAll(...)` | Multiple results |
| `ScanModule(process, module, options)` | First result in a module |
| `ScanModuleAll(...)` | Multiple module results |

`ScanOptions` controls chunk size, maximum results (`0` means unlimited), and
executable/writable filters.

## Patch

`Patch(process, address, replacement)` duplicates the process handle and
snapshots original bytes.

| Member | Result |
|---|---|
| `Apply()` | Write replacement once |
| `Revert()` | Restore original bytes |
| `Valid()` / `Applied()` | State |
| `Target()` / `LastError()` | Diagnostics |
| `SetAutoRevert(bool)` | Control destructor restoration |

Patches are move-only. Auto-revert defaults to true.

## Freezer

`Freezer(process)` stores tagged writes. `Hold`/`HoldBytes` adds or replaces,
`Release`/`ReleaseAll` removes, and `Tick` writes each entry once. `Tick`
returns the number of successful writes.

The caller owns scheduling; EKore does not create a hidden worker thread.
The referenced `Process` must outlive the `Freezer`.
