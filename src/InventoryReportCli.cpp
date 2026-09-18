#include "Config.h"
#include "InventoryFixture.h"
#include "InventoryReport.h"
#include "ItemMetadata.h"
#include "SellValueMetadata.h"

#include <iostream>

namespace {

void printUsage() {
    std::cerr << "Usage: InventoryReportCli <config.ini> <inventory-fixture.txt>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        printUsage();
        return 2;
    }

    const auto config = erdup::loadConfig(argv[1]);
    const auto fixture = erdup::loadInventoryFixture(argv[2]);
    if (!fixture.warnings.empty()) {
        for (const auto& warning : fixture.warnings) {
            std::cerr << "fixture warning: " << warning << "\n";
        }
    }
    if (fixture.items.empty()) {
        std::cerr << "No fixture inventory items were loaded.\n";
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

    const auto report = erdup::buildInventoryReport(
        fixture.items,
        config,
        metadata.size() > 0 ? &metadata : nullptr,
        config.sellValuePath.empty() ? nullptr : &sellValues
    );

    std::cout << erdup::formatInventoryReport(report);
    return 0;
}
