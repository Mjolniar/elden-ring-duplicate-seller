#include "MutationRoutineResolver.h"

#include "PatternScanner.h"

#include <sstream>

namespace erdup {

namespace {

std::string candidateLabel(const MutationRoutineCandidate& candidate) {
    if (!candidate.name.empty()) {
        return candidate.name;
    }
    return mutationRoutineKindToken(candidate.kind);
}

void addBlocker(std::vector<std::string>& blockers, std::string blocker) {
    blockers.push_back(std::move(blocker));
}

bool moduleMetadataComplete(const ModuleInfo& module) {
    return module.loaded && module.baseAddress != 0 && module.imageSize != 0;
}

bool anyCandidateNeedsModuleBytes(const std::vector<MutationRoutineCandidate>& candidates) {
    for (const auto& candidate : candidates) {
        if (!candidate.pattern.empty()) {
            return true;
        }
    }
    return false;
}

} // namespace

MutationRoutineResolutionReport resolveMutationRoutineCandidatesForModule(
    const std::vector<MutationRoutineCandidate>& candidates,
    const ModuleInfo& module,
    const std::uint8_t* moduleBytes,
    std::size_t moduleSize
) {
    MutationRoutineResolutionReport report;

    if (candidates.empty()) {
        report.blockers.push_back("No mutation routine candidates were provided.");
        return report;
    }
    if (!moduleMetadataComplete(module)) {
        report.blockers.push_back("Process module metadata is incomplete.");
    }
    if (anyCandidateNeedsModuleBytes(candidates) && (moduleBytes == nullptr || moduleSize == 0)) {
        report.blockers.push_back("Module bytes are unavailable for pattern resolution.");
    }

    for (const auto& candidate : candidates) {
        MutationRoutineResolution resolution;
        resolution.candidate = candidate;
        resolution.locatorRequired = mutationRoutineRequiresLocator(candidate);
        const auto label = candidateLabel(candidate);

        resolution.versionMatches = !candidate.supportedGameVersion.empty() &&
            candidate.supportedGameVersion == module.fileVersion;
        if (!resolution.versionMatches) {
            addBlocker(
                resolution.blockers,
                label + " version mismatch or missing version: candidate=" +
                    candidate.supportedGameVersion + " module=" + module.fileVersion
            );
        }
        if (!moduleMetadataComplete(module)) {
            addBlocker(resolution.blockers, label + " cannot resolve without complete module metadata");
        }
        if (candidate.status == MutationRoutineStatus::Rejected) {
            addBlocker(resolution.blockers, label + " is rejected");
        }
        if (resolution.locatorRequired && candidate.pattern.empty() && candidate.moduleRva == 0) {
            addBlocker(resolution.blockers, label + " has no pattern or module RVA locator");
        }

        if (moduleMetadataComplete(module) && candidate.moduleRva != 0) {
            if (candidate.moduleRva < module.imageSize) {
                resolution.rvaInModule = true;
                resolution.addresses.push_back(
                    module.baseAddress + static_cast<std::uintptr_t>(candidate.moduleRva)
                );
            } else {
                addBlocker(resolution.blockers, label + " module_rva is outside the module image");
            }
        }

        if (!candidate.pattern.empty()) {
            const auto parsed = parseBytePattern(candidate.pattern);
            for (const auto& error : parsed.errors) {
                addBlocker(resolution.blockers, label + " pattern error: " + error);
            }
            if (parsed.errors.empty()) {
                if (moduleBytes == nullptr || moduleSize == 0) {
                    addBlocker(resolution.blockers, label + " pattern cannot be scanned without module bytes");
                } else {
                    resolution.patternScanned = true;
                    const auto matches = findPattern(moduleBytes, moduleSize, parsed.pattern);
                    if (matches.empty()) {
                        addBlocker(resolution.blockers, label + " pattern had no matches");
                    }
                    if (matches.size() > 1) {
                        addBlocker(resolution.blockers, label + " pattern is ambiguous");
                    }
                    for (const auto offset : matches) {
                        resolution.addresses.push_back(module.baseAddress + offset);
                    }
                }
            }
        }

        resolution.resolved = resolution.versionMatches &&
            (!resolution.locatorRequired || !resolution.addresses.empty()) &&
            resolution.blockers.empty();
        if (!resolution.resolved) {
            report.blockers.push_back("Mutation routine unresolved: " + label);
        }
        report.resolutions.push_back(std::move(resolution));
    }

    return report;
}

std::string formatMutationRoutineResolution(const MutationRoutineResolution& resolution) {
    std::ostringstream out;
    out << "routine_resolution name=\"" << candidateLabel(resolution.candidate) << "\""
        << ", kind=" << mutationRoutineKindToken(resolution.candidate.kind)
        << ", status=" << mutationRoutineStatusToken(resolution.candidate.status)
        << ", removal_strategy=" << removalStrategyToken(resolution.candidate.removalStrategy)
        << ", version=\"" << resolution.candidate.supportedGameVersion << "\""
        << ", version_matches=" << resolution.versionMatches
        << ", resolved=" << resolution.resolved
        << ", locator_required=" << resolution.locatorRequired
        << ", rva_in_module=" << resolution.rvaInModule
        << ", pattern_scanned=" << resolution.patternScanned
        << ", addresses=" << resolution.addresses.size();

    for (const auto address : resolution.addresses) {
        out << ", address=0x" << std::hex << address << std::dec;
    }
    for (const auto& blocker : resolution.blockers) {
        out << ", blocker=\"" << blocker << "\"";
    }

    return out.str();
}

std::string formatMutationRoutineResolutionReport(const MutationRoutineResolutionReport& report) {
    std::ostringstream out;
    out << "mutation_routine_resolution_count=" << report.resolutions.size()
        << ", blockers=" << report.blockers.size();
    for (const auto& blocker : report.blockers) {
        out << ", blocker=\"" << blocker << "\"";
    }
    return out.str();
}

} // namespace erdup
