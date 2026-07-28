# Scanning and pointer chains

## Pointer-chain semantics

For:

```cpp
process.ResolvePointer(base, {a, b, c});
```

EKore calculates:

```text
cursor = read_target_pointer(base) + a
cursor = read_target_pointer(cursor) + b
cursor = read_target_pointer(cursor) + c
result = cursor
```

`base` is the address where the first pointer is stored. With no offsets,
`ResolvePointer` returns `base`.

Each read can fail independently. A level transition or object destruction can
invalidate a previously resolved address, so resolve dynamic chains again when
you use them.

## x86 and x64 targets

`ReadPointer` queries target architecture and reads `u32` for x86 or `u64` for
x64/ARM64. This avoids the common bug where an x64 controller reads eight bytes
from a 32-bit target.

Use an x64 controller for cross-architecture tooling. A 32-bit controller
cannot represent every x64 address and Windows can reject its module snapshot.

## Signature syntax

```cpp
EKore::Pattern pattern(
    "48 8B 05 ? ? ? ? 48 85 C0");
```

- Two hexadecimal digits represent an exact byte.
- `?` and `??` each represent one wildcard byte.
- Whitespace is ignored.
- Empty, malformed, and all-wildcard patterns are invalid.

## Scan ranges

```cpp
EKore::ScanOptions options;
options.maxResults = 32;
options.executableOnly = true;

const auto hits = pattern.ScanModuleAll(
    process, module, options);
```

The scanner:

1. Enumerates virtual-memory regions intersecting the range.
2. Skips uncommitted, guarded, and unreadable regions.
3. Applies executable/writable filters.
4. Reads in bounded chunks with overlap.
5. Preserves remote addresses for matches spanning chunk boundaries.

It does not match across separate virtual-memory regions.

## Value scans

```cpp
const float wanted = 100.0f;
const EKore::Pattern value = EKore::ValuePattern(wanted);

EKore::ScanOptions options;
options.maxResults = 0; // unlimited
options.writableOnly = true;

const auto matches = value.ScanAll(
    process, rangeBase, rangeSize, options);
```

This is an exact byte comparison. Floating-point rounding and values changing
during the scan can prevent a match. Unknown-initial-value and
changed/unchanged scan sessions are intentionally outside the core library;
build them by retaining and comparing region snapshots.

## Signature reliability

Require a unique result before using a signature as a patch or pointer base.
Wildcard relocations, addresses, displacements, and operands that change
between builds, while retaining enough stable instructions for uniqueness.

Scanning is not atomic. The target can change bytes or memory mappings during a
scan; handle zero/multiple results without writing.

