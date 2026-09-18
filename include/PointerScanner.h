#pragma once

#include "MemoryScanner.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace erdup {

struct ModuleInfo;

// A resolved static pointer path: read the pointer at [moduleBase + staticModuleRva],
// then for every offset except the last, add it and dereference; add the final offset
// without dereferencing to reach the target address. This chain is reproducible across
// launches because it is rooted in the module image, unlike a raw heap address.
struct PointerChain {
    std::intptr_t staticModuleRva = 0;
    std::vector<std::size_t> offsets;
};

struct PointerScanLimits {
    int maxDepth = 3;
    std::size_t offsetWindow = 0x800;
    std::size_t maxBytesToScan = static_cast<std::size_t>(3072) * 1024 * 1024;
    std::size_t maxRegionsToVisit = 65536;
    std::size_t maxNodesPerLevel = 256;
    std::size_t maxChains = 64;
};

struct PointerScanReport {
    std::intptr_t targetModuleRelativeAddress = 0;
    int maxDepth = 0;
    int levelsExplored = 0;
    std::size_t nodesExplored = 0;
    std::size_t bytesScanned = 0;
    bool truncated = false;
    std::uint64_t elapsedMilliseconds = 0;
    std::vector<PointerChain> chains;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// Pure, read-only bounded breadth-first search for static pointer chains that reach
// targetAddress. Scans the supplied regions only; does not enumerate the process itself,
// so it is unit-testable against synthetic memory.
class PointerScanSession {
public:
    PointerScanReport findChains(
        const std::vector<MemoryRegion>& regions,
        std::uintptr_t moduleBase,
        std::size_t moduleSize,
        std::uintptr_t targetAddress,
        PointerScanLimits limits = {}
    );
};

// Resolves a chain against the supplied regions, returning the final target address or
// nullopt if any dereference falls outside a readable region. Read-only.
std::optional<std::uintptr_t> resolvePointerChain(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const PointerChain& chain
);

struct PointerChainVerification {
    PointerChain chain;
    bool resolved = false;
    std::uintptr_t resolvedAddress = 0;
    std::intptr_t resolvedModuleRelative = 0;
    bool valueRead = false;
    std::uint64_t value = 0;
};

// Parses a chain spec such as "[module+0x3dd1350]+0x6e8+0x780+0x0" or the looser
// "0x3dd1350 0x6e8 0x780 0x0". First hex token is the static module RVA, the rest are
// offsets. Returns nullopt if there is no usable base + at least one offset.
std::optional<PointerChain> parsePointerChainSpec(const std::string& text);

// Resolves a chain live against the current process (VirtualQuery-guarded reads) and reads
// the value at the resolved address. Read-only; used to verify a candidate chain still
// lands on the expected value on a fresh launch.
PointerChainVerification verifyPointerChainLive(
    std::uintptr_t moduleBase,
    const PointerChain& chain,
    std::size_t valueSize
);

std::string formatPointerChainVerification(const PointerChainVerification& verification);

// Live-process entry point: enumerates committed, readable regions of the current process
// and searches for static pointer chains to targetAddress. Read-only.
PointerScanReport scanProcessForPointerChains(
    const ModuleInfo& module,
    std::uintptr_t targetAddress,
    PointerScanLimits limits = {}
);

std::string formatPointerScanReport(const PointerScanReport& report);

} // namespace erdup
