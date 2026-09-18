#include "LiveMutationGate.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace erdup {

namespace {

void addUnique(std::vector<std::string>& values, std::string value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

} // namespace

LiveMutationGateReport evaluateLiveMutationGate(
    const MutationSafetyReport& safety,
    const MutationRoutineGateReport& routines,
    const CustomRemovalPlan& removalPlan
) {
    LiveMutationGateReport report;
    report.policyAllowed = safety.allowed;
    report.routinesReady = routines.ready;
    report.removalPlanValid = removalPlan.valid;
    report.removalPlanWarningsClear = removalPlan.warnings.empty();
    report.removalOperationCount = removalPlan.operations.size();

    if (!safety.allowed) {
        for (const auto blocker : safety.blockers) {
            addUnique(report.blockers, "policy: " + mutationBlockerReason(blocker));
        }
    }
    if (!routines.ready) {
        if (routines.blockers.empty()) {
            addUnique(report.blockers, "routine gate is not ready");
        }
        for (const auto& blocker : routines.blockers) {
            addUnique(report.blockers, "routine: " + blocker);
        }
    }
    if (!removalPlan.valid) {
        if (removalPlan.errors.empty()) {
            addUnique(report.blockers, "removal plan is invalid");
        }
        for (const auto& error : removalPlan.errors) {
            addUnique(report.blockers, "removal plan: " + error);
        }
    }
    if (removalPlan.operations.empty()) {
        addUnique(report.blockers, "removal plan has no operations");
    }
    for (const auto& warning : removalPlan.warnings) {
        addUnique(report.warnings, warning);
        addUnique(report.blockers, "removal plan validation required: " + warning);
    }

    report.ready = report.policyAllowed &&
        report.routinesReady &&
        report.removalPlanValid &&
        report.removalPlanWarningsClear &&
        report.removalOperationCount > 0 &&
        report.blockers.empty();
    return report;
}

std::string formatLiveMutationGateReport(const LiveMutationGateReport& report) {
    std::ostringstream out;
    out << "live_mutation_ready=" << (report.ready ? 1 : 0)
        << " policy_allowed=" << (report.policyAllowed ? 1 : 0)
        << " routines_ready=" << (report.routinesReady ? 1 : 0)
        << " removal_plan_valid=" << (report.removalPlanValid ? 1 : 0)
        << " removal_plan_warnings_clear=" << (report.removalPlanWarningsClear ? 1 : 0)
        << " removal_operations=" << report.removalOperationCount;

    if (!report.blockers.empty()) {
        out << " blockers=[";
        for (std::size_t i = 0; i < report.blockers.size(); ++i) {
            out << (i == 0 ? "" : "; ") << report.blockers[i];
        }
        out << "]";
    }
    if (!report.warnings.empty()) {
        out << " warnings=[";
        for (std::size_t i = 0; i < report.warnings.size(); ++i) {
            out << (i == 0 ? "" : "; ") << report.warnings[i];
        }
        out << "]";
    }

    return out.str();
}

} // namespace erdup
