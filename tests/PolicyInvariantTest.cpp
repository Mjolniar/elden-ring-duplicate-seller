#include "DuplicateSeller.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t instanceId,
    std::uint32_t baseItemId,
    erdup::ItemCategory category,
    int quantity,
    int sellValue,
    int upgradeLevel = 0,
    bool modified = false
) {
    erdup::ItemInstance instance;
    instance.instanceId = instanceId;
    instance.baseItemId = baseItemId;
    instance.category = category;
    instance.quantity = quantity;
    instance.sellValue = sellValue;
    instance.upgradeLevel = upgradeLevel;
    instance.modified = modified;
    return instance;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

int soldQuantityFor(const erdup::SellResult& result, std::uint32_t baseItemId) {
    int quantity = 0;
    for (const auto& candidate : result.candidates) {
        if (candidate.item.baseItemId == baseItemId) {
            quantity += candidate.quantityToSell;
        }
    }
    return quantity;
}

bool hasCandidateWithCategory(const erdup::SellResult& result, erdup::ItemCategory category) {
    for (const auto& candidate : result.candidates) {
        if (candidate.item.category == category) {
            return true;
        }
    }
    return false;
}

const erdup::SellCandidate* candidateByInstance(const erdup::SellResult& result, std::uint32_t instanceId) {
    for (const auto& candidate : result.candidates) {
        if (candidate.item.instanceId == instanceId) {
            return &candidate;
        }
    }
    return nullptr;
}

int assertSoldPlusKeptInvariant() {
    erdup::Config config;
    const std::vector<erdup::ItemInstance> inventory = {
        item(3, 1000, erdup::ItemCategory::Weapon, 1, 100),
        item(1, 1000, erdup::ItemCategory::Weapon, 2, 100),
        item(2, 1000, erdup::ItemCategory::Weapon, 3, 100),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    const int originalQuantity = 1 + 2 + 3;
    const int soldQuantity = soldQuantityFor(result, 1000);
    const int keptQuantity = originalQuantity - soldQuantity;

    if (soldQuantity != 4 || keptQuantity != config.keepWeapons) {
        return fail("Weapon group should sell only quantity above the configured keep count.");
    }
    for (const auto& candidate : result.candidates) {
        if (candidate.quantityToSell <= 0 || candidate.quantityToSell > candidate.item.quantity) {
            return fail("Candidate quantity violates sold + kept == original for an item stack.");
        }
    }

    return EXIT_SUCCESS;
}

int assertWeaponGroupKeepsHighestUpgradeInvariant() {
    erdup::Config config;
    const std::vector<erdup::ItemInstance> inventory = {
        item(1, 2000000, erdup::ItemCategory::Weapon, 1, 100, 0, false),
        item(2, 2000003, erdup::ItemCategory::Weapon, 1, 100, 3, false),
        item(3, 2000005, erdup::ItemCategory::Weapon, 1, 100, 5, false),
        item(4, 2000100, erdup::ItemCategory::Weapon, 1, 100, 0, true),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (result.totalItems != 2 ||
        soldQuantityFor(result, 2000000) != 1 ||
        soldQuantityFor(result, 2000100) != 1 ||
        soldQuantityFor(result, 2000003) != 0 ||
        soldQuantityFor(result, 2000005) != 0) {
        return fail("Weapon-family group should keep the highest two upgrades and sell the rest.");
    }
    if (candidateByInstance(result, 2) != nullptr || candidateByInstance(result, 3) != nullptr) {
        return fail("Higher-upgrade weapons should be retained ahead of lower-priority variants.");
    }

    return EXIT_SUCCESS;
}

int assertWeaponGroupCanSellUpgradedExcessInvariant() {
    erdup::Config config;
    const std::vector<erdup::ItemInstance> inventory = {
        item(25, 2000025, erdup::ItemCategory::Weapon, 1, 100, 25, false),
        item(20, 2000020, erdup::ItemCategory::Weapon, 1, 100, 20, false),
        item(10, 2000010, erdup::ItemCategory::Weapon, 1, 100, 10, false),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (result.totalItems != 1 || result.candidates.size() != 1) {
        return fail("Three same-base upgraded weapons should produce exactly one sale candidate.");
    }
    const auto* sold = candidateByInstance(result, 10);
    if (sold == nullptr || sold->item.upgradeLevel != 10 || sold->quantityToSell != 1) {
        return fail("The lowest-upgrade excess weapon should be selected for sale.");
    }
    if (candidateByInstance(result, 25) != nullptr || candidateByInstance(result, 20) != nullptr) {
        return fail("The highest two upgraded weapons should be retained.");
    }

    return EXIT_SUCCESS;
}

int assertWeaponGroupCanSellModifiedExcessInvariant() {
    erdup::Config config;
    const std::vector<erdup::ItemInstance> inventory = {
        item(25, 1150025, erdup::ItemCategory::Weapon, 1, 100, 25, false),
        item(20, 1150020, erdup::ItemCategory::Weapon, 1, 100, 20, false),
        item(100, 1150100, erdup::ItemCategory::Weapon, 1, 100, 0, true),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (result.totalItems != 1 || result.candidates.size() != 1) {
        return fail("Two upgraded weapons plus a same-base affinity variant should produce one sale candidate.");
    }
    const auto* sold = candidateByInstance(result, 100);
    if (sold == nullptr || !sold->item.modified || sold->item.baseItemId != 1150100) {
        return fail("The lower-priority affinity variant should be selected for sale.");
    }
    if (candidateByInstance(result, 25) != nullptr || candidateByInstance(result, 20) != nullptr) {
        return fail("The top upgraded weapons should be retained over the affinity variant.");
    }

    return EXIT_SUCCESS;
}

int assertDisabledCategoriesInvariant() {
    erdup::Config config;
    config.sellWeapons = false;
    config.sellShields = false;
    config.sellArmor = false;
    config.sellTalismans = false;
    config.sellAshesOfWar = false;
    config.sellSpells = false;
    config.sellConsumables = false;

    const std::vector<erdup::ItemInstance> inventory = {
        item(1, 3000, erdup::ItemCategory::Weapon, 3, 100),
        item(2, 3100, erdup::ItemCategory::Shield, 2, 60),
        item(3, 3200, erdup::ItemCategory::Armor, 2, 50),
        item(4, 3300, erdup::ItemCategory::Talisman, 2, 25),
        item(5, 3400, erdup::ItemCategory::AshOfWar, 2, 40),
        item(6, 3500, erdup::ItemCategory::Spell, 2, 10),
        item(7, 3600, erdup::ItemCategory::Consumable, 99, 1),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (!result.candidates.empty() || result.totalItems != 0 || result.totalRunes != 0) {
        return fail("Disabled categories should never appear in sale candidates.");
    }
    for (const auto category : {
        erdup::ItemCategory::Weapon,
        erdup::ItemCategory::Shield,
        erdup::ItemCategory::Armor,
        erdup::ItemCategory::Talisman,
        erdup::ItemCategory::AshOfWar,
        erdup::ItemCategory::Spell,
        erdup::ItemCategory::Consumable,
    }) {
        if (hasCandidateWithCategory(result, category)) {
            return fail("Disabled category appeared in a candidate.");
        }
    }

    return EXIT_SUCCESS;
}

int assertMultipleStockRuleInvariant() {
    erdup::Config config;
    config.sellConsumables = true;
    config.itemStockRules[9000] = 25;
    config.itemStockRules[9001] = 5;
    config.itemStockRules[9002] = 10;

    const std::vector<erdup::ItemInstance> inventory = {
        item(1, 9000, erdup::ItemCategory::Consumable, 40, 3),
        item(2, 9001, erdup::ItemCategory::Consumable, 7, 4),
        item(3, 9002, erdup::ItemCategory::Consumable, 3, 5),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (soldQuantityFor(result, 9000) != 15 || soldQuantityFor(result, 9001) != 2 ||
        soldQuantityFor(result, 9002) != 0) {
        return fail("Multiple stock rules did not independently keep configured quantities.");
    }
    if (result.totalItems != 17 || result.totalRunes != 53) {
        return fail("Multiple stock rules produced unexpected totals.");
    }

    return EXIT_SUCCESS;
}

int assertLiveDecodedHardExclusionsInvariant() {
    erdup::Config config;
    config.itemStockRules[52000000] = 2;

    const std::vector<erdup::ItemInstance> inventory = {
        item(1, 110000, erdup::ItemCategory::Weapon, 3, 0),
        item(2, 50020000, erdup::ItemCategory::Weapon, 5, 1),
        item(3, 51040000, erdup::ItemCategory::Weapon, 4, 1),
        item(4, 52000000, erdup::ItemCategory::Weapon, 25, 1),
    };

    const auto result = erdup::planDuplicateSale(inventory, config);
    if (!result.candidates.empty() || result.totalItems != 0 || result.totalRunes != 0) {
        return fail("Live-decoded ammunition and sentinel weapon rows should be hard-excluded.");
    }
    if (erdup::classifySaleEligibility(inventory[0], config) != erdup::SaleEligibility::SentinelItem) {
        return fail("Unarmed sentinel item should have sentinel exclusion eligibility.");
    }
    for (std::size_t i = 1; i < inventory.size(); ++i) {
        if (erdup::classifySaleEligibility(inventory[i], config) != erdup::SaleEligibility::Ammunition) {
            return fail("Ammunition weapon-param item should have ammunition exclusion eligibility.");
        }
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    {
        erdup::Config config;
        const auto plan = erdup::planDuplicateSale({
            item(1, 17030000, erdup::ItemCategory::Weapon, 1, -1),
            item(2, 17030000, erdup::ItemCategory::Weapon, 1, -1),
            item(3, 17030000, erdup::ItemCategory::Weapon, 1, -1),
            item(4, 2000000, erdup::ItemCategory::Weapon, 1, 100),
            item(5, 2000000, erdup::ItemCategory::Weapon, 1, 100),
            item(6, 2000000, erdup::ItemCategory::Weapon, 1, 100)
        }, config);
        if (plan.totalItems != 1 || plan.totalRunes != 100 || plan.candidates.front().item.baseItemId != 2000000)
            return fail("Known unsellable items must be preserved without blocking valid sales.");
    }
    {
        erdup::Config config;
        auto lowEquipped = item(1, 2000000, erdup::ItemCategory::Weapon, 1, 100);
        lowEquipped.equipped = true;
        auto high = item(2, 2000025, erdup::ItemCategory::Weapon, 1, 100, 25);
        auto extra = item(3, 2000024, erdup::ItemCategory::Weapon, 1, 100, 24);
        auto plan = erdup::planDuplicateSale({lowEquipped, high, extra}, config);
        if (plan.totalItems != 1 || plan.candidates.front().item.instanceId != 3)
            return fail("Equipped weapon and best remaining weapon must be kept.");
        high.equipped = true; extra.equipped = true;
        plan = erdup::planDuplicateSale({lowEquipped, high, extra}, config);
        if (plan.totalItems != 0) return fail("Equipped copies exceeding keep count must never be sold.");
        auto worn = item(9, 4000, erdup::ItemCategory::Armor, 1, 100);
        worn.equipped = true;
        plan = erdup::planDuplicateSale({item(1, 4000, erdup::ItemCategory::Armor, 1, 100), worn}, config);
        if (plan.totalItems != 1 || plan.candidates.front().item.instanceId != 1)
            return fail("Equipped armor must count as the retained copy.");
        config.keepAshesOfWar = 2;
        config.sellAshesOfWar = true;
        config.sellConsumables = false;
        plan = erdup::planDuplicateSale({
            item(1, 10100, erdup::ItemCategory::AshOfWar, 1, 300),
            item(2, 10100, erdup::ItemCategory::AshOfWar, 1, 300),
            item(3, 10100, erdup::ItemCategory::AshOfWar, 1, 300),
            item(4, 10200, erdup::ItemCategory::AshOfWar, 1, 300),
            item(5, 10200, erdup::ItemCategory::AshOfWar, 1, 300),
            item(6, 10300, erdup::ItemCategory::AshOfWar, 1, 300),
            item(7, 1000, erdup::ItemCategory::Consumable, 999, 200)
        }, config);
        if (plan.totalItems != 1 || plan.totalRunes != 300 ||
            plan.candidates.front().item.instanceId != 3)
            return fail("Keep two of each Ash of War and preserve every consumable.");
    }
    if (assertSoldPlusKeptInvariant() != EXIT_SUCCESS ||
        assertWeaponGroupKeepsHighestUpgradeInvariant() != EXIT_SUCCESS ||
        assertWeaponGroupCanSellUpgradedExcessInvariant() != EXIT_SUCCESS ||
        assertWeaponGroupCanSellModifiedExcessInvariant() != EXIT_SUCCESS ||
        assertDisabledCategoriesInvariant() != EXIT_SUCCESS ||
        assertMultipleStockRuleInvariant() != EXIT_SUCCESS ||
        assertLiveDecodedHardExclusionsInvariant() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Policy invariant test passed.\n";
    return EXIT_SUCCESS;
}
