#include "HandleDiagnostics.h"

#include "MemoryReader.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <sstream>

namespace erdup {

namespace {

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

bool readContext(
    const MemoryRegion& region,
    std::uintptr_t address,
    const HandleDiagnosticLimits& limits,
    HandleDiagnosticMatch& match
) {
    const auto wordSize = sizeof(std::uint32_t);
    const auto beforeBytes = limits.contextWordsBefore * wordSize;
    const auto afterBytes = limits.contextWordsAfter * wordSize;

    const bool hasBefore = address >= region.baseAddress && address - region.baseAddress >= beforeBytes;
    std::uintptr_t start = hasBefore ? address - beforeBytes : region.baseAddress;
    const auto matchIndex = (address - start) / wordSize;

    if (addWouldOverflow(address, wordSize + afterBytes)) {
        match.matchWordIndex = matchIndex;
        return false;
    }
    const auto desiredEnd = address + wordSize + afterBytes;
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
        match.contextWords.size() == limits.contextWordsBefore + 1 + limits.contextWordsAfter;
    return match.contextComplete;
}

} // namespace

std::vector<HandleDiagnosticTarget> buildHandleDiagnosticTargets(
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
) {
    std::vector<HandleDiagnosticTarget> targets;
    std::set<std::uint32_t> seen;
    for (const auto& item : items) {
        if (targets.size() >= limits.maxHandles) {
            break;
        }
        if (item.instanceId == 0) {
            continue;
        }
        if (!limits.includeConsumables && item.category == ItemCategory::Consumable) {
            continue;
        }
        if (!seen.insert(item.instanceId).second) {
            continue;
        }
        targets.push_back({
            item.instanceId,
            item.baseItemId,
            item.category,
            item.quantity,
        });
    }
    return targets;
}

HandleDiagnosticReport scanHandleReferencesInRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
) {
    HandleDiagnosticReport report;
    const auto targets = buildHandleDiagnosticTargets(items, limits);
    report.targetsConsidered = targets.size();

    if (moduleBase == 0) {
        report.errors.push_back("Module base is unavailable; handle reference RVAs cannot be reported.");
        return report;
    }
    if (limits.maxHandles == 0 || limits.maxMatchesPerHandle == 0 || targets.empty()) {
        return report;
    }

    std::map<std::uint32_t, HandleDiagnosticTarget> targetByHandle;
    std::map<std::uint32_t, std::size_t> matchCountByHandle;
    for (const auto& target : targets) {
        targetByHandle.emplace(target.handle, target);
        matchCountByHandle.emplace(target.handle, 0);
    }

    const std::size_t chunkBytes = std::max<std::size_t>(limits.maxScanChunkBytes, sizeof(std::uint32_t));
    for (const auto& region : regions) {
        if (report.regionsVisited >= limits.maxRegionsToVisit || report.bytesScanned >= limits.maxBytesToScan) {
            report.truncated = true;
            break;
        }

        const auto remainingBytes = limits.maxBytesToScan - report.bytesScanned;
        const auto bytesToScan = std::min(region.size, remainingBytes);
        if (bytesToScan < sizeof(std::uint32_t)) {
            ++report.regionsVisited;
            report.bytesScanned += bytesToScan;
            continue;
        }

        std::size_t chunkOffset = 0;
        while (chunkOffset + sizeof(std::uint32_t) <= bytesToScan) {
            const auto chunkSize = std::min(chunkBytes, bytesToScan - chunkOffset);
            std::vector<std::byte> bytes;
            try {
                bytes.resize(chunkSize);
            } catch (const std::bad_alloc&) {
                report.truncated = true;
                report.warnings.push_back("Handle reference scan chunk allocation failed.");
                return report;
            }

            const auto chunkAddress = region.baseAddress + chunkOffset;
            if (readMemory({region.baseAddress, region.size}, chunkAddress, bytes.data(), bytes.size())) {
                for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= bytes.size(); offset += sizeof(std::uint32_t)) {
                    const auto value = readU32(bytes, offset);
                    const auto target = targetByHandle.find(value);
                    if (target == targetByHandle.end()) {
                        continue;
                    }
                    auto& count = matchCountByHandle[value];
                    if (count >= limits.maxMatchesPerHandle) {
                        continue;
                    }

                    HandleDiagnosticMatch match;
                    match.target = target->second;
                    match.address = chunkAddress + offset;
                    match.moduleRelativeAddress = relativeToModule(match.address, moduleBase);
                    readContext(region, match.address, limits, match);
                    report.matches.push_back(std::move(match));
                    ++count;
                }
            } else {
                report.warnings.push_back("Skipped unreadable chunk during handle reference scan.");
            }

            if (chunkSize == 0) {
                break;
            }
            chunkOffset += chunkSize;
        }

        ++report.regionsVisited;
        report.bytesScanned += bytesToScan;
    }

    return report;
}

HandleDiagnosticReport scanHandleReferencesLive(
    std::uintptr_t moduleBase,
    const std::vector<ItemInstance>& items,
    const HandleDiagnosticLimits& limits
) {
    bool regionEnumerationTruncated = false;
    const auto regions = enumeratePrivateReadWriteRegions(limits.maxRegionsToVisit, regionEnumerationTruncated);
    auto report = scanHandleReferencesInRegions(regions, moduleBase, items, limits);
    report.truncated = report.truncated || regionEnumerationTruncated;
    return report;
}

std::string formatHandleDiagnosticSummary(const HandleDiagnosticReport& report) {
    std::ostringstream out;
    out << "handle_ref_scan:"
        << " targets=" << report.targetsConsidered
        << ", regions_visited=" << report.regionsVisited
        << ", bytes_scanned=" << report.bytesScanned
        << ", matches=" << report.matches.size()
        << ", truncated=" << report.truncated;
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }
    return out.str();
}

std::string formatHandleDiagnosticMatch(const HandleDiagnosticMatch& match) {
    std::ostringstream out;
    out << "handle_ref_match:"
        << " handle=0x" << std::hex << match.target.handle
        << std::dec
        << ", base_item_id=" << match.target.baseItemId
        << ", category=" << categoryToken(match.target.category)
        << ", quantity=" << match.target.quantity
        << ", match_rva=" << formatRelativeAddress(match.moduleRelativeAddress)
        << ", context_complete=" << match.contextComplete
        << ", match_word=" << match.matchWordIndex;

    for (std::size_t i = 0; i < match.contextWords.size(); ++i) {
        out << ", u32[" << i << "]=0x" << std::hex << match.contextWords[i] << std::dec;
    }
    return out.str();
}

} // namespace erdup
