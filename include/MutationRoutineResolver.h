#pragma once

#include "ModuleInfo.h"
#include "MutationRoutineRegistry.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

struct MutationRoutineResolution {
    MutationRoutineCandidate candidate;
    bool versionMatches = false;
    bool resolved = false;
    bool locatorRequired = true;
    bool rvaInModule = false;
    bool patternScanned = false;
    std::vector<std::uintptr_t> addresses;
    std::vector<std::string> blockers;
};

struct MutationRoutineResolutionReport {
    std::vector<MutationRoutineResolution> resolutions;
    std::vector<std::string> blockers;
};

MutationRoutineResolutionReport resolveMutationRoutineCandidatesForModule(
    const std::vector<MutationRoutineCandidate>& candidates,
    const ModuleInfo& module,
    const std::uint8_t* moduleBytes,
    std::size_t moduleSize
);

std::string formatMutationRoutineResolution(const MutationRoutineResolution& resolution);
std::string formatMutationRoutineResolutionReport(const MutationRoutineResolutionReport& report);

} // namespace erdup
