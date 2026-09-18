#include "SnapshotSalePlan.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

erdup::InventoryReadSlot readSlot(
    std::size_t index,
    std::uint32_t handle,
    std::uint32_t rawItemId,
    std::uint32_t quantity
) {
    erdup::InventoryReadSlot slot;
    slot.index = index;
    slot.fieldsReadable = true;
    slot.handle = handle;
    slot.rawItemId = rawItemId;
    slot.quantity = quantity;
    return slot;
}

erdup::InventoryReadSlot emptyReadSlot(std::size_t index) {
    erdup::InventoryReadSlot slot;
    slot.index = index;
    slot.fieldsReadable = true;
    slot.empty = true;
    return slot;
}

int assertKeepsHighestUpgradeWeaponCopiesFromSnapshot() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=4
slot=index:0,handle:0x100,raw:0xf42400,qty:1
slot=index:1,handle:0x101,raw:0xf42405,qty:1
slot=index:2,handle:0x102,raw:0xf42419,qty:1
slot=index:3,handle:0x103,raw:0xf42402,qty:1
)");

    erdup::Config config;
    config.keepWeapons = 2;
    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );

    if (!plan.errors.empty() ||
        !plan.removalPlan.valid ||
        plan.decodedItems != 4 ||
        plan.report.totalItemsToSell != 2 ||
        plan.removalRequests != 2 ||
        plan.removalPlan.finalCount != 2) {
        return fail("Snapshot sale plan should sell the two lowest-upgrade duplicate weapons.");
    }

    if (plan.removalPlan.finalSlots[0].handle != 0x101 ||
        plan.removalPlan.finalSlots[1].handle != 0x102) {
        return fail("Snapshot sale plan should keep the highest two upgraded weapon rows.");
    }

    bool soldPlusZero = false;
    bool soldPlusTwo = false;
    bool keptPlusFive = false;
    bool keptPlusTwentyFive = false;
    for (const auto& row : plan.report.rows) {
        if (row.item.instanceId == 0x100 && row.selectedForSale && row.item.upgradeLevel == 0) {
            soldPlusZero = true;
        }
        if (row.item.instanceId == 0x103 && row.selectedForSale && row.item.upgradeLevel == 2) {
            soldPlusTwo = true;
        }
        if (row.item.instanceId == 0x101 && !row.selectedForSale && row.item.upgradeLevel == 5) {
            keptPlusFive = true;
        }
        if (row.item.instanceId == 0x102 && !row.selectedForSale && row.item.upgradeLevel == 25) {
            keptPlusTwentyFive = true;
        }
    }

    if (!soldPlusZero || !soldPlusTwo || !keptPlusFive || !keptPlusTwentyFive) {
        return fail("Snapshot report rows should reflect top-two weapon keep policy.");
    }

    const auto summary = erdup::formatSnapshotSalePlanSummary(plan);
    if (!contains(summary, "decoded_items=4") ||
        !contains(summary, "items_to_sell=2") ||
        !contains(summary, "removal_valid=1")) {
        return fail("Snapshot sale summary should expose decoded items, sale rows, and removal validity.");
    }

    return EXIT_SUCCESS;
}

int assertLiveReadReportBuildsSnapshotSalePlan() {
    erdup::InventoryReadReport readReport;
    readReport.baseResolved = true;
    readReport.arrayRead = true;
    readReport.countRead = true;
    readReport.rawCount = 3;
    readReport.slots = {
        readSlot(0, 0x700, 0x00f42400, 1),
        readSlot(1, 0x701, 0x00f42405, 1),
        emptyReadSlot(2),
        readSlot(3, 0x703, 0x00f42402, 1),
    };

    erdup::Config config;
    config.keepWeapons = 2;
    const auto plan = erdup::buildSnapshotSalePlan(
        readReport,
        config,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (!plan.errors.empty() ||
        !plan.removalPlan.valid ||
        plan.decodedItems != 3 ||
        plan.report.totalItemsToSell != 1 ||
        plan.removalRequests != 1 ||
        plan.removalPlan.operations.size() != 2 ||
        plan.removalPlan.finalCount != 2 ||
        plan.removalPlan.finalSlots[0].rawItemId != 0xffffffffu ||
        plan.removalPlan.finalSlots[1].handle != 0x701 ||
        plan.removalPlan.finalSlots[3].handle != 0x703) {
        return fail("Live read report sale plan should keep top two weapon rows and clear the lowest row.");
    }

    const auto summary = erdup::formatSnapshotSalePlanSummary(plan);
    if (!contains(summary, "decoded_items=3") ||
        !contains(summary, "items_to_sell=1") ||
        !contains(summary, "removal_mode=mark_empty_and_decrement_count")) {
        return fail("Live read report sale plan summary should expose sale and removal plan details.");
    }

    return EXIT_SUCCESS;
}

int assertUnresolvedLiveReadReportBlocksSnapshotSalePlan() {
    erdup::InventoryReadReport readReport;
    readReport.rawCount = 1;
    readReport.slots = {readSlot(0, 0x800, 0x00100000, 1)};

    erdup::Config config;
    const auto plan = erdup::buildSnapshotSalePlan(
        readReport,
        config,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (plan.errors.empty() ||
        plan.removalPlan.valid ||
        plan.removalRequests != 0 ||
        !contains(erdup::formatSnapshotSalePlanSummary(plan), "error=\"inventory base was not resolved\"")) {
        return fail("Unresolved live read reports should block snapshot sale planning.");
    }

    return EXIT_SUCCESS;
}

int assertNoDuplicateCandidatesProduceNoOpPlan() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=2
slot=index:0,handle:0x200,raw:0x40001234,qty:99
slot=index:1,handle:0x201,raw:0x40005678,qty:1
)");

    erdup::Config config;
    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        erdup::CustomRemovalMode::MarkEmpty
    );

    if (!plan.errors.empty() ||
        !plan.removalPlan.valid ||
        plan.report.totalItemsToSell != 0 ||
        plan.removalRequests != 0 ||
        plan.removalPlan.operations.size() != 0 ||
        plan.removalPlan.finalCount != 2 ||
        plan.warnings.size() != 0 ||
        plan.removalPlan.warnings.empty()) {
        return fail("No duplicate snapshot candidates should produce a valid no-op removal plan.");
    }

    if (!contains(erdup::formatCustomRemovalPlan(plan.removalPlan), "operations=0")) {
        return fail("Formatted no-op custom removal plan should expose zero operations.");
    }

    return EXIT_SUCCESS;
}

int assertSnapshotParseErrorsBlockPlan() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=not_a_number
slot=index:0,handle:0x300,qty:1
)");

    erdup::Config config;
    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );

    if (plan.errors.empty() || plan.removalPlan.valid || plan.decodedItems != 0) {
        return fail("Snapshot parse errors should block sale/removal planning.");
    }

    return EXIT_SUCCESS;
}

int assertSparseSnapshotRowsBeyondCountArePlanned() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=3
slot=index:0,handle:0x500,raw:0x00100000,qty:1
slot=index:1,handle:0x501,raw:0x00100000,qty:1
slot=index:2,empty:true
slot=index:3,handle:0x503,raw:0x00100000,qty:1
)");

    erdup::Config config;
    config.keepWeapons = 2;
    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (!plan.errors.empty() ||
        !plan.removalPlan.valid ||
        plan.decodedItems != 3 ||
        plan.report.totalItemsToSell != 1 ||
        plan.removalRequests != 1 ||
        plan.removalPlan.finalCount != 2 ||
        plan.removalPlan.operations.size() != 2 ||
        plan.removalPlan.finalSlots[3].rawItemId != 0xffffffffu) {
        return fail("Sparse snapshot rows beyond count should still be decoded and planned.");
    }

    const auto formatted = erdup::formatCustomRemovalPlan(plan.removalPlan);
    if (!contains(formatted, "mode=mark_empty_and_decrement_count") ||
        !contains(formatted, "op=clear_slot(3)") ||
        !contains(formatted, "op=update_count(3->2)")) {
        return fail("Sparse removal plan should target the beyond-count duplicate row.");
    }

    return EXIT_SUCCESS;
}

int assertLowDensitySnapshotsAreRejected() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=8
slot=index:0,handle:0x400,raw:0x100000,qty:1
slot=index:7,empty:true
)");

    erdup::Config config;
    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );

    if (plan.errors.empty() ||
        plan.removalPlan.valid ||
        plan.removalRequests != 0 ||
        plan.report.rows.size() != 0) {
        return fail("Low-density transitional snapshots should be rejected before sale planning.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertKeepsHighestUpgradeWeaponCopiesFromSnapshot() != EXIT_SUCCESS ||
        assertLiveReadReportBuildsSnapshotSalePlan() != EXIT_SUCCESS ||
        assertUnresolvedLiveReadReportBlocksSnapshotSalePlan() != EXIT_SUCCESS ||
        assertNoDuplicateCandidatesProduceNoOpPlan() != EXIT_SUCCESS ||
        assertSnapshotParseErrorsBlockPlan() != EXIT_SUCCESS ||
        assertSparseSnapshotRowsBeyondCountArePlanned() != EXIT_SUCCESS ||
        assertLowDensitySnapshotsAreRejected() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Snapshot sale plan test passed.\n";
    return EXIT_SUCCESS;
}
