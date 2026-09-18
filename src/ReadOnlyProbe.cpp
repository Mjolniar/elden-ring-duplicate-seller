#include "ReadOnlyProbe.h"

#include "PatternScanner.h"

#include <sstream>
#include <utility>

namespace erdup {

namespace {

std::string signatureStatusName(SignatureStatus status) {
    switch (status) {
    case SignatureStatus::Unverified:
        return "unverified";
    case SignatureStatus::Candidate:
        return "candidate";
    case SignatureStatus::Verified:
        return "verified";
    }
    return "unknown";
}

} // namespace

std::string defaultSupportedGameVersion() {
    return "2.6.2.0";
}

std::vector<SignatureSpec> defaultReadOnlyProbeSignatures() {
    // Intentionally empty until exact Elden Ring 2.6.2.0 inventory signatures
    // are verified. Unverified byte patterns are worse than no patterns here.
    return {};
}

ReadOnlyProbeReport runReadOnlyProbe(
    const std::vector<SignatureSpec>& signatures,
    const std::string& supportedGameVersion
) {
    return runReadOnlyProbeForModule(signatures, getCurrentProcessModuleInfo(), supportedGameVersion);
}

ReadOnlyProbeReport runReadOnlyProbeForModule(
    const std::vector<SignatureSpec>& signatures,
    ModuleInfo module,
    const std::string& supportedGameVersion
) {
    ReadOnlyProbeReport report;
    report.module = std::move(module);
    report.expectedGameVersion = supportedGameVersion;
    // An empty supportedGameVersion disables ONLY the global version gate. It exists as a
    // test escape hatch so unit tests can exercise signature/readiness paths on a module
    // whose file version isn't 2.6.2.0. It is not a fail-open hole: the production config
    // default is 2.6.2.0 (config/duplicate_seller.ini), and per-signature gating still
    // requires each Verified signature's own version to exactly match the module below.
    report.versionSupported = supportedGameVersion.empty() ||
        report.module.fileVersion == supportedGameVersion;

    if (!report.module.loaded || report.module.baseAddress == 0 || report.module.imageSize == 0) {
        report.blockers.push_back("Process module metadata is incomplete.");
        return report;
    }

    if (!report.versionSupported) {
        report.blockers.push_back(
            "Unsupported eldenring.exe version: " + report.module.fileVersion +
            " (expected " + supportedGameVersion + ")."
        );
        return report;
    }

    if (signatures.empty()) {
        report.blockers.push_back("No verified read-only inventory signatures are configured.");
        return report;
    }

    const auto* base = reinterpret_cast<const std::uint8_t*>(report.module.baseAddress);
    for (const auto& signature : signatures) {
        SignatureFinding finding;
        finding.name = signature.name;
        finding.version = signature.version;
        finding.status = signature.status;

        if (signature.status != SignatureStatus::Verified) {
            finding.errors.push_back(
                "Signature status is " + signatureStatusName(signature.status) + "; only verified signatures are scanned."
            );
            report.signatures.push_back(finding);
            continue;
        }
        if (signature.version.empty()) {
            finding.errors.push_back("Verified signature is missing an exact game version.");
            report.signatures.push_back(finding);
            continue;
        }
        if (signature.version != report.module.fileVersion) {
            finding.errors.push_back(
                "Signature version mismatch: " + signature.version +
                " does not match module file version " + report.module.fileVersion + "."
            );
            report.signatures.push_back(finding);
            continue;
        }

        const auto parsed = parseBytePattern(signature.pattern);
        finding.errors = parsed.errors;
        if (!finding.errors.empty()) {
            report.signatures.push_back(finding);
            continue;
        }

        const auto offsets = findPattern(base, report.module.imageSize, parsed.pattern);
        for (const auto offset : offsets) {
            finding.addresses.push_back(report.module.baseAddress + offset);
        }

        if (finding.addresses.size() != signature.expectedMatches) {
            std::ostringstream error;
            error << "Expected " << signature.expectedMatches << " match(es), found "
                  << finding.addresses.size() << ".";
            finding.errors.push_back(error.str());
        }

        report.signatures.push_back(finding);
    }

    for (const auto& finding : report.signatures) {
        if (!finding.errors.empty()) {
            report.blockers.push_back("Signature unresolved: " + finding.name);
        }
    }

    report.inventoryReadReady = report.blockers.empty();
    return report;
}

bool runtimeVersionAllowsMemoryAccess(const ReadOnlyProbeReport& report) {
    return report.module.loaded && report.module.baseAddress != 0 &&
        report.module.imageSize != 0 && !report.expectedGameVersion.empty() &&
        report.versionSupported && report.module.fileVersion == report.expectedGameVersion;
}

std::string describeReadOnlyProbeReport(const ReadOnlyProbeReport& report) {
    std::ostringstream out;
    out << "module={" << describeModuleInfo(report.module) << "}"
        << ", supported_game_version=\"" << report.expectedGameVersion << "\""
        << ", game_version_supported=" << report.versionSupported
        << ", inventory_read_ready=" << report.inventoryReadReady;

    for (const auto& blocker : report.blockers) {
        out << ", blocker=\"" << blocker << "\"";
    }

    for (const auto& finding : report.signatures) {
        out << ", signature=\"" << finding.name << "\""
            << ", signature_status=\"" << signatureStatusName(finding.status) << "\""
            << ", signature_version=\"" << finding.version << "\""
            << ", matches=" << finding.addresses.size();
        for (const auto& error : finding.errors) {
            out << ", signature_error=\"" << error << "\"";
        }
    }

    return out.str();
}

} // namespace erdup
