#include "InventoryFixture.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

} // namespace

int main() {
    const auto result = erdup::parseInventoryFixture(
        "# fixture inventory\n"
        "item=instance:1,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:100,name:Debug Sword\n"
        "item=instance:2,id:2000,category:shield,quantity:2,upgrade:0,modified:no,sell:50,name:Debug Shield\n"
        "item=instance:3,id:3000,category:EquipParamAccessory,qty:1,upgrade:0,modified:false,sell:10,name:Debug Talisman\n"
        "item=instance:4,id:4000,category:key_item,qty:1,upgrade:0,modified:false,sell:0,name:Debug Key\n"
        "item=instance:5,id:5000,category:consumable,qty:10,upgrade:0,modified:false,sell:5,name:Debug Consumable\n"
        "item=instance:bad,id:5000,category:weapon\n"
        "not_an_item=true\n"
    );

    if (result.items.size() != 5) {
        return fail("Expected five valid fixture items.");
    }
    if (result.items[0].instanceId != 1 || result.items[0].baseItemId != 1000) {
        return fail("Weapon fixture IDs parsed incorrectly.");
    }
    if (result.items[0].category != erdup::ItemCategory::Weapon || result.items[0].quantity != 1) {
        return fail("Weapon fixture category/quantity parsed incorrectly.");
    }
    if (result.items[1].category != erdup::ItemCategory::Shield || result.items[1].quantity != 2) {
        return fail("Shield fixture category/quantity parsed incorrectly.");
    }
    if (result.items[2].category != erdup::ItemCategory::Talisman) {
        return fail("Metadata category token did not map to talisman.");
    }
    if (result.items[3].category != erdup::ItemCategory::KeyItem) {
        return fail("key_item token did not map to key item.");
    }
    if (result.items[4].category != erdup::ItemCategory::Consumable) {
        return fail("consumable token did not map to consumable.");
    }
    if (result.items[0].debugName != "Debug Sword" || result.items[0].sellValue != 100) {
        return fail("Debug name or sell value parsed incorrectly.");
    }
    if (result.warnings.size() != 2) {
        return fail("Expected warnings for invalid item row and unrecognized row.");
    }

    std::cout << "Inventory fixture parser test passed.\n";
    return EXIT_SUCCESS;
}
