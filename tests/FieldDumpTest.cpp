#include "FieldDump.h"

#include <Windows.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

std::uintptr_t slotAddress(const std::vector<std::uint64_t>& block, std::size_t index) {
    return reinterpret_cast<std::uintptr_t>(&block[index]);
}

int assertClassifiesSyntheticSlots() {
    std::vector<std::uint64_t> structSlots(4, 0);
    std::vector<std::uint64_t> childSlots(2, 0);

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    void* noAccessPage = VirtualAlloc(
        nullptr,
        static_cast<std::size_t>(systemInfo.dwPageSize),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_NOACCESS
    );
    if (noAccessPage == nullptr) {
        return fail("VirtualAlloc PAGE_NOACCESS failed.");
    }

    structSlots[0] = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(childSlots.data()));
    structSlots[1] = 0x11223344ull;
    structSlots[2] = 0;
    structSlots[3] = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(noAccessPage));

    erdup::PointerChain chain;
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(structSlots.data());
    const auto report = erdup::dumpStructFieldsAt(
        moduleBase,
        reinterpret_cast<std::uintptr_t>(structSlots.data()),
        chain,
        32,
        8
    );

    VirtualFree(noAccessPage, 0, MEM_RELEASE);

    if (!report.chainResolved || report.slots.size() != 4) {
        return fail("Expected four readable-offset field dump slots.");
    }
    for (std::size_t i = 0; i < report.slots.size(); ++i) {
        if (report.slots[i].offset != i * 8) {
            return fail("Field dump slot offset was not stride-based.");
        }
    }
    if (!report.slots[0].slotRead || !report.slots[0].pointerReadable) {
        return fail("Pointer-sized slot to committed memory should be classified pointer=yes.");
    }
    if (!report.slots[1].slotRead || report.slots[1].pointerReadable || report.slots[1].low32 != 0x11223344u) {
        return fail("Integer-like slot should be pointer=no with the expected low32 value.");
    }
    if (!report.slots[2].slotRead || report.slots[2].pointerReadable || report.slots[2].low32 != 0) {
        return fail("Null qword should be pointer=no and low32=0.");
    }
    if (!report.slots[3].slotRead || report.slots[3].pointerReadable) {
        return fail("Pointer-sized slot to unreadable memory should be classified pointer=no.");
    }

    const auto pointerLine = erdup::formatFieldDumpSlot(report.slots[0]);
    if (pointerLine.find("pointer=yes") == std::string::npos ||
        pointerLine.find("pointer_rva=") == std::string::npos) {
        return fail("Pointer slot format should include pointer=yes and a module-relative address.");
    }

    return EXIT_SUCCESS;
}

int assertByteCapHonored() {
    std::vector<std::uint64_t> structSlots((erdup::kMaxFieldDumpBytes / sizeof(std::uint64_t)) + 64, 0);
    erdup::PointerChain chain;

    const auto report = erdup::dumpStructFieldsAt(
        reinterpret_cast<std::uintptr_t>(structSlots.data()),
        reinterpret_cast<std::uintptr_t>(structSlots.data()),
        chain,
        erdup::kMaxFieldDumpBytes + 0x100,
        8
    );

    if (!report.capped || report.bytesToRead != erdup::kMaxFieldDumpBytes) {
        return fail("Field dump should cap requested bytes at kMaxFieldDumpBytes.");
    }
    if (report.slots.size() != erdup::kMaxFieldDumpBytes / sizeof(std::uint64_t)) {
        return fail("Field dump slot count should match the capped byte count and stride.");
    }

    return EXIT_SUCCESS;
}

int assertUnreadableSlotIsSkipped() {
    erdup::PointerChain chain;
    const auto report = erdup::dumpStructFieldsAt(0x10000000, 1, chain, 8, 8);

    if (!report.chainResolved || report.slots.size() != 1) {
        return fail("Unreadable base test should produce one skipped slot.");
    }
    if (report.slots[0].slotRead || report.slots[0].skippedReason != "unreadable") {
        return fail("Unreadable field dump slot should be skipped without crashing.");
    }

    return EXIT_SUCCESS;
}

int assertLiveChainResolvesBase() {
    std::vector<std::uint64_t> moduleSlots(2, 0);
    std::vector<std::uint64_t> structSlots(2, 0);
    structSlots[0] = 0x55667788ull;

    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleSlots.data());
    moduleSlots[1] = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(structSlots.data()));

    erdup::PointerChain chain;
    chain.staticModuleRva = static_cast<std::intptr_t>(slotAddress(moduleSlots, 1) - moduleBase);
    chain.offsets = {0};

    const auto report = erdup::runFieldDumpLive(moduleBase, chain, 16, 8);
    if (!report.chainResolved || report.baseAddress != reinterpret_cast<std::uintptr_t>(structSlots.data())) {
        return fail("Field dump live chain should resolve to the synthetic struct base.");
    }
    if (report.slots.size() != 2 || report.slots[0].low32 != 0x55667788u) {
        return fail("Field dump live chain should read slots from the resolved struct base.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertClassifiesSyntheticSlots() != EXIT_SUCCESS ||
        assertByteCapHonored() != EXIT_SUCCESS ||
        assertUnreadableSlotIsSkipped() != EXIT_SUCCESS ||
        assertLiveChainResolvesBase() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Field dump test passed.\n";
    return EXIT_SUCCESS;
}
