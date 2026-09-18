#pragma once

#include "Config.h"
#include "DuplicateSeller.h"

#include <string>
#include <vector>

namespace erdup {

enum class MutationBlocker {
    DryRunEnabled,
    NoCandidates,
    UnknownSellValue,
    NonPositiveQuantity,
    NonPositiveRuneTotal,
};

struct MutationSafetyReport {
    bool allowed = false;
    std::vector<MutationBlocker> blockers;
};

MutationSafetyReport evaluateMutationSafety(const SellResult& plan, const Config& config);
std::string mutationBlockerReason(MutationBlocker blocker);
std::string formatMutationSafetyReport(const MutationSafetyReport& report);

} // namespace erdup
