#include "CustomRemovalPlan.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

erdup::ItemInstance item(
    std::uint32_t handle,
    std::uint32_t baseId,
    erdup::ItemCategory category,
    int quantity
) {
    erdup::ItemInstance value;
    value.instanceId = handle;
    value.baseItemId = baseId;
    value.category = category;
    value.quantity = quantity;
    return value;
}

erdup::CustomRemovalSlot slot(
    std::size_t index,
    std::uint32_t handle,
    std::uint32_t baseId,
    erdup::ItemCategory category,
    std::uint32_t quantity
) {
    erdup::CustomRemovalSlot value;
    value.index = index;
    value.handle = handle;
    value.rawItemId = erdup::rawInventoryItemId(category, baseId);
    value.quantity = quantity;
    value.displayIndex = static_cast<std::uint32_t>(index);
    return value;
}

int assertPartialStackDecrementsQuantityOnly() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x100, 0x4000, erdup::ItemCategory::Consumable, 10),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = {{
        item(0x100, 0x4000, erdup::ItemCategory::Consumable, 10),
        4,
    }};

    const auto plan = erdup::planCustomRemoval(
        slots,
        1,
        requests,
        erdup::CustomRemovalMode::MarkEmpty
    );

    if (!plan.valid || plan.finalCount != 1 || plan.operations.size() != 1 ||
        plan.operations[0].kind != erdup::CustomRemovalOperationKind::UpdateQuantity ||
        plan.finalSlots[0].quantity != 6) {
        return fail("Partial stack removal should only decrement quantity.");
    }
    if (!contains(erdup::formatCustomRemovalPlan(plan), "op=update_quantity(0)")) {
        return fail("Formatted partial removal plan should include the quantity update.");
    }

    return EXIT_SUCCESS;
}

int assertMarkEmptyClearsFullStackWithoutChangingCount() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x200, 0x1234, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x201, 0x1235, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = {{
        item(0x200, 0x1234, erdup::ItemCategory::Weapon, 1),
        1,
    }};

    const auto plan = erdup::planCustomRemoval(
        slots,
        2,
        requests,
        erdup::CustomRemovalMode::MarkEmpty
    );

    if (!plan.valid || plan.finalCount != 2 || plan.operations.size() != 1 ||
        plan.operations[0].kind != erdup::CustomRemovalOperationKind::ClearSlot ||
        plan.finalSlots[0].handle != 0 ||
        plan.finalSlots[0].rawItemId != 0xffffffffu ||
        plan.finalSlots[1].handle != 0x201) {
        return fail("Mark-empty mode should clear the matched row and leave count unchanged.");
    }
    if (plan.warnings.empty() ||
        !contains(plan.warnings[0], "empty-slot sentinel validation")) {
        return fail("Mark-empty mode should warn about sentinel validation.");
    }

    return EXIT_SUCCESS;
}

int assertMarkEmptyAndCountDecrementClearsWithoutCompaction() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x260, 0x1234, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x261, 0x1235, erdup::ItemCategory::Weapon, 1),
        slot(2, 0x262, 0x1236, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = { {
        item(0x261, 0x1235, erdup::ItemCategory::Weapon, 1),
        1,
    } };

    const auto plan = erdup::planCustomRemoval(
        slots,
        3,
        requests,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (!plan.valid || plan.finalCount != 2 || plan.operations.size() != 2 ||
        plan.operations[0].kind != erdup::CustomRemovalOperationKind::ClearSlot ||
        plan.operations[1].kind != erdup::CustomRemovalOperationKind::UpdateCount ||
        plan.finalSlots[1].rawItemId != 0xffffffffu ||
        plan.finalSlots[2].handle != 0x262) {
        return fail("Mark-empty/count-decrement mode should clear the row, update count, and not compact rows.");
    }
    const auto formatted = erdup::formatCustomRemovalPlan(plan);
    if (!contains(formatted, "mode=mark_empty_and_decrement_count") ||
        !contains(formatted, "op=clear_slot(1)") ||
        !contains(formatted, "op=update_count(3->2)")) {
        return fail("Formatted mark-empty/count-decrement plan should expose clear and count operations.");
    }

    return EXIT_SUCCESS;
}

int assertMarkEmptyAndCountDecrementCanPlanMultipleHighIndexRows() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x270, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x271, 0x1001, erdup::ItemCategory::Weapon, 1),
        slot(2, 0x272, 0x1002, erdup::ItemCategory::Weapon, 1),
        slot(3, 0x273, 0x1003, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = {
        {
            item(0x272, 0x1002, erdup::ItemCategory::Weapon, 1),
            1,
        },
        {
            item(0x273, 0x1003, erdup::ItemCategory::Weapon, 1),
            1,
        },
    };

    const auto plan = erdup::planCustomRemoval(
        slots,
        4,
        requests,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (!plan.valid || plan.finalCount != 2 || plan.operations.size() != 4 ||
        plan.finalSlots[2].rawItemId != 0xffffffffu ||
        plan.finalSlots[3].rawItemId != 0xffffffffu) {
        return fail("Mark-empty/count-decrement mode should resolve all requests against the original active range.");
    }

    return EXIT_SUCCESS;
}

int assertMarkEmptyAndCountDecrementSearchesSparseSlotsBeyondCount() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x280, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x281, 0x1001, erdup::ItemCategory::Weapon, 1),
        erdup::CustomRemovalSlot{2, 0, 0xffffffffu, 0, 0},
        slot(3, 0x283, 0x1003, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = { {
        item(0x283, 0x1003, erdup::ItemCategory::Weapon, 1),
        1,
    } };

    const auto plan = erdup::planCustomRemoval(
        slots,
        3,
        requests,
        erdup::CustomRemovalMode::MarkEmptyAndDecrementCount
    );

    if (!plan.valid || plan.finalCount != 2 || plan.operations.size() != 2 ||
        plan.operations[0].kind != erdup::CustomRemovalOperationKind::ClearSlot ||
        plan.operations[0].index != 3 ||
        plan.finalSlots[3].rawItemId != 0xffffffffu) {
        return fail("Mark-empty/count-decrement mode should match sparse active rows beyond the count prefix.");
    }

    return EXIT_SUCCESS;
}

int assertCompactModeShiftsRowsAndDecrementsCount() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x300, 0x1000, erdup::ItemCategory::Armor, 1),
        slot(1, 0x301, 0x2000, erdup::ItemCategory::Talisman, 1),
        slot(2, 0x302, 0x3000, erdup::ItemCategory::AshOfWar, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = {{
        item(0x301, 0x2000, erdup::ItemCategory::Talisman, 1),
        1,
    }};

    const auto plan = erdup::planCustomRemoval(
        slots,
        3,
        requests,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );

    if (!plan.valid || plan.finalCount != 2 || plan.operations.size() != 3 ||
        plan.operations[0].kind != erdup::CustomRemovalOperationKind::MoveSlot ||
        plan.operations[1].kind != erdup::CustomRemovalOperationKind::ClearSlot ||
        plan.operations[2].kind != erdup::CustomRemovalOperationKind::UpdateCount) {
        return fail("Compact mode should move the tail row, clear the old tail, and update count.");
    }
    if (plan.finalSlots[1].handle != 0x302 ||
        plan.finalSlots[1].displayIndex != 1 ||
        plan.finalSlots[2].rawItemId != 0xffffffffu) {
        return fail("Compact mode did not produce the expected final row layout.");
    }
    if (!contains(erdup::formatCustomRemovalPlan(plan), "op=move_slot(2->1)") ||
        !contains(erdup::formatCustomRemovalPlan(plan), "op=update_count(3->2)")) {
        return fail("Formatted compact plan should expose move and count operations.");
    }

    return EXIT_SUCCESS;
}

int assertStaleOrUnsafeRequestsAreRejected() {
    const std::vector<erdup::CustomRemovalSlot> slots = {
        slot(0, 0x400, 0x1000, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalRequest> requests = {{
        item(0x999, 0x1000, erdup::ItemCategory::Weapon, 1),
        1,
    }};

    const auto stale = erdup::planCustomRemoval(
        slots,
        1,
        requests,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );
    if (stale.valid || stale.errors.empty() ||
        !contains(erdup::formatCustomRemovalPlan(stale), "did not match")) {
        return fail("Stale handle request should be rejected.");
    }

    const auto badCount = erdup::planCustomRemoval(
        slots,
        2,
        requests,
        erdup::CustomRemovalMode::MarkEmpty
    );
    if (badCount.valid || badCount.errors.empty() ||
        !contains(erdup::formatCustomRemovalPlan(badCount), "active inventory count exceeds")) {
        return fail("Active count beyond provided slots should be rejected.");
    }

    const std::vector<erdup::CustomRemovalRequest> excessive = {{
        item(0x400, 0x1000, erdup::ItemCategory::Weapon, 1),
        2,
    }};
    const auto tooMany = erdup::planCustomRemoval(
        slots,
        1,
        excessive,
        erdup::CustomRemovalMode::MarkEmpty
    );
    if (tooMany.valid || tooMany.errors.empty() ||
        !contains(erdup::formatCustomRemovalPlan(tooMany), "exceeds slot quantity")) {
        return fail("Request above slot quantity should be rejected.");
    }

    return EXIT_SUCCESS;
}

int assertCategoryEncodingIsStable() {
    if (erdup::rawInventoryItemId(erdup::ItemCategory::Weapon, 0x1234) != 0x00001234u ||
        erdup::rawInventoryItemId(erdup::ItemCategory::Armor, 0x1234) != 0x10001234u ||
        erdup::rawInventoryItemId(erdup::ItemCategory::Talisman, 0x1234) != 0x20001234u ||
        erdup::rawInventoryItemId(erdup::ItemCategory::Consumable, 0x1234) != 0x40001234u ||
        erdup::rawInventoryItemId(erdup::ItemCategory::AshOfWar, 0x1234) != 0x80001234u) {
        return fail("Raw inventory item ID category encoding changed unexpectedly.");
    }
    if (erdup::customRemovalModeToken(erdup::CustomRemovalMode::MarkEmpty) != "mark_empty" ||
        erdup::customRemovalModeToken(erdup::CustomRemovalMode::MarkEmptyAndDecrementCount) !=
            "mark_empty_and_decrement_count" ||
        erdup::customRemovalOperationKindToken(erdup::CustomRemovalOperationKind::MoveSlot) !=
            "move_slot") {
        return fail("Custom removal diagnostic tokens changed unexpectedly.");
    }

    return EXIT_SUCCESS;
}

int assertLargeCompactPlanFormattingIsTruncated() {
    std::vector<erdup::CustomRemovalSlot> slots;
    slots.reserve(96);
    for (std::size_t i = 0; i < 96; ++i) {
        slots.push_back(slot(
            i,
            static_cast<std::uint32_t>(0x500 + i),
            static_cast<std::uint32_t>(0x1000 + i),
            erdup::ItemCategory::Weapon,
            1
        ));
    }
    const std::vector<erdup::CustomRemovalRequest> requests = {{
        item(0x500, 0x1000, erdup::ItemCategory::Weapon, 1),
        1,
    }};

    const auto plan = erdup::planCustomRemoval(
        slots,
        static_cast<std::uint32_t>(slots.size()),
        requests,
        erdup::CustomRemovalMode::CompactAndDecrementCount
    );
    const auto formatted = erdup::formatCustomRemovalPlan(plan);

    if (!plan.valid || !contains(formatted, "operations=97") ||
        !contains(formatted, "operations_truncated=17")) {
        return fail("Large compact plan formatting should retain counts and truncate operation details.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertPartialStackDecrementsQuantityOnly() != EXIT_SUCCESS ||
        assertMarkEmptyClearsFullStackWithoutChangingCount() != EXIT_SUCCESS ||
        assertMarkEmptyAndCountDecrementClearsWithoutCompaction() != EXIT_SUCCESS ||
        assertMarkEmptyAndCountDecrementCanPlanMultipleHighIndexRows() != EXIT_SUCCESS ||
        assertMarkEmptyAndCountDecrementSearchesSparseSlotsBeyondCount() != EXIT_SUCCESS ||
        assertCompactModeShiftsRowsAndDecrementsCount() != EXIT_SUCCESS ||
        assertStaleOrUnsafeRequestsAreRejected() != EXIT_SUCCESS ||
        assertCategoryEncodingIsStable() != EXIT_SUCCESS ||
        assertLargeCompactPlanFormattingIsTruncated() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Custom removal plan test passed.\n";
    return EXIT_SUCCESS;
}
