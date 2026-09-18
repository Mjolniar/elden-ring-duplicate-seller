#pragma once

#include "GameAdapter.h"
#include "MemoryScanner.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

struct CompactRowDiagnosticLimits {
    std::size_t maxTargets = 128;
    std::size_t maxRows = 512;
    std::size_t maxBytesToScan = 1024ull * 1024ull * 1024ull;
    std::size_t maxRegionsToVisit = 65536;
    std::size_t maxScanChunkBytes = 16ull * 1024ull * 1024ull;
    std::size_t minRunLength = 4;
    std::size_t maxRuns = 32;
    std::size_t contextWordsBefore = 4;
    std::size_t contextWordsAfter = 8;
    bool includeConsumables = false;
};

struct CompactRowDiagnosticTarget {
    std::uint32_t handle = 0;
    std::uint32_t baseItemId = 0;
    ItemCategory category = ItemCategory::Unknown;
    std::uint32_t categoryCode = 0;
    int quantity = 0;
};

struct CompactRowDiagnosticMatch {
    CompactRowDiagnosticTarget target;
    std::uintptr_t address = 0;
    std::intptr_t moduleRelativeAddress = 0;
    std::vector<std::uint32_t> contextWords;
    std::size_t matchWordIndex = 0;
    bool contextComplete = false;
};

struct CompactRowDiagnosticRun {
    std::uintptr_t startAddress = 0;
    std::uintptr_t endAddress = 0;
    std::intptr_t startModuleRelative = 0;
    std::intptr_t endModuleRelative = 0;
    std::size_t strideBytes = 0;
    std::size_t rowCount = 0;
};

struct CompactRowDiagnosticReport {
    std::size_t targetsConsidered = 0;
    std::size_t regionsVisited = 0;
    std::size_t bytesScanned = 0;
    bool truncated = false;
    std::vector<CompactRowDiagnosticMatch> matches;
    std::vector<CompactRowDiagnosticRun> runs;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct CompactRowDumpLimits {
    std::size_t maxRuns = 1;
    std::size_t maxRowsPerRun = 16;
    std::size_t backtrackBytes = 0x10;
    std::size_t maxStrideBytes = 0x100;
};

struct CompactRowDumpRow {
    std::size_t runIndex = 0;
    std::size_t rowIndex = 0;
    std::uintptr_t rowBaseAddress = 0;
    std::uintptr_t matchAddress = 0;
    std::intptr_t rowBaseModuleRelative = 0;
    std::intptr_t matchModuleRelative = 0;
    std::size_t strideBytes = 0;
    std::size_t backtrackBytes = 0;
    bool readable = false;
    bool complete = false;
    bool hasTarget = false;
    CompactRowDiagnosticTarget target;
    std::vector<std::uint32_t> words;
};

struct CompactRowDumpRun {
    std::size_t runIndex = 0;
    CompactRowDiagnosticRun sourceRun;
    std::size_t rowsDumped = 0;
    bool truncated = false;
};

struct CompactRowDumpReport {
    std::size_t runsConsidered = 0;
    std::size_t rowsDumped = 0;
    bool truncated = false;
    CompactRowDumpLimits limits;
    std::vector<CompactRowDumpRun> runs;
    std::vector<CompactRowDumpRow> rows;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

std::vector<CompactRowDiagnosticTarget> buildCompactRowDiagnosticTargets(
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
);

CompactRowDiagnosticReport scanCompactRowsInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
);

CompactRowDiagnosticReport scanCompactRowsLive(
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
);

CompactRowDumpReport dumpCompactRowRunsInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticReport& scanReport,
    const CompactRowDumpLimits& limits
);

CompactRowDumpReport dumpCompactRowRunsLive(
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticReport& scanReport,
    const CompactRowDumpLimits& limits
);

std::string formatCompactRowDiagnosticSummary(const CompactRowDiagnosticReport& report);
std::string formatCompactRowDiagnosticRun(const CompactRowDiagnosticRun& run);
std::string formatCompactRowDiagnosticMatch(const CompactRowDiagnosticMatch& match);
std::string formatCompactRowDumpSummary(const CompactRowDumpReport& report);
std::string formatCompactRowDumpRun(const CompactRowDumpRun& run);
std::string formatCompactRowDumpRow(const CompactRowDumpRow& row);

} // namespace erdup
