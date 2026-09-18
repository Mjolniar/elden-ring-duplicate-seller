#include "InventoryReport.h"

#include "DuplicateSeller.h"

#include <map>
#include <sstream>
#include <tuple>

namespace erdup {

namespace {

using CandidateKey = std::tuple<ItemCategory, std::uint32_t, std::uint32_t>;

struct ValuedItem {
    ItemInstance item;
    SellValueResolution resolution = SellValueResolution::Unknown;
};

CandidateKey candidateKey(const ItemInstance& item) {
    return {item.category, item.baseItemId, item.instanceId};
}

std::string normalizedVariantName(const std::string& baseName, const ItemInstance& item) {
    if (item.upgradeLevel > 0) {
        return baseName + " (+" + std::to_string(item.upgradeLevel) + ")";
    }

    std::ostringstream out;
    out << baseName << " [variant " << item.baseItemId << "]";
    return out.str();
}

std::string displayNameForItem(const ItemInstance& item, const MetadataIndex* metadata) {
    if (metadata != nullptr) {
        if (const auto* exact = metadata->find(item.category, item.baseItemId)) {
            return exact->name;
        }
        if (const auto* fallback = metadata->find(item.baseItemId)) {
            return fallback->name;
        }

        const auto normalizedId = normalizeWeaponFamilyBaseId(
            item.category,
            item.baseItemId,
            item.upgradeLevel,
            item.modified
        );
        if (normalizedId != item.baseItemId) {
            if (const auto* exactBase = metadata->find(item.category, normalizedId)) {
                return normalizedVariantName(exactBase->name, item);
            }
            if (const auto* fallbackBase = metadata->find(normalizedId)) {
                return normalizedVariantName(fallbackBase->name, item);
            }
        }
    }

    if (!item.debugName.empty()) {
        return item.debugName;
    }

    return "<unknown>";
}

std::map<CandidateKey, SellCandidate> saleCandidateMap(const SellResult& result) {
    std::map<CandidateKey, SellCandidate> byItem;
    for (const auto& candidate : result.candidates) {
        byItem.emplace(candidateKey(candidate.item), candidate);
    }
    return byItem;
}

SellValueResolution resolveSellValue(ItemInstance& item, const SellValueIndex* sellValues) {
    if (sellValues == nullptr) {
        return SellValueResolution::Unknown;
    }

    if (const auto exact = sellValues->find(item.category, item.baseItemId)) {
        item.sellValue = exact.value();
        return SellValueResolution::Resolved;
    }

    const auto normalizedId = normalizeWeaponFamilyBaseId(
        item.category,
        item.baseItemId,
        item.upgradeLevel,
        item.modified
    );
    if (normalizedId != item.baseItemId) {
        if (const auto approximate = sellValues->find(item.category, normalizedId)) {
            item.sellValue = approximate.value();
            return SellValueResolution::Approximate;
        }
    }

    item.sellValue = 0;
    return SellValueResolution::Unknown;
}

std::vector<ValuedItem> resolveSellValues(
    const std::vector<ItemInstance>& inventory,
    const SellValueIndex* sellValues
) {
    std::vector<ValuedItem> valued;
    valued.reserve(inventory.size());
    for (auto item : inventory) {
        const auto resolution = resolveSellValue(item, sellValues);
        valued.push_back(ValuedItem{item, resolution});
    }
    return valued;
}

std::vector<ItemInstance> itemsOnly(const std::vector<ValuedItem>& valuedInventory) {
    std::vector<ItemInstance> items;
    items.reserve(valuedInventory.size());
    for (const auto& valued : valuedInventory) {
        items.push_back(valued.item);
    }
    return items;
}

void countSellValueResolution(InventoryReport& report, SellValueResolution resolution) {
    switch (resolution) {
    case SellValueResolution::Resolved:
        ++report.resolvedSellValueRows;
        break;
    case SellValueResolution::Approximate:
        ++report.approximateSellValueRows;
        break;
    case SellValueResolution::Unknown:
        ++report.unknownSellValueRows;
        break;
    }
}

} // namespace

InventoryReport buildInventoryReport(
    const std::vector<ItemInstance>& inventory,
    const Config& config,
    const MetadataIndex* metadata,
    const SellValueIndex* sellValues
) {
    InventoryReport report;
    const auto valuedInventory = resolveSellValues(inventory, sellValues);
    const auto salePlan = planDuplicateSale(itemsOnly(valuedInventory), config);
    const auto candidates = saleCandidateMap(salePlan);

    for (const auto& valued : valuedInventory) {
        const auto& item = valued.item;
        InventoryReportRow row;
        row.item = item;
        row.displayName = displayNameForItem(item, metadata);
        row.categoryName = itemCategoryToken(item.category);
        row.sellValueResolution = valued.resolution;
        countSellValueResolution(report, row.sellValueResolution);
        row.keepCount = keepCountForItem(item, config);

        const auto candidate = candidates.find(candidateKey(item));
        if (candidate != candidates.end()) {
            row.selectedForSale = true;
            row.quantityToSell = candidate->second.quantityToSell;
            row.runeValue = candidate->second.runeValue;
            row.reason = "duplicate above keep count";
            report.totalItemsToSell += row.quantityToSell;
            report.totalRunes += row.runeValue;
        } else {
            const auto eligibility = classifySaleEligibility(item, config);
            if (eligibility == SaleEligibility::Eligible) {
                row.reason = "retained by keep count";
            } else {
                row.reason = saleEligibilityReason(eligibility, item.category);
            }
        }

        report.rows.push_back(row);
    }

    return report;
}

std::string sellValueResolutionToken(SellValueResolution resolution) {
    switch (resolution) {
    case SellValueResolution::Resolved: return "resolved";
    case SellValueResolution::Approximate: return "approximate";
    case SellValueResolution::Unknown: return "unknown";
    }
    return "unknown";
}

std::string formatInventoryReport(const InventoryReport& report) {
    std::ostringstream out;
    out << "Inventory dry-run report: sell " << report.totalItemsToSell
        << " item(s) for " << report.totalRunes << " rune(s)\n";
    out << "Sell value summary: resolved=" << report.resolvedSellValueRows
        << " approximate=" << report.approximateSellValueRows
        << " unknown=" << report.unknownSellValueRows << "\n";

    for (const auto& row : report.rows) {
        out << (row.selectedForSale ? "SELL" : "SKIP")
            << " id=" << row.item.baseItemId
            << " name=\"" << row.displayName << "\""
            << " category=" << row.categoryName
            << " qty=" << row.item.quantity
            << " upgrade=" << row.item.upgradeLevel
            << " modified=" << row.item.modified
            << " keep=" << row.keepCount
            << " sell=" << row.quantityToSell
            << " runes=" << row.runeValue
            << " sell_value=" << sellValueResolutionToken(row.sellValueResolution)
            << " reason=\"" << row.reason << "\"\n";
    }

    return out.str();
}

} // namespace erdup
