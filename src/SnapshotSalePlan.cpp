#include "SnapshotSalePlan.h"

#include "DuplicateSeller.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>

namespace erdup {

namespace {

constexpr std::uint32_t kEmptyRawItemId = 0xffffffffu;
constexpr std::uint32_t kMinimumWeaponParamId = 100000;
constexpr std::uint32_t kWeaponAffinityStep = 100;
constexpr std::uint32_t kMaxKnownWeaponAffinitySuffix = 1200;
constexpr std::uint32_t kMaxKnownWeaponUpgradeLevel = 25;
constexpr std::size_t kMinimumDecodedPercent = 75;

ItemCategory categoryFromRawItemId(std::uint32_t rawItemId) {
    switch ((rawItemId >> 28) & 0xfu) {
    case 0x0: return ItemCategory::Weapon;
    case 0x1: return ItemCategory::Armor;
    case 0x2: return ItemCategory::Talisman;
    case 0x4: return ItemCategory::Consumable;
    case 0x8: return ItemCategory::AshOfWar;
    default: return ItemCategory::Unknown;
    }
}

bool isWeaponFamilyCategory(ItemCategory category) {
    return category == ItemCategory::Weapon || category == ItemCategory::Shield;
}

int decodeWeaponUpgradeLevel(std::uint32_t baseItemId, ItemCategory category) {
    if (!isWeaponFamilyCategory(category) || baseItemId < kMinimumWeaponParamId) {
        return 0;
    }

    const auto upgradeLevel = baseItemId % 100u;
    if (upgradeLevel > kMaxKnownWeaponUpgradeLevel) {
        return 0;
    }
    return static_cast<int>(upgradeLevel);
}

bool isLikelyModifiedWeaponParamId(std::uint32_t baseItemId, ItemCategory category) {
    if (!isWeaponFamilyCategory(category) || baseItemId < kMinimumWeaponParamId) {
        return false;
    }

    const auto suffix = baseItemId % 10000u;
    const auto upgradeLevel = suffix % 100u;
    const auto affinitySuffix = suffix - upgradeLevel;
    return affinitySuffix >= kWeaponAffinityStep &&
        affinitySuffix <= kMaxKnownWeaponAffinitySuffix &&
        (affinitySuffix % kWeaponAffinityStep) == 0;
}

bool isEmptySnapshotSlot(const CustomRemovalSlot& slot) {
    return slot.handle == 0 || slot.rawItemId == kEmptyRawItemId || slot.quantity == 0;
}

CustomRemovalSlot customRemovalSlotFromReadSlot(const InventoryReadSlot& readSlot) {
    CustomRemovalSlot slot;
    slot.index = readSlot.index;
    slot.displayIndex = static_cast<std::uint32_t>(readSlot.index);
    if (readSlot.fieldsReadable && !readSlot.empty) {
        slot.handle = readSlot.handle;
        slot.rawItemId = readSlot.rawItemId;
        slot.quantity = readSlot.quantity;
    } else {
        slot.rawItemId = kEmptyRawItemId;
    }
    return slot;
}

InventorySnapshotFixtureResult snapshotFromReadReport(const InventoryReadReport& readReport) {
    InventorySnapshotFixtureResult snapshot;
    snapshot.activeCount = readReport.rawCount;
    snapshot.activeCountExplicit = readReport.countRead;
    snapshot.warnings = readReport.warnings;
    snapshot.errors = readReport.errors;

    if (!readReport.baseResolved) {
        snapshot.errors.push_back("inventory base was not resolved");
    }
    if (!readReport.arrayRead) {
        snapshot.errors.push_back("inventory array pointer was not readable");
    }
    if (!readReport.countRead) {
        snapshot.errors.push_back("inventory count was not readable");
    }
    if (readReport.rawCount == 0) {
        snapshot.errors.push_back("inventory count was zero");
    }
    if (readReport.slots.empty()) {
        snapshot.errors.push_back("inventory read retained no rows");
    }

    snapshot.slots.reserve(readReport.slots.size());
    for (const auto& readSlot : readReport.slots) {
        if (!readSlot.fieldsReadable) {
            snapshot.errors.push_back(
                "inventory row " + std::to_string(readSlot.index) + " had unreadable fields"
            );
        }
        snapshot.slots.push_back(customRemovalSlotFromReadSlot(readSlot));
    }

    return snapshot;
}

std::size_t activeSlotLimit(const InventorySnapshotFixtureResult& snapshot) {
    if (snapshot.activeCount == 0) {
        return 0;
    }

    std::size_t nonEmptyCount = 0;
    for (std::size_t i = 0; i < snapshot.slots.size(); ++i) {
        if (!isEmptySnapshotSlot(snapshot.slots[i])) {
            ++nonEmptyCount;
        }
        if (nonEmptyCount >= snapshot.activeCount) {
            return i + 1;
        }
    }

    return snapshot.slots.size();
}

std::vector<CustomRemovalRequest> removalRequestsFromReport(const InventoryReport& report) {
    std::vector<CustomRemovalRequest> requests;
    for (const auto& row : report.rows) {
        if (!row.selectedForSale || row.quantityToSell <= 0) {
            continue;
        }
        requests.push_back(CustomRemovalRequest{row.item, row.quantityToSell});
    }
    return requests;
}

CustomRemovalPlan noOpRemovalPlan(
    const InventorySnapshotFixtureResult& snapshot,
    CustomRemovalMode removalMode
) {
    CustomRemovalPlan plan;
    plan.valid = true;
    plan.mode = removalMode;
    plan.initialCount = snapshot.activeCount;
    plan.finalCount = snapshot.activeCount;
    plan.finalSlots = snapshot.slots;
    plan.warnings.push_back("no duplicate sale candidates were selected");
    return plan;
}

} // namespace

std::vector<ItemInstance> decodeSnapshotItems(const InventorySnapshotFixtureResult& snapshot) {
    std::vector<ItemInstance> items;
    const auto limit = activeSlotLimit(snapshot);
    items.reserve(limit);

    for (std::size_t i = 0; i < limit; ++i) {
        const auto& slot = snapshot.slots[i];
        if (isEmptySnapshotSlot(slot)) {
            continue;
        }

        ItemInstance item;
        item.instanceId = slot.handle;
        item.baseItemId = slot.rawItemId & 0x0fffffffu;
        item.category = categoryFromRawItemId(slot.rawItemId);
        item.quantity = slot.quantity > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(slot.quantity);
        item.upgradeLevel = decodeWeaponUpgradeLevel(item.baseItemId, item.category);
        item.modified = isLikelyModifiedWeaponParamId(item.baseItemId, item.category);
        items.push_back(item);
    }

    return items;
}

SnapshotSalePlan buildSnapshotSalePlan(
    const InventorySnapshotFixtureResult& snapshot,
    const Config& config,
    CustomRemovalMode removalMode,
    const MetadataIndex* metadata,
    const SellValueIndex* sellValues
) {
    SnapshotSalePlan plan;
    plan.warnings = snapshot.warnings;
    plan.errors = snapshot.errors;

    if (!snapshot.errors.empty()) {
        plan.removalPlan.mode = removalMode;
        plan.removalPlan.initialCount = snapshot.activeCount;
        plan.removalPlan.finalCount = snapshot.activeCount;
        plan.removalPlan.finalSlots = snapshot.slots;
        plan.removalPlan.errors = snapshot.errors;
        return plan;
    }

    const auto items = decodeSnapshotItems(snapshot);
    plan.decodedItems = items.size();
    if (snapshot.activeCount > 0 &&
        (plan.decodedItems * 100u) < (static_cast<std::size_t>(snapshot.activeCount) * kMinimumDecodedPercent)) {
        plan.errors.push_back(
            "snapshot decoded density is too low for sale planning; wait for a stable inventory snapshot"
        );
        plan.removalPlan.mode = removalMode;
        plan.removalPlan.initialCount = snapshot.activeCount;
        plan.removalPlan.finalCount = snapshot.activeCount;
        plan.removalPlan.finalSlots = snapshot.slots;
        plan.removalPlan.errors = plan.errors;
        return plan;
    }

    plan.report = buildInventoryReport(items, config, metadata, sellValues);

    const auto requests = removalRequestsFromReport(plan.report);
    plan.removalRequests = requests.size();
    if (requests.empty()) {
        plan.removalPlan = noOpRemovalPlan(snapshot, removalMode);
        return plan;
    }

    plan.removalPlan = planCustomRemoval(snapshot.slots, snapshot.activeCount, requests, removalMode);
    for (const auto& warning : plan.removalPlan.warnings) {
        plan.warnings.push_back(warning);
    }
    for (const auto& error : plan.removalPlan.errors) {
        plan.errors.push_back(error);
    }
    return plan;
}

SnapshotSalePlan buildSnapshotSalePlan(
    const InventoryReadReport& readReport,
    const Config& config,
    CustomRemovalMode removalMode,
    const MetadataIndex* metadata,
    const SellValueIndex* sellValues
) {
    return buildSnapshotSalePlan(
        snapshotFromReadReport(readReport),
        config,
        removalMode,
        metadata,
        sellValues
    );
}

std::string formatSnapshotSalePlanSummary(const SnapshotSalePlan& plan) {
    std::ostringstream out;
    out << "snapshot_sale_plan:"
        << " decoded_items=" << plan.decodedItems
        << " items_to_sell=" << plan.report.totalItemsToSell
        << " removal_requests=" << plan.removalRequests
        << " removal_valid=" << (plan.removalPlan.valid ? 1 : 0)
        << " removal_mode=" << customRemovalModeToken(plan.removalPlan.mode)
        << " removal_operations=" << plan.removalPlan.operations.size()
        << " warnings=" << plan.warnings.size()
        << " errors=" << plan.errors.size();

    for (const auto& warning : plan.warnings) {
        out << " warning=\"" << warning << "\"";
    }
    for (const auto& error : plan.errors) {
        out << " error=\"" << error << "\"";
    }

    return out.str();
}

} // namespace erdup
