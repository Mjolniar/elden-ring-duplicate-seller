#pragma once

#include "Config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

enum class MutationRoutineKind {
    RemoveItem,
    AddRunes,
};

enum class RemovalStrategy {
    GameRoutine,
    CustomContainerEdit,
};

enum class MutationRoutineStatus {
    Unset,
    Candidate,
    SignatureMatched,
    ReadOnlyObserved,
    NoOpValidated,
    ThrowawaySaveValidated,
    Rejected,
};

struct CustomRemovalValidation {
    bool inventoryLayoutValidated = false;
    bool recognizedRemovalShapeValidated = false;
    bool emptySlotSentinelValidated = false;
    bool countAndOrderRepairValidated = false;
    bool reloadRoundTripValidated = false;
};

struct MutationRoutineCandidate {
    MutationRoutineKind kind = MutationRoutineKind::RemoveItem;
    RemovalStrategy removalStrategy = RemovalStrategy::GameRoutine;
    std::string name;
    std::string supportedGameVersion;
    std::string source;
    std::string pattern;
    std::uint64_t moduleRva = 0;
    MutationRoutineStatus status = MutationRoutineStatus::Unset;
    CustomRemovalValidation customRemoval;
    std::vector<std::string> blockers;
};

struct MutationRoutineGateReport {
    bool ready = false;
    bool removeItemReady = false;
    bool addRunesReady = false;
    std::vector<std::string> selectedRoutineNames;
    std::vector<std::string> blockers;
};

std::string mutationRoutineKindToken(MutationRoutineKind kind);
std::string removalStrategyToken(RemovalStrategy strategy);
std::string mutationRoutineStatusToken(MutationRoutineStatus status);
bool isMutationRoutineValidatedForLiveMutation(MutationRoutineStatus status);
bool mutationRoutineRequiresLocator(const MutationRoutineCandidate& candidate);

MutationRoutineGateReport evaluateMutationRoutineGate(
    const std::vector<MutationRoutineCandidate>& candidates,
    const Config& config,
    const std::string& observedGameVersion
);

std::string formatMutationRoutineGateReport(const MutationRoutineGateReport& report);

} // namespace erdup
