#pragma once

#include "ModuleInfo.h"

#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

enum class SignatureStatus {
    Unverified,
    Candidate,
    Verified,
};

struct SignatureSpec {
    std::string name;
    std::string pattern;
    std::size_t expectedMatches = 1;
    std::string version;
    SignatureStatus status = SignatureStatus::Unverified;
};

struct SignatureFinding {
    std::string name;
    std::string version;
    SignatureStatus status = SignatureStatus::Unverified;
    std::vector<std::uintptr_t> addresses;
    std::vector<std::string> errors;
};

struct ReadOnlyProbeReport {
    ModuleInfo module;
    std::vector<SignatureFinding> signatures;
    std::vector<std::string> blockers;
    std::string expectedGameVersion;
    bool versionSupported = false;
    bool inventoryReadReady = false;
};

std::string defaultSupportedGameVersion();
std::vector<SignatureSpec> defaultReadOnlyProbeSignatures();
ReadOnlyProbeReport runReadOnlyProbe(
    const std::vector<SignatureSpec>& signatures,
    const std::string& supportedGameVersion = defaultSupportedGameVersion()
);
ReadOnlyProbeReport runReadOnlyProbeForModule(
    const std::vector<SignatureSpec>& signatures,
    ModuleInfo module,
    const std::string& supportedGameVersion
);
std::string describeReadOnlyProbeReport(const ReadOnlyProbeReport& report);
// Runtime workers require an explicit matching version even when test probes
// intentionally omit the version gate. This does not certify inventory layout.
bool runtimeVersionAllowsMemoryAccess(const ReadOnlyProbeReport& report);

} // namespace erdup
