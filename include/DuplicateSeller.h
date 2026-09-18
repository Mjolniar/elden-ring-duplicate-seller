#pragma once

#include "Config.h"
#include "GameAdapter.h"

#include <string>
#include <vector>

namespace erdup {

enum class SaleEligibility {
    Eligible,
    CategoryDisabled,
    Ammunition,
    SentinelItem,
    Unsellable,
    Upgraded,
    Modified,
    NonPositiveQuantity,
};

struct SellCandidate {
    ItemInstance item;
    int quantityToSell = 0;
    int runeValue = 0;
};

struct SellResult {
    std::vector<SellCandidate> candidates;
    int totalRunes = 0;
    int totalItems = 0;
    std::string message;
};

bool isCategorySaleEnabled(ItemCategory category, const Config& config);
int keepCountForItem(const ItemInstance& item, const Config& config);
std::uint32_t normalizeWeaponFamilyBaseId(
    ItemCategory category,
    std::uint32_t itemId,
    int upgradeLevel,
    bool modified
);
std::uint32_t duplicateGroupItemId(const ItemInstance& item);
SaleEligibility classifySaleEligibility(const ItemInstance& item, const Config& config);
std::string saleEligibilityReason(SaleEligibility eligibility, ItemCategory category);

SellResult planDuplicateSale(const std::vector<ItemInstance>& inventory, const Config& config);
SellResult executeDuplicateSale(IGameAdapter& adapter, const Config& config);

} // namespace erdup
