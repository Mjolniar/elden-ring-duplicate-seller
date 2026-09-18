#include "MutationRoutineLoader.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool containsWarning(const erdup::MutationRoutineLoadResult& result, const std::string& needle) {
    for (const auto& warning : result.warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::filesystem::path writeTempRoutines(const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() /
        ("erdup_mutation_routines_" + std::to_string(std::rand()) + ".ini");
    std::ofstream file(path, std::ios::trunc);
    file << contents;
    return path;
}

int assertParsesRoutineSections() {
    const auto parsed = erdup::parseMutationRoutineCandidates(
        "# local notes\n"
        "[routine]\n"
        "kind=remove_item\n"
        "name=Candidate remove\n"
        "supported_game_version=2.6.2.0\n"
        "source=local static analysis notes\n"
        "pattern=48 89 5C 24 ??\n"
        "status=read_only_observed\n"
        "blocker=signature has not survived relaunch validation\n"
        "\n"
        "[routine]\n"
        "kind=add_runes\n"
        "name=Candidate runes\n"
        "version=2.6.2.0\n"
        "source=local notes\n"
        "module_rva=0x123456\n"
        "status=candidate\n"
    );

    if (parsed.candidates.size() != 2 || !parsed.warnings.empty()) {
        return fail("Expected two clean routine candidates.");
    }
    if (parsed.candidates[0].kind != erdup::MutationRoutineKind::RemoveItem ||
        parsed.candidates[0].name != "Candidate remove" ||
        parsed.candidates[0].supportedGameVersion != "2.6.2.0" ||
        parsed.candidates[0].source != "local static analysis notes" ||
        parsed.candidates[0].pattern != "48 89 5C 24 ??" ||
        parsed.candidates[0].status != erdup::MutationRoutineStatus::ReadOnlyObserved ||
        parsed.candidates[0].blockers.size() != 1) {
        return fail("First routine candidate did not parse as expected.");
    }
    if (parsed.candidates[1].kind != erdup::MutationRoutineKind::AddRunes ||
        parsed.candidates[1].moduleRva != 0x123456 ||
        parsed.candidates[1].status != erdup::MutationRoutineStatus::Candidate) {
        return fail("Second routine candidate did not parse aliases/RVA as expected.");
    }

    return EXIT_SUCCESS;
}

int assertParsesCustomRemovalValidationKeys() {
    const auto parsed = erdup::parseMutationRoutineCandidates(
        "[routine]\n"
        "kind=remove_item\n"
        "removal_strategy=custom_container_edit\n"
        "name=Custom remove\n"
        "supported_game_version=2.6.2.0\n"
        "source=throwaway-save custom removal notes\n"
        "status=no_op_validated\n"
        "custom_inventory_layout_validated=true\n"
        "custom_recognized_removal_shape_validated=true\n"
        "custom_empty_slot_sentinel_validated=yes\n"
        "custom_count_order_repair_validated=1\n"
        "custom_reload_roundtrip_validated=on\n"
    );

    if (parsed.candidates.size() != 1 || !parsed.warnings.empty()) {
        return fail("Expected custom removal candidate to parse without warnings.");
    }

    const auto& candidate = parsed.candidates[0];
    if (candidate.removalStrategy != erdup::RemovalStrategy::CustomContainerEdit ||
        !candidate.customRemoval.inventoryLayoutValidated ||
        !candidate.customRemoval.recognizedRemovalShapeValidated ||
        !candidate.customRemoval.emptySlotSentinelValidated ||
        !candidate.customRemoval.countAndOrderRepairValidated ||
        !candidate.customRemoval.reloadRoundTripValidated) {
        return fail("Custom removal validation keys did not parse correctly.");
    }

    return EXIT_SUCCESS;
}

int assertFileLoadedValidatedStatusIsDowngraded() {
    const auto parsed = erdup::parseMutationRoutineCandidates(
        "[routine]\n"
        "kind=remove_item\n"
        "name=Too trusted\n"
        "supported_game_version=2.6.2.0\n"
        "source=external paste\n"
        "module_rva=0x10\n"
        "status=throwaway_save_validated\n"
    );

    if (parsed.candidates.size() != 1 ||
        parsed.candidates[0].status != erdup::MutationRoutineStatus::NoOpValidated ||
        parsed.candidates[0].blockers.empty() ||
        !containsWarning(parsed, "downgraded to no_op_validated")) {
        return fail("File-loaded throwaway validation should be downgraded with a blocker.");
    }

    erdup::Config config;
    config.dryRun = false;
    const auto gate = erdup::evaluateMutationRoutineGate(parsed.candidates, config, "2.6.2.0");
    if (gate.ready || gate.removeItemReady) {
        return fail("Downgraded file-loaded routine should not pass the mutation gate.");
    }

    return EXIT_SUCCESS;
}

int assertMalformedLinesWarnConservatively() {
    const auto parsed = erdup::parseMutationRoutineCandidates(
        "kind=remove_item\n"
        "[routine]\n"
        "kind=not_real\n"
        "removal_strategy=not_real\n"
        "name=Broken\n"
        "supported_game_version=2.6.2.0\n"
        "source=notes\n"
        "module_rva=not_hex\n"
        "status=impossible\n"
        "custom_inventory_layout_validated=maybe\n"
        "custom_recognized_removal_shape_validated=maybe\n"
        "unknown_key=value\n"
        "no separator\n"
    );

    if (parsed.candidates.size() != 1 ||
        !containsWarning(parsed, "outside [routine]") ||
        !containsWarning(parsed, "Unknown routine kind") ||
        !containsWarning(parsed, "Unknown removal strategy") ||
        !containsWarning(parsed, "Invalid module_rva") ||
        !containsWarning(parsed, "Unknown routine status") ||
        !containsWarning(parsed, "Invalid boolean") ||
        !containsWarning(parsed, "Unknown routine key") ||
        !containsWarning(parsed, "malformed routine line") ||
        !containsWarning(parsed, "has no pattern or module_rva")) {
        return fail("Malformed routine text should produce conservative warnings.");
    }

    return EXIT_SUCCESS;
}

int assertLoadFromFileAndMissingFile() {
    const auto path = writeTempRoutines(
        "[routine]\n"
        "kind=add_runes\n"
        "name=Loaded from file\n"
        "supported_game_version=2.6.2.0\n"
        "source=temp file\n"
        "module_rva=42\n"
    );

    const auto loaded = erdup::loadMutationRoutineCandidates(path);
    std::filesystem::remove(path);

    if (loaded.candidates.size() != 1 ||
        loaded.candidates[0].kind != erdup::MutationRoutineKind::AddRunes ||
        loaded.candidates[0].moduleRva != 42) {
        return fail("Routine loader should parse candidates from a file.");
    }

    const auto missing = erdup::loadMutationRoutineCandidates(path);
    if (!missing.candidates.empty() || !containsWarning(missing, "file not found")) {
        return fail("Missing routine file should warn and return no candidates.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertParsesRoutineSections() != EXIT_SUCCESS ||
        assertParsesCustomRemovalValidationKeys() != EXIT_SUCCESS ||
        assertFileLoadedValidatedStatusIsDowngraded() != EXIT_SUCCESS ||
        assertMalformedLinesWarnConservatively() != EXIT_SUCCESS ||
        assertLoadFromFileAndMissingFile() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Mutation routine loader test passed.\n";
    return EXIT_SUCCESS;
}
