#include "MutationRoutineRegistry.h"

#include <algorithm>
#include <sstream>

namespace erdup {

namespace {

void addBlocker(std::vector<std::string>& blockers, std::string blocker) {
    if (std::find(blockers.begin(), blockers.end(), blocker) == blockers.end()) {
        blockers.push_back(std::move(blocker));
    }
}

bool hasLocator(const MutationRoutineCandidate& candidate) {
    return candidate.moduleRva != 0 || !candidate.pattern.empty();
}

std::string candidateLabel(const MutationRoutineCandidate& candidate) {
    if (!candidate.name.empty()) {
        return candidate.name;
    }
    return mutationRoutineKindToken(candidate.kind);
}

void collectCustomRemovalBlockers(
    const MutationRoutineCandidate& candidate,
    std::vector<std::string>* blockers,
    bool& passes
) {
    if (candidate.kind != MutationRoutineKind::RemoveItem ||
        candidate.removalStrategy != RemovalStrategy::CustomContainerEdit) {
        return;
    }

    const auto label = candidateLabel(candidate);
    const auto require = [&](bool condition, const char* description) {
        if (condition) {
            return;
        }
        passes = false;
        if (blockers != nullptr) {
            addBlocker(*blockers, label + " custom removal missing validation: " + description);
        }
    };

    require(candidate.customRemoval.inventoryLayoutValidated, "inventory layout");
    require(candidate.customRemoval.recognizedRemovalShapeValidated, "recognized before/after removal shape");
    require(candidate.customRemoval.emptySlotSentinelValidated, "empty-slot sentinel");
    require(candidate.customRemoval.countAndOrderRepairValidated, "count/order repair");
    require(candidate.customRemoval.reloadRoundTripValidated, "reload round-trip");
}

bool candidatePasses(
    const MutationRoutineCandidate& candidate,
    const Config& config,
    const std::string& observedGameVersion,
    std::vector<std::string>* blockers
) {
    const auto label = candidateLabel(candidate);
    bool passes = true;

    if (!isMutationRoutineValidatedForLiveMutation(candidate.status)) {
        passes = false;
        if (blockers != nullptr) {
            addBlocker(
                *blockers,
                label + " is not throwaway-save validated (status=" +
                    mutationRoutineStatusToken(candidate.status) + ")"
            );
        }
    }
    if (candidate.supportedGameVersion.empty()) {
        passes = false;
        if (blockers != nullptr) {
            addBlocker(*blockers, label + " has no supported game version");
        }
    } else if (candidate.supportedGameVersion != config.supportedGameVersion ||
        candidate.supportedGameVersion != observedGameVersion) {
        passes = false;
        if (blockers != nullptr) {
            addBlocker(
                *blockers,
                label + " version mismatch: candidate=" + candidate.supportedGameVersion +
                    " config=" + config.supportedGameVersion +
                    " observed=" + observedGameVersion
            );
        }
    }
    if (candidate.source.empty()) {
        passes = false;
        if (blockers != nullptr) {
            addBlocker(*blockers, label + " has no documented source");
        }
    }
    if (mutationRoutineRequiresLocator(candidate) && !hasLocator(candidate)) {
        passes = false;
        if (blockers != nullptr) {
            addBlocker(*blockers, label + " has no signature pattern or module RVA locator");
        }
    }
    collectCustomRemovalBlockers(candidate, blockers, passes);
    for (const auto& blocker : candidate.blockers) {
        passes = false;
        if (blockers != nullptr && !blocker.empty()) {
            addBlocker(*blockers, label + " blocker: " + blocker);
        }
    }

    return passes;
}

const MutationRoutineCandidate* findReadyCandidate(
    const std::vector<MutationRoutineCandidate>& candidates,
    MutationRoutineKind kind,
    const Config& config,
    const std::string& observedGameVersion
) {
    for (const auto& candidate : candidates) {
        if (candidate.kind == kind && candidatePasses(candidate, config, observedGameVersion, nullptr)) {
            return &candidate;
        }
    }
    return nullptr;
}

void collectKindBlockers(
    const std::vector<MutationRoutineCandidate>& candidates,
    MutationRoutineKind kind,
    const Config& config,
    const std::string& observedGameVersion,
    std::vector<std::string>& blockers
) {
    bool sawKind = false;
    for (const auto& candidate : candidates) {
        if (candidate.kind == kind) {
            sawKind = true;
            candidatePasses(candidate, config, observedGameVersion, &blockers);
        }
    }

    if (!sawKind) {
        addBlocker(blockers, mutationRoutineKindToken(kind) + " routine is missing");
    }
}

} // namespace

std::string mutationRoutineKindToken(MutationRoutineKind kind) {
    switch (kind) {
    case MutationRoutineKind::RemoveItem:
        return "remove_item";
    case MutationRoutineKind::AddRunes:
        return "add_runes";
    }
    return "unknown";
}

std::string removalStrategyToken(RemovalStrategy strategy) {
    switch (strategy) {
    case RemovalStrategy::GameRoutine:
        return "game_routine";
    case RemovalStrategy::CustomContainerEdit:
        return "custom_container_edit";
    }
    return "unknown";
}

std::string mutationRoutineStatusToken(MutationRoutineStatus status) {
    switch (status) {
    case MutationRoutineStatus::Unset:
        return "unset";
    case MutationRoutineStatus::Candidate:
        return "candidate";
    case MutationRoutineStatus::SignatureMatched:
        return "signature_matched";
    case MutationRoutineStatus::ReadOnlyObserved:
        return "read_only_observed";
    case MutationRoutineStatus::NoOpValidated:
        return "no_op_validated";
    case MutationRoutineStatus::ThrowawaySaveValidated:
        return "throwaway_save_validated";
    case MutationRoutineStatus::Rejected:
        return "rejected";
    }
    return "unknown";
}

bool isMutationRoutineValidatedForLiveMutation(MutationRoutineStatus status) {
    return status == MutationRoutineStatus::ThrowawaySaveValidated;
}

bool mutationRoutineRequiresLocator(const MutationRoutineCandidate& candidate) {
    if (candidate.kind == MutationRoutineKind::RemoveItem &&
        candidate.removalStrategy == RemovalStrategy::CustomContainerEdit) {
        return false;
    }
    return true;
}

MutationRoutineGateReport evaluateMutationRoutineGate(
    const std::vector<MutationRoutineCandidate>& candidates,
    const Config& config,
    const std::string& observedGameVersion
) {
    MutationRoutineGateReport report;

    if (config.dryRun) {
        addBlocker(report.blockers, "dry_run is enabled");
    }
    if (observedGameVersion.empty()) {
        addBlocker(report.blockers, "observed game version is empty");
    } else if (observedGameVersion != config.supportedGameVersion) {
        addBlocker(
            report.blockers,
            "observed game version does not match supported_game_version: observed=" +
                observedGameVersion + " supported=" + config.supportedGameVersion
        );
    }

    const auto* removeItem = findReadyCandidate(
        candidates,
        MutationRoutineKind::RemoveItem,
        config,
        observedGameVersion
    );
    const auto* addRunes = findReadyCandidate(
        candidates,
        MutationRoutineKind::AddRunes,
        config,
        observedGameVersion
    );

    report.removeItemReady = removeItem != nullptr;
    report.addRunesReady = addRunes != nullptr;

    if (removeItem != nullptr) {
        report.selectedRoutineNames.push_back(candidateLabel(*removeItem));
    } else {
        collectKindBlockers(
            candidates,
            MutationRoutineKind::RemoveItem,
            config,
            observedGameVersion,
            report.blockers
        );
    }

    if (addRunes != nullptr) {
        report.selectedRoutineNames.push_back(candidateLabel(*addRunes));
    } else {
        collectKindBlockers(
            candidates,
            MutationRoutineKind::AddRunes,
            config,
            observedGameVersion,
            report.blockers
        );
    }

    report.ready = !config.dryRun &&
        observedGameVersion == config.supportedGameVersion &&
        report.removeItemReady &&
        report.addRunesReady &&
        report.blockers.empty();
    return report;
}

std::string formatMutationRoutineGateReport(const MutationRoutineGateReport& report) {
    std::ostringstream text;
    text << "mutation_routines_ready=" << (report.ready ? 1 : 0)
         << " remove_item_ready=" << (report.removeItemReady ? 1 : 0)
         << " add_runes_ready=" << (report.addRunesReady ? 1 : 0);

    if (!report.selectedRoutineNames.empty()) {
        text << " selected=[";
        for (std::size_t i = 0; i < report.selectedRoutineNames.size(); ++i) {
            text << (i == 0 ? "" : ",") << report.selectedRoutineNames[i];
        }
        text << "]";
    }

    if (!report.blockers.empty()) {
        text << " mutation_blockers=[";
        for (std::size_t i = 0; i < report.blockers.size(); ++i) {
            text << (i == 0 ? "" : "; ") << report.blockers[i];
        }
        text << "]";
    }

    return text.str();
}

} // namespace erdup
