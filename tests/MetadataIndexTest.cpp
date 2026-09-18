#include "Config.h"
#include "ItemMetadata.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

std::filesystem::path writeTempFile(const std::string& name, const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::trunc);
    file << contents;
    return path;
}

} // namespace

int main() {
    // --- Index behavior over embedded sample data ---
    erdup::MetadataLoadResult sample;
    erdup::appendMetadata(sample, erdup::parseNameTableMetadata(
        "1000000 Dagger\n"
        "1000000 Dagger Duplicate Row\n",
        erdup::ItemCategory::Weapon,
        "EquipParamWeapon"
    ));
    erdup::appendMetadata(sample, erdup::parseNameTableMetadata(
        "2000000 Round Shield\n",
        erdup::ItemCategory::Shield,
        "EquipParamShield"
    ));
    erdup::appendMetadata(sample, erdup::parseItemInfoMetadata(
        "ItemNames:\n"
        "  goods:\n"
        "    100: Tarnished's Furled Finger\n",
        "iteminfo.txt"
    ));

    erdup::MetadataIndex index;
    index.add(sample);

    if (index.size() != 3) {
        return fail("Expected three unique (category,id) entries after collapsing the duplicate row.");
    }

    const auto* dagger = index.find(erdup::ItemCategory::Weapon, 1000000);
    if (dagger == nullptr || dagger->name != "Dagger") {
        return fail("Expected the first-seen name to win for a duplicate (category,id) entry.");
    }

    bool sawDuplicateWarning = false;
    for (const auto& warning : index.warnings()) {
        if (warning.find("duplicate") != std::string::npos) {
            sawDuplicateWarning = true;
        }
    }
    if (!sawDuplicateWarning) {
        return fail("Expected a warning to be recorded for the duplicate (category,id) entry.");
    }

    // Exact lookup must not match under the wrong category...
    if (index.find(erdup::ItemCategory::Armor, 2000000) != nullptr) {
        return fail("Exact lookup must not resolve a different category for the same ID.");
    }
    // ...but the itemId-only fallback should still resolve it.
    const auto* shieldFallback = index.find(2000000u);
    if (shieldFallback == nullptr || shieldFallback->category != erdup::ItemCategory::Shield) {
        return fail("Expected the itemId-only fallback to resolve the shield entry.");
    }

    const auto* goods = index.find(erdup::ItemCategory::Consumable, 100);
    if (goods == nullptr || goods->name != "Tarnished's Furled Finger") {
        return fail("Expected an exact (category,id) match for the goods entry.");
    }

    if (index.find(erdup::ItemCategory::Weapon, 9999999) != nullptr) {
        return fail("Unknown (category,id) lookup should return nullptr.");
    }
    if (index.find(9999999u) != nullptr) {
        return fail("Unknown itemId fallback lookup should return nullptr.");
    }

    // Identical numeric IDs in different tables are distinct items. Exact
    // lookup must keep them separate, while fallback lookup remains lossy.
    erdup::MetadataIndex crossTableIndex;
    crossTableIndex.add(std::vector<erdup::ItemMetadata>{
        erdup::ItemMetadata{42, erdup::ItemCategory::Weapon, "Table Sword", "EquipParamWeapon"},
        erdup::ItemMetadata{42, erdup::ItemCategory::Armor, "Table Armor", "EquipParamProtector"},
        erdup::ItemMetadata{42, erdup::ItemCategory::Talisman, "Table Talisman", "EquipParamAccessory"},
    });

    if (crossTableIndex.size() != 3) {
        return fail("Expected same numeric ID in different metadata tables to remain distinct.");
    }
    const auto* tableSword = crossTableIndex.find(erdup::ItemCategory::Weapon, 42);
    const auto* tableArmor = crossTableIndex.find(erdup::ItemCategory::Armor, 42);
    const auto* tableTalisman = crossTableIndex.find(erdup::ItemCategory::Talisman, 42);
    if (tableSword == nullptr || tableSword->name != "Table Sword") {
        return fail("Expected exact lookup to resolve the weapon entry for a duplicated numeric ID.");
    }
    if (tableArmor == nullptr || tableArmor->name != "Table Armor") {
        return fail("Expected exact lookup to resolve the armor entry for a duplicated numeric ID.");
    }
    if (tableTalisman == nullptr || tableTalisman->name != "Table Talisman") {
        return fail("Expected exact lookup to resolve the talisman entry for a duplicated numeric ID.");
    }
    const auto* tableFallback = crossTableIndex.find(42u);
    if (tableFallback == nullptr || tableFallback->category != erdup::ItemCategory::Weapon) {
        return fail("Expected itemId-only fallback to keep the first registered table entry.");
    }
    if (!crossTableIndex.warnings().empty()) {
        return fail("Same numeric ID across different metadata tables should not be treated as a duplicate.");
    }

    // --- Config-driven metadata loading, parsed from a real ini file ---
    const auto weaponNamesPath = writeTempFile("erdup_test_weapon_names.txt", "500000 Test Sword\n");
    const auto itemInfoPath = writeTempFile(
        "erdup_test_iteminfo.txt",
        "ItemNames:\n"
        "  accessory:\n"
        "    700000: Test Talisman\n"
    );
    const auto iniPath = writeTempFile(
        "erdup_test_config.ini",
        "metadata_weapon_names_path=" + weaponNamesPath.string() + "\n" +
        "metadata_iteminfo_path=" + itemInfoPath.string() + "\n"
    );

    const auto config = erdup::loadConfig(iniPath);
    if (config.metadataWeaponNamesPath != weaponNamesPath.string()) {
        return fail("Expected metadata_weapon_names_path to be parsed from the config file.");
    }
    if (config.metadataItemInfoPath != itemInfoPath.string()) {
        return fail("Expected metadata_iteminfo_path to be parsed from the config file.");
    }

    const auto loaded = erdup::loadConfiguredMetadata(config);
    if (loaded.items.size() != 2) {
        return fail("Expected the config-driven loader to read both configured metadata sources.");
    }

    erdup::MetadataIndex configuredIndex;
    configuredIndex.add(loaded);
    if (configuredIndex.find(erdup::ItemCategory::Weapon, 500000) == nullptr) {
        return fail("Expected the configured weapon names path to be indexed.");
    }
    if (configuredIndex.find(erdup::ItemCategory::Talisman, 700000) == nullptr) {
        return fail("Expected the configured iteminfo path to be indexed.");
    }

    std::filesystem::remove(weaponNamesPath);
    std::filesystem::remove(itemInfoPath);
    std::filesystem::remove(iniPath);

    // Unset optional paths must not load anything and must not warn or crash.
    const erdup::Config emptyConfig;
    const auto emptyResult = erdup::loadConfiguredMetadata(emptyConfig);
    if (!emptyResult.items.empty() || !emptyResult.warnings.empty()) {
        return fail("Expected no items or warnings when no metadata paths are configured.");
    }

    std::cout << "Metadata index test passed.\n";
    return EXIT_SUCCESS;
}
