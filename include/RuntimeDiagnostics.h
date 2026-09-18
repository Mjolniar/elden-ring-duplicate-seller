#pragma once

#include "MutationRoutineRegistry.h"
#include "ReadOnlyProbe.h"
#include "RuntimeGuards.h"

#include <string>
#include <vector>

namespace erdup {

struct RuntimeDiagnosticReport {
    RuntimeGuardReport guard;
    ReadOnlyProbeReport probe;
    MutationRoutineGateReport mutationGate;
    bool probeExecuted = false;
    bool mutationGateEvaluated = false;
    bool expectedHost = false;
    bool inventoryBlocked = true;
    std::vector<std::string> notes;
};

RuntimeDiagnosticReport buildRuntimeDiagnosticReport(
    const RuntimeGuardReport& guard,
    const ReadOnlyProbeReport* probe,
    const MutationRoutineGateReport* mutationGate = nullptr
);

std::string formatRuntimeDiagnosticReport(const RuntimeDiagnosticReport& report);

} // namespace erdup
