#include "MemoryReader.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

} // namespace

int main() {
    std::array<std::byte, 16> bytes{};
    bytes[4] = std::byte{0x11};
    bytes[5] = std::byte{0x22};
    bytes[6] = std::byte{0x33};
    bytes[7] = std::byte{0x44};

    const erdup::SafeMemoryRange range{
        reinterpret_cast<std::uintptr_t>(bytes.data()),
        bytes.size(),
    };

    std::array<std::byte, 4> out{};
    if (!erdup::readMemory(range, range.baseAddress + 4, out.data(), out.size())) {
        return fail("Read fully inside the synthetic range should succeed.");
    }
    if (out[0] != std::byte{0x11} || out[3] != std::byte{0x44}) {
        return fail("Read did not copy expected bytes.");
    }
    if (erdup::readMemory(range, range.baseAddress + 14, out.data(), out.size())) {
        return fail("Read straddling the synthetic range should be rejected.");
    }
    if (!erdup::readMemory(range, range.baseAddress + bytes.size(), nullptr, 0)) {
        return fail("Zero-length read at range end should be accepted.");
    }

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const auto pageSize = static_cast<std::size_t>(systemInfo.dwPageSize);

    void* readWritePage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (readWritePage == nullptr) {
        return fail("VirtualAlloc PAGE_READWRITE failed.");
    }
    const auto readWriteAddress = reinterpret_cast<std::uintptr_t>(readWritePage);
    if (!erdup::isAddressRangeCommittedReadable(readWriteAddress, 1)) {
        VirtualFree(readWritePage, 0, MEM_RELEASE);
        return fail("Committed PAGE_READWRITE page should be readable.");
    }
    VirtualFree(readWritePage, 0, MEM_RELEASE);

    void* noAccessPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
    if (noAccessPage == nullptr) {
        return fail("VirtualAlloc PAGE_NOACCESS failed.");
    }
    const auto noAccessAddress = reinterpret_cast<std::uintptr_t>(noAccessPage);
    if (erdup::isAddressRangeCommittedReadable(noAccessAddress, 1)) {
        VirtualFree(noAccessPage, 0, MEM_RELEASE);
        return fail("PAGE_NOACCESS page should not be readable.");
    }
    std::byte noAccessOutput{};
    if (erdup::readCommittedMemory(noAccessAddress, &noAccessOutput, 1)) {
        VirtualFree(noAccessPage, 0, MEM_RELEASE);
        return fail("PAGE_NOACCESS read should fail without crashing.");
    }
    VirtualFree(noAccessPage, 0, MEM_RELEASE);

    std::cout << "Memory reader test passed.\n";
    return EXIT_SUCCESS;
}
