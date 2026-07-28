# EKore vcpkg port

Install the repository's overlay port:

```powershell
vcpkg install ekore:x64-windows-static `
  --overlay-ports=C:/path/to/EKore/packaging/vcpkg/ports
```

Use `x86-windows-static` for a 32-bit controller. An x64 controller is
recommended when inspecting both x86 and x64 targets.

The installed CMake target is:

```cmake
find_package(EKore CONFIG REQUIRED)
target_link_libraries(MyTool PRIVATE EKore::Library)
```

