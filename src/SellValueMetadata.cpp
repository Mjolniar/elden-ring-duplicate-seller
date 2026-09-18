#include "SellValueMetadata.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
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

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string stripInlineComment(std::string line) {
    const auto comment = line.find_first_of("#;");
    if (comment != std::string::npos) {
        line = line.substr(0, comment);
    }
    return trim(line);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<ItemCategory> parseCategoryToken(const std::string& token) {
    const auto value = lower(trim(token));
    if (value == "weapon" || value == "equipparamweapon") {
        return ItemCategory::Weapon;
    }
    if (value == "shield") {
        return ItemCategory::Shield;
    }
    if (value == "armor" || value == "protector" || value == "equipparamprotector") {
        return ItemCategory::Armor;
    }
    if (value == "talisman" || value == "accessory" || value == "equipparamaccessory") {
        return ItemCategory::Talisman;
    }
    if (value == "ash" || value == "ashofwar" || value == "ash_of_war" ||
        value == "gem" || value == "equipparamgem") {
        return ItemCategory::AshOfWar;
    }
    if (value == "consumable" || value == "good" || value == "goods" || value == "equipparamgoods") {
        return ItemCategory::Consumable;
    }
    if (value == "material") {
        return ItemCategory::Material;
    }
    if (value == "keyitem" || value == "key_item") {
        return ItemCategory::KeyItem;
    }
    if (value == "spell") {
        return ItemCategory::Spell;
    }
    if (value == "unknown") {
        return ItemCategory::Unknown;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> parseUint32AutoBase(const std::string& value) {
    const auto cleaned = trim(value);
    if (cleaned.empty() || startsWith(cleaned, "-")) {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(cleaned, &consumed, 0);
        if (consumed != cleaned.size() || parsed > UINT32_MAX) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> parseNonNegativeIntAutoBase(const std::string& value) {
    const auto cleaned = trim(value);
    // Explicit metadata distinguishes a known unsellable item from missing prices.
    if (lower(cleaned) == "unsellable") return -1;
    if (cleaned.empty() || startsWith(cleaned, "-") || startsWith(cleaned, "[ERROR]")) {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoll(cleaned, &consumed, 0);
        if (consumed != cleaned.size() || parsed < 0 || parsed > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::string sourcePrefix(const std::string& sourceName) {
    return sourceName.empty() ? std::string{} : sourceName + ": ";
}

} // namespace

SellValueLoadResult parseSellValueMetadata(
    const std::string& text,
    const std::string& sourceName
) {
    SellValueLoadResult result;
    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const auto cleaned = stripInlineComment(line);
        if (cleaned.empty() || startsWith(cleaned, "[ERROR]")) {
            continue;
        }

        std::istringstream row(cleaned);
        std::string firstToken;
        std::string idToken;
        std::string valueToken;
        row >> firstToken >> idToken >> valueToken;
        if (firstToken.empty() || idToken.empty()) {
            result.warnings.push_back(
                sourcePrefix(sourceName) +
                "Skipped sell-value row without id and value at line " +
                std::to_string(lineNumber)
            );
            continue;
        }

        std::optional<ItemCategory> category;
        if (valueToken.empty()) {
            valueToken = idToken;
            idToken = firstToken;
        } else {
            category = parseCategoryToken(firstToken);
            if (!category.has_value()) {
                result.warnings.push_back(
                    sourcePrefix(sourceName) +
                    "Skipped sell-value row with invalid category at line " +
                    std::to_string(lineNumber)
                );
                continue;
            }
        }

        const auto itemId = parseUint32AutoBase(idToken);
        if (!itemId.has_value()) {
            result.warnings.push_back(
                sourcePrefix(sourceName) +
                "Skipped sell-value row with invalid ID at line " +
                std::to_string(lineNumber)
            );
            continue;
        }

        const auto sellValue = parseNonNegativeIntAutoBase(valueToken);
        if (!sellValue.has_value()) {
            if (startsWith(trim(valueToken), "[ERROR]")) {
                continue;
            }
            result.warnings.push_back(
                sourcePrefix(sourceName) +
                "Skipped sell-value row with invalid value at line " +
                std::to_string(lineNumber)
            );
            continue;
        }

        if (category.has_value()) {
            result.categorizedValues[{category.value(), itemId.value()}] = sellValue.value();
        } else {
            result.values[itemId.value()] = sellValue.value();
        }
    }

    return result;
}

SellValueLoadResult loadSellValueMetadata(const std::filesystem::path& path) {
    SellValueLoadResult result;
    std::ifstream file(path);
    if (!file) {
        result.warnings.push_back("Could not open sell-value file: " + path.string());
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseSellValueMetadata(buffer.str(), path.filename().string());
}

SellValueLoadResult loadConfiguredSellValues(const Config& config) {
    if (config.sellValuePath.empty()) {
        return {};
    }
    return loadSellValueMetadata(config.sellValuePath);
}

void SellValueIndex::add(const SellValueLoadResult& result) {
    add(result.values);
    add(result.categorizedValues);
    warnings_.insert(warnings_.end(), result.warnings.begin(), result.warnings.end());
}

void SellValueIndex::add(const std::map<std::uint32_t, int>& values) {
    for (const auto& [itemId, value] : values) {
        values_[itemId] = value;
    }
}

void SellValueIndex::add(const std::map<std::pair<ItemCategory, std::uint32_t>, int>& values) {
    for (const auto& [key, value] : values) {
        categorizedValues_[key] = value;
    }
}

std::optional<int> SellValueIndex::find(ItemCategory category, std::uint32_t itemId) const {
    if (const auto it = categorizedValues_.find({category, itemId}); it != categorizedValues_.end()) {
        return it->second;
    }
    return find(itemId);
}

std::optional<int> SellValueIndex::find(std::uint32_t itemId) const {
    const auto it = values_.find(itemId);
    if (it == values_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t SellValueIndex::size() const {
    return values_.size() + categorizedValues_.size();
}

const std::vector<std::string>& SellValueIndex::warnings() const {
    return warnings_;
}

} // namespace erdup
