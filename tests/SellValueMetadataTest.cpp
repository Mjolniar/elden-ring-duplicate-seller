#include "Config.h"
#include "SellValueMetadata.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

std::filesystem::path writeTempSellValues(const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() /
        ("erdup_sell_values_test_" + std::to_string(std::rand()) + ".txt");
    std::ofstream file(path, std::ios::trunc);
    file << contents;
    return path;
}

} // namespace

int main() {
    const auto unsellable = erdup::parseSellValueMetadata("weapon 17030000 unsellable\nweapon 17030001 -5\n");
    if (unsellable.categorizedValues.size() != 1 ||
        unsellable.categorizedValues.at({erdup::ItemCategory::Weapon, 17030000}) != -1 ||
        unsellable.warnings.size() != 1)
        return fail("Explicit unsellable marker must be preserved; arbitrary negative prices remain invalid.");
    const auto parsed = erdup::parseSellValueMetadata(
        "# comment row\n"
        "; another comment row\n"
        "\n"
        "[ERROR] whole row skipped\n"
        "1000 10\n"
        "0x3E9 20\n"
        "weapon 1000 30\n"
        "ash_of_war 1000 40\n"
        "1002 [ERROR]\n"
        "bad 30\n"
        "1003 bad\n"
        "1004 0x2A ; inline comment\n",
        "sample"
    );

    if (parsed.values.size() != 3 ||
        parsed.categorizedValues.size() != 2 ||
        parsed.values.at(1000) != 10 ||
        parsed.values.at(1001) != 20 ||
        parsed.categorizedValues.at({erdup::ItemCategory::Weapon, 1000}) != 30 ||
        parsed.categorizedValues.at({erdup::ItemCategory::AshOfWar, 1000}) != 40 ||
        parsed.values.at(1004) != 42) {
        return fail("Sell-value parser should keep decimal, 0x, and category-qualified rows.");
    }
    if (parsed.warnings.size() != 2) {
        return fail("Sell-value parser should warn for invalid ID and invalid value rows.");
    }

    erdup::SellValueIndex index;
    index.add(parsed);
    if (!index.find(1000).has_value() || index.find(1000).value() != 10 ||
        !index.find(erdup::ItemCategory::Weapon, 1000).has_value() ||
        index.find(erdup::ItemCategory::Weapon, 1000).value() != 30 ||
        !index.find(erdup::ItemCategory::AshOfWar, 1000).has_value() ||
        index.find(erdup::ItemCategory::AshOfWar, 1000).value() != 40 ||
        !index.find(erdup::ItemCategory::Talisman, 1000).has_value() ||
        index.find(erdup::ItemCategory::Talisman, 1000).value() != 10 ||
        !index.find(1001).has_value() || index.find(1001).value() != 20 ||
        index.find(1002).has_value()) {
        return fail("Sell-value index lookup should prefer category-qualified rows and retain legacy fallback.");
    }

    const auto path = writeTempSellValues("0x10 5\n17 6\n");
    erdup::Config config;
    config.sellValuePath = path.string();
    const auto loaded = erdup::loadConfiguredSellValues(config);
    std::filesystem::remove(path);
    if (loaded.values.size() != 2 || loaded.values.at(0x10) != 5 || loaded.values.at(17) != 6) {
        return fail("Configured sell-value path did not load expected rows.");
    }

    const erdup::Config emptyConfig;
    const auto emptyLoaded = erdup::loadConfiguredSellValues(emptyConfig);
    if (!emptyLoaded.values.empty() || !emptyLoaded.warnings.empty()) {
        return fail("Empty sell_value_path should skip loading without warnings.");
    }

    const auto directory = std::filesystem::temp_directory_path() /
        ("erdup portable prices " + std::to_string(std::rand()));
    std::filesystem::create_directory(directory);
    std::ofstream(directory / "sell_values.txt") << "weapon 1000 123\n";
    std::ofstream(directory / "custom.txt") << "weapon 1000 456\n";
    config.nativeSaleEnabled = true;
    config.sellValuePath.clear();
    auto portable = erdup::loadConfiguredSellValues(config, directory);
    config.sellValuePath = "custom.txt";
    auto relative = erdup::loadConfiguredSellValues(config, directory);
    config.sellValuePath = (directory / "custom.txt").string();
    auto absolute = erdup::loadConfiguredSellValues(config, directory / "missing");
    config.sellValuePath = "missing.txt";
    auto missing = erdup::loadConfiguredSellValues(config, directory);
    std::filesystem::remove_all(directory);
    if (portable.categorizedValues.at({erdup::ItemCategory::Weapon, 1000}) != 123 ||
        relative.categorizedValues.at({erdup::ItemCategory::Weapon, 1000}) != 456 ||
        absolute.categorizedValues.at({erdup::ItemCategory::Weapon, 1000}) != 456 ||
        missing.warnings.empty() || !missing.categorizedValues.empty()) {
        return fail("Native prices must resolve beside the DLL, honor absolute overrides, and report missing files.");
    }

    std::cout << "Sell-value metadata test passed.\n";
    return EXIT_SUCCESS;
}
