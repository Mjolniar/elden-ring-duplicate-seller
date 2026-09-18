#include "CustomRemovalPlan.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace erdup {

namespace {

constexpr std::uint32_t kEmptyRawItemId = 0xffffffffu;
constexpr std::size_t kMaxFormattedOperations = 80;

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

bool isEmptySlot(const CustomRemovalSlot& slot) {
    return slot.handle == 0 || slot.rawItemId == kEmptyRawItemId || slot.quantity == 0;
}

bool slotMatchesRequest(const CustomRemovalSlot& slot, const CustomRemovalRequest& request) {
    if (isEmptySlot(slot)) {
        return false;
    }
    if (slot.handle != request.item.instanceId) {
        return false;
    }
    if ((slot.rawItemId & 0x0fffffffu) != request.item.baseItemId) {
        return false;
    }
    return categoryFromRawItemId(slot.rawItemId) == request.item.category;
}

CustomRemovalSlot emptySlotAt(std::size_t index) {
    CustomRemovalSlot slot;
    slot.index = index;
    slot.rawItemId = kEmptyRawItemId;
    return slot;
}

void addError(CustomRemovalPlan& plan, std::string error) {
    plan.errors.push_back(std::move(error));
}

void addWarning(CustomRemovalPlan& plan, std::string warning) {
    if (std::find(plan.warnings.begin(), plan.warnings.end(), warning) == plan.warnings.end()) {
        plan.warnings.push_back(std::move(warning));
    }
}

void renumberSlots(std::vector<CustomRemovalSlot>& slots) {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        slots[i].index = i;
        if (!isEmptySlot(slots[i])) {
            slots[i].displayIndex = static_cast<std::uint32_t>(i);
        }
    }
}

} // namespace

std::uint32_t categoryNibble(ItemCategory category) {
    switch (category) {
    case ItemCategory::Weapon:
    case ItemCategory::Shield:
        return 0x0u;
    case ItemCategory::Armor:
        return 0x1u;
    case ItemCategory::Talisman:
        return 0x2u;
    case ItemCategory::Consumable:
    case ItemCategory::Material:
    case ItemCategory::KeyItem:
    case ItemCategory::Spell:
        return 0x4u;
    case ItemCategory::AshOfWar:
        return 0x8u;
    case ItemCategory::Unknown:
        return 0xfu;
    }
    return 0xfu;
}

std::uint32_t rawInventoryItemId(ItemCategory category, std::uint32_t baseItemId) {
    return (categoryNibble(category) << 28) | (baseItemId & 0x0fffffffu);
}

std::string customRemovalModeToken(CustomRemovalMode mode) {
    switch (mode) {
    case CustomRemovalMode::MarkEmpty: return "mark_empty";
    case CustomRemovalMode::MarkEmptyAndDecrementCount:
        return "mark_empty_and_decrement_count";
    case CustomRemovalMode::CompactAndDecrementCount: return "compact_and_decrement_count";
    }
    return "unknown";
}

std::string customRemovalOperationKindToken(CustomRemovalOperationKind kind) {
    switch (kind) {
    case CustomRemovalOperationKind::UpdateQuantity: return "update_quantity";
    case CustomRemovalOperationKind::ClearSlot: return "clear_slot";
    case CustomRemovalOperationKind::MoveSlot: return "move_slot";
    case CustomRemovalOperationKind::UpdateCount: return "update_count";
    }
    return "unknown";
}

CustomRemovalPlan planCustomRemoval(
    const std::vector<CustomRemovalSlot>& slots,
    std::uint32_t activeCount,
    const std::vector<CustomRemovalRequest>& requests,
    CustomRemovalMode mode
) {
    CustomRemovalPlan plan;
    plan.mode = mode;
    plan.initialCount = activeCount;
    plan.finalCount = activeCount;
    plan.finalSlots = slots;
    renumberSlots(plan.finalSlots);

    if (activeCount > slots.size()) {
        addError(plan, "active inventory count exceeds provided slot count");
        return plan;
    }
    if (activeCount > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        addError(plan, "active inventory count is unreasonably large");
        return plan;
    }
    if (requests.empty()) {
        addError(plan, "no removal requests were provided");
        return plan;
    }

    if (mode == CustomRemovalMode::MarkEmpty) {
        addWarning(plan, "mark-empty mode requires empty-slot sentinel validation before live use");
    } else if (mode == CustomRemovalMode::MarkEmptyAndDecrementCount) {
        addWarning(plan, "mark-empty/count-decrement mode requires live single-removal validation before live use");
    } else {
        addWarning(plan, "compact mode requires count/order repair validation before live use");
    }

    for (const auto& request : requests) {
        if (request.quantityToRemove <= 0) {
            addError(plan, "removal request has non-positive quantity");
            continue;
        }

        const auto matchLimit = mode == CustomRemovalMode::MarkEmptyAndDecrementCount
            ? plan.finalSlots.size()
            : plan.finalCount;

        auto match = plan.finalSlots.end();
        for (auto it = plan.finalSlots.begin();
            it != plan.finalSlots.begin() + static_cast<std::ptrdiff_t>(matchLimit);
            ++it) {
            if (slotMatchesRequest(*it, request)) {
                match = it;
                break;
            }
        }

        if (match == plan.finalSlots.end()) {
            addError(plan, "removal request did not match an active inventory slot");
            continue;
        }
        if (static_cast<std::uint32_t>(request.quantityToRemove) > match->quantity) {
            addError(plan, "removal request exceeds slot quantity");
            continue;
        }

        if (static_cast<std::uint32_t>(request.quantityToRemove) < match->quantity) {
            CustomRemovalOperation operation;
            operation.kind = CustomRemovalOperationKind::UpdateQuantity;
            operation.index = match->index;
            operation.before = *match;
            match->quantity -= static_cast<std::uint32_t>(request.quantityToRemove);
            operation.after = *match;
            plan.operations.push_back(operation);
            continue;
        }

        const auto removalIndex = match->index;
        if (mode == CustomRemovalMode::MarkEmpty) {
            CustomRemovalOperation operation;
            operation.kind = CustomRemovalOperationKind::ClearSlot;
            operation.index = removalIndex;
            operation.before = *match;
            *match = emptySlotAt(removalIndex);
            operation.after = *match;
            plan.operations.push_back(operation);
            continue;
        }

        if (mode == CustomRemovalMode::MarkEmptyAndDecrementCount) {
            CustomRemovalOperation clear;
            clear.kind = CustomRemovalOperationKind::ClearSlot;
            clear.index = removalIndex;
            clear.before = *match;
            *match = emptySlotAt(removalIndex);
            clear.after = *match;
            plan.operations.push_back(clear);

            if (plan.finalCount == 0) {
                addError(plan, "cannot decrement an empty active inventory count");
                continue;
            }

            CustomRemovalOperation count;
            count.kind = CustomRemovalOperationKind::UpdateCount;
            count.oldCount = plan.finalCount;
            count.newCount = plan.finalCount - 1;
            plan.finalCount = count.newCount;
            plan.operations.push_back(count);
            continue;
        }

        const auto oldCount = plan.finalCount;
        for (std::size_t index = removalIndex; index + 1 < oldCount; ++index) {
            CustomRemovalOperation move;
            move.kind = CustomRemovalOperationKind::MoveSlot;
            move.index = index;
            move.fromIndex = index + 1;
            move.before = plan.finalSlots[index];
            plan.finalSlots[index] = plan.finalSlots[index + 1];
            plan.finalSlots[index].index = index;
            plan.finalSlots[index].displayIndex = static_cast<std::uint32_t>(index);
            move.after = plan.finalSlots[index];
            plan.operations.push_back(move);
        }

        CustomRemovalOperation clear;
        clear.kind = CustomRemovalOperationKind::ClearSlot;
        clear.index = oldCount - 1;
        clear.before = plan.finalSlots[oldCount - 1];
        plan.finalSlots[oldCount - 1] = emptySlotAt(oldCount - 1);
        clear.after = plan.finalSlots[oldCount - 1];
        plan.operations.push_back(clear);

        CustomRemovalOperation count;
        count.kind = CustomRemovalOperationKind::UpdateCount;
        count.oldCount = oldCount;
        count.newCount = oldCount - 1;
        plan.finalCount = count.newCount;
        plan.operations.push_back(count);
    }

    plan.valid = plan.errors.empty();
    return plan;
}

std::string formatCustomRemovalPlan(const CustomRemovalPlan& plan) {
    std::ostringstream out;
    out << "custom_removal_plan:"
        << " valid=" << (plan.valid ? 1 : 0)
        << " mode=" << customRemovalModeToken(plan.mode)
        << " initial_count=" << plan.initialCount
        << " final_count=" << plan.finalCount
        << " operations=" << plan.operations.size()
        << " warnings=" << plan.warnings.size()
        << " errors=" << plan.errors.size();

    for (const auto& warning : plan.warnings) {
        out << " warning=\"" << warning << "\"";
    }
    for (const auto& error : plan.errors) {
        out << " error=\"" << error << "\"";
    }
    const auto operationsToFormat = std::min(plan.operations.size(), kMaxFormattedOperations);
    for (std::size_t i = 0; i < operationsToFormat; ++i) {
        const auto& operation = plan.operations[i];
        out << " op=" << customRemovalOperationKindToken(operation.kind);
        if (operation.kind == CustomRemovalOperationKind::MoveSlot) {
            out << "(" << operation.fromIndex << "->" << operation.index << ")";
        } else if (operation.kind == CustomRemovalOperationKind::UpdateCount) {
            out << "(" << operation.oldCount << "->" << operation.newCount << ")";
        } else {
            out << "(" << operation.index << ")";
        }
    }
    if (plan.operations.size() > operationsToFormat) {
        out << " operations_truncated=" << (plan.operations.size() - operationsToFormat);
    }

    return out.str();
}

} // namespace erdup
