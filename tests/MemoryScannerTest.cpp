#include "MemoryScanner.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

void writeU32(std::byte* target, std::uint32_t value) {
    std::memcpy(target, &value, sizeof(value));
}

} // namespace

int main() {
    std::string error;
    const auto parsed = erdup::parseMemoryScanTargetText("1234\n", 4, error);
    if (!parsed.has_value() || parsed.value() != 1234 || !error.empty()) {
        return fail("Expected decimal target parsing to succeed.");
    }
    const auto tooLarge = erdup::parseMemoryScanTargetText("0x100000000", 4, error);
    if (tooLarge.has_value() || error.empty()) {
        return fail("Expected oversized 4-byte target to be rejected.");
    }

    std::array<std::byte, 64> bytes{};
    const auto base = reinterpret_cast<std::uintptr_t>(bytes.data());
    writeU32(bytes.data() + 4, 1234);
    writeU32(bytes.data() + 12, 1234);
    writeU32(bytes.data() + 24, 7777);

    erdup::MemoryScanSession session;
    const erdup::MemoryRegion region{base, bytes.size()};
    const auto initial = session.scanRegions({region}, base, 1234, 4);
    if (initial.narrowed) {
        return fail("First scan should be an initial scan.");
    }
    if (initial.candidates.size() != 2) {
        return fail("Initial scan should find two 4-byte candidates.");
    }
    if (initial.candidates[0].moduleRelativeAddress != 4 ||
        initial.candidates[1].moduleRelativeAddress != 12) {
        return fail("Initial scan returned unexpected module-relative offsets.");
    }

    writeU32(bytes.data() + 4, 4321);
    writeU32(bytes.data() + 12, 5678);
    const auto narrowed = session.scanRegions({region}, base, 5678, 4);
    if (!narrowed.narrowed) {
        return fail("Second scan should narrow existing candidates.");
    }
    if (narrowed.candidates.size() != 1 || narrowed.candidates[0].moduleRelativeAddress != 12) {
        return fail("Narrowing scan should keep only the changed matching candidate.");
    }

    const auto invalidSize = session.scanRegions({region}, base, 1, 2);
    if (invalidSize.errors.empty()) {
        return fail("Invalid memory scan value size should be rejected.");
    }

    // Chunk-boundary regression: with a 16-byte chunk size, a 4-byte value planted at
    // offset 14 straddles the first chunk boundary and one at 30 straddles the second.
    // Each must be found exactly once, alongside a non-straddling value at offset 4.
    std::array<std::byte, 64> boundaryBytes{};
    const auto boundaryBase = reinterpret_cast<std::uintptr_t>(boundaryBytes.data());
    const std::uint32_t marker = 0xAABBCCDD;
    writeU32(boundaryBytes.data() + 4, marker);
    writeU32(boundaryBytes.data() + 14, marker);
    writeU32(boundaryBytes.data() + 30, marker);

    erdup::MemoryScanLimits chunkedLimits;
    chunkedLimits.maxScanChunkBytes = 16;

    erdup::MemoryScanSession boundarySession;
    const erdup::MemoryRegion boundaryRegion{boundaryBase, boundaryBytes.size()};
    const auto boundaryScan = boundarySession.scanRegions({boundaryRegion}, boundaryBase, marker, 4, chunkedLimits);
    if (boundaryScan.candidates.size() != 3) {
        return fail("Chunk-boundary scan should find all three values exactly once.");
    }
    if (boundaryScan.candidates[0].moduleRelativeAddress != 4 ||
        boundaryScan.candidates[1].moduleRelativeAddress != 14 ||
        boundaryScan.candidates[2].moduleRelativeAddress != 30) {
        return fail("Chunk-boundary scan returned unexpected offsets (straddling value missed or duplicated).");
    }

    std::cout << "Memory scanner test passed.\n";
    return EXIT_SUCCESS;
}
