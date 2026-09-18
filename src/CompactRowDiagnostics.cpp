#include "CompactRowDiagnostics.h"

#include "MemoryReader.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <sstream>

namespace erdup {

namespace {

constexpr std::size_t kCompactRowBytes = 4 * sizeof(std::uint32_t);
constexpr std::size_t kMaxInferredStrideBytes = 0x800;
constexpr std::array<std::size_t, 9> kDefaultRunStrides = {
    kCompactRowBytes,
    0x40,
    0x50,
    0x80,
    0xc0,
    0x100,
    0x140,
    0x180,
    0x200,
};

bool addWouldOverflow(std::uintptr_t value, std::size_t addend) {
    return addend > std::numeric_limits<std::uintptr_t>::max() - value;
}

std::intptr_t relativeToModule(std::uintptr_t address, std::uintptr_t moduleBase) {
    if (address >= moduleBase) {
        return static_cast<std::intptr_t>(address - moduleBase);
    }
    return -static_cast<std::intptr_t>(moduleBase - address);
}

std::string formatRelativeAddress(std::intptr_t relativeAddress) {
    std::ostringstream out;
    if (relativeAddress < 0) {
        out << "-0x" << std::hex << (static_cast<std::uintptr_t>(-(relativeAddress + 1)) + 1);
    } else {
        out << "0x" << std::hex << static_cast<std::uintptr_t>(relativeAddress);
    }
    return out.str();
}

std::string categoryToken(ItemCategory category) {
    switch (category) {
    case ItemCategory::Weapon: return "weapon";
    case ItemCategory::Shield: return "shield";
    case ItemCategory::Armor: return "armor";
    case ItemCategory::Talisman: return "talisman";
    case ItemCategory::AshOfWar: return "ash_of_war";
    case ItemCategory::Spell: return "spell";
    case ItemCategory::Consumable: return "consumable";
    case ItemCategory::Material: return "material";
    case ItemCategory::KeyItem: return "keyitem";
    case ItemCategory::Unknown: return "unknown";
    }
    return "unknown";
}

std::optional<std::uint32_t> compactCategoryCode(ItemCategory category) {
    switch (category) {
    case ItemCategory::Weapon:
    case ItemCategory::Shield:
        return 0;
    case ItemCategory::Armor:
        return 2;
    case ItemCategory::Talisman:
        return 3;
    case ItemCategory::AshOfWar:
        return 4;
    default:
        return std::nullopt;
    }
}

bool isPrivateReadWrite(const MEMORY_BASIC_INFORMATION& info) {
    if (info.State != MEM_COMMIT || info.Type != MEM_PRIVATE) {
        return false;
    }
    if ((info.Protect & PAGE_GUARD) != 0 || (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    return (info.Protect & 0xff) == PAGE_READWRITE;
}

std::vector<MemoryRegion> enumeratePrivateReadWriteRegions(std::size_t maxRegions, bool& truncated) {
    std::vector<MemoryRegion> regions;
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);

    auto cursor = reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const auto maximum = reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    while (cursor < maximum) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) {
            break;
        }

        const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (addWouldOverflow(regionBase, info.RegionSize)) {
            break;
        }
        const auto regionEnd = regionBase + info.RegionSize;
        if (regionEnd <= cursor) {
            break;
        }

        if (isPrivateReadWrite(info)) {
            if (regions.size() >= maxRegions) {
                truncated = true;
                break;
            }
            regions.push_back({regionBase, info.RegionSize});
        }

        cursor = regionEnd;
    }

    return regions;
}

std::uint32_t readU32(const std::vector<std::byte>& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < sizeof(value); ++i) {
        value |= static_cast<std::uint32_t>(std::to_integer<unsigned int>(bytes[offset + i])) << (8 * i);
    }
    return value;
}

std::array<std::uint32_t, 4> rowKey(
    std::uint32_t handle,
    std::uint32_t baseItemId,
    std::uint32_t categoryCode,
    int quantity
) {
    const auto normalizedQuantity = quantity < 0 ? 0u : static_cast<std::uint32_t>(quantity);
    return {handle, baseItemId, categoryCode, normalizedQuantity};
}

bool readContext(
    const MemoryRegion& region,
    std::uintptr_t address,
    const CompactRowDiagnosticLimits& limits,
    CompactRowDiagnosticMatch& match
) {
    const auto wordSize = sizeof(std::uint32_t);
    const auto beforeBytes = limits.contextWordsBefore * wordSize;
    const auto afterBytes = limits.contextWordsAfter * wordSize;

    const bool hasBefore = address >= region.baseAddress && address - region.baseAddress >= beforeBytes;
    std::uintptr_t start = hasBefore ? address - beforeBytes : region.baseAddress;
    const auto matchIndex = (address - start) / wordSize;

    if (addWouldOverflow(address, kCompactRowBytes + afterBytes)) {
        match.matchWordIndex = matchIndex;
        return false;
    }
    const auto desiredEnd = address + kCompactRowBytes + afterBytes;
    if (addWouldOverflow(region.baseAddress, region.size)) {
        match.matchWordIndex = matchIndex;
        return false;
    }
    const auto regionEnd = region.baseAddress + region.size;
    const auto end = std::min(desiredEnd, regionEnd);
    if (end <= start || ((end - start) % wordSize) != 0) {
        match.matchWordIndex = matchIndex;
        return false;
    }

    std::vector<std::uint32_t> words;
    words.reserve((end - start) / wordSize);
    for (auto cursor = start; cursor < end; cursor += wordSize) {
        std::uint32_t word = 0;
        if (!readMemory({region.baseAddress, region.size}, cursor, &word, sizeof(word))) {
            match.contextWords = std::move(words);
            match.matchWordIndex = matchIndex;
            match.contextComplete = false;
            return false;
        }
        words.push_back(word);
    }

    match.contextWords = std::move(words);
    match.matchWordIndex = matchIndex;
    match.contextComplete =
        hasBefore &&
        desiredEnd <= regionEnd &&
        match.contextWords.size() == limits.contextWordsBefore + 4 + limits.contextWordsAfter;
    return match.contextComplete;
}

std::vector<std::size_t> inferRunStrides(const std::vector<std::uintptr_t>& addresses) {
    std::map<std::size_t, std::size_t> adjacentDeltaCounts;
    for (std::size_t i = 1; i < addresses.size(); ++i) {
        const auto delta = addresses[i] - addresses[i - 1];
        if (delta >= kCompactRowBytes &&
            delta <= kMaxInferredStrideBytes &&
            (delta % sizeof(std::uint32_t)) == 0) {
            ++adjacentDeltaCounts[static_cast<std::size_t>(delta)];
        }
    }

    std::vector<std::pair<std::size_t, std::size_t>> rankedDeltas;
    rankedDeltas.reserve(adjacentDeltaCounts.size());
    for (const auto& [stride, count] : adjacentDeltaCounts) {
        rankedDeltas.emplace_back(stride, count);
    }
    std::sort(rankedDeltas.begin(), rankedDeltas.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        }
        return lhs.first < rhs.first;
    });

    std::set<std::size_t> strideSet;
    for (const auto stride : kDefaultRunStrides) {
        strideSet.insert(stride);
    }
    for (std::size_t i = 0; i < rankedDeltas.size() && i < 8; ++i) {
        strideSet.insert(rankedDeltas[i].first);
    }

    return {strideSet.begin(), strideSet.end()};
}

std::vector<CompactRowDiagnosticRun> buildRuns(
    const std::vector<CompactRowDiagnosticMatch>& matches,
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticLimits& limits
) {
    std::vector<CompactRowDiagnosticRun> runs;
    if (matches.empty() || limits.maxRuns == 0) {
        return runs;
    }

    std::vector<std::uintptr_t> addresses;
    addresses.reserve(matches.size());
    for (const auto& match : matches) {
        addresses.push_back(match.address);
    }
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());

    const std::set<std::uintptr_t> addressSet(addresses.begin(), addresses.end());
    const auto candidateStrides = inferRunStrides(addresses);
    std::vector<CompactRowDiagnosticRun> candidates;

    for (const auto stride : candidateStrides) {
        for (const auto start : addresses) {
            if (start >= stride && addressSet.contains(start - stride)) {
                continue;
            }

            std::size_t rowCount = 1;
            auto end = start;
            while (!addWouldOverflow(end, stride) && addressSet.contains(end + stride)) {
                end += stride;
                ++rowCount;
            }

            if (rowCount >= limits.minRunLength) {
                candidates.push_back({
                    start,
                    end,
                    relativeToModule(start, moduleBase),
                    relativeToModule(end, moduleBase),
                    stride,
                    rowCount,
                });
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.rowCount != rhs.rowCount) {
            return lhs.rowCount > rhs.rowCount;
        }
        if (lhs.strideBytes != rhs.strideBytes) {
            return lhs.strideBytes < rhs.strideBytes;
        }
        return lhs.startAddress < rhs.startAddress;
    });

    for (const auto& candidate : candidates) {
        runs.push_back(candidate);
        if (runs.size() >= limits.maxRuns) {
            break;
        }
    }

    return runs;
}

const MemoryRegion* findContainingRegion(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t address,
    std::size_t length
) {
    for (const auto& region : regions) {
        if (isRangeContained({region.baseAddress, region.size}, address, length)) {
            return &region;
        }
    }
    return nullptr;
}

bool readDumpBytes(
    const std::vector<MemoryRegion>* regions,
    std::uintptr_t address,
    void* output,
    std::size_t length
) {
    if (regions == nullptr) {
        return readCommittedMemory(address, output, length);
    }

    const auto* region = findContainingRegion(*regions, address, length);
    if (region == nullptr) {
        return false;
    }
    return readMemory({region->baseAddress, region->size}, address, output, length);
}

CompactRowDumpReport dumpCompactRowRuns(
    const std::vector<MemoryRegion>* regions,
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticReport& scanReport,
    const CompactRowDumpLimits& limits
) {
    CompactRowDumpReport report;
    report.limits = limits;
    report.runsConsidered = scanReport.runs.size();

    if (moduleBase == 0) {
        report.errors.push_back("Module base is unavailable; compact row dump RVAs cannot be reported.");
        return report;
    }
    if (limits.maxRuns == 0 || limits.maxRowsPerRun == 0 || scanReport.runs.empty()) {
        return report;
    }
    if (limits.maxStrideBytes < sizeof(std::uint32_t)) {
        report.errors.push_back("Compact row dump max stride is smaller than one u32.");
        return report;
    }

    std::map<std::uintptr_t, CompactRowDiagnosticTarget> targetByAddress;
    for (const auto& match : scanReport.matches) {
        targetByAddress.emplace(match.address, match.target);
    }

    const auto selectedRunCount = std::min(scanReport.runs.size(), limits.maxRuns);
    if (selectedRunCount < scanReport.runs.size()) {
        report.truncated = true;
    }

    for (std::size_t runIndex = 0; runIndex < selectedRunCount; ++runIndex) {
        const auto& sourceRun = scanReport.runs[runIndex];
        CompactRowDumpRun dumpedRun;
        dumpedRun.runIndex = runIndex;
        dumpedRun.sourceRun = sourceRun;

        if (sourceRun.strideBytes == 0 ||
            sourceRun.strideBytes > limits.maxStrideBytes ||
            (sourceRun.strideBytes % sizeof(std::uint32_t)) != 0) {
            report.warnings.push_back(
                "Skipped compact row dump run " + std::to_string(runIndex) +
                " because stride is zero, too large, or not u32-aligned."
            );
            report.runs.push_back(dumpedRun);
            continue;
        }

        const auto rowLimit = std::min(sourceRun.rowCount, limits.maxRowsPerRun);
        if (rowLimit < sourceRun.rowCount) {
            dumpedRun.truncated = true;
            report.truncated = true;
        }

        for (std::size_t rowIndex = 0; rowIndex < rowLimit; ++rowIndex) {
            if (rowIndex != 0 && addWouldOverflow(sourceRun.startAddress, sourceRun.strideBytes * rowIndex)) {
                dumpedRun.truncated = true;
                report.truncated = true;
                report.warnings.push_back("Stopped compact row dump after address arithmetic overflow.");
                break;
            }

            const auto matchAddress = sourceRun.startAddress + sourceRun.strideBytes * rowIndex;
            CompactRowDumpRow row;
            row.runIndex = runIndex;
            row.rowIndex = rowIndex;
            row.matchAddress = matchAddress;
            row.matchModuleRelative = relativeToModule(matchAddress, moduleBase);
            row.strideBytes = sourceRun.strideBytes;
            row.backtrackBytes = limits.backtrackBytes;

            if (matchAddress < limits.backtrackBytes) {
                report.warnings.push_back("Skipped compact row dump row because backtrack underflowed.");
                report.rows.push_back(std::move(row));
                ++dumpedRun.rowsDumped;
                ++report.rowsDumped;
                continue;
            }

            row.rowBaseAddress = matchAddress - limits.backtrackBytes;
            row.rowBaseModuleRelative = relativeToModule(row.rowBaseAddress, moduleBase);

            const auto target = targetByAddress.find(matchAddress);
            if (target != targetByAddress.end()) {
                row.hasTarget = true;
                row.target = target->second;
            }

            std::vector<std::byte> bytes(sourceRun.strideBytes);
            row.readable = readDumpBytes(regions, row.rowBaseAddress, bytes.data(), bytes.size());
            if (row.readable) {
                row.words.reserve(bytes.size() / sizeof(std::uint32_t));
                for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= bytes.size();
                     offset += sizeof(std::uint32_t)) {
                    row.words.push_back(readU32(bytes, offset));
                }
                row.complete = row.words.size() == sourceRun.strideBytes / sizeof(std::uint32_t);
            }

            report.rows.push_back(std::move(row));
            ++dumpedRun.rowsDumped;
            ++report.rowsDumped;
        }

        report.runs.push_back(dumpedRun);
    }

    return report;
}

} // namespace

std::vector<CompactRowDiagnosticTarget> buildCompactRowDiagnosticTargets(
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
) {
    std::vector<CompactRowDiagnosticTarget> targets;
    std::set<std::array<std::uint32_t, 4>> seenRows;
    for (const auto& item : items) {
        if (targets.size() >= limits.maxTargets) {
            break;
        }
        if (item.instanceId == 0 || item.quantity < 0) {
            continue;
        }
        if (!limits.includeConsumables && item.category == ItemCategory::Consumable) {
            continue;
        }
        const auto categoryCode = compactCategoryCode(item.category);
        if (!categoryCode.has_value()) {
            continue;
        }

        const auto key = rowKey(item.instanceId, item.baseItemId, categoryCode.value(), item.quantity);
        if (!seenRows.insert(key).second) {
            continue;
        }

        targets.push_back({
            item.instanceId,
            item.baseItemId,
            item.category,
            categoryCode.value(),
            item.quantity,
        });
    }
    return targets;
}

CompactRowDiagnosticReport scanCompactRowsInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
) {
    CompactRowDiagnosticReport report;
    const auto targets = buildCompactRowDiagnosticTargets(items, limits);
    report.targetsConsidered = targets.size();

    if (moduleBase == 0) {
        report.errors.push_back("Module base is unavailable; compact row RVAs cannot be reported.");
        return report;
    }
    if (limits.maxTargets == 0 || limits.maxRows == 0 || targets.empty()) {
        return report;
    }

    std::map<std::array<std::uint32_t, 4>, CompactRowDiagnosticTarget> targetByRow;
    for (const auto& target : targets) {
        targetByRow.emplace(
            rowKey(target.handle, target.baseItemId, target.categoryCode, target.quantity),
            target
        );
    }

    const std::size_t chunkBytes = std::max<std::size_t>(limits.maxScanChunkBytes, kCompactRowBytes);
    for (const auto& region : regions) {
        if (report.regionsVisited >= limits.maxRegionsToVisit ||
            report.bytesScanned >= limits.maxBytesToScan ||
            report.matches.size() >= limits.maxRows) {
            report.truncated = true;
            break;
        }

        const auto remainingBytes = limits.maxBytesToScan - report.bytesScanned;
        const auto bytesToScan = std::min(region.size, remainingBytes);
        if (bytesToScan < kCompactRowBytes) {
            ++report.regionsVisited;
            report.bytesScanned += bytesToScan;
            continue;
        }

        std::size_t chunkOffset = 0;
        while (chunkOffset + kCompactRowBytes <= bytesToScan) {
            if (report.matches.size() >= limits.maxRows) {
                report.truncated = true;
                break;
            }

            const auto chunkSize = std::min(chunkBytes, bytesToScan - chunkOffset);
            std::vector<std::byte> bytes;
            try {
                bytes.resize(chunkSize);
            } catch (const std::bad_alloc&) {
                report.truncated = true;
                report.warnings.push_back("Compact row scan chunk allocation failed.");
                report.runs = buildRuns(report.matches, moduleBase, limits);
                return report;
            }

            const auto chunkAddress = region.baseAddress + chunkOffset;
            if (readMemory({region.baseAddress, region.size}, chunkAddress, bytes.data(), bytes.size())) {
                for (std::size_t offset = 0; offset + kCompactRowBytes <= bytes.size(); offset += sizeof(std::uint32_t)) {
                    const auto key = std::array<std::uint32_t, 4>{
                        readU32(bytes, offset),
                        readU32(bytes, offset + sizeof(std::uint32_t)),
                        readU32(bytes, offset + 2 * sizeof(std::uint32_t)),
                        readU32(bytes, offset + 3 * sizeof(std::uint32_t)),
                    };
                    const auto target = targetByRow.find(key);
                    if (target == targetByRow.end()) {
                        continue;
                    }

                    CompactRowDiagnosticMatch match;
                    match.target = target->second;
                    match.address = chunkAddress + offset;
                    match.moduleRelativeAddress = relativeToModule(match.address, moduleBase);
                    readContext(region, match.address, limits, match);
                    report.matches.push_back(std::move(match));

                    if (report.matches.size() >= limits.maxRows) {
                        report.truncated = true;
                        break;
                    }
                }
            } else {
                report.warnings.push_back("Skipped unreadable chunk during compact row scan.");
            }

            if (chunkSize == 0) {
                break;
            }
            chunkOffset += chunkSize;
        }

        ++report.regionsVisited;
        report.bytesScanned += bytesToScan;
    }

    report.runs = buildRuns(report.matches, moduleBase, limits);
    return report;
}

CompactRowDiagnosticReport scanCompactRowsLive(
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const CompactRowDiagnosticLimits& limits
) {
    bool regionEnumerationTruncated = false;
    const auto regions = enumeratePrivateReadWriteRegions(limits.maxRegionsToVisit, regionEnumerationTruncated);
    auto report = scanCompactRowsInRegions(regions, moduleBase, items, limits);
    report.truncated = report.truncated || regionEnumerationTruncated;
    return report;
}

CompactRowDumpReport dumpCompactRowRunsInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticReport& scanReport,
    const CompactRowDumpLimits& limits
) {
    return dumpCompactRowRuns(&regions, moduleBase, scanReport, limits);
}

CompactRowDumpReport dumpCompactRowRunsLive(
    std::uintptr_t moduleBase,
    const CompactRowDiagnosticReport& scanReport,
    const CompactRowDumpLimits& limits
) {
    return dumpCompactRowRuns(nullptr, moduleBase, scanReport, limits);
}

std::string formatCompactRowDiagnosticSummary(const CompactRowDiagnosticReport& report) {
    std::ostringstream out;
    out << "compact_row_scan:"
        << " targets=" << report.targetsConsidered
        << ", regions_visited=" << report.regionsVisited
        << ", bytes_scanned=" << report.bytesScanned
        << ", rows=" << report.matches.size()
        << ", runs=" << report.runs.size()
        << ", truncated=" << report.truncated;
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }
    return out.str();
}

std::string formatCompactRowDumpSummary(const CompactRowDumpReport& report) {
    std::ostringstream out;
    out << "compact_row_dump_start:"
        << " runs_considered=" << report.runsConsidered
        << ", runs_dumped=" << report.runs.size()
        << ", rows=" << report.rowsDumped
        << ", truncated=" << report.truncated
        << ", max_runs=" << report.limits.maxRuns
        << ", max_rows_per_run=" << report.limits.maxRowsPerRun
        << ", backtrack=0x" << std::hex << report.limits.backtrackBytes
        << ", max_stride=0x" << report.limits.maxStrideBytes << std::dec;
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }
    return out.str();
}

std::string formatCompactRowDiagnosticRun(const CompactRowDiagnosticRun& run) {
    std::ostringstream out;
    out << "compact_row_run:"
        << " start_rva=" << formatRelativeAddress(run.startModuleRelative)
        << ", end_rva=" << formatRelativeAddress(run.endModuleRelative)
        << ", stride=0x" << std::hex << run.strideBytes << std::dec
        << ", row_count=" << run.rowCount;
    return out.str();
}

std::string formatCompactRowDumpRun(const CompactRowDumpRun& run) {
    std::ostringstream out;
    out << "compact_row_dump_run:"
        << " run_index=" << run.runIndex
        << ", start_rva=" << formatRelativeAddress(run.sourceRun.startModuleRelative)
        << ", end_rva=" << formatRelativeAddress(run.sourceRun.endModuleRelative)
        << ", stride=0x" << std::hex << run.sourceRun.strideBytes << std::dec
        << ", source_row_count=" << run.sourceRun.rowCount
        << ", rows_dumped=" << run.rowsDumped
        << ", truncated=" << run.truncated;
    return out.str();
}

std::string formatCompactRowDiagnosticMatch(const CompactRowDiagnosticMatch& match) {
    std::ostringstream out;
    out << "compact_row_match:"
        << " handle=0x" << std::hex << match.target.handle
        << std::dec
        << ", base_item_id=" << match.target.baseItemId
        << ", category=" << categoryToken(match.target.category)
        << ", category_code=" << match.target.categoryCode
        << ", quantity=" << match.target.quantity
        << ", row_rva=" << formatRelativeAddress(match.moduleRelativeAddress)
        << ", context_complete=" << match.contextComplete
        << ", match_word=" << match.matchWordIndex;

    for (std::size_t i = 0; i < match.contextWords.size(); ++i) {
        out << ", u32[" << i << "]=0x" << std::hex << match.contextWords[i] << std::dec;
    }
    return out.str();
}

std::string formatCompactRowDumpRow(const CompactRowDumpRow& row) {
    std::ostringstream out;
    out << "compact_row_dump:"
        << " run_index=" << row.runIndex
        << ", row_index=" << row.rowIndex
        << ", row_base_rva=" << formatRelativeAddress(row.rowBaseModuleRelative)
        << ", match_rva=" << formatRelativeAddress(row.matchModuleRelative)
        << ", stride=0x" << std::hex << row.strideBytes
        << ", backtrack=0x" << row.backtrackBytes << std::dec
        << ", readable=" << row.readable
        << ", complete=" << row.complete;

    if (row.hasTarget) {
        out << ", handle=0x" << std::hex << row.target.handle << std::dec
            << ", base_item_id=" << row.target.baseItemId
            << ", category=" << categoryToken(row.target.category)
            << ", category_code=" << row.target.categoryCode
            << ", quantity=" << row.target.quantity;
    } else {
        out << ", handle=<unknown>"
            << ", base_item_id=<unknown>"
            << ", category=unknown"
            << ", quantity=<unknown>";
    }

    for (std::size_t i = 0; i < row.words.size(); ++i) {
        out << ", u32[" << i << "]=0x" << std::hex << row.words[i] << std::dec;
    }
    return out.str();
}

} // namespace erdup
