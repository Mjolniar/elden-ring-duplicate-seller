#pragma once

#include "CustomRemovalPlan.h"
#include "MutationRoutineRegistry.h"
#include "MutationSafety.h"

#include <string>
#include <vector>

namespace erdup {

struct LiveMutationGateReport {
    bool ready = false;
    bool policyAllowed = false;
    bool routinesReady = false;
    bool removalPlanValid = false;
    bool removalPlanWarningsClear = false;
    std::size_t removalOperationCount = 0;
    std::vector<std::string> blockers;
    std::vector<std::string> warnings;
};

LiveMutationGateReport evaluateLiveMutationGate(
    const MutationSafetyReport& safety,
    const MutationRoutineGateReport& routines,
    const CustomRemovalPlan& removalPlan
);

std::string formatLiveMutationGateReport(const LiveMutationGateReport& report);

} // namespace erdup
