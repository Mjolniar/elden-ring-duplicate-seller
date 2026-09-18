#pragma once

#include "CustomRemovalPlan.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace erdup {

enum class InventoryChangeKind {
    NoChange,
    QuantityDecrement,
    MarkedEmpty,
    MarkedEmptyAndCountDecremented,
    CompactedAndCountDecremented,
    TailRangeCleared,
    AddedOrUnexpected,
    Unknown,
};

constexpr std::size_t kNoInventoryChangeIndex =
    (std::numeric_limits<std::size_t>::max)();

struct InventoryChangeAnalysis {
    InventoryChangeKind kind = InventoryChangeKind::Unknown;
    bool inputValid = true;
    bool recognizedRemovalShape = false;
    std::uint32_t beforeCount = 0;
    std::uint32_t afterCount = 0;
    std::size_t changedIndex = kNoInventoryChangeIndex;
    std::uint32_t removedHandle = 0;
    std::uint32_t removedRawItemId = 0xffffffffu;
    std::uint32_t beforeQuantity = 0;
    std::uint32_t afterQuantity = 0;
    std::uint32_t removedQuantity = 0;
    std::size_t movedSlotCount = 0;
    std::size_t clearedSlotCount = 0;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

std::string inventoryChangeKindToken(InventoryChangeKind kind);

InventoryChangeAnalysis analyzeInventoryChange(
    const std::vector<CustomRemovalSlot>& beforeSlots,
    std::uint32_t beforeCount,
    const std::vector<CustomRemovalSlot>& afterSlots,
    std::uint32_t afterCount
);

std::string formatInventoryChangeAnalysis(const InventoryChangeAnalysis& analysis);

} // namespace erdup
