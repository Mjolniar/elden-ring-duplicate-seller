#include "DuplicateSeller.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t instanceId,
    std::uint32_t baseItemId,
    erdup::ItemCategory category,
    int upgradeLevel,
    int sellValue,
    int quantity = 1
) {
    erdup::ItemInstance instance;
    instance.instanceId = instanceId;
    instance.baseItemId = baseItemId;
    instance.category = category;
    instance.quantity = quantity;
    instance.upgradeLevel = upgradeLevel;
    instance.sellValue = sellValue;
    return instance;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

} // namespace

int main() {
    erdup::Config config;
    config.requireUnupgraded = true;
    config.requireUnmodified = true;

    const std::vector<erdup::ItemInstance> inventory = {
        item(100, 2000, erdup::ItemCategory::Weapon, 0, 100),
        item(101, 2000, erdup::ItemCategory::Weapon, 0, 100),
        item(105, 2000, erdup::ItemCategory::Weapon, 0, 100),
        item(102, 2000, erdup::ItemCategory::Weapon, 3, 100),
        item(103, 3000, erdup::ItemCategory::Armor, 0, 50),
        item(104, 3000, erdup::ItemCategory::Armor, 0, 50),
        item(106, 4000, erdup::ItemCategory::AshOfWar, 0, 40),
        item(107, 4000, erdup::ItemCategory::AshOfWar, 0, 40),
        item(108, 5000, erdup::ItemCategory::Consumable, 0, 10, 50),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);

    if (result.totalItems != 4) {
        return fail("Expected two weapon copies, armor, and ash duplicates to be selected.");
    }

    if (result.totalRunes != 290) {
        return fail("Expected rune total to keep the upgraded weapon and exclude consumables.");
    }

    for (const auto& candidate : result.candidates) {
        if (candidate.item.upgradeLevel != 0) {
            return fail("Highest-upgrade weapon should have been retained.");
        }
        if (candidate.item.category == erdup::ItemCategory::Consumable) {
            return fail("Consumable was selected while sell_consumables=false.");
        }
    }

    erdup::Config stockConfig;
    stockConfig.sellConsumables = true;
    stockConfig.itemStockRules[5000] = 25;
    const auto stockResult = erdup::planDuplicateSale(inventory, stockConfig);
    bool soldConsumableStock = false;
    for (const auto& candidate : stockResult.candidates) {
        if (candidate.item.category == erdup::ItemCategory::Consumable && candidate.quantityToSell == 25) {
            soldConsumableStock = true;
        }
    }
    if (!soldConsumableStock) {
        return fail("Per-item stock rule did not keep the configured consumable count.");
    }

    std::cout << result.message << "\n";
    return EXIT_SUCCESS;
}
