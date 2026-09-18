#include "RuntimeDiagnostics.h"

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

} // namespace

int main() {
    erdup::RuntimeGuardReport guard;
    guard.allowed = true;
    guard.processPath = "D:/SteamLibrary/steamapps/common/ELDEN RING/Game/eldenring.exe";

    erdup::ReadOnlyProbeReport probe;
    probe.module.loaded = true;
    probe.module.path = guard.processPath;
    probe.module.baseAddress = 0x140000000;
    probe.module.imageSize = 123456;
    probe.module.fileVersion = "2.6.2.0";
    probe.expectedGameVersion = "2.6.2.0";
    probe.versionSupported = true;
    probe.inventoryReadReady = false;
    probe.blockers.push_back("No verified read-only inventory signatures are configured.");

    erdup::MutationRoutineGateReport mutationGate;
    mutationGate.ready = false;
    mutationGate.removeItemReady = false;
    mutationGate.addRunesReady = false;
    mutationGate.blockers.push_back("remove_item routine is missing");
    mutationGate.blockers.push_back("add_runes routine is missing");

    const auto allowedReport = erdup::buildRuntimeDiagnosticReport(guard, &probe, &mutationGate);
    const auto allowedText = erdup::formatRuntimeDiagnosticReport(allowedReport);

    if (!allowedReport.expectedHost || !allowedReport.probeExecuted || !allowedReport.inventoryBlocked ||
        !allowedReport.mutationGateEvaluated) {
        return fail("Allowed diagnostic report did not preserve expected host/probe/block state.");
    }
    if (!contains(allowedText, "expected_host=1") ||
        !contains(allowedText, "runtime_allowed=1") ||
        !contains(allowedText, "probe_executed=1") ||
        !contains(allowedText, "module_base=0x140000000") ||
        !contains(allowedText, "module_size=123456") ||
        !contains(allowedText, "file_version=\"2.6.2.0\"") ||
        !contains(allowedText, "supported_game_version=\"2.6.2.0\"") ||
        !contains(allowedText, "game_version_supported=1") ||
        !contains(allowedText, "inventory_read_ready=0") ||
        !contains(allowedText, "probe_blocker=\"No verified read-only inventory signatures are configured.\"") ||
        !contains(allowedText, "mutation_routines_ready=0") ||
        !contains(allowedText, "remove_item_ready=0") ||
        !contains(allowedText, "add_runes_ready=0") ||
        !contains(allowedText, "remove_item routine is missing") ||
        !contains(allowedText, "add_runes routine is missing")) {
        return fail("Allowed diagnostic report is missing required probe fields.");
    }

    erdup::RuntimeGuardReport blockedGuard;
    blockedGuard.allowed = false;
    blockedGuard.processPath = "C:/Temp/not_eldenring.exe";
    blockedGuard.reasons.push_back("Process is not eldenring.exe: not_eldenring.exe");

    const auto blockedReport = erdup::buildRuntimeDiagnosticReport(blockedGuard, nullptr);
    const auto blockedText = erdup::formatRuntimeDiagnosticReport(blockedReport);

    if (blockedReport.expectedHost || blockedReport.probeExecuted || !blockedReport.inventoryBlocked) {
        return fail("Blocked diagnostic report did not preserve blocked guard state.");
    }
    if (!contains(blockedText, "expected_host=0") ||
        !contains(blockedText, "runtime_allowed=0") ||
        !contains(blockedText, "probe_executed=0") ||
        !contains(blockedText, "inventory_blocked=1") ||
        !contains(blockedText, "guard_reason=\"Process is not eldenring.exe: not_eldenring.exe\"") ||
        !contains(blockedText, "note=\"Read-only probe not executed because runtime guards blocked this host.\"")) {
        return fail("Blocked diagnostic report is missing required guard fields.");
    }

    std::cout << "Runtime diagnostics test passed.\n";
    return EXIT_SUCCESS;
}
