#include "ItemMetadata.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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

bool allDigits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch);
    });
}

bool usableName(const std::string& value) {
    const auto cleaned = trim(value);
    return !cleaned.empty() && !startsWith(cleaned, "[ERROR]");
}

std::string normalizeMetadataName(const std::string& value) {
    auto cleaned = trim(value);
    if (cleaned.size() >= 2) {
        const auto first = cleaned.front();
        const auto last = cleaned.back();
        if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
            cleaned = trim(cleaned.substr(1, cleaned.size() - 2));
        }
    }
    return cleaned;
}

std::optional<std::uint32_t> parseUint32(const std::string& value) {
    const auto cleaned = trim(value);
    if (!allDigits(cleaned)) {
        return std::nullopt;
    }

    try {
        const auto parsed = std::stoull(cleaned);
        if (parsed > UINT32_MAX) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::string readTextFile(const std::filesystem::path& path, MetadataLoadResult& result) {
    std::ifstream file(path);
    if (!file) {
        result.warnings.push_back("Could not open metadata file: " + path.string());
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string sourceOrFileName(const std::filesystem::path& path, const std::string& sourceTable) {
    if (!sourceTable.empty()) {
        return sourceTable;
    }
    return path.filename().string();
}

} // namespace

std::optional<ItemCategory> itemCategoryFromMetadataToken(const std::string& token) {
    const auto value = lower(trim(token));
    if (value == "weapon" || value == "equipparamweapon") {
        return ItemCategory::Weapon;
    }
    if (value == "protector" || value == "armor" || value == "equipparamprotector") {
        return ItemCategory::Armor;
    }
    if (value == "accessory" || value == "talisman" || value == "equipparamaccessory") {
        return ItemCategory::Talisman;
    }
    if (value == "gem" || value == "itemashes" || value == "upgradeashes" || value == "ashofwar" || value == "ash_of_war" || value == "equipparamgem") {
        return ItemCategory::AshOfWar;
    }
    if (value == "goods" || value == "good" || value == "equipparamgoods") {
        return ItemCategory::Consumable;
    }
    return std::nullopt;
}

std::string itemCategoryToken(ItemCategory category) {
    switch (category) {
    case ItemCategory::Weapon: return "weapon";
    case ItemCategory::Shield: return "shield";
    case ItemCategory::Armor: return "armor";
    case ItemCategory::Talisman: return "talisman";
    case ItemCategory::AshOfWar: return "ash_of_war";
    case ItemCategory::Spell: return "spell";
    case ItemCategory::Consumable: return "consumable";
    case ItemCategory::Material: return "material";
    case ItemCategory::KeyItem: return "keyitem";
    case ItemCategory::Unknown: return "unknown";
    }
    return "unknown";
}

MetadataLoadResult parseNameTableMetadata(
    const std::string& text,
    ItemCategory category,
    const std::string& sourceTable
) {
    MetadataLoadResult result;
    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const auto cleaned = trim(line);
        if (cleaned.empty() || startsWith(cleaned, "#")) {
            continue;
        }

        const auto firstSpace = cleaned.find_first_of(" \t");
        if (firstSpace == std::string::npos) {
            result.warnings.push_back("Skipped name row without a name at line " + std::to_string(lineNumber));
            continue;
        }

        const auto id = parseUint32(cleaned.substr(0, firstSpace));
        const auto name = normalizeMetadataName(cleaned.substr(firstSpace + 1));
        if (!id.has_value()) {
            result.warnings.push_back("Skipped name row with invalid ID at line " + std::to_string(lineNumber));
            continue;
        }
        if (!usableName(name)) {
            continue;
        }

        result.items.push_back(ItemMetadata{ id.value(), category, name, sourceTable });
    }

    return result;
}

MetadataLoadResult parseItemInfoMetadata(
    const std::string& text,
    const std::string& sourceTable
) {
    MetadataLoadResult result;
    std::istringstream input(text);
    std::string line;
    bool inItemNames = false;
    std::optional<ItemCategory> currentCategory;
    std::string currentCategoryToken;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        const auto cleaned = trim(line);
        if (cleaned == "ItemNames:") {
            inItemNames = true;
            currentCategory.reset();
            currentCategoryToken.clear();
            continue;
        }

        if (!inItemNames) {
            continue;
        }

        if (!std::isspace(static_cast<unsigned char>(line[0]))) {
            break;
        }

        if (startsWith(line, "  ") && !startsWith(line, "    ") && cleaned.ends_with(':')) {
            currentCategoryToken = cleaned.substr(0, cleaned.size() - 1);
            currentCategory = itemCategoryFromMetadataToken(currentCategoryToken);
            continue;
        }

        if (!currentCategory.has_value() || !startsWith(line, "    ")) {
            continue;
        }

        const auto separator = cleaned.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        const auto id = parseUint32(cleaned.substr(0, separator));
        const auto name = normalizeMetadataName(cleaned.substr(separator + 1));
        if (!id.has_value()) {
            result.warnings.push_back("Skipped iteminfo row with invalid ID at line " + std::to_string(lineNumber));
            continue;
        }
        if (!usableName(name)) {
            continue;
        }

        result.items.push_back(ItemMetadata{
            id.value(),
            currentCategory.value(),
            name,
            sourceTable + ":" + currentCategoryToken
        });
    }

    return result;
}

MetadataLoadResult loadNameTableMetadata(
    const std::filesystem::path& path,
    ItemCategory category,
    const std::string& sourceTable
) {
    MetadataLoadResult result;
    const auto text = readTextFile(path, result);
    if (text.empty()) {
        return result;
    }
    return parseNameTableMetadata(text, category, sourceOrFileName(path, sourceTable));
}

MetadataLoadResult loadItemInfoMetadata(
    const std::filesystem::path& path,
    const std::string& sourceTable
) {
    MetadataLoadResult result;
    const auto text = readTextFile(path, result);
    if (text.empty()) {
        return result;
    }
    return parseItemInfoMetadata(text, sourceOrFileName(path, sourceTable));
}

void appendMetadata(MetadataLoadResult& target, MetadataLoadResult source) {
    target.items.insert(target.items.end(), source.items.begin(), source.items.end());
    target.warnings.insert(target.warnings.end(), source.warnings.begin(), source.warnings.end());
}

MetadataLoadResult loadConfiguredMetadata(const Config& config) {
    MetadataLoadResult result;

    if (!config.metadataItemInfoPath.empty()) {
        appendMetadata(result, loadItemInfoMetadata(config.metadataItemInfoPath));
    }
    if (!config.metadataWeaponNamesPath.empty()) {
        appendMetadata(result, loadNameTableMetadata(
            config.metadataWeaponNamesPath, ItemCategory::Weapon, "EquipParamWeapon"));
    }
    if (!config.metadataProtectorNamesPath.empty()) {
        appendMetadata(result, loadNameTableMetadata(
            config.metadataProtectorNamesPath, ItemCategory::Armor, "EquipParamProtector"));
    }
    if (!config.metadataAccessoryNamesPath.empty()) {
        appendMetadata(result, loadNameTableMetadata(
            config.metadataAccessoryNamesPath, ItemCategory::Talisman, "EquipParamAccessory"));
    }
    if (!config.metadataGoodsNamesPath.empty()) {
        appendMetadata(result, loadNameTableMetadata(
            config.metadataGoodsNamesPath, ItemCategory::Consumable, "EquipParamGoods"));
    }

    return result;
}

void MetadataIndex::add(const MetadataLoadResult& result) {
    add(result.items);
    warnings_.insert(warnings_.end(), result.warnings.begin(), result.warnings.end());
}

void MetadataIndex::add(const std::vector<ItemMetadata>& items) {
    for (const auto& item : items) {
        const auto key = std::make_pair(item.category, item.itemId);
        const auto [it, inserted] = byCategoryAndId_.emplace(key, item);
        if (!inserted) {
            warnings_.push_back(
                "Skipped duplicate metadata entry for " + itemCategoryToken(item.category) +
                " id " + std::to_string(item.itemId) + " from " + item.sourceTable +
                " (kept entry from " + it->second.sourceTable + ")"
            );
            continue;
        }

        byIdFallback_.emplace(item.itemId, item);
    }
}

const ItemMetadata* MetadataIndex::find(ItemCategory category, std::uint32_t itemId) const {
    const auto it = byCategoryAndId_.find({category, itemId});
    return it != byCategoryAndId_.end() ? &it->second : nullptr;
}

const ItemMetadata* MetadataIndex::find(std::uint32_t itemId) const {
    const auto it = byIdFallback_.find(itemId);
    return it != byIdFallback_.end() ? &it->second : nullptr;
}

std::size_t MetadataIndex::size() const {
    return byCategoryAndId_.size();
}

const std::vector<std::string>& MetadataIndex::warnings() const {
    return warnings_;
}

} // namespace erdup
