#pragma once

#include "Config.h"
#include "GameAdapter.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace erdup {

struct ItemMetadata {
    std::uint32_t itemId = 0;
    ItemCategory category = ItemCategory::Unknown;
    std::string name;
    std::string sourceTable;
};

struct MetadataLoadResult {
    std::vector<ItemMetadata> items;
    std::vector<std::string> warnings;
};

std::optional<ItemCategory> itemCategoryFromMetadataToken(const std::string& token);
std::string itemCategoryToken(ItemCategory category);

MetadataLoadResult parseNameTableMetadata(
    const std::string& text,
    ItemCategory category,
    const std::string& sourceTable
);

MetadataLoadResult parseItemInfoMetadata(
    const std::string& text,
    const std::string& sourceTable
);

MetadataLoadResult loadNameTableMetadata(
    const std::filesystem::path& path,
    ItemCategory category,
    const std::string& sourceTable = {}
);

MetadataLoadResult loadItemInfoMetadata(
    const std::filesystem::path& path,
    const std::string& sourceTable = {}
);

void appendMetadata(MetadataLoadResult& target, MetadataLoadResult source);

// Loads metadata from whichever optional `metadata_*_path` settings are
// present in `config`. Missing/unset paths are skipped silently; unreadable
// paths that are set produce a warning (see readTextFile in ItemMetadata.cpp).
MetadataLoadResult loadConfiguredMetadata(const Config& config);

// Indexes parsed metadata for lookup. This is a reporting/logging aid only:
// per project constraints, metadata categories do not prove sale safety and
// must never gate item removal or memory writes on their own.
//
// Exact lookup is keyed by (category, itemId), since Elden Ring's EquipParam
// tables use independent ID namespaces per category (e.g. weapon id 100 and
// protector id 100 are unrelated items). The itemId-only fallback ignores
// category and ties are resolved first-registered-wins, so it is only
// suitable for best-effort display when the category is unknown.
class MetadataIndex {
public:
    void add(const MetadataLoadResult& result);
    void add(const std::vector<ItemMetadata>& items);

    const ItemMetadata* find(ItemCategory category, std::uint32_t itemId) const;
    const ItemMetadata* find(std::uint32_t itemId) const;

    std::size_t size() const;
    const std::vector<std::string>& warnings() const;

private:
    std::map<std::pair<ItemCategory, std::uint32_t>, ItemMetadata> byCategoryAndId_;
    std::map<std::uint32_t, ItemMetadata> byIdFallback_;
    std::vector<std::string> warnings_;
};

} // namespace erdup
