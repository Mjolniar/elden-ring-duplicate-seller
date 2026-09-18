#include "ItemMetadata.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

} // namespace

int main() {
    const auto weaponNames = erdup::parseNameTableMetadata(
        "1000 [ERROR]\n"
        "1000000 Dagger\n"
        "1000100 Heavy Dagger\n",
        erdup::ItemCategory::Weapon,
        "EquipParamWeapon"
    );

    if (weaponNames.items.size() != 2) {
        return fail("Expected parser to skip [ERROR] names and keep two weapons.");
    }
    if (weaponNames.items[0].itemId != 1000000 || weaponNames.items[0].name != "Dagger") {
        return fail("Weapon name row parsed incorrectly.");
    }
    if (weaponNames.items[0].category != erdup::ItemCategory::Weapon) {
        return fail("Name table category was not assigned.");
    }

    const auto nonAsciiNames = erdup::parseNameTableMetadata(
        std::string("1000200 ") + "\xC3\x89pee courbee\n",
        erdup::ItemCategory::Weapon,
        "EquipParamWeapon"
    );
    if (nonAsciiNames.items.size() != 1 || nonAsciiNames.items[0].name != std::string("\xC3\x89pee courbee")) {
        return fail("Expected UTF-8 metadata names to be preserved as raw text.");
    }

    std::ostringstream largeTable;
    for (int i = 0; i < 5000; ++i) {
        largeTable << (2000000 + i) << " Synthetic Item " << i << "\n";
    }
    const auto largeNames = erdup::parseNameTableMetadata(
        largeTable.str(),
        erdup::ItemCategory::Armor,
        "EquipParamProtector"
    );
    if (largeNames.items.size() != 5000) {
        return fail("Expected large synthetic name table to parse every row.");
    }
    if (largeNames.items.back().itemId != 2004999 || largeNames.items.back().name != "Synthetic Item 4999") {
        return fail("Large synthetic name table parsed the final row incorrectly.");
    }

    const auto itemInfo = erdup::parseItemInfoMetadata(
        "ItemNames:\n"
        "  AcCeSsOrY:\n"
        "    1000: Crimson Amber Medallion\n"
        "  Goods:\n"
        "    100: Tarnished's Furled Finger\n"
        "  GeM:\n"
        "    200: 'Storm Blade'\n"
        "    201: \"Golden Slam\"\n"
        "  unknown_bucket:\n"
        "    1: Should Not Parse\n"
        "ItemCounts:\n"
        "  accessory:\n"
        "    1000: 1\n",
        "iteminfo.txt"
    );

    if (itemInfo.items.size() != 4) {
        return fail("Expected iteminfo parser to read four known-category names.");
    }
    if (itemInfo.items[0].category != erdup::ItemCategory::Talisman) {
        return fail("Accessory category did not map to talisman.");
    }
    if (itemInfo.items[1].category != erdup::ItemCategory::Consumable) {
        return fail("Goods category did not map to consumable.");
    }
    if (itemInfo.items[2].category != erdup::ItemCategory::AshOfWar) {
        return fail("Gem category did not map to ash of war.");
    }
    if (itemInfo.items[2].name != "Storm Blade" || itemInfo.items[3].name != "Golden Slam") {
        return fail("Iteminfo parser should strip matching YAML-style wrapping quotes from names.");
    }

    const auto category = erdup::itemCategoryFromMetadataToken("EquipParamProtector");
    if (!category.has_value() || category.value() != erdup::ItemCategory::Armor) {
        return fail("EquipParamProtector did not map to armor.");
    }

    const auto mixedCaseCategory = erdup::itemCategoryFromMetadataToken("eQuIpPaRaMgOoDs");
    if (!mixedCaseCategory.has_value() || mixedCaseCategory.value() != erdup::ItemCategory::Consumable) {
        return fail("Metadata category tokens should be parsed case-insensitively.");
    }

    std::cout << "Item metadata parser smoke test passed.\n";
    return EXIT_SUCCESS;
}
