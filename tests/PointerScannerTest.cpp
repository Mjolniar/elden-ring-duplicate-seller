#include "PointerScanner.h"

#include <cstddef>
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

std::uintptr_t slotAddress(const std::vector<std::uintptr_t>& block, std::size_t index) {
    return reinterpret_cast<std::uintptr_t>(&block[index]);
}

erdup::MemoryRegion regionOf(const std::vector<std::uintptr_t>& block) {
    return {reinterpret_cast<std::uintptr_t>(block.data()), block.size() * sizeof(std::uintptr_t)};
}

bool anyChainResolvesTo(
    const erdup::PointerScanReport& report,
    const std::vector<erdup::MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    std::uintptr_t expected
) {
    for (const auto& chain : report.chains) {
        const auto resolved = erdup::resolvePointerChain(regions, moduleBase, chain);
        if (resolved.has_value() && resolved.value() == expected) {
            return true;
        }
    }
    return false;
}

// Scenario A: a direct static pointer to the target's struct (depth 1).
int assertDepthOneStaticChain() {
    std::vector<std::uintptr_t> moduleBlock(4, 0);
    std::vector<std::uintptr_t> heapBlock(8, 0);

    const auto target = slotAddress(heapBlock, 4);
    moduleBlock[1] = slotAddress(heapBlock, 0); // target - 0x20, within the offset window

    const std::vector<erdup::MemoryRegion> regions = {regionOf(moduleBlock), regionOf(heapBlock)};
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleBlock.data());
    const auto moduleSize = moduleBlock.size() * sizeof(std::uintptr_t);

    erdup::PointerScanSession session;
    const auto report = session.findChains(regions, moduleBase, moduleSize, target);

    if (report.chains.empty()) {
        return fail("Depth-1 scenario should find at least one static pointer chain.");
    }
    if (!anyChainResolvesTo(report, regions, moduleBase, target)) {
        return fail("No discovered chain resolved back to the target in the depth-1 scenario.");
    }
    return EXIT_SUCCESS;
}

// Scenario B: the only static pointer is two dereferences away, and its value is far from
// the target so no depth-1 chain can exist.
int assertDepthTwoStaticChain() {
    std::vector<std::uintptr_t> moduleBlock(4, 0);
    std::vector<std::uintptr_t> heapBlock(1024, 0);

    const auto target = slotAddress(heapBlock, 1000);
    heapBlock[100] = slotAddress(heapBlock, 992);        // target - 0x40 (near the target)
    moduleBlock[1] = slotAddress(heapBlock, 96);         // &heap[100] - 0x20 (near the location, far from target)

    const std::vector<erdup::MemoryRegion> regions = {regionOf(moduleBlock), regionOf(heapBlock)};
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleBlock.data());
    const auto moduleSize = moduleBlock.size() * sizeof(std::uintptr_t);

    erdup::PointerScanSession session;
    const auto report = session.findChains(regions, moduleBase, moduleSize, target);

    if (report.levelsExplored < 2) {
        return fail("Depth-2 scenario should explore at least two levels.");
    }
    bool foundTwoOffsetChain = false;
    for (const auto& chain : report.chains) {
        if (chain.offsets.size() == 2) {
            foundTwoOffsetChain = true;
        }
    }
    if (!foundTwoOffsetChain) {
        return fail("Depth-2 scenario should find a two-offset chain.");
    }
    if (!anyChainResolvesTo(report, regions, moduleBase, target)) {
        return fail("No discovered chain resolved back to the target in the depth-2 scenario.");
    }

    erdup::PointerScanLimits depthOne;
    depthOne.maxDepth = 1;
    const auto shallow = session.findChains(regions, moduleBase, moduleSize, target, depthOne);
    if (!shallow.chains.empty()) {
        return fail("Depth-1 limit should find no chain when the only root is two levels deep.");
    }
    return EXIT_SUCCESS;
}

// A target that nothing points at should yield no chains and must not crash.
int assertNoFalsePositives() {
    std::vector<std::uintptr_t> moduleBlock(4, 0);
    std::vector<std::uintptr_t> heapBlock(8, 0);

    const auto target = slotAddress(heapBlock, 4);
    const std::vector<erdup::MemoryRegion> regions = {regionOf(moduleBlock), regionOf(heapBlock)};
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleBlock.data());
    const auto moduleSize = moduleBlock.size() * sizeof(std::uintptr_t);

    erdup::PointerScanSession session;
    const auto report = session.findChains(regions, moduleBase, moduleSize, target);
    if (!report.chains.empty()) {
        return fail("Scan should find no chains when nothing points at the target.");
    }
    return EXIT_SUCCESS;
}

// The chain-spec parser should accept the exact format the scanner logs, plus a looser
// whitespace-separated form.
int assertChainSpecParsing() {
    const auto fromLog = erdup::parsePointerChainSpec("chain=[module+0x3dd1350]+0x6e8+0x780+0x0");
    if (!fromLog.has_value()) {
        return fail("Failed to parse the logged chain format.");
    }
    if (fromLog->staticModuleRva != 0x3dd1350) {
        return fail("Parsed the wrong static module RVA.");
    }
    if (fromLog->offsets.size() != 3 ||
        fromLog->offsets[0] != 0x6e8 || fromLog->offsets[1] != 0x780 || fromLog->offsets[2] != 0x0) {
        return fail("Parsed the wrong offsets from the logged chain format.");
    }

    const auto loose = erdup::parsePointerChainSpec("0x48012a8 0x198 0x750 0x0");
    if (!loose.has_value() || loose->staticModuleRva != 0x48012a8 || loose->offsets.size() != 3) {
        return fail("Failed to parse the whitespace-separated chain format.");
    }

    if (erdup::parsePointerChainSpec("0x3dd1350").has_value()) {
        return fail("A base with no offsets should not parse as a chain.");
    }
    if (erdup::parsePointerChainSpec("# comment").has_value()) {
        return fail("A non-hex line should not parse as a chain.");
    }
    return EXIT_SUCCESS;
}

int assertNullLiveVerificationIsUnresolved() {
    std::vector<std::uintptr_t> moduleBlock(4, 0);
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleBlock.data());

    erdup::PointerChain chain;
    chain.staticModuleRva = static_cast<std::intptr_t>(slotAddress(moduleBlock, 1) - moduleBase);
    chain.offsets = {0};

    const auto verification = erdup::verifyPointerChainLive(moduleBase, chain, sizeof(std::uint32_t));
    if (verification.resolved) {
        return fail("A live chain resolving to address zero should be reported unresolved.");
    }

    const auto formatted = erdup::formatPointerChainVerification(verification);
    if (formatted.find("-> unresolved") == std::string::npos) {
        return fail("Null live verification should format as unresolved.");
    }
    if (formatted.find("resolved_rva") != std::string::npos ||
        formatted.find("value=unreadable") != std::string::npos) {
        return fail("Null live verification should not format a bogus resolved RVA or unreadable value.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertDepthOneStaticChain() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (assertDepthTwoStaticChain() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (assertNoFalsePositives() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (assertChainSpecParsing() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (assertNullLiveVerificationIsUnresolved() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Pointer scanner test passed.\n";
    return EXIT_SUCCESS;
}
