#pragma once

#include "GameAdapter.h"
#include "MemoryScanner.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

struct HandleDiagnosticLimits {
    std::size_t maxHandles = 32;
    std::size_t maxMatchesPerHandle = 8;
    std::size_t maxBytesToScan = 1024ull * 1024ull * 1024ull;
    std::size_t maxRegionsToVisit = 65536;
    std::size_t maxScanChunkBytes = 16ull * 1024ull * 1024ull;
    std::size_t contextWordsBefore = 6;
    std::size_t contextWordsAfter = 12;
    bool includeConsumables = false;
};

struct HandleDiagnosticTarget {
    std::uint32_t handle = 0;
    std::uint32_t baseItemId = 0;
    ItemCategory category = ItemCategory::Unknown;
    int quantity = 0;
};

struct HandleDiagnosticMatch {
    HandleDiagnosticTarget target;
    std::uintptr_t address = 0;
    std::intptr_t moduleRelativeAddress = 0;
    std::vector<std::uint32_t> contextWords;
    std::size_t matchWordIndex = 0;
    bool contextComplete = false;
};

struct HandleDiagnosticReport {
    std::size_t targetsConsidered = 0;
    std::size_t regionsVisited = 0;
    std::size_t bytesScanned = 0;
    bool truncated = false;
    std::vector<HandleDiagnosticMatch> matches;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

std::vector<HandleDiagnosticTarget> buildHandleDiagnosticTargets(
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
);

HandleDiagnosticReport scanHandleReferencesInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
);

HandleDiagnosticReport scanHandleReferencesLive(
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
);

std::string formatHandleDiagnosticSummary(const HandleDiagnosticReport& report);
std::string formatHandleDiagnosticMatch(const HandleDiagnosticMatch& match);

} // namespace erdup
