#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace EKore {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

/// A remote virtual address. Always 64-bit so an x64 controller can represent
/// both 32-bit and 64-bit targets without changing the public ABI.
using Address = std::uint64_t;
using Offset  = std::int64_t;

inline constexpr Address kMaxAddress = std::numeric_limits<Address>::max();

enum class Architecture {
    Unknown,
    X86,
    X64,
    Arm64,
};

struct ProcessInfo {
    DWORD        id = 0;
    std::wstring name;
};

struct Module {
    std::wstring name;
    std::wstring path;
    Address      base = 0;
    std::size_t  size = 0;

    [[nodiscard]] explicit operator bool() const { return base != 0 && size != 0; }
    [[nodiscard]] Address End() const { return base + static_cast<Address>(size); }
};

struct MemoryRegion {
    Address     base = 0;
    std::size_t size = 0;
    DWORD       state = 0;
    DWORD       protection = 0;
    DWORD       type = 0;

    [[nodiscard]] Address End() const { return base + static_cast<Address>(size); }
    [[nodiscard]] bool Committed() const { return state == MEM_COMMIT; }
    [[nodiscard]] bool Guarded() const { return (protection & PAGE_GUARD) != 0; }
    [[nodiscard]] bool Readable() const;
    [[nodiscard]] bool Writable() const;
    [[nodiscard]] bool Executable() const;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Matrix4x4 {
    float values[16]{};

    [[nodiscard]] constexpr float& operator[](std::size_t index) {
        return values[index];
    }
    [[nodiscard]] constexpr const float& operator[](std::size_t index) const {
        return values[index];
    }
};

static_assert(sizeof(Vec2) == 8);
static_assert(sizeof(Vec3) == 12);
static_assert(sizeof(Vec4) == 16);
static_assert(sizeof(Matrix4x4) == 64);

} // namespace EKore

