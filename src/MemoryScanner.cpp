#include "MemoryScanner.h"

#include "MemoryReader.h"
#include "ModuleInfo.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>

namespace erdup {

namespace {

bool validValueSize(std::size_t valueSize) {
    return valueSize == 4 || valueSize == 8;
}

std::uint64_t maxValueForSize(std::size_t valueSize) {
    if (valueSize == 4) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return std::numeric_limits<std::uint64_t>::max();
}

bool addWouldOverflow(std::uintptr_t value, std::size_t addend) {
    return addend > std::numeric_limits<std::uintptr_t>::max() - value;
}

bool addRelative(std::uintptr_t base, std::intptr_t relative, std::uintptr_t& address) {
    if (relative >= 0) {
        const auto unsignedRelative = static_cast<std::uintptr_t>(relative);
        if (unsignedRelative > std::numeric_limits<std::uintptr_t>::max() - base) {
            return false;
        }
        address = base + unsignedRelative;
        return true;
    }

    const auto distance = static_cast<std::uintptr_t>(-(relative + 1)) + 1;
    if (distance > base) {
        return false;
    }
    address = base - distance;
    return true;
}

std::intptr_t relativeToModule(std::uintptr_t address, std::uintptr_t moduleBase) {
    if (address >= moduleBase) {
        return static_cast<std::intptr_t>(address - moduleBase);
    }
    return -static_cast<std::intptr_t>(moduleBase - address);
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

std::uint64_t readLittleEndianValue(const std::vector<std::byte>& bytes, std::size_t offset, std::size_t valueSize) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < valueSize; ++i) {
        value |= static_cast<std::uint64_t>(std::to_integer<unsigned int>(bytes[offset + i])) << (8 * i);
    }
    return value;
}

// Reports candidate start offsets [0, reportCount) within the buffer, so long as the full
// value fits in the buffer. The buffer is read with up to (valueSize - 1) extra tail bytes
// beyond reportCount so a value whose start is in this chunk but whose bytes cross into the
// next chunk is still fully present and reported here exactly once.
void scanBufferForValue(
    const std::vector<std::byte>& bytes,
    std::uintptr_t chunkBase,
    std::uintptr_t moduleBase,
    std::uint64_t targetValue,
    std::size_t valueSize,
    MemoryScanLimits limits,
    MemoryScanReport& report,
    std::size_t reportCount
) {
    if (bytes.size() < valueSize) {
        return;
    }

    // Last start offset whose value is fully contained in the buffer, then clamp to the
    // range this chunk is responsible for reporting.
    const std::size_t maxStartOffset = bytes.size() - valueSize;
    const std::size_t reportLimit = std::min(reportCount, maxStartOffset + 1);
    for (std::size_t offset = 0; offset < reportLimit; ++offset) {
        if (readLittleEndianValue(bytes, offset, valueSize) != targetValue) {
            continue;
        }

        if (report.candidates.size() >= limits.maxCandidates) {
            report.truncated = true;
            return;
        }

        report.candidates.push_back({
            relativeToModule(chunkBase + offset, moduleBase),
        });
    }
}

const MemoryRegion* findContainingRegion(const std::vector<MemoryRegion>& regions, std::uintptr_t address, std::size_t length) {
    for (const auto& region : regions) {
        if (isRangeContained({region.baseAddress, region.size}, address, length)) {
            return &region;
        }
    }
    return nullptr;
}

std::string formatRelativeAddress(std::intptr_t relativeAddress) {
    std::ostringstream out;
    if (relativeAddress < 0) {
        out << "-0x" << std::hex << static_cast<std::uintptr_t>(-(relativeAddress + 1) + 1);
    } else {
        out << "0x" << std::hex << static_cast<std::uintptr_t>(relativeAddress);
    }
    return out.str();
}

void scanRegionForValue(
    const MemoryRegion& region,
    std::size_t bytesToScan,
    std::uintptr_t moduleBase,
    std::uint64_t targetValue,
    std::size_t valueSize,
    MemoryScanLimits limits,
    MemoryScanReport& report
) {
    if (bytesToScan < valueSize) {
        return;
    }

    const std::size_t chunkBytes = std::max<std::size_t>(limits.maxScanChunkBytes, 1);
    const std::size_t tail = valueSize - 1;
    const std::size_t lastStartOffset = bytesToScan - valueSize; // inclusive

    // Each chunk owns a contiguous, non-overlapping range of start offsets and reads a small
    // tail beyond it so values straddling the chunk boundary are captured exactly once.
    std::size_t reportStart = 0;
    while (reportStart <= lastStartOffset && report.candidates.size() < limits.maxCandidates) {
        const std::size_t reportCount = std::min(chunkBytes, (lastStartOffset - reportStart) + 1);
        const std::size_t readLength = std::min(reportCount + tail, bytesToScan - reportStart);

        std::vector<std::byte> bytes;
        try {
            bytes.resize(readLength);
        } catch (const std::bad_alloc&) {
            report.truncated = true;
            report.warnings.push_back("Memory scan chunk allocation failed.");
            return;
        }

        const auto chunkAddress = region.baseAddress + reportStart;
        if (readMemory({region.baseAddress, region.size}, chunkAddress, bytes.data(), bytes.size())) {
            scanBufferForValue(
                bytes,
                chunkAddress,
                moduleBase,
                targetValue,
                valueSize,
                limits,
                report,
                reportCount
            );
        } else {
            report.warnings.push_back("Skipped unreadable chunk during memory scan.");
        }

        reportStart += reportCount;
    }
}

} // namespace

std::optional<std::uint64_t> parseMemoryScanTargetText(
    const std::string& text,
    std::size_t valueSize,
    std::string& error
) {
    error.clear();
    if (!validValueSize(valueSize)) {
        error = "memory_scan_value_size must be 4 or 8.";
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(text, &consumed, 0);
        while (consumed < text.size() && std::isspace(static_cast<unsigned char>(text[consumed]))) {
            ++consumed;
        }
        if (consumed != text.size()) {
            error = "memory_scan_target.txt must contain only one integer value.";
            return std::nullopt;
        }
        if (value > maxValueForSize(valueSize)) {
            error = "memory scan target does not fit in configured value size.";
            return std::nullopt;
        }
        return value;
    } catch (...) {
        error = "Could not parse memory_scan_target.txt as an integer.";
        return std::nullopt;
    }
}

std::optional<std::uint64_t> loadMemoryScanTarget(
    const std::filesystem::path& path,
    std::size_t valueSize,
    std::string& error
) {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open " + path.string() + ".";
        return std::nullopt;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return parseMemoryScanTargetText(contents.str(), valueSize, error);
}

MemoryScanReport MemoryScanSession::scanProcess(
    const ModuleInfo& module,
    std::uint64_t targetValue,
    std::size_t valueSize,
    MemoryScanLimits limits
) {
    bool regionEnumerationTruncated = false;
    auto regions = enumeratePrivateReadWriteRegions(limits.maxRegionsToVisit, regionEnumerationTruncated);
    auto report = scanRegions(regions, module.baseAddress, targetValue, valueSize, limits);
    report.truncated = report.truncated || regionEnumerationTruncated;
    return report;
}

MemoryScanReport MemoryScanSession::scanRegions(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    std::uint64_t targetValue,
    std::size_t valueSize,
    MemoryScanLimits limits
) {
    const auto start = std::chrono::steady_clock::now();
    MemoryScanReport report;
    report.narrowed = !previousCandidates_.empty();
    report.targetValue = targetValue;
    report.valueSize = valueSize;

    if (!validValueSize(valueSize)) {
        report.errors.push_back("memory_scan_value_size must be 4 or 8.");
        return report;
    }
    if (targetValue > maxValueForSize(valueSize)) {
        report.errors.push_back("memory scan target does not fit in configured value size.");
        return report;
    }
    if (moduleBase == 0) {
        report.errors.push_back("Module base is unavailable; candidate RVAs cannot be reported.");
        return report;
    }

    if (report.narrowed) {
        for (const auto& previous : previousCandidates_) {
            std::uintptr_t address = 0;
            if (!addRelative(moduleBase, previous.moduleRelativeAddress, address)) {
                continue;
            }

            const auto* region = findContainingRegion(regions, address, valueSize);
            if (region == nullptr) {
                continue;
            }

            std::uint64_t currentValue = 0;
            if (!readMemory({region->baseAddress, region->size}, address, &currentValue, valueSize)) {
                continue;
            }
            if (valueSize == 4) {
                currentValue &= std::numeric_limits<std::uint32_t>::max();
            }
            if (currentValue == targetValue) {
                report.candidates.push_back(previous);
            }
        }
    } else {
        for (const auto& region : regions) {
            if (report.regionsVisited >= limits.maxRegionsToVisit ||
                report.bytesScanned >= limits.maxBytesToScan ||
                report.candidates.size() >= limits.maxCandidates) {
                report.truncated = true;
                break;
            }

            const auto remainingBytes = limits.maxBytesToScan - report.bytesScanned;
            const auto bytesToRead = std::min(region.size, remainingBytes);
            if (bytesToRead < valueSize) {
                ++report.regionsVisited;
                report.bytesScanned += bytesToRead;
                continue;
            }

            scanRegionForValue(region, bytesToRead, moduleBase, targetValue, valueSize, limits, report);

            ++report.regionsVisited;
            report.bytesScanned += bytesToRead;
        }
    }

    previousCandidates_ = report.candidates;
    const auto elapsed = std::chrono::steady_clock::now() - start;
    report.elapsedMilliseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    return report;
}

bool MemoryScanSession::hasPreviousCandidates() const {
    return !previousCandidates_.empty();
}

const std::vector<MemoryScanCandidate>& MemoryScanSession::currentCandidates() const {
    return previousCandidates_;
}

std::string formatMemoryScanReport(const MemoryScanReport& report) {
    std::ostringstream out;
    out << "memory_scan:"
        << " mode=" << (report.narrowed ? "narrow" : "initial")
        << ", value_size=" << report.valueSize
        << ", target_value=" << report.targetValue
        << ", regions_visited=" << report.regionsVisited
        << ", bytes_scanned=" << report.bytesScanned
        << ", candidates=" << report.candidates.size()
        << ", truncated=" << report.truncated
        << ", elapsed_ms=" << report.elapsedMilliseconds;

    for (const auto& candidate : report.candidates) {
        out << ", candidate_rva=" << formatRelativeAddress(candidate.moduleRelativeAddress);
    }
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }

    return out.str();
}

} // namespace erdup
