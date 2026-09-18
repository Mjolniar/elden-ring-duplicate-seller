#include "CompactRowDiagnostics.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t handle,
    std::uint32_t baseItemId,
    erdup::ItemCategory category,
    int quantity = 1
) {
    erdup::ItemInstance instance;
    instance.instanceId = handle;
    instance.baseItemId = baseItemId;
    instance.category = category;
    instance.quantity = quantity;
    return instance;
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void writeCompactRow(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t handle,
    std::uint32_t baseItemId,
    std::uint32_t categoryCode,
    std::uint32_t quantity
) {
    writeU32(bytes, offset, handle);
    writeU32(bytes, offset + 4, baseItemId);
    writeU32(bytes, offset + 8, categoryCode);
    writeU32(bytes, offset + 12, quantity);
}

bool contains(const std::string& text, const std::string& expected) {
    return text.find(expected) != std::string::npos;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

int assertCompactRowsAndRunsAreDetected() {
    std::vector<std::byte> regionBytes(512);
    writeU32(regionBytes, 0x20, 0xdeadbeef);
    writeCompactRow(regionBytes, 0x40, 0x80800001, 1000, 0, 1);
    writeCompactRow(regionBytes, 0x50, 0x90800002, 2000, 2, 1);
    writeCompactRow(regionBytes, 0x60, 0xa0000003, 3000, 3, 2);
    writeCompactRow(regionBytes, 0x90, 0x80800004, 4000, 0, 1);
    writeCompactRow(regionBytes, 0x100, 0x80800001, 1000, 0, 1);
    writeCompactRow(regionBytes, 0x140, 0x90800002, 2000, 2, 1);
    writeCompactRow(regionBytes, 0x180, 0xa0000003, 3000, 3, 2);
    writeCompactRow(regionBytes, 0x1c0, 0x80800004, 4000, 0, 1);

    const std::vector<erdup::ItemInstance> items = {
        item(0x80800001, 1000, erdup::ItemCategory::Weapon),
        item(0x90800002, 2000, erdup::ItemCategory::Armor),
        item(0xa0000003, 3000, erdup::ItemCategory::Talisman, 2),
        item(0x80800004, 4000, erdup::ItemCategory::Shield),
    };

    erdup::CompactRowDiagnosticLimits limits;
    limits.maxTargets = 8;
    limits.maxRows = 8;
    limits.minRunLength = 3;
    limits.maxRuns = 4;
    limits.contextWordsBefore = 1;
    limits.contextWordsAfter = 1;

    const auto base = reinterpret_cast<std::uintptr_t>(regionBytes.data());
    const auto report = erdup::scanCompactRowsInRegions(
        {{base, regionBytes.size()}},
        base,
        items,
        limits
    );

    if (report.targetsConsidered != 4 || report.matches.size() != 8 || report.runs.size() < 2) {
        return fail("Expected compact row matches and multiple stride-aware runs.");
    }
    bool foundCompactStrideRun = false;
    bool foundWideStrideRun = false;
    for (const auto& run : report.runs) {
        foundCompactStrideRun = foundCompactStrideRun || (run.strideBytes == 0x10 && run.rowCount == 3);
        foundWideStrideRun = foundWideStrideRun || (run.strideBytes == 0x40 && run.rowCount == 4);
    }
    if (!foundCompactStrideRun || !foundWideStrideRun) {
        return fail("Expected both 0x10 and 0x40 compact row runs.");
    }
    if (report.matches.front().target.categoryCode != 0 ||
        report.matches[1].target.categoryCode != 2 ||
        report.matches[2].target.categoryCode != 3) {
        return fail("Compact category codes did not match expected equipment category mapping.");
    }

    const auto summary = erdup::formatCompactRowDiagnosticSummary(report);
    const auto run = erdup::formatCompactRowDiagnosticRun(report.runs.front());
    const auto match = erdup::formatCompactRowDiagnosticMatch(report.matches.front());
    if (!contains(summary, "compact_row_scan:") ||
        !contains(summary, "rows=8") ||
        !contains(summary, "runs=") ||
        !contains(run, "compact_row_run:") ||
        !contains(run, "stride=0x") ||
        !contains(run, "row_count=") ||
        !contains(match, "compact_row_match:") ||
        !contains(match, "category_code=0") ||
        !contains(match, "u32[1]=0x80800001")) {
        return fail("Formatted compact row diagnostics are missing expected fields.");
    }

    return EXIT_SUCCESS;
}

int assertUnknownCategoriesAreSkipped() {
    std::vector<std::byte> regionBytes(128);
    writeCompactRow(regionBytes, 0x20, 0x40800001, 5000, 1, 9);

    const std::vector<erdup::ItemInstance> items = {
        item(0x40800001, 5000, erdup::ItemCategory::Consumable, 9),
        item(0x50800002, 6000, erdup::ItemCategory::Unknown),
    };

    erdup::CompactRowDiagnosticLimits limits;
    limits.includeConsumables = true;

    const auto base = reinterpret_cast<std::uintptr_t>(regionBytes.data());
    const auto report = erdup::scanCompactRowsInRegions(
        {{base, regionBytes.size()}},
        base,
        items,
        limits
    );

    if (report.targetsConsidered != 0 || !report.matches.empty()) {
        return fail("Compact row scanner should skip categories without verified compact codes.");
    }

    return EXIT_SUCCESS;
}

int assertCompactRowRunDumpBacktracksAndReadsFullRows() {
    std::vector<std::byte> regionBytes(512);

    const std::array<std::size_t, 3> rowBases = {0x40, 0x90, 0xe0};
    const std::array<std::uint32_t, 3> handles = {0x80800001, 0x80800002, 0x80800003};
    for (std::size_t i = 0; i < rowBases.size(); ++i) {
        const auto rowBase = rowBases[i];
        writeU32(regionBytes, rowBase + 0x00, 0);
        writeU32(regionBytes, rowBase + 0x04, 0);
        writeU32(regionBytes, rowBase + 0x08, 0xf);
        writeU32(regionBytes, rowBase + 0x0c, static_cast<std::uint32_t>(i));
        writeCompactRow(regionBytes, rowBase + 0x10, handles[i], 1000, 0, 1);
        writeU32(regionBytes, rowBase + 0x20, 0xabc00000 + static_cast<std::uint32_t>(i));
        writeU32(regionBytes, rowBase + 0x4c, 0xfeed0000 + static_cast<std::uint32_t>(i));
    }

    const std::vector<erdup::ItemInstance> items = {
        item(handles[0], 1000, erdup::ItemCategory::Weapon),
        item(handles[1], 1000, erdup::ItemCategory::Weapon),
        item(handles[2], 1000, erdup::ItemCategory::Weapon),
    };

    erdup::CompactRowDiagnosticLimits scanLimits;
    scanLimits.maxTargets = 8;
    scanLimits.maxRows = 8;
    scanLimits.minRunLength = 3;
    scanLimits.maxRuns = 2;
    scanLimits.contextWordsBefore = 4;
    scanLimits.contextWordsAfter = 2;

    const auto base = reinterpret_cast<std::uintptr_t>(regionBytes.data());
    const auto scanReport = erdup::scanCompactRowsInRegions(
        {{base, regionBytes.size()}},
        base,
        items,
        scanLimits
    );

    erdup::CompactRowDumpLimits dumpLimits;
    dumpLimits.maxRuns = 1;
    dumpLimits.maxRowsPerRun = 2;
    dumpLimits.backtrackBytes = 0x10;
    dumpLimits.maxStrideBytes = 0x50;

    const auto dumpReport = erdup::dumpCompactRowRunsInRegions(
        {{base, regionBytes.size()}},
        base,
        scanReport,
        dumpLimits
    );

    if (dumpReport.runsConsidered == 0 ||
        dumpReport.runs.empty() ||
        dumpReport.rows.size() != 2 ||
        dumpReport.rowsDumped != 2 ||
        !dumpReport.truncated) {
        return fail("Expected compact row dump to read two rows and report truncation.");
    }
    if (dumpReport.rows[0].rowBaseAddress != base + rowBases[0] ||
        dumpReport.rows[0].matchAddress != base + rowBases[0] + 0x10 ||
        !dumpReport.rows[0].readable ||
        !dumpReport.rows[0].complete ||
        !dumpReport.rows[0].hasTarget) {
        return fail("Expected compact row dump to backtrack to the row base and attach target metadata.");
    }
    if (dumpReport.rows[0].words.size() != 0x50 / sizeof(std::uint32_t) ||
        dumpReport.rows[0].words[2] != 0xf ||
        dumpReport.rows[0].words[4] != handles[0] ||
        dumpReport.rows[0].words[5] != 1000 ||
        dumpReport.rows[0].words[6] != 0 ||
        dumpReport.rows[0].words[7] != 1 ||
        dumpReport.rows[0].words[19] != 0xfeed0000) {
        return fail("Expected compact row dump words to include preamble and compact item block.");
    }

    const auto summary = erdup::formatCompactRowDumpSummary(dumpReport);
    const auto run = erdup::formatCompactRowDumpRun(dumpReport.runs.front());
    const auto row = erdup::formatCompactRowDumpRow(dumpReport.rows.front());
    if (!contains(summary, "compact_row_dump_start:") ||
        !contains(summary, "rows=2") ||
        !contains(run, "compact_row_dump_run:") ||
        !contains(run, "stride=0x50") ||
        !contains(row, "compact_row_dump:") ||
        !contains(row, "backtrack=0x10") ||
        !contains(row, "u32[4]=0x80800001") ||
        !contains(row, "u32[19]=0xfeed0000")) {
        return fail("Formatted compact row dump is missing expected fields.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertCompactRowsAndRunsAreDetected() != EXIT_SUCCESS ||
        assertUnknownCategoriesAreSkipped() != EXIT_SUCCESS ||
        assertCompactRowRunDumpBacktracksAndReadsFullRows() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Compact row diagnostics test passed.\n";
    return EXIT_SUCCESS;
}
