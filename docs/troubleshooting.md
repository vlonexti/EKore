# Safety and troubleshooting

## Open fails with access denied

Request only `Process::ReadAccess` when writes are unnecessary. A controller
cannot normally open a higher-integrity or protected target with broad rights.
Matching elevation may be required for authorized targets.

Some protected processes intentionally reject user-mode memory access; EKore
does not bypass those protections.

## Process name is not found

`OpenByName` is case-insensitive and accepts names with or without `.exe`, but
it returns the first matching process. Use `Process::List` and open a specific
PID when several instances exist.

## Module enumeration fails

Build the controller as x64 when inspecting an x64 target. A 32-bit
`CreateToolhelp32Snapshot` can fail against a 64-bit process with
`ERROR_PARTIAL_COPY`.

Modules loaded after the snapshot are absent. Call `Modules` again or use
`WaitForModule`.

## Reads return `nullopt`

Inspect:

```cpp
process.LastError()
process.LastErrorMessage()
process.Query(address)
```

Common causes are an unmapped address, insufficient rights, a guard page, a
range crossing into an unreadable region, or the target freeing memory between
query and read.

`Read<T>` requires the entire value. `ReadPartial` is available when partial
data is useful.

## Pointer chains work in x64 but not x86

Do not read pointers with `Read<std::uintptr_t>`; that uses the controller's
width. Use `ReadPointer` or `ResolvePointer`, which use the target width.

Confirm `base` is the address storing the first pointer and that offsets are
applied after each dereference.

## Writes fail

The process must be opened with `PROCESS_VM_WRITE` and
`PROCESS_VM_OPERATION`. `WriteRaw` retries after temporarily changing page
protection unless disabled.

A write can succeed but protection restoration can fail; EKore reports the
overall operation as failed in that case. Check `LastError`.

## A scan misses a known pattern

- Confirm the scan range and module are current.
- Confirm the pattern is valid.
- Disable `executableOnly`/`writableOnly` if the expected section differs.
- Increase `maxResults` when diagnosing ambiguity.
- Re-read the target after an update and wildcard changing operands.

The target can modify or unmap bytes while a scan is running.

## Patch cleanup

`Patch` duplicates the process handle, so it can revert even if the original
`Process` object moves or closes. Auto-revert is enabled by default.

Explicitly call `Revert` when failure must be reported before scope exit.
Destructor cleanup cannot report an error to the caller.

## Freezing stalls the controller

`Freezer::Tick` performs one remote write per entry. Call it at an appropriate
rate and avoid thousands of tiny writes. Group adjacent values into one
trivially copyable structure or byte span when possible.

`Freezer` is caller-scheduled and not internally synchronized. Protect it if
multiple controller threads modify entries or call `Tick`.

## Target exits during an operation

Check `Alive()` at workflow boundaries and still handle every read/write
failure. The process can exit immediately after an alive check.

No validation can remove that race; optional/bool results are part of the
normal API contract.

