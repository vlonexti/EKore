#include <EKore/EKore.hpp>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <span>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition)
        return;
    ++g_failures;
    std::cerr << "FAIL: " << message << '\n';
}

template <typename T>
EKore::Address AddressOf(T& value) {
    return static_cast<EKore::Address>(
        reinterpret_cast<std::uintptr_t>(&value));
}

} // namespace

int main() {
    using namespace EKore;

    Process process = Process::Open(::GetCurrentProcessId());
    Check(process.Valid(), "open current process");
    Check(process.Alive(), "current process is alive");
    Check(process.Id() == ::GetCurrentProcessId(), "PID is retained");
    Check(process.PointerSize() == sizeof(void*), "pointer width detection");
    Check(process.MainModule().has_value(), "main module enumeration");
    Check(process.FindModule(L"kernel32.dll").has_value(),
          "case-insensitive module lookup");

    int value = 41;
    const Address valueAddress = AddressOf(value);
    Check(process.Read<int>(valueAddress).value_or(0) == 41,
          "typed read");
    Check(process.Write<int>(valueAddress, 42), "typed write");
    Check(value == 42, "typed write changed target");

    const std::array<int, 3> input{1, 2, 3};
    std::array<int, 3> target{};
    Check(process.WriteArray<int>(AddressOf(target), input),
          "array write");
    const auto readArray =
        process.ReadArray<int>(AddressOf(target), target.size());
    Check(readArray && (*readArray)[2] == 3, "array read");

    char text[32] = "hello";
    Check(process.ReadString(AddressOf(text), sizeof(text))
                  .value_or("") == "hello",
          "narrow string read");
    Check(process.WriteString(AddressOf(text), "world"),
          "narrow string write");
    Check(std::string(text) == "world", "narrow string changed target");

    wchar_t wideText[32] = L"wide";
    Check(process.ReadWideString(AddressOf(wideText), 32)
                  .value_or(L"") == L"wide",
          "wide string read");
    Check(process.WriteWideString(AddressOf(wideText), L"updated"),
          "wide string write");
    Check(std::wstring(wideText) == L"updated",
          "wide string changed target");

    int pointerValue = 77;
    int* pointer = &pointerValue;
    const std::array<Offset, 1> offsets{0};
    const auto resolved =
        process.ResolvePointer(AddressOf(pointer), offsets);
    Check(resolved && *resolved == AddressOf(pointerValue),
          "target-width pointer resolution");

    RemoteValue<int> remote(process, AddressOf(pointer),
                            std::vector<Offset>{0});
    Check(remote.GetOr() == 77, "RemoteValue read");
    Check(remote.Set(88) && pointerValue == 88, "RemoteValue write");

    std::array<u8, 11> signatureBytes{
        0x11, 0x48, 0x8B, 0x05, 0x99,
        0xAA, 0xBB, 0xCC, 0x74, 0x05, 0x22};
    Pattern wildcard("48 8B 05 ? ? ? ? 74 05");
    Check(wildcard.Valid(), "IDA pattern parsing");
    const auto signatureHit = wildcard.Scan(
        process, AddressOf(signatureBytes), signatureBytes.size());
    Check(signatureHit &&
              *signatureHit == AddressOf(signatureBytes) + 1,
          "wildcard pattern scan");
    Check(!Pattern("? ??").Valid(), "all-wildcard pattern rejected");

    std::array<u8, 32> boundaryBytes{};
    std::copy(signatureBytes.begin() + 1,
              signatureBytes.begin() + 10,
              boundaryBytes.begin() + 8);
    ScanOptions boundaryOptions;
    boundaryOptions.chunkSize = wildcard.Size();
    const auto boundaryHit = wildcard.Scan(
        process,
        AddressOf(boundaryBytes),
        boundaryBytes.size(),
        boundaryOptions);
    Check(boundaryHit &&
              *boundaryHit == AddressOf(boundaryBytes) + 8,
          "pattern scan across chunk overlap");

    const int searchedValue = 88;
    const auto valueHit = ValuePattern(searchedValue).Scan(
        process, AddressOf(pointerValue), sizeof(pointerValue));
    Check(valueHit && *valueHit == AddressOf(pointerValue),
          "exact value scan");

    int patchTarget = 10;
    const int replacement = 20;
    const auto* replacementBytes =
        reinterpret_cast<const u8*>(&replacement);
    Patch patch(
        process,
        AddressOf(patchTarget),
        std::span<const u8>(replacementBytes, sizeof(replacement)));
    Check(patch.Valid(), "patch construction");
    Check(patch.Apply() && patchTarget == 20, "patch apply");
    Check(patch.Revert() && patchTarget == 10, "patch revert");

    {
        Patch autoPatch(
            process,
            AddressOf(patchTarget),
            std::span<const u8>(replacementBytes, sizeof(replacement)));
        Check(autoPatch.Apply() && patchTarget == 20,
              "auto-revert patch apply");
    }
    Check(patchTarget == 10, "patch destructor auto-revert");

    Freezer freezer(process);
    freezer.Hold("value", AddressOf(value), 99);
    value = 1;
    Check(freezer.Tick() == 1 && value == 99, "freezer tick");
    freezer.Release("value");
    Check(freezer.Count() == 0, "freezer release");

    const auto allocation = process.Allocate(4096);
    Check(allocation.has_value(), "remote allocation");
    if (allocation) {
        Check(process.Write<int>(*allocation, 123), "allocation write");
        Check(process.Read<int>(*allocation).value_or(0) == 123,
              "allocation read");
        Check(process.Free(*allocation), "remote free");
    }

    Process invalid = Process::Open(0);
    Check(!invalid.Valid(), "invalid PID rejected");
    Check(invalid.LastError() == ERROR_INVALID_PARAMETER,
          "invalid PID error retained");

    if (g_failures != 0) {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All EKore tests passed\n";
    return 0;
}
