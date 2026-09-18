#include "InventoryFixture.h"

#include "ItemMetadata.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

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

std::optional<int> parseInt(const std::string& value) {
    try {
        return std::stoi(trim(value), nullptr, 0);
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
    if (const auto metadataCategory = itemCategoryFromMetadataToken(value)) {
        return metadataCategory;
    }

    const auto normalized = lower(trim(value));
    if (normalized == "shield") {
        return ItemCategory::Shield;
    }
    if (normalized == "spell") {
        return ItemCategory::Spell;
    }
    if (normalized == "consumable") {
        return ItemCategory::Consumable;
    }
    if (normalized == "material") {
        return ItemCategory::Material;
    }
    if (normalized == "keyitem" || normalized == "key_item" || normalized == "key-item") {
        return ItemCategory::KeyItem;
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

std::string missingFieldMessage(int lineNumber, const std::string& fieldName) {
    return "Skipped fixture item at line " + std::to_string(lineNumber) +
        ": missing or invalid " + fieldName + ".";
}

std::string invalidFieldMessage(int lineNumber, const std::string& fieldName) {
    return "Fixture item at line " + std::to_string(lineNumber) +
        " has invalid " + fieldName + ".";
}

} // namespace

InventoryFixtureResult parseInventoryFixture(const std::string& text) {
    InventoryFixtureResult result;
    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;
    std::set<std::uint32_t> instanceIds;

    while (std::getline(input, line)) {
        ++lineNumber;
        const auto cleaned = trim(line);
        if (cleaned.empty() || startsWith(cleaned, "#") || startsWith(cleaned, ";")) {
            continue;
        }

        if (!startsWith(cleaned, "item=")) {
            result.warnings.push_back("Skipped unrecognized fixture row at line " + std::to_string(lineNumber) + ".");
            continue;
        }

        const auto fields = parseFields(cleaned.substr(5));
        const auto instanceField = fields.find("instance");
        const auto idField = fields.find("id");
        const auto categoryField = fields.find("category");
        if (instanceField == fields.end()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "instance"));
            continue;
        }
        if (idField == fields.end()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "id"));
            continue;
        }
        if (categoryField == fields.end()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "category"));
            continue;
        }

        const auto instanceId = parseUint32(instanceField->second);
        const auto baseItemId = parseUint32(idField->second);
        const auto category = parseCategory(categoryField->second);
        if (!instanceId.has_value()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "instance"));
            continue;
        }
        if (!baseItemId.has_value()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "id"));
            continue;
        }
        if (!category.has_value()) {
            result.warnings.push_back(missingFieldMessage(lineNumber, "category"));
            continue;
        }

        ItemInstance item;
        item.instanceId = instanceId.value();
        item.baseItemId = baseItemId.value();
        item.category = category.value();
        if (!instanceIds.insert(item.instanceId).second) {
            result.warnings.push_back(
                "Fixture item at line " + std::to_string(lineNumber) +
                " reuses instance id " + std::to_string(item.instanceId) + "."
            );
        }

        if (const auto quantity = fields.find("qty"); quantity != fields.end()) {
            if (const auto parsed = parseInt(quantity->second)) {
                item.quantity = parsed.value();
            } else {
                result.warnings.push_back(invalidFieldMessage(lineNumber, "qty"));
            }
        }
        if (const auto quantity = fields.find("quantity"); quantity != fields.end()) {
            if (const auto parsed = parseInt(quantity->second)) {
                item.quantity = parsed.value();
            } else {
                result.warnings.push_back(invalidFieldMessage(lineNumber, "quantity"));
            }
        }
        if (item.quantity <= 0) {
            result.warnings.push_back(
                "Fixture item at line " + std::to_string(lineNumber) +
                " has non-positive quantity " + std::to_string(item.quantity) + "."
            );
        }
        if (const auto upgrade = fields.find("upgrade"); upgrade != fields.end()) {
            if (const auto parsed = parseInt(upgrade->second)) {
                item.upgradeLevel = parsed.value();
            } else {
                result.warnings.push_back(invalidFieldMessage(lineNumber, "upgrade"));
            }
        }
        if (const auto modified = fields.find("modified"); modified != fields.end()) {
            if (const auto parsed = parseBool(modified->second)) {
                item.modified = parsed.value();
            } else {
                result.warnings.push_back(invalidFieldMessage(lineNumber, "modified"));
            }
        }
        if (const auto sell = fields.find("sell"); sell != fields.end()) {
            if (const auto parsed = parseInt(sell->second)) {
                item.sellValue = parsed.value();
            } else {
                result.warnings.push_back(invalidFieldMessage(lineNumber, "sell"));
            }
        }
        if (const auto name = fields.find("name"); name != fields.end()) {
            item.debugName = name->second;
        }

        result.items.push_back(item);
    }

    return result;
}

InventoryFixtureResult loadInventoryFixture(const std::filesystem::path& path) {
    InventoryFixtureResult result;
    std::ifstream file(path);
    if (!file) {
        result.warnings.push_back("Could not open inventory fixture: " + path.string());
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseInventoryFixture(buffer.str());
}

} // namespace erdup
