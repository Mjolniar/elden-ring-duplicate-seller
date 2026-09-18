#pragma once

#include "Config.h"
#include "GameAdapter.h"
#include "ItemMetadata.h"
#include "SellValueMetadata.h"

#include <string>
#include <vector>

namespace erdup {

enum class SellValueResolution {
    Unknown,
    Resolved,
    Approximate,
};

struct InventoryReportRow {
    ItemInstance item;
    std::string displayName;
    std::string categoryName;
    SellValueResolution sellValueResolution = SellValueResolution::Unknown;
    int keepCount = 0;
    int quantityToSell = 0;
    int runeValue = 0;
    bool selectedForSale = false;
    std::string reason;
};

struct InventoryReport {
    std::vector<InventoryReportRow> rows;
    int totalItemsToSell = 0;
    int totalRunes = 0;
    int resolvedSellValueRows = 0;
    int approximateSellValueRows = 0;
    int unknownSellValueRows = 0;
};

InventoryReport buildInventoryReport(
    const std::vector<ItemInstance>& inventory,
    const Config& config,
    const MetadataIndex* metadata = nullptr,
    const SellValueIndex* sellValues = nullptr
);

std::string sellValueResolutionToken(SellValueResolution resolution);
std::string formatInventoryReport(const InventoryReport& report);

} // namespace erdup
