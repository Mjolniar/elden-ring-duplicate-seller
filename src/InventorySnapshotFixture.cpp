#include "InventorySnapshotFixture.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>

namespace erdup {

namespace {

constexpr std::uint32_t kEmptyRawItemId = 0xffffffffu;

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

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::optional<std::uint32_t> parseUint32(const std::string& value) {
    try {
        const auto parsed = std::stoull(trim(value), nullptr, 0);
        if (parsed > UINT32_MAX) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> parseBool(const std::string& value) {
    const auto normalized = lower(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<ItemCategory> parseCategory(const std::string& value) {
    const auto normalized = lower(trim(value));
    if (normalized == "weapon" || normalized == "wep") {
        return ItemCategory::Weapon;
    }
    if (normalized == "shield") {
        return ItemCategory::Shield;
    }
    if (normalized == "armor" || normalized == "protector") {
        return ItemCategory::Armor;
    }
    if (normalized == "talisman" || normalized == "accessory") {
        return ItemCategory::Talisman;
    }
    if (normalized == "ash" || normalized == "ashofwar" ||
        normalized == "ash_of_war" || normalized == "ash-of-war") {
        return ItemCategory::AshOfWar;
    }
    if (normalized == "consumable" || normalized == "good" || normalized == "goods") {
        return ItemCategory::Consumable;
    }
    if (normalized == "material") {
        return ItemCategory::Material;
    }
    if (normalized == "keyitem" || normalized == "key_item" || normalized == "key-item") {
        return ItemCategory::KeyItem;
    }
    if (normalized == "spell") {
        return ItemCategory::Spell;
    }
    if (normalized == "unknown") {
        return ItemCategory::Unknown;
    }
    return std::nullopt;
}

std::map<std::string, std::string> parseFields(const std::string& fieldText) {
    std::map<std::string, std::string> fields;
    std::istringstream input(fieldText);
    std::string token;
    while (std::getline(input, token, ',')) {
        const auto separator = token.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        fields[lower(trim(token.substr(0, separator)))] = trim(token.substr(separator + 1));
    }
    return fields;
}

CustomRemovalSlot emptySlotAt(std::size_t index) {
    CustomRemovalSlot slot;
    slot.index = index;
    slot.rawItemId = kEmptyRawItemId;
    slot.displayIndex = static_cast<std::uint32_t>(index);
    return slot;
}

std::optional<std::string> fieldValue(
    const std::map<std::string, std::string>& fields,
    const std::vector<std::string>& names
) {
    for (const auto& name : names) {
        if (const auto it = fields.find(name); it != fields.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

void appendSlotAtIndex(
    InventorySnapshotFixtureResult& result,
    CustomRemovalSlot slot,
    std::size_t slotIndex,
    int lineNumber
) {
    if (slotIndex < result.slots.size()) {
        result.errors.push_back(
            "Snapshot slot at line " + std::to_string(lineNumber) +
            " has duplicate or out-of-order index " + std::to_string(slotIndex) + "."
        );
        return;
    }

    while (result.slots.size() < slotIndex) {
        const auto missingIndex = result.slots.size();
        result.warnings.push_back(
            "Inserted empty placeholder for missing snapshot slot index " +
            std::to_string(missingIndex) + "."
        );
        result.slots.push_back(emptySlotAt(missingIndex));
    }

    slot.index = slotIndex;
    if (slot.displayIndex == 0 && slot.handle != 0) {
        slot.displayIndex = static_cast<std::uint32_t>(slotIndex);
    }
    result.slots.push_back(slot);
}

void parseSlotRow(
    InventorySnapshotFixtureResult& result,
    const std::string& fieldText,
    int lineNumber
) {
    const auto fields = parseFields(fieldText);
    std::size_t slotIndex = result.slots.size();
    if (const auto indexText = fieldValue(fields, {"index", "slot"})) {
        const auto parsed = parseUint32(indexText.value());
        if (!parsed) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid index."
            );
            return;
        }
        slotIndex = parsed.value();
    }

    const auto empty = fieldValue(fields, {"empty"});
    if (empty) {
        const auto parsed = parseBool(empty.value());
        if (!parsed) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid empty flag."
            );
            return;
        }
        if (parsed.value()) {
            appendSlotAtIndex(result, emptySlotAt(slotIndex), slotIndex, lineNumber);
            return;
        }
    }

    const auto handleText = fieldValue(fields, {"handle", "instance"});
    const auto quantityText = fieldValue(fields, {"qty", "quantity"});
    if (!handleText) {
        result.errors.push_back(
            "Snapshot slot at line " + std::to_string(lineNumber) +
            " is missing handle."
        );
        return;
    }
    if (!quantityText) {
        result.errors.push_back(
            "Snapshot slot at line " + std::to_string(lineNumber) +
            " is missing qty."
        );
        return;
    }

    const auto handle = parseUint32(handleText.value());
    const auto quantity = parseUint32(quantityText.value());
    if (!handle) {
        result.errors.push_back(
            "Snapshot slot at line " + std::to_string(lineNumber) +
            " has invalid handle."
        );
        return;
    }
    if (!quantity) {
        result.errors.push_back(
            "Snapshot slot at line " + std::to_string(lineNumber) +
            " has invalid qty."
        );
        return;
    }

    std::optional<std::uint32_t> rawItemId;
    if (const auto rawText = fieldValue(fields, {"raw", "raw_id", "rawitemid", "raw_item_id"})) {
        rawItemId = parseUint32(rawText.value());
        if (!rawItemId) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid raw item id."
            );
            return;
        }
    } else {
        const auto idText = fieldValue(fields, {"id", "base", "base_id", "baseitemid"});
        const auto categoryText = fieldValue(fields, {"category", "cat"});
        if (!idText || !categoryText) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " needs raw item id or id plus category."
            );
            return;
        }

        const auto baseId = parseUint32(idText.value());
        const auto category = parseCategory(categoryText.value());
        if (!baseId) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid id."
            );
            return;
        }
        if (!category) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid category."
            );
            return;
        }
        rawItemId = rawInventoryItemId(category.value(), baseId.value());
    }

    CustomRemovalSlot slot;
    slot.handle = handle.value();
    slot.rawItemId = rawItemId.value();
    slot.quantity = quantity.value();
    if (const auto displayText = fieldValue(fields, {"display", "display_index"})) {
        const auto display = parseUint32(displayText.value());
        if (!display) {
            result.errors.push_back(
                "Snapshot slot at line " + std::to_string(lineNumber) +
                " has invalid display index."
            );
            return;
        }
        slot.displayIndex = display.value();
    }

    appendSlotAtIndex(result, slot, slotIndex, lineNumber);
}

} // namespace

InventorySnapshotFixtureResult parseInventorySnapshotFixture(const std::string& text) {
    InventorySnapshotFixtureResult result;
    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const auto cleaned = trim(line);
        const auto normalized = lower(cleaned);
        if (cleaned.empty() || startsWith(cleaned, "#") || startsWith(cleaned, ";")) {
            continue;
        }

        if (startsWith(normalized, "count=")) {
            const auto count = parseUint32(cleaned.substr(6));
            if (!count) {
                result.errors.push_back(
                    "Snapshot count at line " + std::to_string(lineNumber) + " is invalid."
                );
                continue;
            }
            result.activeCount = count.value();
            result.activeCountExplicit = true;
            continue;
        }

        if (startsWith(normalized, "slot=")) {
            parseSlotRow(result, cleaned.substr(5), lineNumber);
            continue;
        }

        result.warnings.push_back(
            "Skipped unrecognized snapshot row at line " + std::to_string(lineNumber) + "."
        );
    }

    if (!result.activeCountExplicit) {
        result.activeCount = static_cast<std::uint32_t>(result.slots.size());
    }
    return result;
}

InventorySnapshotFixtureResult loadInventorySnapshotFixture(const std::filesystem::path& path) {
    InventorySnapshotFixtureResult result;
    std::ifstream file(path);
    if (!file) {
        result.errors.push_back("Could not open inventory snapshot fixture: " + path.string());
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseInventorySnapshotFixture(buffer.str());
}

} // namespace erdup
