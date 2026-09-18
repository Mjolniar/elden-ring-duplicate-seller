#include "Config.h"
#include "ItemMetadata.h"
#include "SellValueMetadata.h"
#include "SnapshotSalePlan.h"

#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cerr << "Usage: SnapshotSalePlanCli <config.ini> <inventory-snapshot.txt> "
                 "[mark_empty|mark_empty_and_decrement_count|compact_and_decrement_count]\n";
}

erdup::CustomRemovalMode parseMode(const std::string& value, bool& ok) {
    ok = true;
    if (value == "mark_empty") {
        return erdup::CustomRemovalMode::MarkEmpty;
    }
    if (value == "mark_empty_and_decrement_count") {
        return erdup::CustomRemovalMode::MarkEmptyAndDecrementCount;
    }
    if (value == "compact_and_decrement_count" || value == "compact") {
        return erdup::CustomRemovalMode::CompactAndDecrementCount;
    }

    ok = false;
    return erdup::CustomRemovalMode::MarkEmptyAndDecrementCount;
}

void printSnapshotMessages(const erdup::InventorySnapshotFixtureResult& snapshot) {
    for (const auto& warning : snapshot.warnings) {
        std::cerr << "snapshot warning: " << warning << "\n";
    }
    for (const auto& error : snapshot.errors) {
        std::cerr << "snapshot error: " << error << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        printUsage();
        return 2;
    }

    auto mode = erdup::CustomRemovalMode::MarkEmptyAndDecrementCount;
    if (argc == 4) {
        bool modeOk = false;
        mode = parseMode(argv[3], modeOk);
        if (!modeOk) {
            printUsage();
            return 2;
        }
    }

    const auto config = erdup::loadConfig(argv[1]);
    const auto snapshot = erdup::loadInventorySnapshotFixture(argv[2]);
    printSnapshotMessages(snapshot);
    if (!snapshot.errors.empty()) {
        return 1;
    }

    const auto metadataLoad = erdup::loadConfiguredMetadata(config);
    erdup::MetadataIndex metadata;
    metadata.add(metadataLoad);
    for (const auto& warning : metadata.warnings()) {
        std::cerr << "metadata warning: " << warning << "\n";
    }

    const auto sellValueLoad = erdup::loadConfiguredSellValues(config);
    erdup::SellValueIndex sellValues;
    sellValues.add(sellValueLoad);
    for (const auto& warning : sellValues.warnings()) {
        std::cerr << "sell value warning: " << warning << "\n";
    }

    const auto plan = erdup::buildSnapshotSalePlan(
        snapshot,
        config,
        mode,
        metadata.size() > 0 ? &metadata : nullptr,
        config.sellValuePath.empty() ? nullptr : &sellValues
    );

    std::cout << erdup::formatSnapshotSalePlanSummary(plan) << "\n";
    std::cout << erdup::formatCustomRemovalPlan(plan.removalPlan) << "\n";
    std::cout << erdup::formatInventoryReport(plan.report);

    return plan.errors.empty() ? 0 : 1;
}
