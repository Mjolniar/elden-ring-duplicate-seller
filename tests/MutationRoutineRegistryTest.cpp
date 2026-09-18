#include "MutationRoutineRegistry.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::MutationRoutineCandidate routine(
    erdup::MutationRoutineKind kind,
    std::string name,
    erdup::MutationRoutineStatus status
) {
    erdup::MutationRoutineCandidate candidate;
    candidate.kind = kind;
    candidate.name = std::move(name);
    candidate.supportedGameVersion = "2.6.2.0";
    candidate.source = "local throwaway-save validation notes";
    candidate.moduleRva = kind == erdup::MutationRoutineKind::RemoveItem ? 0x123456 : 0x654321;
    candidate.status = status;
    return candidate;
}

erdup::MutationRoutineCandidate customRemovalRoutine(
    std::string name,
    erdup::MutationRoutineStatus status
) {
    auto candidate = routine(
        erdup::MutationRoutineKind::RemoveItem,
        std::move(name),
        status
    );
    candidate.removalStrategy = erdup::RemovalStrategy::CustomContainerEdit;
    candidate.moduleRva = 0;
    return candidate;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool hasBlocker(const erdup::MutationRoutineGateReport& report, const std::string& needle) {
    for (const auto& blocker : report.blockers) {
        if (contains(blocker, needle)) {
            return true;
        }
    }
    return false;
}

std::vector<erdup::MutationRoutineCandidate> validatedRoutineSet() {
    return {
        routine(
            erdup::MutationRoutineKind::RemoveItem,
            "validated remove item",
            erdup::MutationRoutineStatus::ThrowawaySaveValidated
        ),
        routine(
            erdup::MutationRoutineKind::AddRunes,
            "validated add runes",
            erdup::MutationRoutineStatus::ThrowawaySaveValidated
        ),
    };
}

int assertDryRunBlocksEvenValidatedRoutines() {
    erdup::Config config;
    config.dryRun = true;

    const auto report = erdup::evaluateMutationRoutineGate(
        validatedRoutineSet(),
        config,
        "2.6.2.0"
    );

    if (report.ready || !report.removeItemReady || !report.addRunesReady ||
        !hasBlocker(report, "dry_run is enabled")) {
        return fail("dry_run should block the routine gate even when candidates are validated.");
    }
    if (!contains(erdup::formatMutationRoutineGateReport(report), "mutation_blockers=[")) {
        return fail("Blocked routine gate report should label mutation blockers explicitly.");
    }

    return EXIT_SUCCESS;
}

int assertValidatedRoutineSetCanPassWhenDryRunDisabled() {
    erdup::Config config;
    config.dryRun = false;

    const auto report = erdup::evaluateMutationRoutineGate(
        validatedRoutineSet(),
        config,
        "2.6.2.0"
    );

    if (!report.ready || !report.removeItemReady || !report.addRunesReady ||
        !report.blockers.empty() || report.selectedRoutineNames.size() != 2) {
        return fail("Both throwaway-save validated routines should pass when dry_run is disabled.");
    }

    const auto formatted = erdup::formatMutationRoutineGateReport(report);
    if (!contains(formatted, "mutation_routines_ready=1") ||
        !contains(formatted, "validated remove item") ||
        !contains(formatted, "validated add runes")) {
        return fail("Formatted routine gate report should include readiness and selected routines.");
    }

    return EXIT_SUCCESS;
}

int assertCandidateOnlyStatusBlocksMutationReadiness() {
    erdup::Config config;
    config.dryRun = false;

    auto routines = validatedRoutineSet();
    routines[0].status = erdup::MutationRoutineStatus::ReadOnlyObserved;

    const auto report = erdup::evaluateMutationRoutineGate(routines, config, "2.6.2.0");
    if (report.ready || report.removeItemReady ||
        !hasBlocker(report, "read_only_observed")) {
        return fail("Read-only observed routine status should not authorize mutation readiness.");
    }

    return EXIT_SUCCESS;
}

int assertVersionMismatchBlocksMutationReadiness() {
    erdup::Config config;
    config.dryRun = false;

    const auto report = erdup::evaluateMutationRoutineGate(
        validatedRoutineSet(),
        config,
        "9.9.9.9"
    );

    if (report.ready ||
        !hasBlocker(report, "observed game version does not match supported_game_version") ||
        !hasBlocker(report, "version mismatch")) {
        return fail("Observed game version mismatch should block routine readiness.");
    }

    return EXIT_SUCCESS;
}

int assertMissingRoutineBlocksMutationReadiness() {
    erdup::Config config;
    config.dryRun = false;

    const std::vector<erdup::MutationRoutineCandidate> routines = {
        routine(
            erdup::MutationRoutineKind::RemoveItem,
            "validated remove item",
            erdup::MutationRoutineStatus::ThrowawaySaveValidated
        ),
    };

    const auto report = erdup::evaluateMutationRoutineGate(routines, config, "2.6.2.0");
    if (report.ready || !report.removeItemReady || report.addRunesReady ||
        !hasBlocker(report, "add_runes routine is missing")) {
        return fail("Missing rune-award routine should block routine readiness.");
    }

    return EXIT_SUCCESS;
}

int assertCandidateBlockerAndMissingLocatorBlockReadiness() {
    erdup::Config config;
    config.dryRun = false;

    auto routines = validatedRoutineSet();
    routines[0].blockers.push_back("signature has not survived relaunch validation");
    routines[1].moduleRva = 0;

    const auto report = erdup::evaluateMutationRoutineGate(routines, config, "2.6.2.0");
    if (report.ready ||
        !hasBlocker(report, "signature has not survived relaunch validation") ||
        !hasBlocker(report, "no signature pattern or module RVA locator")) {
        return fail("Candidate blockers and missing locators should block routine readiness.");
    }

    return EXIT_SUCCESS;
}

int assertCustomRemovalNeedsExplicitValidation() {
    erdup::Config config;
    config.dryRun = false;

    auto routines = validatedRoutineSet();
    routines[0] = customRemovalRoutine(
        "custom container remove",
        erdup::MutationRoutineStatus::ThrowawaySaveValidated
    );

    const auto report = erdup::evaluateMutationRoutineGate(routines, config, "2.6.2.0");
    if (report.ready || report.removeItemReady) {
        return fail("Custom removal should not pass without every custom validation flag.");
    }
    if (!hasBlocker(report, "inventory layout") ||
        !hasBlocker(report, "recognized before/after removal shape") ||
        !hasBlocker(report, "empty-slot sentinel") ||
        !hasBlocker(report, "count/order repair") ||
        !hasBlocker(report, "reload round-trip")) {
        return fail("Custom removal should report every missing validation gate.");
    }
    if (hasBlocker(report, "no signature pattern or module RVA locator")) {
        return fail("Custom removal should not require a game routine locator.");
    }

    return EXIT_SUCCESS;
}

int assertCustomRemovalCanPassWithAllValidation() {
    erdup::Config config;
    config.dryRun = false;

    auto routines = validatedRoutineSet();
    routines[0] = customRemovalRoutine(
        "custom container remove",
        erdup::MutationRoutineStatus::ThrowawaySaveValidated
    );
    routines[0].customRemoval.inventoryLayoutValidated = true;
    routines[0].customRemoval.recognizedRemovalShapeValidated = true;
    routines[0].customRemoval.emptySlotSentinelValidated = true;
    routines[0].customRemoval.countAndOrderRepairValidated = true;
    routines[0].customRemoval.reloadRoundTripValidated = true;

    const auto report = erdup::evaluateMutationRoutineGate(routines, config, "2.6.2.0");
    if (!report.ready || !report.removeItemReady || !report.addRunesReady ||
        !report.blockers.empty()) {
        return fail("Fully validated custom removal should pass the routine gate.");
    }

    return EXIT_SUCCESS;
}

int assertStatusTokensAreStable() {
    if (erdup::mutationRoutineKindToken(erdup::MutationRoutineKind::RemoveItem) != "remove_item" ||
        erdup::mutationRoutineKindToken(erdup::MutationRoutineKind::AddRunes) != "add_runes" ||
        erdup::removalStrategyToken(erdup::RemovalStrategy::GameRoutine) != "game_routine" ||
        erdup::removalStrategyToken(erdup::RemovalStrategy::CustomContainerEdit) !=
            "custom_container_edit" ||
        erdup::mutationRoutineStatusToken(erdup::MutationRoutineStatus::Candidate) != "candidate" ||
        erdup::mutationRoutineStatusToken(erdup::MutationRoutineStatus::ThrowawaySaveValidated) !=
            "throwaway_save_validated") {
        return fail("Routine kind/status tokens changed unexpectedly.");
    }
    if (erdup::isMutationRoutineValidatedForLiveMutation(
            erdup::MutationRoutineStatus::NoOpValidated) ||
        !erdup::isMutationRoutineValidatedForLiveMutation(
            erdup::MutationRoutineStatus::ThrowawaySaveValidated)) {
        return fail("Only throwaway-save validated routines should be live-mutation validated.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertDryRunBlocksEvenValidatedRoutines() != EXIT_SUCCESS ||
        assertValidatedRoutineSetCanPassWhenDryRunDisabled() != EXIT_SUCCESS ||
        assertCandidateOnlyStatusBlocksMutationReadiness() != EXIT_SUCCESS ||
        assertVersionMismatchBlocksMutationReadiness() != EXIT_SUCCESS ||
        assertMissingRoutineBlocksMutationReadiness() != EXIT_SUCCESS ||
        assertCandidateBlockerAndMissingLocatorBlockReadiness() != EXIT_SUCCESS ||
        assertCustomRemovalNeedsExplicitValidation() != EXIT_SUCCESS ||
        assertCustomRemovalCanPassWithAllValidation() != EXIT_SUCCESS ||
        assertStatusTokensAreStable() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Mutation routine registry test passed.\n";
    return EXIT_SUCCESS;
}
