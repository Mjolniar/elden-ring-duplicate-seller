#include "RuntimeDiagnostics.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace erdup {

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isExpectedHost(const RuntimeGuardReport& guard) {
    return lower(guard.processPath.filename().string()) == "eldenring.exe";
}

} // namespace

RuntimeDiagnosticReport buildRuntimeDiagnosticReport(
    const RuntimeGuardReport& guard,
    const ReadOnlyProbeReport* probe,
    const MutationRoutineGateReport* mutationGate
) {
    RuntimeDiagnosticReport report;
    report.guard = guard;
    report.expectedHost = isExpectedHost(guard);

    if (probe != nullptr) {
        report.probe = *probe;
        report.probeExecuted = true;
        report.inventoryBlocked = !probe->inventoryReadReady;
    } else {
        report.inventoryBlocked = true;
        report.notes.push_back("Read-only probe not executed because runtime guards blocked this host.");
    }

    if (mutationGate != nullptr) {
        report.mutationGate = *mutationGate;
        report.mutationGateEvaluated = true;
    }

    if (report.probeExecuted && !report.probe.inventoryReadReady) {
        report.notes.push_back("Inventory decoding remains disabled until verified signatures and layouts are configured.");
    }

    return report;
}

std::string formatRuntimeDiagnosticReport(const RuntimeDiagnosticReport& report) {
    std::ostringstream out;
    out << "process=\"" << report.guard.processPath.string() << "\""
        << ", expected_host=" << report.expectedHost
        << ", runtime_allowed=" << report.guard.allowed
        << ", probe_executed=" << report.probeExecuted
        << ", inventory_blocked=" << report.inventoryBlocked;

    for (const auto& reason : report.guard.reasons) {
        out << ", guard_reason=\"" << reason << "\"";
    }

    if (report.probeExecuted) {
        out << ", module_path=\"" << report.probe.module.path.string() << "\""
            << ", module_base=0x" << std::hex << report.probe.module.baseAddress << std::dec
            << ", module_size=" << report.probe.module.imageSize
            << ", file_version=\"" << report.probe.module.fileVersion << "\""
            << ", supported_game_version=\"" << report.probe.expectedGameVersion << "\""
            << ", game_version_supported=" << report.probe.versionSupported
            << ", inventory_read_ready=" << report.probe.inventoryReadReady;

        for (const auto& warning : report.probe.module.warnings) {
            out << ", module_warning=\"" << warning << "\"";
        }
        for (const auto& blocker : report.probe.blockers) {
            out << ", probe_blocker=\"" << blocker << "\"";
        }
        for (const auto& finding : report.probe.signatures) {
            out << ", signature=\"" << finding.name << "\""
                << ", matches=" << finding.addresses.size();
            for (const auto& error : finding.errors) {
                out << ", signature_error=\"" << error << "\"";
            }
        }
    }

    if (report.mutationGateEvaluated) {
        out << ", " << formatMutationRoutineGateReport(report.mutationGate);
    }

    for (const auto& note : report.notes) {
        out << ", note=\"" << note << "\"";
    }

    return out.str();
}

} // namespace erdup
