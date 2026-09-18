#include "MutationRoutineLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace erdup {

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::uint64_t parseUint64(const std::string& value, bool& ok) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(trim(value), &consumed, 0);
        ok = consumed == trim(value).size();
        return static_cast<std::uint64_t>(parsed);
    } catch (...) {
        ok = false;
        return 0;
    }
}

bool parseKind(const std::string& value, MutationRoutineKind& kind) {
    const auto token = lower(trim(value));
    if (token == "remove_item" || token == "removeitem") {
        kind = MutationRoutineKind::RemoveItem;
        return true;
    }
    if (token == "add_runes" || token == "addrunes" || token == "award_runes") {
        kind = MutationRoutineKind::AddRunes;
        return true;
    }
    return false;
}

bool parseStatus(const std::string& value, MutationRoutineStatus& status) {
    const auto token = lower(trim(value));
    if (token == "unset") {
        status = MutationRoutineStatus::Unset;
        return true;
    }
    if (token == "candidate") {
        status = MutationRoutineStatus::Candidate;
        return true;
    }
    if (token == "signature_matched") {
        status = MutationRoutineStatus::SignatureMatched;
        return true;
    }
    if (token == "read_only_observed") {
        status = MutationRoutineStatus::ReadOnlyObserved;
        return true;
    }
    if (token == "no_op_validated") {
        status = MutationRoutineStatus::NoOpValidated;
        return true;
    }
    if (token == "throwaway_save_validated") {
        status = MutationRoutineStatus::ThrowawaySaveValidated;
        return true;
    }
    if (token == "rejected") {
        status = MutationRoutineStatus::Rejected;
        return true;
    }
    return false;
}

bool parseRemovalStrategy(const std::string& value, RemovalStrategy& strategy) {
    const auto token = lower(trim(value));
    if (token == "game_routine" || token == "routine" || token == "game") {
        strategy = RemovalStrategy::GameRoutine;
        return true;
    }
    if (token == "custom_container_edit" || token == "custom" || token == "container_edit") {
        strategy = RemovalStrategy::CustomContainerEdit;
        return true;
    }
    return false;
}

bool parseBool(const std::string& value, bool& parsed) {
    const auto token = lower(trim(value));
    if (token == "true" || token == "1" || token == "yes" || token == "on") {
        parsed = true;
        return true;
    }
    if (token == "false" || token == "0" || token == "no" || token == "off") {
        parsed = false;
        return true;
    }
    return false;
}

void finalizeCandidate(
    MutationRoutineLoadResult& result,
    MutationRoutineCandidate& candidate,
    bool& inRoutine,
    int routineStartLine
) {
    if (!inRoutine) {
        return;
    }

    const auto label = candidate.name.empty() ? ("line " + std::to_string(routineStartLine)) : candidate.name;
    if (candidate.name.empty()) {
        result.warnings.push_back("Routine candidate at " + label + " has no name.");
    }
    if (candidate.supportedGameVersion.empty()) {
        result.warnings.push_back("Routine candidate " + label + " has no supported_game_version.");
    }
    if (candidate.source.empty()) {
        result.warnings.push_back("Routine candidate " + label + " has no source.");
    }
    if (mutationRoutineRequiresLocator(candidate) && candidate.pattern.empty() && candidate.moduleRva == 0) {
        result.warnings.push_back("Routine candidate " + label + " has no pattern or module_rva.");
    }
    if (candidate.status == MutationRoutineStatus::ThrowawaySaveValidated) {
        candidate.status = MutationRoutineStatus::NoOpValidated;
        candidate.blockers.push_back("file-loaded throwaway_save_validated status was downgraded pending local confirmation");
        result.warnings.push_back("Routine candidate " + label + " requested throwaway_save_validated; downgraded to no_op_validated.");
    }

    result.candidates.push_back(candidate);
    candidate = {};
    inRoutine = false;
}

} // namespace

MutationRoutineLoadResult parseMutationRoutineCandidates(const std::string& text) {
    MutationRoutineLoadResult result;
    MutationRoutineCandidate current;
    bool inRoutine = false;
    int routineStartLine = 0;

    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line == "[routine]") {
            finalizeCandidate(result, current, inRoutine, routineStartLine);
            inRoutine = true;
            routineStartLine = lineNumber;
            current = {};
            current.status = MutationRoutineStatus::Candidate;
            continue;
        }

        if (!inRoutine) {
            result.warnings.push_back("Ignoring line outside [routine] section: " + std::to_string(lineNumber));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            result.warnings.push_back("Ignoring malformed routine line: " + std::to_string(lineNumber));
            continue;
        }

        const auto key = lower(trim(line.substr(0, separator)));
        const auto value = trim(line.substr(separator + 1));

        if (key == "kind") {
            MutationRoutineKind parsed{};
            if (parseKind(value, parsed)) {
                current.kind = parsed;
            } else {
                result.warnings.push_back("Unknown routine kind at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "removal_strategy" || key == "remove_strategy") {
            RemovalStrategy parsed{};
            if (parseRemovalStrategy(value, parsed)) {
                current.removalStrategy = parsed;
            } else {
                result.warnings.push_back("Unknown removal strategy at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "name") {
            current.name = value;
        } else if (key == "supported_game_version" || key == "version") {
            current.supportedGameVersion = value;
        } else if (key == "source") {
            current.source = value;
        } else if (key == "pattern") {
            current.pattern = value;
        } else if (key == "module_rva" || key == "rva") {
            bool ok = false;
            const auto parsed = parseUint64(value, ok);
            if (ok) {
                current.moduleRva = parsed;
            } else {
                result.warnings.push_back("Invalid module_rva at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "status") {
            MutationRoutineStatus parsed{};
            if (parseStatus(value, parsed)) {
                current.status = parsed;
            } else {
                result.warnings.push_back("Unknown routine status at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "blocker") {
            if (!value.empty()) {
                current.blockers.push_back(value);
            }
        } else if (key == "custom_inventory_layout_validated") {
            if (!parseBool(value, current.customRemoval.inventoryLayoutValidated)) {
                result.warnings.push_back("Invalid boolean at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "custom_recognized_removal_shape_validated") {
            if (!parseBool(value, current.customRemoval.recognizedRemovalShapeValidated)) {
                result.warnings.push_back("Invalid boolean at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "custom_empty_slot_sentinel_validated") {
            if (!parseBool(value, current.customRemoval.emptySlotSentinelValidated)) {
                result.warnings.push_back("Invalid boolean at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "custom_count_order_repair_validated") {
            if (!parseBool(value, current.customRemoval.countAndOrderRepairValidated)) {
                result.warnings.push_back("Invalid boolean at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else if (key == "custom_reload_roundtrip_validated") {
            if (!parseBool(value, current.customRemoval.reloadRoundTripValidated)) {
                result.warnings.push_back("Invalid boolean at line " + std::to_string(lineNumber) + ": " + value);
            }
        } else {
            result.warnings.push_back("Unknown routine key at line " + std::to_string(lineNumber) + ": " + key);
        }
    }

    finalizeCandidate(result, current, inRoutine, routineStartLine);
    return result;
}

MutationRoutineLoadResult loadMutationRoutineCandidates(const std::filesystem::path& path) {
    MutationRoutineLoadResult result;
    std::ifstream file(path);
    if (!file) {
        result.warnings.push_back("Mutation routine candidate file not found: " + path.string());
        return result;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return parseMutationRoutineCandidates(contents.str());
}

} // namespace erdup
