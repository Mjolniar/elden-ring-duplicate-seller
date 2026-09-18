#include "InventoryFixture.h"
#include "InventoryReport.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

const erdup::InventoryReportRow* findRow(
    const erdup::InventoryReport& report,
    std::uint32_t baseItemId,
    bool selectedForSale
) {
    for (const auto& row : report.rows) {
        if (row.item.baseItemId == baseItemId && row.selectedForSale == selectedForSale) {
            return &row;
        }
    }
    return nullptr;
}

int soldQuantityFor(const erdup::InventoryReport& report, std::uint32_t baseItemId) {
    int quantity = 0;
    for (const auto& row : report.rows) {
        if (row.item.baseItemId == baseItemId && row.selectedForSale) {
            quantity += row.quantityToSell;
        }
    }
    return quantity;
}

bool hasReason(const erdup::InventoryReport& report, std::uint32_t baseItemId, const std::string& reason) {
    for (const auto& row : report.rows) {
        if (row.item.baseItemId == baseItemId && row.reason == reason) {
            return true;
        }
    }
    return false;
}

erdup::InventoryFixtureResult parseFixtureOrFail(const std::string& fixture) {
    auto parsed = erdup::parseInventoryFixture(fixture);
    if (!parsed.warnings.empty()) {
        std::cerr << "Unexpected fixture warning: " << parsed.warnings.front() << "\n";
        std::exit(EXIT_FAILURE);
    }
    return parsed;
}

int assertDefaultPolicyContract() {
    const auto parsed = parseFixtureOrFail(
        "item=instance:1,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:100,name:Base Sword\n"
        "item=instance:2,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:100,name:Base Sword\n"
        "item=instance:3,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:100,name:Base Sword\n"
        "item=instance:4,id:1100000,category:weapon,qty:1,upgrade:0,modified:false,sell:100,name:Plain Upgrade Test Sword\n"
        "item=instance:5,id:1100003,category:weapon,qty:1,upgrade:3,modified:false,sell:100,name:Upgrade Test Sword +3\n"
        "item=instance:6,id:1100005,category:weapon,qty:1,upgrade:5,modified:false,sell:100,name:Upgrade Test Sword +5\n"
        "item=instance:7,id:1200100,category:weapon,qty:1,upgrade:0,modified:true,sell:100,name:Heavy Modified Sword\n"
        "item=instance:8,id:1200200,category:weapon,qty:1,upgrade:0,modified:true,sell:100,name:Keen Modified Sword\n"
        "item=instance:9,id:1200300,category:weapon,qty:1,upgrade:0,modified:true,sell:100,name:Quality Modified Sword\n"
        "item=instance:10,id:2000,category:ash_of_war,qty:1,upgrade:0,modified:false,sell:40,name:Debug Ash\n"
        "item=instance:11,id:2000,category:ash_of_war,qty:1,upgrade:0,modified:false,sell:40,name:Debug Ash\n"
        "item=instance:12,id:3000,category:armor,qty:1,upgrade:0,modified:false,sell:50,name:Debug Armor\n"
        "item=instance:13,id:3000,category:armor,qty:1,upgrade:0,modified:false,sell:50,name:Debug Armor\n"
        "item=instance:14,id:4000,category:talisman,qty:1,upgrade:0,modified:false,sell:25,name:Debug Talisman\n"
        "item=instance:15,id:4000,category:talisman,qty:1,upgrade:0,modified:false,sell:25,name:Debug Talisman\n"
        "item=instance:16,id:5000,category:consumable,qty:99,upgrade:0,modified:false,sell:2,name:Debug Consumable\n"
        "item=instance:17,id:6000,category:material,qty:99,upgrade:0,modified:false,sell:1,name:Debug Material\n"
        "item=instance:18,id:7000,category:keyitem,qty:1,upgrade:0,modified:false,sell:0,name:Debug Key\n"
        "item=instance:19,id:8000,category:unknown,qty:1,upgrade:0,modified:false,sell:0,name:Unknown Thing\n"
    );

    const erdup::Config config;
    const auto report = erdup::buildInventoryReport(parsed.items, config);

    if (report.totalItemsToSell != 6 || report.totalRunes != 415) {
        return fail("Default contract should sell lower-priority weapon duplicates plus ash, armor, and talisman duplicates for 415 runes.");
    }
    if (soldQuantityFor(report, 1000) != 1 ||
        soldQuantityFor(report, 1100000) != 1 ||
        soldQuantityFor(report, 1200300) != 1 ||
        soldQuantityFor(report, 2000) != 1 ||
        soldQuantityFor(report, 3000) != 1 ||
        soldQuantityFor(report, 4000) != 1) {
        return fail("Default contract did not sell the expected lower-priority duplicate rows.");
    }
    if (soldQuantityFor(report, 1100003) != 0 || soldQuantityFor(report, 1100005) != 0 ||
        soldQuantityFor(report, 1200100) != 0 || soldQuantityFor(report, 1200200) != 0 ||
        soldQuantityFor(report, 5000) != 0 || soldQuantityFor(report, 6000) != 0 ||
        soldQuantityFor(report, 7000) != 0 || soldQuantityFor(report, 8000) != 0) {
        return fail("Default contract sold a kept high-priority weapon, consumable, safe-excluded, or unknown item.");
    }

    const auto* soldWeapon = findRow(report, 1000, true);
    if (soldWeapon == nullptr || soldWeapon->keepCount != 2 ||
        soldWeapon->quantityToSell != 1 || soldWeapon->reason != "duplicate above keep count") {
        return fail("Weapon duplicate row did not show keep 2 and duplicate sale reason.");
    }
    const auto* soldAsh = findRow(report, 2000, true);
    if (soldAsh == nullptr || soldAsh->keepCount != 1 || soldAsh->quantityToSell != 1) {
        return fail("Ash of War duplicate row did not show keep 1 and sell 1.");
    }
    const auto* soldArmor = findRow(report, 3000, true);
    if (soldArmor == nullptr || soldArmor->keepCount != 1 || soldArmor->quantityToSell != 1) {
        return fail("Armor duplicate row did not show keep 1 and sell 1.");
    }
    const auto* soldTalisman = findRow(report, 4000, true);
    if (soldTalisman == nullptr || soldTalisman->keepCount != 1 || soldTalisman->quantityToSell != 1) {
        return fail("Talisman duplicate row did not show keep 1 and sell 1.");
    }

    if (!hasReason(report, 1000, "retained by keep count") ||
        !hasReason(report, 1100005, "retained by keep count") ||
        !hasReason(report, 1100003, "retained by keep count") ||
        !hasReason(report, 1200100, "retained by keep count") ||
        !hasReason(report, 1200200, "retained by keep count") ||
        !hasReason(report, 5000, "consumable selling disabled") ||
        !hasReason(report, 6000, "safe-excluded category skipped") ||
        !hasReason(report, 7000, "safe-excluded category skipped") ||
        !hasReason(report, 8000, "unknown category skipped")) {
        return fail("Default contract report is missing one or more representative row reasons.");
    }

    return EXIT_SUCCESS;
}

int assertConsumableStockContract() {
    const auto parsed = parseFixtureOrFail(
        "item=instance:1,id:9000,category:consumable,qty:40,upgrade:0,modified:false,sell:3,name:Debug Arrow\n"
    );

    erdup::Config config;
    config.sellConsumables = true;
    config.itemStockRules[9000] = 25;

    const auto report = erdup::buildInventoryReport(parsed.items, config);
    if (report.totalItemsToSell != 15 || report.totalRunes != 45) {
        return fail("Consumable stock contract should sell only the 15 items above configured stock.");
    }

    const auto* stockRow = findRow(report, 9000, true);
    if (stockRow == nullptr || stockRow->keepCount != 25 ||
        stockRow->quantityToSell != 15 || stockRow->reason != "duplicate above keep count") {
        return fail("Consumable stock row did not show keep 25 and sell 15.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertDefaultPolicyContract() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (assertConsumableStockContract() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Default policy contract test passed.\n";
    return EXIT_SUCCESS;
}
