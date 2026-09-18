#pragma once

#include "CustomRemovalPlan.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct InventorySnapshotFixtureResult {
    std::vector<CustomRemovalSlot> slots;
    std::uint32_t activeCount = 0;
    bool activeCountExplicit = false;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

InventorySnapshotFixtureResult parseInventorySnapshotFixture(const std::string& text);
InventorySnapshotFixtureResult loadInventorySnapshotFixture(const std::filesystem::path& path);

} // namespace erdup
