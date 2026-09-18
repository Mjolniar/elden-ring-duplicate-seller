#pragma once
#include "InventoryReader.h"
#include <algorithm>
#include <array>
#include <map>
#include <tuple>

namespace erdup {
using NativeInventoryIdentities = std::map<std::size_t, std::tuple<unsigned, unsigned, unsigned>>;
inline NativeInventoryIdentities nativeInventoryIdentities(const InventoryReadReport& report) {
    NativeInventoryIdentities result;
    for (const auto& slot : report.slots) if (!slot.empty && slot.fieldsReadable)
        result.emplace(slot.index, std::make_tuple(slot.handle, slot.rawItemId, slot.quantity));
    return result;
}
inline void markNativeEquipped(InventoryReadReport& report, const std::array<int, 22>& indices) {
    for (auto& item : report.items)
        item.equipped = std::find(indices.begin(), indices.end(), static_cast<int>(item.instanceId) + 383) != indices.end();
}
}
