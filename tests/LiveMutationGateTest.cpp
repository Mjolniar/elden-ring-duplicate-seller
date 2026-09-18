#include "LiveMutationGate.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool hasBlocker(const erdup::LiveMutationGateReport& report, const std::string& needle) {
    for (const auto& blocker : report.blockers) {
        if (contains(blocker, needle)) {
            return true;
        }
    }
    return false;
}

erdup::MutationSafetyReport allowedSafety() {
    erdup::MutationSafetyReport report;
    report.allowed = true;
    return report;
}

erdup::MutationRoutineGateReport readyRoutines() {
    erdup::MutationRoutineGateReport report;
    report.ready = true;
    report.removeItemReady = true;
    report.addRunesReady = true;
    return report;
}

erdup::CustomRemovalPlan validRemovalPlan() {
    erdup::CustomRemovalPlan plan;
    plan.valid = true;
    erdup::CustomRemovalOperation operation;
    operation.kind = erdup::CustomRemovalOperationKind::ClearSlot;
    operation.index = 1;
    plan.operations.push_back(operation);
    return plan;
}

int assertPolicyBlockersAreReported() {
    erdup::MutationSafetyReport safety;
    safety.allowed = false;
    safety.blockers.push_back(erdup::MutationBlocker::DryRunEnabled);

    const auto report = erdup::evaluateLiveMutationGate(
        safety,
        readyRoutines(),
        validRemovalPlan()
    );

    if (report.ready || report.policyAllowed ||
        !hasBlocker(report, "policy: dry_run is enabled")) {
        return fail("Live gate should report policy blockers.");
    }
    if (!contains(erdup::formatLiveMutationGateReport(report), "policy_allowed=0")) {
        return fail("Formatted live gate should include policy readiness.");
    }

    return EXIT_SUCCESS;
}

int assertRoutineBlockersAreReported() {
    auto routines = readyRoutines();
    routines.ready = false;
    routines.addRunesReady = false;
    routines.blockers.push_back("add_runes routine is missing");

    const auto report = erdup::evaluateLiveMutationGate(
        allowedSafety(),
        routines,
        validRemovalPlan()
    );

    if (report.ready || report.routinesReady ||
        !hasBlocker(report, "routine: add_runes routine is missing")) {
        return fail("Live gate should report routine blockers.");
    }
    if (!contains(erdup::formatLiveMutationGateReport(report), "routines_ready=0")) {
        return fail("Formatted live gate should include routine readiness.");
    }

    return EXIT_SUCCESS;
}

int assertInvalidRemovalPlanBlocksReadiness() {
    auto plan = validRemovalPlan();
    plan.valid = false;
    plan.errors.push_back("removal request did not match an active inventory slot");

    const auto report = erdup::evaluateLiveMutationGate(
        allowedSafety(),
        readyRoutines(),
        plan
    );

    if (report.ready || report.removalPlanValid ||
        !hasBlocker(report, "removal request did not match")) {
        return fail("Live gate should report invalid removal plans.");
    }

    return EXIT_SUCCESS;
}

int assertRemovalWarningsBlockLiveUse() {
    auto plan = validRemovalPlan();
    plan.warnings.push_back("mark-empty/count-decrement mode requires live single-removal validation before live use");

    const auto report = erdup::evaluateLiveMutationGate(
        allowedSafety(),
        readyRoutines(),
        plan
    );

    const auto formatted = erdup::formatLiveMutationGateReport(report);
    if (report.ready || report.removalPlanWarningsClear ||
        !hasBlocker(report, "live single-removal validation") ||
        !contains(formatted, "warnings=[")) {
        return fail("Removal plan warnings should block live mutation readiness.");
    }

    return EXIT_SUCCESS;
}

int assertEmptyOperationPlanBlocksReadiness() {
    auto plan = validRemovalPlan();
    plan.operations.clear();

    const auto report = erdup::evaluateLiveMutationGate(
        allowedSafety(),
        readyRoutines(),
        plan
    );

    if (report.ready || !hasBlocker(report, "removal plan has no operations")) {
        return fail("Live gate should block a plan with no removal operations.");
    }

    return EXIT_SUCCESS;
}

int assertAllClearPasses() {
    const auto report = erdup::evaluateLiveMutationGate(
        allowedSafety(),
        readyRoutines(),
        validRemovalPlan()
    );

    const auto formatted = erdup::formatLiveMutationGateReport(report);
    if (!report.ready || !report.policyAllowed || !report.routinesReady ||
        !report.removalPlanValid || !report.removalPlanWarningsClear ||
        report.removalOperationCount != 1 || !report.blockers.empty()) {
        return fail("Live gate should pass when every component is ready.");
    }
    if (!contains(formatted, "live_mutation_ready=1") ||
        !contains(formatted, "removal_operations=1")) {
        return fail("Formatted live gate should show successful readiness and operation count.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertPolicyBlockersAreReported() != EXIT_SUCCESS ||
        assertRoutineBlockersAreReported() != EXIT_SUCCESS ||
        assertInvalidRemovalPlanBlocksReadiness() != EXIT_SUCCESS ||
        assertRemovalWarningsBlockLiveUse() != EXIT_SUCCESS ||
        assertEmptyOperationPlanBlocksReadiness() != EXIT_SUCCESS ||
        assertAllClearPasses() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Live mutation gate test passed.\n";
    return EXIT_SUCCESS;
}
