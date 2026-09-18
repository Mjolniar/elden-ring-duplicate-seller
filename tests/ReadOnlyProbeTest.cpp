#include "ReadOnlyProbe.h"

#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool containsBlocker(const erdup::ReadOnlyProbeReport& report, const std::string& blocker) {
    for (const auto& entry : report.blockers) {
        if (entry == blocker) {
            return true;
        }
    }
    return false;
}

bool containsBlockerPrefix(const erdup::ReadOnlyProbeReport& report, const std::string& prefix) {
    for (const auto& entry : report.blockers) {
        if (entry.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    const auto emptyReport = erdup::runReadOnlyProbe({}, "");
    if (emptyReport.inventoryReadReady) {
        return fail("Empty read-only probe signatures should not be inventory-ready.");
    }
    if (!emptyReport.module.loaded || emptyReport.module.baseAddress == 0 || emptyReport.module.imageSize == 0) {
        return fail("Read-only probe did not populate local process module metadata.");
    }
    if (!containsBlocker(emptyReport, "No verified read-only inventory signatures are configured.")) {
        return fail("Empty read-only probe signatures did not report the expected blocker.");
    }

    erdup::ModuleInfo matchingModule;
    matchingModule.loaded = true;
    matchingModule.path = "D:/SteamLibrary/steamapps/common/ELDEN RING/Game/eldenring.exe";
    matchingModule.baseAddress = 0x140000000;
    matchingModule.imageSize = 1024;
    matchingModule.fileVersion = "2.6.2.0";

    const auto invalidPatternReport = erdup::runReadOnlyProbeForModule({
        {"invalid_test_signature", "48 GG 10", 1, "2.6.2.0", erdup::SignatureStatus::Verified},
    }, matchingModule, "2.6.2.0");
    if (invalidPatternReport.inventoryReadReady) {
        return fail("Invalid signature pattern should not be inventory-ready.");
    }
    if (invalidPatternReport.signatures.size() != 1) {
        return fail("Invalid signature pattern should produce exactly one signature finding.");
    }
    if (invalidPatternReport.signatures[0].name != "invalid_test_signature") {
        return fail("Invalid signature finding did not preserve the signature name.");
    }
    if (invalidPatternReport.signatures[0].errors.empty()) {
        return fail("Invalid signature pattern did not surface a parse error.");
    }
    if (!containsBlocker(invalidPatternReport, "Signature unresolved: invalid_test_signature")) {
        return fail("Invalid signature pattern did not report the unresolved signature blocker.");
    }

    const auto unverifiedReport = erdup::runReadOnlyProbeForModule({
        {"unverified_test_signature", "48 8B 10", 1, "2.6.2.0", erdup::SignatureStatus::Unverified},
    }, matchingModule, "2.6.2.0");
    if (unverifiedReport.inventoryReadReady) {
        return fail("Unverified signature should not make the probe inventory-ready.");
    }
    if (!containsBlocker(unverifiedReport, "Signature unresolved: unverified_test_signature")) {
        return fail("Unverified signature should produce an unresolved-signature blocker.");
    }
    if (unverifiedReport.signatures.empty() || unverifiedReport.signatures[0].errors.empty()) {
        return fail("Unverified signature should record a status error.");
    }

    const auto candidateReport = erdup::runReadOnlyProbeForModule({
        {"candidate_test_signature", "48 8B 10", 1, "2.6.2.0", erdup::SignatureStatus::Candidate},
    }, matchingModule, "2.6.2.0");
    if (candidateReport.inventoryReadReady) {
        return fail("Candidate signature should not make the probe inventory-ready.");
    }
    if (!containsBlocker(candidateReport, "Signature unresolved: candidate_test_signature")) {
        return fail("Candidate signature should produce an unresolved-signature blocker.");
    }
    if (candidateReport.signatures.size() != 1 || candidateReport.signatures[0].errors.empty()) {
        return fail("Candidate signature should record a status error and not be scanned.");
    }
    if (!candidateReport.signatures[0].addresses.empty()) {
        return fail("Candidate signature must not be scanned for matches.");
    }

    const auto mismatchedSignatureReport = erdup::runReadOnlyProbeForModule({
        {"wrong_version_test_signature", "48 8B 10", 1, "9.9.9.9", erdup::SignatureStatus::Verified},
    }, matchingModule, "2.6.2.0");
    if (mismatchedSignatureReport.inventoryReadReady) {
        return fail("Version-mismatched signature should not make the probe inventory-ready.");
    }
    if (!containsBlocker(mismatchedSignatureReport, "Signature unresolved: wrong_version_test_signature")) {
        return fail("Version-mismatched signature should produce an unresolved-signature blocker.");
    }

    std::array<std::uint8_t, 4> moduleBytes{0x48, 0x8B, 0x99, 0x10};
    erdup::ModuleInfo syntheticModule = matchingModule;
    syntheticModule.baseAddress = reinterpret_cast<std::uintptr_t>(moduleBytes.data());
    syntheticModule.imageSize = moduleBytes.size();
    const auto verifiedReport = erdup::runReadOnlyProbeForModule({
        {"verified_test_signature", "48 8B ?? 10", 1, "2.6.2.0", erdup::SignatureStatus::Verified},
    }, syntheticModule, "2.6.2.0");
    if (!verifiedReport.inventoryReadReady) {
        return fail("Verified same-version signature should make the synthetic probe inventory-ready.");
    }
    if (verifiedReport.signatures.size() != 1 || verifiedReport.signatures[0].addresses.size() != 1) {
        return fail("Verified same-version signature should resolve one synthetic match.");
    }

    const auto matchingVersionReport = erdup::runReadOnlyProbeForModule({}, matchingModule, "2.6.2.0");
    if (!matchingVersionReport.versionSupported) {
        return fail("Matching game version should be reported as supported.");
    }
    if (containsBlockerPrefix(matchingVersionReport, "Unsupported eldenring.exe version:")) {
        return fail("Matching game version should not produce an unsupported-version blocker.");
    }

    erdup::ModuleInfo mismatchedModule = matchingModule;
    mismatchedModule.fileVersion = "9.9.9.9";
    const auto mismatchedVersionReport = erdup::runReadOnlyProbeForModule({
        {"not_scanned_after_version_mismatch", "??", 1},
    }, mismatchedModule, "2.6.2.0");

    if (mismatchedVersionReport.versionSupported) {
        return fail("Mismatched game version should be reported as unsupported.");
    }
    if (mismatchedVersionReport.inventoryReadReady) {
        return fail("Mismatched game version should force inventory readiness off.");
    }
    if (!containsBlocker(mismatchedVersionReport, "Unsupported eldenring.exe version: 9.9.9.9 (expected 2.6.2.0).")) {
        return fail("Mismatched game version did not report the expected blocker.");
    }
    if (!mismatchedVersionReport.signatures.empty()) {
        return fail("Mismatched game version should block before signature scanning.");
    }

    if (!erdup::runtimeVersionAllowsMemoryAccess(matchingVersionReport) ||
        erdup::runtimeVersionAllowsMemoryAccess(mismatchedVersionReport)) {
        return fail("Runtime memory access must require the validated version.");
    }
    auto updatedModule = matchingModule;
    updatedModule.fileVersion = "2.7.1.0";
    const auto updatedReport = erdup::runReadOnlyProbeForModule({}, updatedModule, "2.6.2.0");
    const auto ungatedReport = erdup::runReadOnlyProbeForModule({}, updatedModule, "");
    if (erdup::runtimeVersionAllowsMemoryAccess(updatedReport) ||
        erdup::runtimeVersionAllowsMemoryAccess(ungatedReport)) {
        return fail("Updated game and empty test version gate must not enable runtime access.");
    }
    auto incompleteReport = matchingVersionReport;
    incompleteReport.module.baseAddress = 0;
    if (erdup::runtimeVersionAllowsMemoryAccess(incompleteReport)) {
        return fail("Incomplete module must block runtime memory access.");
    }

    std::cout << "Read-only probe test passed.\n";
    return EXIT_SUCCESS;
}
