#include "MutationRoutineResolver.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::ModuleInfo moduleFor(const std::uint8_t* bytes, std::size_t size) {
    erdup::ModuleInfo module;
    module.loaded = true;
    module.path = "D:/SteamLibrary/steamapps/common/ELDEN RING/Game/eldenring.exe";
    module.baseAddress = reinterpret_cast<std::uintptr_t>(bytes);
    module.imageSize = size;
    module.fileVersion = "2.6.2.0";
    return module;
}

erdup::ModuleInfo syntheticModuleWithoutBytes() {
    erdup::ModuleInfo module;
    module.loaded = true;
    module.path = "D:/SteamLibrary/steamapps/common/ELDEN RING/Game/eldenring.exe";
    module.baseAddress = 0x10000000;
    module.imageSize = 4;
    module.fileVersion = "2.6.2.0";
    return module;
}

erdup::MutationRoutineCandidate candidate(
    erdup::MutationRoutineKind kind,
    std::string name
) {
    erdup::MutationRoutineCandidate item;
    item.kind = kind;
    item.name = std::move(name);
    item.supportedGameVersion = "2.6.2.0";
    item.source = "test";
    item.status = erdup::MutationRoutineStatus::Candidate;
    return item;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool hasBlocker(const erdup::MutationRoutineResolution& resolution, const std::string& needle) {
    for (const auto& blocker : resolution.blockers) {
        if (blocker.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int assertRvaInsideModuleResolves() {
    const std::array<std::uint8_t, 8> bytes{0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    auto item = candidate(erdup::MutationRoutineKind::RemoveItem, "RVA candidate");
    item.moduleRva = 4;

    const auto report = erdup::resolveMutationRoutineCandidatesForModule({item}, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());
    if (report.resolutions.size() != 1 || !report.blockers.empty()) {
        return fail("In-module RVA should resolve without report blockers.");
    }
    const auto& resolution = report.resolutions[0];
    if (!resolution.resolved || !resolution.rvaInModule || resolution.addresses.size() != 1 ||
        resolution.addresses[0] != reinterpret_cast<std::uintptr_t>(bytes.data() + 4)) {
        return fail("In-module RVA resolution did not produce the expected address.");
    }

    return EXIT_SUCCESS;
}

int assertPatternResolutionAndAmbiguity() {
    const std::array<std::uint8_t, 9> bytes{0x48, 0x8B, 0x01, 0x10, 0x90, 0x48, 0x8B, 0x02, 0x10};
    auto item = candidate(erdup::MutationRoutineKind::AddRunes, "Pattern candidate");
    item.pattern = "48 8B 01 10";

    const auto report = erdup::resolveMutationRoutineCandidatesForModule({item}, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());
    if (report.resolutions.size() != 1 || !report.blockers.empty() ||
        !report.resolutions[0].resolved || !report.resolutions[0].patternScanned ||
        report.resolutions[0].addresses.size() != 1) {
        return fail("Unique pattern should resolve exactly once.");
    }

    item.pattern = "48 8B ?? 10";
    const auto ambiguous = erdup::resolveMutationRoutineCandidatesForModule({item}, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());
    if (ambiguous.resolutions.size() != 1 || ambiguous.resolutions[0].resolved ||
        !hasBlocker(ambiguous.resolutions[0], "pattern is ambiguous")) {
        return fail("Multiple pattern matches should be marked ambiguous.");
    }

    return EXIT_SUCCESS;
}

int assertBlocksInvalidOrUnsafeCandidates() {
    const std::array<std::uint8_t, 4> bytes{0x48, 0x8B, 0x01, 0x10};
    auto invalidPattern = candidate(erdup::MutationRoutineKind::RemoveItem, "Invalid pattern");
    invalidPattern.pattern = "48 GG";

    auto outsideRva = candidate(erdup::MutationRoutineKind::RemoveItem, "Outside RVA");
    outsideRva.moduleRva = 0x100;

    auto noLocator = candidate(erdup::MutationRoutineKind::AddRunes, "No locator");

    auto wrongVersion = candidate(erdup::MutationRoutineKind::AddRunes, "Wrong version");
    wrongVersion.moduleRva = 1;
    wrongVersion.supportedGameVersion = "9.9.9.9";

    auto rejected = candidate(erdup::MutationRoutineKind::AddRunes, "Rejected");
    rejected.moduleRva = 1;
    rejected.status = erdup::MutationRoutineStatus::Rejected;

    const auto report = erdup::resolveMutationRoutineCandidatesForModule({
        invalidPattern,
        outsideRva,
        noLocator,
        wrongVersion,
        rejected,
    }, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());

    if (report.resolutions.size() != 5 || report.blockers.size() != 5) {
        return fail("Invalid candidates should each produce an unresolved report blocker.");
    }
    if (!hasBlocker(report.resolutions[0], "pattern error") ||
        !hasBlocker(report.resolutions[1], "outside the module image") ||
        !hasBlocker(report.resolutions[2], "no pattern or module RVA locator") ||
        !hasBlocker(report.resolutions[3], "version mismatch") ||
        !hasBlocker(report.resolutions[4], "is rejected")) {
        return fail("Invalid candidate blockers were not reported as expected.");
    }

    return EXIT_SUCCESS;
}

int assertCustomRemovalDoesNotRequireRoutineLocator() {
    auto item = candidate(erdup::MutationRoutineKind::RemoveItem, "Custom remove");
    item.removalStrategy = erdup::RemovalStrategy::CustomContainerEdit;

    const auto report = erdup::resolveMutationRoutineCandidatesForModule(
        {item},
        syntheticModuleWithoutBytes(),
        nullptr,
        0
    );

    if (report.resolutions.size() != 1 || !report.blockers.empty()) {
        return fail("Custom removal without a locator should not produce resolution blockers.");
    }

    const auto& resolution = report.resolutions[0];
    if (!resolution.resolved || resolution.locatorRequired ||
        resolution.rvaInModule || resolution.patternScanned ||
        !resolution.addresses.empty()) {
        return fail("Custom removal should resolve as a no-locator strategy.");
    }

    const auto text = erdup::formatMutationRoutineResolution(resolution);
    if (!contains(text, "removal_strategy=custom_container_edit") ||
        !contains(text, "locator_required=0")) {
        return fail("Custom removal resolution formatting should expose strategy and locator state.");
    }

    return EXIT_SUCCESS;
}

int assertReportFormatting() {
    const std::array<std::uint8_t, 4> bytes{0x90, 0x90, 0x90, 0x90};
    auto item = candidate(erdup::MutationRoutineKind::RemoveItem, "Format candidate");
    item.moduleRva = 2;
    const auto report = erdup::resolveMutationRoutineCandidatesForModule({item}, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());
    const auto text = erdup::formatMutationRoutineResolution(report.resolutions[0]);
    const auto summary = erdup::formatMutationRoutineResolutionReport(report);

    if (!contains(text, "routine_resolution") ||
        !contains(text, "name=\"Format candidate\"") ||
        !contains(text, "resolved=1") ||
        !contains(text, "locator_required=1") ||
        !contains(summary, "mutation_routine_resolution_count=1")) {
        return fail("Resolution formatting should include stable diagnostic fields.");
    }

    return EXIT_SUCCESS;
}

int assertEmptyInputBlocks() {
    const std::array<std::uint8_t, 1> bytes{0};
    const auto report = erdup::resolveMutationRoutineCandidatesForModule({}, moduleFor(bytes.data(), bytes.size()), bytes.data(), bytes.size());
    if (!report.resolutions.empty() || report.blockers.size() != 1 ||
        report.blockers[0] != "No mutation routine candidates were provided.") {
        return fail("Empty candidate input should produce the expected report blocker.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertRvaInsideModuleResolves() != EXIT_SUCCESS ||
        assertPatternResolutionAndAmbiguity() != EXIT_SUCCESS ||
        assertBlocksInvalidOrUnsafeCandidates() != EXIT_SUCCESS ||
        assertCustomRemovalDoesNotRequireRoutineLocator() != EXIT_SUCCESS ||
        assertReportFormatting() != EXIT_SUCCESS ||
        assertEmptyInputBlocks() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Mutation routine resolver test passed.\n";
    return EXIT_SUCCESS;
}
