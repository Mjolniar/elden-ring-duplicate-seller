#include "InventoryReport.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t instanceId,
    std::uint32_t baseItemId,
    erdup::ItemCategory category,
    int upgradeLevel,
    int sellValue,
    int quantity = 1,
    std::string debugName = {},
    bool modified = false
) {
    erdup::ItemInstance instance;
    instance.instanceId = instanceId;
    instance.baseItemId = baseItemId;
    instance.category = category;
    instance.quantity = quantity;
    instance.upgradeLevel = upgradeLevel;
    instance.sellValue = sellValue;
    instance.debugName = std::move(debugName);
    instance.modified = modified;
    return instance;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

int main() {
    erdup::Config config;
    config.itemStockRules[9000] = 25;

    const std::vector<erdup::ItemInstance> inventory = {
        item(1, 1000, erdup::ItemCategory::Weapon, 0, 100, 1, "Debug Sword"),
        item(2, 1000, erdup::ItemCategory::Weapon, 0, 100, 1, "Debug Sword"),
        item(3, 1000, erdup::ItemCategory::Weapon, 0, 100, 1, "Debug Sword"),
        item(4, 2000, erdup::ItemCategory::Armor, 0, 50, 1, "Debug Armor"),
        item(5, 2000, erdup::ItemCategory::Armor, 0, 50, 1, "Debug Armor"),
        item(6, 3000, erdup::ItemCategory::Weapon, 3, 100, 1, "Upgraded Sword"),
        item(7, 4000, erdup::ItemCategory::Consumable, 0, 10, 10, "Debug Consumable"),
        item(8, 9000, erdup::ItemCategory::Consumable, 0, 2, 40, "Debug Arrow"),
        item(9, 7777, erdup::ItemCategory::Talisman, 0, 10),
        item(10, 8888, erdup::ItemCategory::Unknown, 0, 10),
        item(11, 52000000, erdup::ItemCategory::Weapon, 0, 1, 25, "Debug Bolt"),
        item(12, 110000, erdup::ItemCategory::Weapon, 0, 0, 3, "Debug Unarmed"),
        item(13, 2000025, erdup::ItemCategory::Weapon, 25, 100),
        item(14, 2000100, erdup::ItemCategory::Weapon, 0, 100, 1, {}, true),
        item(15, 2010200, erdup::ItemCategory::Weapon, 0, 100, 1, "Exact Heavy Sword", true),
        item(16, 9999025, erdup::ItemCategory::Weapon, 25, 100),
    };

    erdup::MetadataIndex metadata;
    metadata.add(std::vector<erdup::ItemMetadata>{
        {1000, erdup::ItemCategory::Weapon, "Metadata Sword", "test"},
        {2000, erdup::ItemCategory::Armor, "Metadata Armor", "test"},
        {4000, erdup::ItemCategory::Consumable, "Metadata Consumable", "test"},
        {52000000, erdup::ItemCategory::Weapon, "Metadata Bolt", "test"},
        {110000, erdup::ItemCategory::Weapon, "Metadata Unarmed", "test"},
        {2000000, erdup::ItemCategory::Weapon, "Metadata Longsword", "test"},
        {2010200, erdup::ItemCategory::Weapon, "Metadata Heavy Longsword", "test"},
    });

    const auto report = erdup::buildInventoryReport(inventory, config, &metadata);
    const auto text = erdup::formatInventoryReport(report);

    if (report.totalItemsToSell != 2 || report.totalRunes != 150) {
        return fail("Expected one weapon duplicate and one armor duplicate in the report totals.");
    }
    if (report.resolvedSellValueRows != 0 ||
        report.approximateSellValueRows != 0 ||
        report.unknownSellValueRows != static_cast<int>(inventory.size())) {
        return fail("Report without configured sell values should keep current rune totals and mark values unknown.");
    }
    if (!contains(text, "Sell value summary: resolved=0 approximate=0 unknown=16")) {
        return fail("Report should include sell-value summary counts.");
    }

    if (!contains(text, "name=\"Metadata Sword\"")) {
        return fail("Metadata name should be preferred over debugName.");
    }
    if (!contains(text, "id=2000025 name=\"Metadata Longsword (+25)\"")) {
        return fail("Upgraded weapon variant should fall back to normalized base metadata.");
    }
    if (!contains(text, "id=2000100 name=\"Metadata Longsword [variant 2000100]\"")) {
        return fail("Affinity weapon variant should fall back to normalized base metadata.");
    }
    if (!contains(text, "id=2010200 name=\"Metadata Heavy Longsword\"")) {
        return fail("Exact variant metadata should remain preferred over normalized base metadata.");
    }
    if (!contains(text, "name=\"Metadata Bolt\"") || !contains(text, "reason=\"ammunition skipped\"")) {
        return fail("Ammunition row should keep its metadata name and hard-excluded reason.");
    }
    if (!contains(text, "name=\"Metadata Unarmed\"") || !contains(text, "reason=\"sentinel item skipped\"")) {
        return fail("Sentinel row should keep its metadata name and hard-excluded reason.");
    }
    if (!contains(text, "name=\"Upgraded Sword\"")) {
        return fail("debugName should be used when metadata is unavailable.");
    }
    if (!contains(text, "name=\"<unknown>\"")) {
        return fail("Unknown item should use the <unknown> fallback name.");
    }
    if (!contains(text, "id=9999025 name=\"<unknown>\"")) {
        return fail("Genuinely unknown normalized weapon variant should remain unknown.");
    }
    if (!contains(text, "reason=\"unknown category skipped\"")) {
        return fail("Unknown category skip reason missing.");
    }
    if (!contains(text, "id=3000 name=\"Upgraded Sword\" category=weapon qty=1 upgrade=3 modified=0 keep=2 sell=0 runes=0 sell_value=unknown reason=\"retained by keep count\"")) {
        return fail("Kept upgraded weapon row should show upgrade level and retained reason.");
    }
    if (!contains(text, "reason=\"consumable selling disabled\"")) {
        return fail("Disabled consumable skip reason missing.");
    }
    if (!contains(text, "id=9000") || !contains(text, "keep=25")) {
        return fail("Configured stock rule keep count missing from report.");
    }
    if (!contains(text, "SELL id=1000") || !contains(text, "keep=2") || !contains(text, "reason=\"duplicate above keep count\"")) {
        return fail("Duplicate sale candidate did not show expected keep count and reason.");
    }

    const std::vector<erdup::ItemInstance> valuedInventory = {
        item(1, 1000, erdup::ItemCategory::Weapon, 0, 999),
        item(2, 1000, erdup::ItemCategory::Weapon, 0, 999),
        item(3, 1000, erdup::ItemCategory::Weapon, 0, 999),
        item(4, 2000000, erdup::ItemCategory::Weapon, 0, 999),
        item(5, 2000000, erdup::ItemCategory::Weapon, 0, 999),
        item(6, 2000100, erdup::ItemCategory::Weapon, 0, 999, 1, {}, true),
        item(7, 3000, erdup::ItemCategory::Armor, 0, 999),
        item(8, 3000, erdup::ItemCategory::Armor, 0, 999),
    };

    erdup::SellValueLoadResult valueLoad;
    valueLoad.values = {
        {1000, 11},
        {2000000, 22},
    };
    erdup::SellValueIndex sellValues;
    sellValues.add(valueLoad);

    const auto valuedReport = erdup::buildInventoryReport(valuedInventory, config, nullptr, &sellValues);
    const auto valuedText = erdup::formatInventoryReport(valuedReport);
    if (valuedReport.totalItemsToSell != 3 || valuedReport.totalRunes != 33) {
        return fail("Loaded sell values should drive exact and approximate dry-run rune totals.");
    }
    if (valuedReport.resolvedSellValueRows != 5 ||
        valuedReport.approximateSellValueRows != 1 ||
        valuedReport.unknownSellValueRows != 2) {
        return fail("Sell-value resolution counts should include resolved, approximate, and unknown rows.");
    }
    if (!contains(valuedText, "Sell value summary: resolved=5 approximate=1 unknown=2")) {
        return fail("Loaded sell-value report should show resolution summary.");
    }
    if (!contains(valuedText, "SELL id=1000") ||
        !contains(valuedText, "runes=11 sell_value=resolved")) {
        return fail("Exact sell value should be marked resolved and contribute runes.");
    }
    if (!contains(valuedText, "SELL id=2000100") ||
        !contains(valuedText, "runes=22 sell_value=approximate")) {
        return fail("Normalized sell value fallback should be marked approximate and contribute runes.");
    }
    if (!contains(valuedText, "SELL id=3000") ||
        !contains(valuedText, "runes=0 sell_value=unknown")) {
        return fail("Missing sell value should be marked unknown and contribute zero runes.");
    }

    std::cout << text;
    return EXIT_SUCCESS;
}
