#pragma once

#include "Config.h"
#include "CustomRemovalPlan.h"
#include "InventoryReader.h"
#include "InventoryReport.h"
#include "InventorySnapshotFixture.h"
#include "ItemMetadata.h"
#include "SellValueMetadata.h"

#include <cstddef>
#include <string>
#include <vector>

namespace erdup {

struct SnapshotSalePlan {
    std::size_t decodedItems = 0;
    std::size_t removalRequests = 0;
    InventoryReport report;
    CustomRemovalPlan removalPlan;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

std::vector<ItemInstance> decodeSnapshotItems(const InventorySnapshotFixtureResult& snapshot);

SnapshotSalePlan buildSnapshotSalePlan(
    const InventorySnapshotFixtureResult& snapshot,
    const Config& config,
    CustomRemovalMode removalMode,
    const MetadataIndex* metadata = nullptr,
    const SellValueIndex* sellValues = nullptr
);

SnapshotSalePlan buildSnapshotSalePlan(
    const InventoryReadReport& readReport,
    const Config& config,
    CustomRemovalMode removalMode,
    const MetadataIndex* metadata = nullptr,
    const SellValueIndex* sellValues = nullptr
);

std::string formatSnapshotSalePlanSummary(const SnapshotSalePlan& plan);

} // namespace erdup
