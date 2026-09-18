#pragma once

#include "GameAdapter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

enum class CustomRemovalMode {
    MarkEmpty,
    MarkEmptyAndDecrementCount,
    CompactAndDecrementCount,
};

enum class CustomRemovalOperationKind {
    UpdateQuantity,
    ClearSlot,
    MoveSlot,
    UpdateCount,
};

struct CustomRemovalSlot {
    std::size_t index = 0;
    std::uint32_t handle = 0;
    std::uint32_t rawItemId = 0xffffffffu;
    std::uint32_t quantity = 0;
    std::uint32_t displayIndex = 0;
};

struct CustomRemovalRequest {
    ItemInstance item;
    int quantityToRemove = 0;
};

struct CustomRemovalOperation {
    CustomRemovalOperationKind kind = CustomRemovalOperationKind::UpdateQuantity;
    std::size_t index = 0;
    std::size_t fromIndex = 0;
    CustomRemovalSlot before;
    CustomRemovalSlot after;
    std::uint32_t oldCount = 0;
    std::uint32_t newCount = 0;
};

struct CustomRemovalPlan {
    bool valid = false;
    CustomRemovalMode mode = CustomRemovalMode::MarkEmpty;
    std::uint32_t initialCount = 0;
    std::uint32_t finalCount = 0;
    std::vector<CustomRemovalOperation> operations;
    std::vector<CustomRemovalSlot> finalSlots;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

std::uint32_t categoryNibble(ItemCategory category);
std::uint32_t rawInventoryItemId(ItemCategory category, std::uint32_t baseItemId);
std::string customRemovalModeToken(CustomRemovalMode mode);
std::string customRemovalOperationKindToken(CustomRemovalOperationKind kind);

CustomRemovalPlan planCustomRemoval(
    const std::vector<CustomRemovalSlot>& slots,
    std::uint32_t activeCount,
    const std::vector<CustomRemovalRequest>& requests,
    CustomRemovalMode mode
);

std::string formatCustomRemovalPlan(const CustomRemovalPlan& plan);

} // namespace erdup
