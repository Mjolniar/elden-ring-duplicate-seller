#pragma once

#include "MutationRoutineRegistry.h"

#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct MutationRoutineLoadResult {
    std::vector<MutationRoutineCandidate> candidates;
    std::vector<std::string> warnings;
};

MutationRoutineLoadResult parseMutationRoutineCandidates(const std::string& text);
MutationRoutineLoadResult loadMutationRoutineCandidates(const std::filesystem::path& path);

} // namespace erdup
