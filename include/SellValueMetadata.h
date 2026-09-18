#pragma once

#include "Config.h"
#include "GameAdapter.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace erdup {

struct SellValueLoadResult {
    std::map<std::uint32_t, int> values;
    std::map<std::pair<ItemCategory, std::uint32_t>, int> categorizedValues;
    std::vector<std::string> warnings;
};

SellValueLoadResult parseSellValueMetadata(
    const std::string& text,
    const std::string& sourceName = {}
);

SellValueLoadResult loadSellValueMetadata(const std::filesystem::path& path);
SellValueLoadResult loadConfiguredSellValues(const Config& config,
    const std::filesystem::path& dllDirectory = {});

class SellValueIndex {
public:
    void add(const SellValueLoadResult& result);
    void add(const std::map<std::uint32_t, int>& values);
    void add(const std::map<std::pair<ItemCategory, std::uint32_t>, int>& values);

    std::optional<int> find(ItemCategory category, std::uint32_t itemId) const;
    std::optional<int> find(std::uint32_t itemId) const;
    std::size_t size() const;
    const std::vector<std::string>& warnings() const;

private:
    std::map<std::uint32_t, int> values_;
    std::map<std::pair<ItemCategory, std::uint32_t>, int> categorizedValues_;
    std::vector<std::string> warnings_;
};

} // namespace erdup
