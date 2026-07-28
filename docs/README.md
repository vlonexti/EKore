# EKore documentation

EKore is an external Windows process-memory library. Your tool remains in its
own process and uses Win32 APIs to inspect a target.

| Guide | Contents |
|---|---|
| [Getting started](getting-started.md) | Build, link, open a process, and handle failures |
| [API reference](api-reference.md) | Complete public surface by type |
| [Scanning and pointer chains](scanning.md) | Target-width pointers, signatures, values, and region filters |
| [Safety and troubleshooting](troubleshooting.md) | Architecture, permissions, partial reads, races, and patch cleanup |
| [MenuKit](menu-kit.md) | Standalone D3D11 menu host, pages, themes, widgets, hotkeys, and notifications |

## External versus internal

| | EKore | IKore |
|---|---|---|
| Runs | Separate controller process | Inside the target |
| Failure isolation | Controller normally crashes alone | A bug can crash the target |
| Memory access | `ReadProcessMemory` / `WriteProcessMemory` | Direct memory |
| Hooks/game calls | Not provided | Supported |
| Menu UI | Standalone controller window | In-frame overlay |
| Best for | Editors, scanners, trainers, inspectors | Hooks, mods, in-frame UI |

The two projects share naming and C++ conventions but are independent
libraries. EKore does not inject IKore.
