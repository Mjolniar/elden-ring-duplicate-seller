#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace erdup {

struct ModuleInfo;

struct MemoryRegion {
    std::uintptr_t baseAddress = 0;
    std::size_t size = 0;
};

struct MemoryScanLimits {
    std::size_t maxBytesToScan = 64 * 1024 * 1024;
    std::size_t maxRegionsToVisit = 4096;
    std::size_t maxCandidates = 4096;
    // Region reads are split into chunks of at most this many bytes to bound allocation.
    // Production default is 64 MB; tests may shrink it to exercise chunk-boundary handling.
    std::size_t maxScanChunkBytes = 64 * 1024 * 1024;
};

struct MemoryScanCandidate {
    std::intptr_t moduleRelativeAddress = 0;
};

struct MemoryScanReport {
    bool narrowed = false;
    std::uint64_t targetValue = 0;
    std::size_t valueSize = 0;
    std::size_t regionsVisited = 0;
    std::size_t bytesScanned = 0;
    bool truncated = false;
    std::uint64_t elapsedMilliseconds = 0;
    std::vector<MemoryScanCandidate> candidates;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

std::optional<std::uint64_t> parseMemoryScanTargetText(
    const std::string& text,
    std::size_t valueSize,
    std::string& error
);
std::optional<std::uint64_t> loadMemoryScanTarget(
    const std::filesystem::path& path,
    std::size_t valueSize,
    std::string& error
);

class MemoryScanSession {
public:
    MemoryScanReport scanProcess(
        const ModuleInfo& module,
        std::uint64_t targetValue,
        std::size_t valueSize,
        MemoryScanLimits limits = {}
    );
    MemoryScanReport scanRegions(
        const std::vector<MemoryRegion>& regions,
        std::uintptr_t moduleBase,
        std::uint64_t targetValue,
        std::size_t valueSize,
        MemoryScanLimits limits = {}
    );
    bool hasPreviousCandidates() const;
    const std::vector<MemoryScanCandidate>& currentCandidates() const;

private:
    std::vector<MemoryScanCandidate> previousCandidates_;
};

std::string formatMemoryScanReport(const MemoryScanReport& report);

} // namespace erdup
