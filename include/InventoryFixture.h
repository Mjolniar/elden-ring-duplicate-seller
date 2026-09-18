#pragma once

#include "GameAdapter.h"

#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct InventoryFixtureResult {
    std::vector<ItemInstance> items;
    std::vector<std::string> warnings;
};

InventoryFixtureResult parseInventoryFixture(const std::string& text);
InventoryFixtureResult loadInventoryFixture(const std::filesystem::path& path);

} // namespace erdup
