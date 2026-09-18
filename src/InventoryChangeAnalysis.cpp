#include "InventoryChangeAnalysis.h"

#include <algorithm>
#include <sstream>

namespace erdup {

namespace {

constexpr std::uint32_t kEmptyRawItemId = 0xffffffffu;

bool isEmptySlot(const CustomRemovalSlot& slot) {
    return slot.handle == 0 || slot.rawItemId == kEmptyRawItemId || slot.quantity == 0;
}

bool sameIdentity(const CustomRemovalSlot& lhs, const CustomRemovalSlot& rhs) {
    return lhs.handle == rhs.handle && lhs.rawItemId == rhs.rawItemId;
}

bool sameState(const CustomRemovalSlot& lhs, const CustomRemovalSlot& rhs) {
    return sameIdentity(lhs, rhs) && lhs.quantity == rhs.quantity;
}

bool activeContainsIdentity(
    const std::vector<CustomRemovalSlot>& slots,
    std::size_t activeCount,
    const CustomRemovalSlot& target
) {
    return std::any_of(slots.begin(), slots.begin() + static_cast<std::ptrdiff_t>(activeCount),
        [&](const CustomRemovalSlot& slot) {
            return sameIdentity(slot, target);
        });
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << value;
    return out.str();
}

void setRemovedSlot(
    InventoryChangeAnalysis& analysis,
    std::size_t index,
    const CustomRemovalSlot& before,
    std::uint32_t afterQuantity
) {
    analysis.changedIndex = index;
    analysis.removedHandle = before.handle;
    analysis.removedRawItemId = before.rawItemId;
    analysis.beforeQuantity = before.quantity;
    analysis.afterQuantity = afterQuantity;
    analysis.removedQuantity = before.quantity - afterQuantity;
}

bool detectNoChange(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount != afterCount) {
        return false;
    }

    for (std::size_t index = 0; index < beforeCount; ++index) {
        if (!sameState(beforeSlots[index], afterSlots[index])) {
            return false;
        }
    }

    analysis.kind = InventoryChangeKind::NoChange;
    return true;
}

bool detectQuantityDecrement(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount != afterCount) {
        return false;
    }

    std::size_t changedIndex = kNoInventoryChangeIndex;
    for (std::size_t index = 0; index < beforeCount; ++index) {
        const auto& before = beforeSlots[index];
        const auto& after = afterSlots[index];

        if (!sameIdentity(before, after)) {
            return false;
        }
        if (before.quantity == after.quantity) {
            continue;
        }
        if (isEmptySlot(before) || isEmptySlot(after) || after.quantity >= before.quantity ||
            changedIndex != kNoInventoryChangeIndex) {
            return false;
        }

        changedIndex = index;
    }

    if (changedIndex == kNoInventoryChangeIndex) {
        return false;
    }

    analysis.kind = InventoryChangeKind::QuantityDecrement;
    analysis.recognizedRemovalShape = true;
    setRemovedSlot(analysis, changedIndex, beforeSlots[changedIndex], afterSlots[changedIndex].quantity);
    return true;
}

bool detectMarkedEmpty(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount != afterCount) {
        return false;
    }

    std::size_t changedIndex = kNoInventoryChangeIndex;
    for (std::size_t index = 0; index < beforeCount; ++index) {
        const auto& before = beforeSlots[index];
        const auto& after = afterSlots[index];

        if (sameState(before, after)) {
            continue;
        }
        if (isEmptySlot(before) || !isEmptySlot(after) ||
            changedIndex != kNoInventoryChangeIndex) {
            return false;
        }

        changedIndex = index;
    }

    if (changedIndex == kNoInventoryChangeIndex) {
        return false;
    }

    analysis.kind = InventoryChangeKind::MarkedEmpty;
    analysis.recognizedRemovalShape = true;
    setRemovedSlot(analysis, changedIndex, beforeSlots[changedIndex], 0);
    return true;
}

bool detectMarkedEmptyAndCountDecremented(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount == 0 || beforeCount != afterCount + 1) {
        return false;
    }

    std::size_t changedIndex = kNoInventoryChangeIndex;
    for (std::size_t index = 0; index < afterCount; ++index) {
        const auto& before = beforeSlots[index];
        const auto& after = afterSlots[index];

        if (sameState(before, after)) {
            continue;
        }
        if (isEmptySlot(before) || !isEmptySlot(after) ||
            changedIndex != kNoInventoryChangeIndex) {
            return false;
        }

        changedIndex = index;
    }

    if (changedIndex == kNoInventoryChangeIndex) {
        return false;
    }

    analysis.kind = InventoryChangeKind::MarkedEmptyAndCountDecremented;
    analysis.recognizedRemovalShape = true;
    setRemovedSlot(analysis, changedIndex, beforeSlots[changedIndex], 0);
    analysis.clearedSlotCount = 1;
    return true;
}

bool compactMatchesAtIndex(
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount,
    std::size_t removedIndex
) {
    for (std::size_t index = 0; index < removedIndex; ++index) {
        if (!sameState(beforeSlots[index], afterSlots[index])) {
            return false;
        }
    }

    for (std::size_t index = removedIndex; index < afterCount; ++index) {
        if (!sameState(beforeSlots[index + 1], afterSlots[index])) {
            return false;
        }
    }

    return beforeCount == afterCount + 1;
}

bool detectCompactedAndCountDecremented(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount == 0 || beforeCount != afterCount + 1) {
        return false;
    }

    for (std::size_t removedIndex = 0; removedIndex < beforeCount; ++removedIndex) {
        if (isEmptySlot(beforeSlots[removedIndex])) {
            continue;
        }
        if (!compactMatchesAtIndex(beforeSlots, beforeCount, afterSlots, afterCount, removedIndex)) {
            continue;
        }

        analysis.kind = InventoryChangeKind::CompactedAndCountDecremented;
        analysis.recognizedRemovalShape = true;
        setRemovedSlot(analysis, removedIndex, beforeSlots[removedIndex], 0);
        analysis.movedSlotCount = beforeCount - removedIndex - 1;
        if (afterCount < afterSlots.size() && !isEmptySlot(afterSlots[afterCount])) {
            analysis.warnings.push_back(
                "inactive tail row after compaction was not empty; tail clearing remains unresolved"
            );
        }
        return true;
    }

    return false;
}

bool detectTailRangeCleared(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (beforeCount == 0 || afterCount > beforeCount) {
        return false;
    }

    std::size_t firstChanged = kNoInventoryChangeIndex;
    const auto comparableCount = std::min(beforeCount, afterCount);
    for (std::size_t index = 0; index < comparableCount; ++index) {
        if (sameState(beforeSlots[index], afterSlots[index])) {
            continue;
        }
        firstChanged = index;
        break;
    }

    if (firstChanged == kNoInventoryChangeIndex) {
        if (afterCount == beforeCount) {
            return false;
        }
        firstChanged = afterCount;
    }

    std::size_t clearedCount = 0;
    for (std::size_t index = firstChanged; index < beforeCount; ++index) {
        if (isEmptySlot(beforeSlots[index])) {
            continue;
        }
        if (index < afterCount && !isEmptySlot(afterSlots[index])) {
            return false;
        }
        ++clearedCount;
    }

    if (clearedCount <= 1) {
        return false;
    }

    analysis.kind = InventoryChangeKind::TailRangeCleared;
    analysis.changedIndex = firstChanged;
    analysis.clearedSlotCount = clearedCount;
    analysis.warnings.push_back(
        "tail range cleared; reader/layout is not stable enough for mutation validation"
    );
    return true;
}

bool detectAddedOrUnexpected(
    InventoryChangeAnalysis& analysis,
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::size_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::size_t afterCount
) {
    if (afterCount > beforeCount) {
        analysis.kind = InventoryChangeKind::AddedOrUnexpected;
        analysis.warnings.push_back("after snapshot has more active rows than before snapshot");
        return true;
    }

    for (std::size_t index = 0; index < afterCount; ++index) {
        if (!isEmptySlot(afterSlots[index]) &&
            !activeContainsIdentity(beforeSlots, beforeCount, afterSlots[index])) {
            analysis.kind = InventoryChangeKind::AddedOrUnexpected;
            analysis.warnings.push_back("after snapshot contains an item identity absent from before snapshot");
            return true;
        }
    }

    return false;
}

} // namespace

std::string inventoryChangeKindToken(InventoryChangeKind kind) {
    switch (kind) {
    case InventoryChangeKind::NoChange: return "no_change";
    case InventoryChangeKind::QuantityDecrement: return "quantity_decrement";
    case InventoryChangeKind::MarkedEmpty: return "marked_empty";
    case InventoryChangeKind::MarkedEmptyAndCountDecremented:
        return "marked_empty_and_count_decremented";
    case InventoryChangeKind::CompactedAndCountDecremented:
        return "compacted_and_count_decremented";
    case InventoryChangeKind::TailRangeCleared: return "tail_range_cleared";
    case InventoryChangeKind::AddedOrUnexpected: return "added_or_unexpected";
    case InventoryChangeKind::Unknown: return "unknown";
    }
    return "unknown";
}

InventoryChangeAnalysis analyzeInventoryChange(
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::uint32_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::uint32_t afterCount
) {
    InventoryChangeAnalysis analysis;
    analysis.beforeCount = beforeCount;
    analysis.afterCount = afterCount;

    if (beforeCount > beforeSlots.size()) {
        analysis.errors.push_back("before active count exceeds provided slot count");
    }
    if (afterCount > afterSlots.size()) {
        analysis.errors.push_back("after active count exceeds provided slot count");
    }
    if (!analysis.errors.empty()) {
        analysis.inputValid = false;
        return analysis;
    }

    const auto beforeActive = static_cast<std::size_t>(beforeCount);
    const auto afterActive = static_cast<std::size_t>(afterCount);

    if (detectNoChange(analysis, beforeSlots, beforeActive, afterSlots, afterActive) ||
        detectQuantityDecrement(analysis, beforeSlots, beforeActive, afterSlots, afterActive) ||
        detectMarkedEmpty(analysis, beforeSlots, beforeActive, afterSlots, afterActive) ||
        detectMarkedEmptyAndCountDecremented(
            analysis,
            beforeSlots,
            beforeActive,
            afterSlots,
            afterActive
        ) ||
        detectCompactedAndCountDecremented(
            analysis,
            beforeSlots,
            beforeActive,
            afterSlots,
            afterActive
        ) ||
        detectTailRangeCleared(analysis, beforeSlots, beforeActive, afterSlots, afterActive) ||
        detectAddedOrUnexpected(analysis, beforeSlots, beforeActive, afterSlots, afterActive)) {
        return analysis;
    }

    analysis.kind = InventoryChangeKind::Unknown;
    analysis.warnings.push_back("snapshot delta does not match a known single-removal shape");
    return analysis;
}

std::string formatInventoryChangeAnalysis(const InventoryChangeAnalysis& analysis) {
    std::ostringstream out;
    out << "inventory_change_analysis:"
        << " kind=" << inventoryChangeKindToken(analysis.kind)
        << " input_valid=" << (analysis.inputValid ? 1 : 0)
        << " recognized_removal_shape=" << (analysis.recognizedRemovalShape ? 1 : 0)
        << " before_count=" << analysis.beforeCount
        << " after_count=" << analysis.afterCount;

    if (analysis.kind == InventoryChangeKind::TailRangeCleared &&
        analysis.changedIndex != kNoInventoryChangeIndex) {
        out << " changed_index=" << analysis.changedIndex
            << " cleared_slots=" << analysis.clearedSlotCount;
    } else if (analysis.changedIndex != kNoInventoryChangeIndex) {
        out << " changed_index=" << analysis.changedIndex
            << " removed_handle=" << hex32(analysis.removedHandle)
            << " removed_raw_item_id=" << hex32(analysis.removedRawItemId)
            << " before_quantity=" << analysis.beforeQuantity
            << " after_quantity=" << analysis.afterQuantity
            << " removed_quantity=" << analysis.removedQuantity
            << " moved_slots=" << analysis.movedSlotCount
            << " cleared_slots=" << analysis.clearedSlotCount;
    }

    out << " warnings=" << analysis.warnings.size()
        << " errors=" << analysis.errors.size();

    for (const auto& warning : analysis.warnings) {
        out << " warning=\"" << warning << "\"";
    }
    for (const auto& error : analysis.errors) {
        out << " error=\"" << error << "\"";
    }

    return out.str();
}

} // namespace erdup
