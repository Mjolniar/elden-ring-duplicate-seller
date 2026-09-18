#include "PointerScanner.h"

#include "MemoryReader.h"
#include "ModuleInfo.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace erdup {

namespace {

constexpr std::size_t kMaxPointerScanChunkBytes = 64 * 1024 * 1024;

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

std::uintptr_t readPointer(const std::vector<std::byte>& buffer, std::size_t offset) {
    std::uintptr_t value = 0;
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    return value;
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

bool readPointerAt(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t address,
    std::uintptr_t& output
) {
    const auto* region = findContainingRegion(regions, address, sizeof(std::uintptr_t));
    if (region == nullptr) {
        return false;
    }
    return readMemory({region->baseAddress, region->size}, address, &output, sizeof(output));
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

bool isReadableCommitted(const MEMORY_BASIC_INFORMATION& info) {
    if (info.State != MEM_COMMIT) {
        return false;
    }
    if ((info.Protect & PAGE_GUARD) != 0 || (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const DWORD baseProtect = info.Protect & 0xff;
    return baseProtect == PAGE_READONLY ||
        baseProtect == PAGE_READWRITE ||
        baseProtect == PAGE_WRITECOPY ||
        baseProtect == PAGE_EXECUTE_READ ||
        baseProtect == PAGE_EXECUTE_READWRITE ||
        baseProtect == PAGE_EXECUTE_WRITECOPY;
}

// Only two kinds of region can take part in a static pointer chain: the module image
// itself (where a reproducible root pointer lives) and private read/write heap (where
// intermediate object pointers live). Restricting enumeration to these keeps the scan
// small and complete instead of wading through mapped files and other modules.
bool isPrivateReadWrite(const MEMORY_BASIC_INFORMATION& info) {
    if (info.State != MEM_COMMIT || info.Type != MEM_PRIVATE) {
        return false;
    }
    if ((info.Protect & PAGE_GUARD) != 0 || (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    return (info.Protect & 0xff) == PAGE_READWRITE;
}

std::vector<MemoryRegion> enumeratePointerScanRegions(
    std::uintptr_t moduleBase,
    std::size_t moduleSize,
    std::size_t maxRegions,
    bool& truncated
) {
    const auto moduleEnd = addWouldOverflow(moduleBase, moduleSize)
        ? std::numeric_limits<std::uintptr_t>::max()
        : moduleBase + moduleSize;

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

        const bool withinModule = regionBase >= moduleBase && regionBase < moduleEnd;
        if (isReadableCommitted(info) && (withinModule || isPrivateReadWrite(info))) {
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

} // namespace

PointerScanReport PointerScanSession::findChains(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    std::size_t moduleSize,
    std::uintptr_t targetAddress,
    PointerScanLimits limits
) {
    const auto start = std::chrono::steady_clock::now();
    PointerScanReport report;
    report.maxDepth = limits.maxDepth;
    report.targetModuleRelativeAddress = relativeToModule(targetAddress, moduleBase);

    if (moduleBase == 0 || moduleSize == 0) {
        report.errors.push_back("Module range is unavailable; static pointer roots cannot be identified.");
        return report;
    }
    if (targetAddress == 0) {
        report.errors.push_back("Pointer scan target address is null.");
        return report;
    }
    if (limits.maxDepth < 1) {
        report.errors.push_back("Pointer scan max depth must be at least 1.");
        return report;
    }

    const auto moduleEnd = addWouldOverflow(moduleBase, moduleSize)
        ? std::numeric_limits<std::uintptr_t>::max()
        : moduleBase + moduleSize;

    struct Node {
        std::uintptr_t address;
        std::vector<std::size_t> offsets;
    };

    std::vector<Node> frontier;
    frontier.push_back({targetAddress, {}});
    std::unordered_set<std::uintptr_t> visited;
    visited.insert(targetAddress);

    for (int depth = 1; depth <= limits.maxDepth && !frontier.empty(); ++depth) {
        report.levelsExplored = depth;

        std::vector<std::uintptr_t> sortedAddresses;
        sortedAddresses.reserve(frontier.size());
        std::unordered_map<std::uintptr_t, std::size_t> addressToNode;
        for (std::size_t i = 0; i < frontier.size(); ++i) {
            sortedAddresses.push_back(frontier[i].address);
            addressToNode[frontier[i].address] = i;
        }
        std::sort(sortedAddresses.begin(), sortedAddresses.end());

        std::vector<Node> nextFrontier;
        bool budgetExhausted = false;
        // The byte budget is applied per level, since each level rescans the same regions.
        std::size_t levelBytesScanned = 0;

        for (const auto& region : regions) {
            if (levelBytesScanned >= limits.maxBytesToScan) {
                report.truncated = true;
                budgetExhausted = true;
                break;
            }

            const auto remaining = limits.maxBytesToScan - levelBytesScanned;
            const auto bytesToRead = std::min(region.size, remaining);
            if (bytesToRead < sizeof(std::uintptr_t)) {
                levelBytesScanned += bytesToRead;
                report.bytesScanned += bytesToRead;
                continue;
            }

            std::size_t chunkStart = 0;
            while (chunkStart < bytesToRead) {
                const auto chunkSize = std::min(kMaxPointerScanChunkBytes, bytesToRead - chunkStart);
                if (chunkSize < sizeof(std::uintptr_t)) {
                    break;
                }

                std::vector<std::byte> buffer;
                try {
                    buffer.resize(chunkSize);
                } catch (const std::bad_alloc&) {
                    report.warnings.push_back("Pointer scan chunk allocation failed.");
                    report.truncated = true;
                    break;
                }

                const auto chunkAddress = region.baseAddress + chunkStart;
                if (!readMemory({region.baseAddress, region.size}, chunkAddress, buffer.data(), buffer.size())) {
                    report.warnings.push_back("Skipped unreadable chunk during pointer scan.");
                    chunkStart += chunkSize;
                    continue;
                }

                for (std::size_t offset = 0; offset + sizeof(std::uintptr_t) <= buffer.size(); offset += sizeof(std::uintptr_t)) {
                    const auto value = readPointer(buffer, offset);
                    const auto windowEnd = addWouldOverflow(value, limits.offsetWindow)
                        ? std::numeric_limits<std::uintptr_t>::max()
                        : value + limits.offsetWindow;

                    auto it = std::lower_bound(sortedAddresses.begin(), sortedAddresses.end(), value);
                    for (; it != sortedAddresses.end() && *it <= windowEnd; ++it) {
                        const auto pointedTarget = *it;
                        const auto addend = static_cast<std::size_t>(pointedTarget - value);
                        const auto location = chunkAddress + offset;
                        const auto& node = frontier[addressToNode[pointedTarget]];

                        std::vector<std::size_t> newOffsets;
                        newOffsets.reserve(node.offsets.size() + 1);
                        newOffsets.push_back(addend);
                        newOffsets.insert(newOffsets.end(), node.offsets.begin(), node.offsets.end());

                        if (location >= moduleBase && location < moduleEnd) {
                            if (report.chains.size() < limits.maxChains) {
                                report.chains.push_back({relativeToModule(location, moduleBase), std::move(newOffsets)});
                            } else {
                                report.truncated = true;
                            }
                        } else if (visited.find(location) == visited.end()) {
                            if (nextFrontier.size() < limits.maxNodesPerLevel) {
                                visited.insert(location);
                                nextFrontier.push_back({location, std::move(newOffsets)});
                            } else {
                                report.truncated = true;
                            }
                        }
                    }
                }

                chunkStart += chunkSize;
                if (report.chains.size() >= limits.maxChains || report.truncated) {
                    break;
                }
            }

            levelBytesScanned += bytesToRead;
            report.bytesScanned += bytesToRead;
        }

        report.nodesExplored += frontier.size();
        if (budgetExhausted || report.chains.size() >= limits.maxChains) {
            break;
        }
        frontier = std::move(nextFrontier);
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    report.elapsedMilliseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    return report;
}

std::optional<std::uintptr_t> resolvePointerChain(
    const std::vector<MemoryRegion>& regions,
    std::uintptr_t moduleBase,
    const PointerChain& chain
) {
    if (chain.offsets.empty()) {
        return std::nullopt;
    }

    std::uintptr_t base = 0;
    if (!addRelative(moduleBase, chain.staticModuleRva, base)) {
        return std::nullopt;
    }

    std::uintptr_t pointer = 0;
    if (!readPointerAt(regions, base, pointer)) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i + 1 < chain.offsets.size(); ++i) {
        if (addWouldOverflow(pointer, chain.offsets[i])) {
            return std::nullopt;
        }
        std::uintptr_t next = 0;
        if (!readPointerAt(regions, pointer + chain.offsets[i], next)) {
            return std::nullopt;
        }
        pointer = next;
    }

    if (addWouldOverflow(pointer, chain.offsets.back())) {
        return std::nullopt;
    }
    return pointer + chain.offsets.back();
}

namespace {

bool readAbsolute(std::uintptr_t address, void* output, std::size_t length) {
    return readCommittedMemory(address, output, length);
}

} // namespace

std::optional<PointerChain> parsePointerChainSpec(const std::string& text) {
    std::string cleaned = text;
    for (auto& ch : cleaned) {
        if (ch == '[' || ch == ']' || ch == '+' || ch == ',' || ch == '=') {
            ch = ' ';
        }
    }

    const auto looksHex = [](const std::string& value) {
        std::size_t i = 0;
        if (i < value.size() && (value[i] == '+' || value[i] == '-')) {
            ++i;
        }
        if (i + 1 < value.size() && value[i] == '0' && (value[i + 1] == 'x' || value[i + 1] == 'X')) {
            i += 2;
        }
        if (i >= value.size()) {
            return false;
        }
        for (; i < value.size(); ++i) {
            if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
                return false;
            }
        }
        return true;
    };

    // Keep only hex tokens, dropping labels like "chain" or "module" so the logged format
    // and a bare "rva off off" line both parse.
    std::istringstream input(cleaned);
    std::string token;
    std::vector<std::string> tokens;
    while (input >> token) {
        if (looksHex(token)) {
            tokens.push_back(token);
        }
    }
    if (tokens.empty()) {
        return std::nullopt;
    }

    PointerChain chain;
    try {
        chain.staticModuleRva = static_cast<std::intptr_t>(std::stoll(tokens[0], nullptr, 16));
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            chain.offsets.push_back(static_cast<std::size_t>(std::stoull(tokens[i], nullptr, 16)));
        }
    } catch (...) {
        return std::nullopt;
    }

    if (chain.offsets.empty()) {
        return std::nullopt;
    }
    return chain;
}

PointerChainVerification verifyPointerChainLive(
    std::uintptr_t moduleBase,
    const PointerChain& chain,
    std::size_t valueSize
) {
    PointerChainVerification verification;
    verification.chain = chain;
    if (chain.offsets.empty()) {
        return verification;
    }

    std::uintptr_t base = 0;
    if (!addRelative(moduleBase, chain.staticModuleRva, base)) {
        return verification;
    }

    std::uintptr_t pointer = 0;
    if (!readAbsolute(base, &pointer, sizeof(pointer))) {
        return verification;
    }
    for (std::size_t i = 0; i + 1 < chain.offsets.size(); ++i) {
        if (addWouldOverflow(pointer, chain.offsets[i])) {
            return verification;
        }
        std::uintptr_t next = 0;
        if (!readAbsolute(pointer + chain.offsets[i], &next, sizeof(next))) {
            return verification;
        }
        pointer = next;
    }
    if (addWouldOverflow(pointer, chain.offsets.back())) {
        return verification;
    }

    verification.resolvedAddress = pointer + chain.offsets.back();
    if (verification.resolvedAddress == 0) {
        return verification;
    }
    verification.resolved = true;
    verification.resolvedModuleRelative = relativeToModule(verification.resolvedAddress, moduleBase);

    if (valueSize == 4 || valueSize == 8) {
        std::uint64_t value = 0;
        if (readAbsolute(verification.resolvedAddress, &value, valueSize)) {
            if (valueSize == 4) {
                value &= std::numeric_limits<std::uint32_t>::max();
            }
            verification.value = value;
            verification.valueRead = true;
        }
    }

    return verification;
}

std::string formatPointerChainVerification(const PointerChainVerification& verification) {
    std::ostringstream out;
    out << "chain=[module" << (verification.chain.staticModuleRva >= 0 ? "+" : "")
        << formatRelativeAddress(verification.chain.staticModuleRva) << "]";
    for (const auto offset : verification.chain.offsets) {
        out << "+0x" << std::hex << offset << std::dec;
    }

    if (!verification.resolved) {
        out << " -> unresolved";
        return out.str();
    }

    out << " -> resolved_rva=" << formatRelativeAddress(verification.resolvedModuleRelative);
    if (verification.valueRead) {
        out << ", value=" << verification.value;
    } else {
        out << ", value=unreadable";
    }
    return out.str();
}

PointerScanReport scanProcessForPointerChains(
    const ModuleInfo& module,
    std::uintptr_t targetAddress,
    PointerScanLimits limits
) {
    bool enumerationTruncated = false;
    const auto regions = enumeratePointerScanRegions(
        module.baseAddress, module.imageSize, limits.maxRegionsToVisit, enumerationTruncated);

    PointerScanSession session;
    auto report = session.findChains(regions, module.baseAddress, module.imageSize, targetAddress, limits);
    report.truncated = report.truncated || enumerationTruncated;
    return report;
}

std::string formatPointerScanReport(const PointerScanReport& report) {
    std::ostringstream out;
    out << "pointer_scan:"
        << " target_rva=" << formatRelativeAddress(report.targetModuleRelativeAddress)
        << ", max_depth=" << report.maxDepth
        << ", levels_explored=" << report.levelsExplored
        << ", nodes_explored=" << report.nodesExplored
        << ", bytes_scanned=" << report.bytesScanned
        << ", chains=" << report.chains.size()
        << ", truncated=" << report.truncated
        << ", elapsed_ms=" << report.elapsedMilliseconds;

    for (const auto& chain : report.chains) {
        out << ", chain=[module" << (chain.staticModuleRva >= 0 ? "+" : "")
            << formatRelativeAddress(chain.staticModuleRva) << "]";
        for (const auto offset : chain.offsets) {
            out << "+0x" << std::hex << offset << std::dec;
        }
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
