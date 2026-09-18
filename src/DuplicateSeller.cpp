#include "DuplicateSeller.h"
#include "MutationSafety.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <tuple>

namespace erdup {

namespace {

constexpr std::uint32_t kUnarmedSentinelItemId = 110000;
constexpr std::uint32_t kAmmunitionStartItemId = 50000000;
constexpr std::uint32_t kAmmunitionEndExclusiveItemId = 54000000;
constexpr std::uint32_t kMinimumWeaponParamId = 100000;
constexpr std::uint32_t kWeaponAffinityStep = 100;
constexpr std::uint32_t kMaxKnownWeaponAffinitySuffix = 1200;

bool isEligibleDuplicate(const ItemInstance& item, const Config& config) {
    return classifySaleEligibility(item, config) == SaleEligibility::Eligible;
}

bool isWeaponFamilyCategory(ItemCategory category) {
    return category == ItemCategory::Weapon || category == ItemCategory::Shield;
}

bool isWeaponFamilyItem(const ItemInstance& item) {
    return isWeaponFamilyCategory(item.category);
}

bool isUnarmedSentinelItem(const ItemInstance& item) {
    return item.category == ItemCategory::Weapon && item.baseItemId == kUnarmedSentinelItemId;
}

bool isAmmunitionItem(const ItemInstance& item) {
    return item.category == ItemCategory::Weapon &&
        item.baseItemId >= kAmmunitionStartItemId &&
        item.baseItemId < kAmmunitionEndExclusiveItemId;
}

using GroupKey = std::tuple<ItemCategory, std::uint32_t>;

} // namespace

bool isCategorySaleEnabled(ItemCategory category, const Config& config) {
    switch (category) {
    case ItemCategory::Weapon: return config.sellWeapons;
    case ItemCategory::Shield: return config.sellShields;
    case ItemCategory::Armor: return config.sellArmor;
    case ItemCategory::Talisman: return config.sellTalismans;
    case ItemCategory::AshOfWar: return config.sellAshesOfWar;
    case ItemCategory::Spell: return config.sellSpells;
    case ItemCategory::Consumable: return config.sellConsumables;
    default: return false;
    }
}

int keepCountForItem(const ItemInstance& item, const Config& config) {
    const auto stockRule = config.itemStockRules.find(item.baseItemId);
    if (stockRule != config.itemStockRules.end()) {
        return stockRule->second;
    }

    switch (item.category) {
    case ItemCategory::Weapon: return config.keepWeapons;
    case ItemCategory::Shield: return config.keepShields;
    case ItemCategory::Armor: return config.keepArmor;
    case ItemCategory::Talisman: return config.keepTalismans;
    case ItemCategory::AshOfWar: return config.keepAshesOfWar;
    case ItemCategory::Spell: return config.keepSpells;
    case ItemCategory::Consumable: return config.keepConsumables;
    default: return config.keepCount;
    }
}

std::uint32_t normalizeWeaponFamilyBaseId(
    ItemCategory category,
    std::uint32_t itemId,
    int upgradeLevel,
    bool modified
) {
    if (!isWeaponFamilyCategory(category) || itemId < kMinimumWeaponParamId) {
        return itemId;
    }

    auto normalized = itemId;
    if (upgradeLevel > 0 &&
        upgradeLevel <= 25 &&
        normalized >= static_cast<std::uint32_t>(upgradeLevel)) {
        normalized -= static_cast<std::uint32_t>(upgradeLevel);
    }

    const auto suffix = normalized % 10000u;
    if (modified &&
        suffix >= kWeaponAffinityStep &&
        suffix <= kMaxKnownWeaponAffinitySuffix &&
        (suffix % kWeaponAffinityStep) == 0) {
        normalized -= suffix;
    }

    return normalized;
}

std::uint32_t duplicateGroupItemId(const ItemInstance& item) {
    return normalizeWeaponFamilyBaseId(
        item.category,
        item.baseItemId,
        item.upgradeLevel,
        item.modified
    );
}

SaleEligibility classifySaleEligibility(const ItemInstance& item, const Config& config) {
    if (item.sellValue < 0) {
        return SaleEligibility::Unsellable;
    }
    if (isUnarmedSentinelItem(item)) {
        return SaleEligibility::SentinelItem;
    }
    if (isAmmunitionItem(item)) {
        return SaleEligibility::Ammunition;
    }
    if (!isCategorySaleEnabled(item.category, config)) {
        return SaleEligibility::CategoryDisabled;
    }
    if (config.requireUnupgraded && !isWeaponFamilyItem(item) && item.upgradeLevel != 0) {
        return SaleEligibility::Upgraded;
    }
    if (config.requireUnmodified && !isWeaponFamilyItem(item) && item.modified) {
        return SaleEligibility::Modified;
    }
    if (item.quantity <= 0) {
        return SaleEligibility::NonPositiveQuantity;
    }
    return SaleEligibility::Eligible;
}

std::string saleEligibilityReason(SaleEligibility eligibility, ItemCategory category) {
    switch (eligibility) {
    case SaleEligibility::Eligible: return "eligible";
    case SaleEligibility::Ammunition: return "ammunition skipped";
    case SaleEligibility::SentinelItem: return "sentinel item skipped";
    case SaleEligibility::Unsellable: return "traders do not buy this item";
    case SaleEligibility::CategoryDisabled:
        if (category == ItemCategory::Consumable) {
            return "consumable selling disabled";
        }
        if (category == ItemCategory::Unknown) {
            return "unknown category skipped";
        }
        if (category == ItemCategory::Material || category == ItemCategory::KeyItem) {
            return "safe-excluded category skipped";
        }
        return "category disabled";
    case SaleEligibility::Upgraded: return "upgraded item skipped";
    case SaleEligibility::Modified: return "modified item skipped";
    case SaleEligibility::NonPositiveQuantity: return "non-positive quantity skipped";
    }
    return "unknown reason";
}

SellResult planDuplicateSale(const std::vector<ItemInstance>& inventory, const Config& config) {
    SellResult result;
    if (!config.enabled) {
        result.message = "Duplicate seller is disabled.";
        return result;
    }

    std::map<GroupKey, std::vector<ItemInstance>> groups;
    for (const auto& item : inventory) {
        if (isEligibleDuplicate(item, config)) {
            groups[{item.category, duplicateGroupItemId(item)}].push_back(item);
        }
    }

    for (auto& [_, items] : groups) {
        std::sort(items.begin(), items.end(), [](const ItemInstance& left, const ItemInstance& right) {
            if (left.equipped != right.equipped) {
                return left.equipped;
            }
            if (isWeaponFamilyItem(left) && isWeaponFamilyItem(right) &&
                left.upgradeLevel != right.upgradeLevel) {
                return left.upgradeLevel > right.upgradeLevel;
            }
            return left.instanceId < right.instanceId;
        });

        int remainingToKeep = keepCountForItem(items.front(), config);
        for (const auto& item : items) {
            const int keptFromThisStack = std::min(remainingToKeep, item.quantity);
            remainingToKeep = std::max(0, remainingToKeep - keptFromThisStack);

            const int sellQuantity = item.quantity - keptFromThisStack;
            if (sellQuantity <= 0 || item.equipped) {
                continue;
            }

            SellCandidate candidate;
            candidate.item = item;
            candidate.quantityToSell = sellQuantity;
            candidate.runeValue = sellQuantity * std::max(0, item.sellValue);
            result.totalItems += sellQuantity;
            result.totalRunes += candidate.runeValue;
            result.candidates.push_back(candidate);
        }
    }

    std::ostringstream message;
    message << "Planned " << result.totalItems << " duplicate item sale(s) for "
            << result.totalRunes << " rune(s).";
    result.message = message.str();
    return result;
}

SellResult executeDuplicateSale(IGameAdapter& adapter, const Config& config) {
    if (!adapter.isReady()) {
        SellResult result;
        result.message = "Game adapter is not implemented for this Elden Ring version yet.";
        return result;
    }

    auto result = planDuplicateSale(adapter.readInventory(), config);
    if (config.dryRun || result.candidates.empty()) {
        adapter.showToast(result.message);
        return result;
    }

    const auto safety = evaluateMutationSafety(result, config);
    if (!safety.allowed) {
        result.message = "Mutation blocked: " + formatMutationSafetyReport(safety);
        adapter.showToast(result.message);
        return result;
    }

    int soldItems = 0;
    int awardedRunes = 0;
    for (const auto& candidate : result.candidates) {
        if (!adapter.removeItem(candidate.item, candidate.quantityToSell)) {
            continue;
        }
        soldItems += candidate.quantityToSell;
        awardedRunes += candidate.runeValue;
    }

    if (awardedRunes > 0) {
        adapter.addRunes(awardedRunes);
    }

    std::ostringstream message;
    message << "Sold " << soldItems << " duplicate item(s) for "
            << awardedRunes << " rune(s).";
    result.message = message.str();
    adapter.showToast(result.message);
    return result;
}

} // namespace erdup
