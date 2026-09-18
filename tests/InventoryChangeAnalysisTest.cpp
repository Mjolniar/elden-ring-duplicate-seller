#include "InventoryChangeAnalysis.h"

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

erdup::CustomRemovalSlot emptySlot(std::size_t index) {
    erdup::CustomRemovalSlot value;
    value.index = index;
    value.rawItemId = 0xffffffffu;
    return value;
}

int assertQuantityDecrementIsDetected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x100, 0x4000, erdup::ItemCategory::Consumable, 10),
        slot(1, 0x101, 0x2000, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x100, 0x4000, erdup::ItemCategory::Consumable, 6),
        slot(1, 0x101, 0x2000, erdup::ItemCategory::Weapon, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 2, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::QuantityDecrement ||
        !analysis.recognizedRemovalShape ||
        analysis.changedIndex != 0 ||
        analysis.removedQuantity != 4) {
        return fail("Quantity decrement should be detected as a single removal shape.");
    }
    const auto formatted = erdup::formatInventoryChangeAnalysis(analysis);
    if (!contains(formatted, "kind=quantity_decrement") ||
        !contains(formatted, "removed_quantity=4")) {
        return fail("Formatted quantity decrement should include stable tokens.");
    }

    return EXIT_SUCCESS;
}

int assertMarkedEmptyIsDetected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x200, 0x1234, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x201, 0x5678, erdup::ItemCategory::Talisman, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        emptySlot(0),
        slot(1, 0x201, 0x5678, erdup::ItemCategory::Talisman, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 2, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::MarkedEmpty ||
        !analysis.recognizedRemovalShape ||
        analysis.changedIndex != 0 ||
        analysis.removedHandle != 0x200 ||
        analysis.removedQuantity != 1) {
        return fail("Mark-empty removal should be detected.");
    }

    return EXIT_SUCCESS;
}

int assertZeroQuantitySameIdentityIsMarkedEmpty() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x250, 0x1234, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x251, 0x5678, erdup::ItemCategory::Talisman, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x250, 0x1234, erdup::ItemCategory::Weapon, 0),
        slot(1, 0x251, 0x5678, erdup::ItemCategory::Talisman, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 2, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::MarkedEmpty ||
        !analysis.recognizedRemovalShape ||
        analysis.changedIndex != 0 ||
        analysis.removedQuantity != 1) {
        return fail("Zero-quantity same-identity row should be treated as an empty row.");
    }

    return EXIT_SUCCESS;
}

int assertMarkedEmptyWithCountDecrementIsDetected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x260, 0x1234, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x261, 0x5678, erdup::ItemCategory::Weapon, 1),
        slot(2, 0x262, 0x9999, erdup::ItemCategory::Talisman, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x260, 0x1234, erdup::ItemCategory::Weapon, 1),
        emptySlot(1),
        slot(2, 0x262, 0x9999, erdup::ItemCategory::Talisman, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 3, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::MarkedEmptyAndCountDecremented ||
        !analysis.recognizedRemovalShape ||
        analysis.changedIndex != 1 ||
        analysis.removedHandle != 0x261 ||
        analysis.removedQuantity != 1 ||
        analysis.clearedSlotCount != 1) {
        return fail("Mark-empty plus count decrement should be detected as the live single-removal shape.");
    }

    const auto formatted = erdup::formatInventoryChangeAnalysis(analysis);
    if (!contains(formatted, "kind=marked_empty_and_count_decremented") ||
        !contains(formatted, "recognized_removal_shape=1") ||
        !contains(formatted, "cleared_slots=1")) {
        return fail("Formatted mark-empty/count-decrement analysis should expose stable tokens.");
    }

    return EXIT_SUCCESS;
}

int assertCompactAndCountDecrementIsDetected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x300, 0x1000, erdup::ItemCategory::Armor, 1),
        slot(1, 0x301, 0x2000, erdup::ItemCategory::Talisman, 1),
        slot(2, 0x302, 0x3000, erdup::ItemCategory::AshOfWar, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x300, 0x1000, erdup::ItemCategory::Armor, 1),
        slot(1, 0x302, 0x3000, erdup::ItemCategory::AshOfWar, 1),
        emptySlot(2),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 3, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::CompactedAndCountDecremented ||
        !analysis.recognizedRemovalShape ||
        analysis.changedIndex != 1 ||
        analysis.removedHandle != 0x301 ||
        analysis.movedSlotCount != 1) {
        return fail("Compaction with count decrement should be detected.");
    }
    if (!contains(erdup::formatInventoryChangeAnalysis(analysis), "moved_slots=1")) {
        return fail("Formatted compact analysis should include moved slot count.");
    }

    return EXIT_SUCCESS;
}

int assertUnexpectedAdditionIsDetected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x400, 0x1000, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x400, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x401, 0x1001, erdup::ItemCategory::Weapon, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 1, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::AddedOrUnexpected ||
        analysis.recognizedRemovalShape ||
        analysis.warnings.empty()) {
        return fail("Unexpected added active row should not be treated as removal.");
    }

    return EXIT_SUCCESS;
}

int assertTailRangeClearedIsDetectedAsUnsafe() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x700, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x701, 0x1001, erdup::ItemCategory::Weapon, 1),
        slot(2, 0x702, 0x1002, erdup::ItemCategory::Weapon, 1),
        slot(3, 0x703, 0x1003, erdup::ItemCategory::AshOfWar, 1),
        emptySlot(4),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x700, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x701, 0x1001, erdup::ItemCategory::Weapon, 1),
        emptySlot(2),
        emptySlot(3),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 5, after, 4);
    if (analysis.kind != erdup::InventoryChangeKind::TailRangeCleared ||
        analysis.recognizedRemovalShape ||
        analysis.changedIndex != 2 ||
        analysis.clearedSlotCount != 2 ||
        analysis.warnings.empty()) {
        return fail("A contiguous multi-row tail clear should be classified as unsafe evidence.");
    }
    const auto formatted = erdup::formatInventoryChangeAnalysis(analysis);
    if (!contains(formatted, "kind=tail_range_cleared") ||
        !contains(formatted, "cleared_slots=2") ||
        !contains(formatted, "recognized_removal_shape=0")) {
        return fail("Formatted tail clear analysis should include stable unsafe-evidence tokens.");
    }

    return EXIT_SUCCESS;
}

int assertInvalidCountsAreRejected() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x500, 0x1000, erdup::ItemCategory::Weapon, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 2, before, 1);
    if (analysis.inputValid || analysis.errors.empty() ||
        !contains(erdup::formatInventoryChangeAnalysis(analysis), "before active count exceeds")) {
        return fail("Active count beyond provided slots should be rejected.");
    }

    return EXIT_SUCCESS;
}

int assertUnknownDeltaIsReported() {
    const std::vector<erdup::CustomRemovalSlot> before = {
        slot(0, 0x600, 0x1000, erdup::ItemCategory::Weapon, 1),
        slot(1, 0x601, 0x1001, erdup::ItemCategory::Weapon, 1),
    };
    const std::vector<erdup::CustomRemovalSlot> after = {
        slot(0, 0x600, 0x1000, erdup::ItemCategory::Weapon, 2),
        slot(1, 0x601, 0x1001, erdup::ItemCategory::Weapon, 1),
    };

    const auto analysis = erdup::analyzeInventoryChange(before, 2, after, 2);
    if (analysis.kind != erdup::InventoryChangeKind::Unknown ||
        analysis.recognizedRemovalShape ||
        analysis.warnings.empty()) {
        return fail("Unrecognized delta should remain unknown.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertQuantityDecrementIsDetected() != EXIT_SUCCESS ||
        assertMarkedEmptyIsDetected() != EXIT_SUCCESS ||
        assertZeroQuantitySameIdentityIsMarkedEmpty() != EXIT_SUCCESS ||
        assertMarkedEmptyWithCountDecrementIsDetected() != EXIT_SUCCESS ||
        assertCompactAndCountDecrementIsDetected() != EXIT_SUCCESS ||
        assertUnexpectedAdditionIsDetected() != EXIT_SUCCESS ||
        assertTailRangeClearedIsDetectedAsUnsafe() != EXIT_SUCCESS ||
        assertInvalidCountsAreRejected() != EXIT_SUCCESS ||
        assertUnknownDeltaIsReported() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Inventory change analysis test passed.\n";
    return EXIT_SUCCESS;
}
