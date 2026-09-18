#include "ConfigFile.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int assertRoundTripPreservesBytes() {
    const std::string original =
        "# heading\r\n"
        "\r\n"
        "enabled=true\r\n"
        "unknown_key = keep me ; unknown inline comment\r\n"
        "; comment\r\n"
        "metadata_weapon_names_path=C:\\mods\\weapon.txt\r\n";

    const auto document = erdup::ConfigFileDocument::parse(original);
    if (document.serialize() != original) {
        return fail("ConfigFileDocument should preserve an unedited file byte-for-byte.");
    }
    if (!document.getValue("enabled").has_value() || document.getValue("enabled").value() != "true") {
        return fail("ConfigFileDocument should read known key values.");
    }
    if (!document.getValue("unknown_key").has_value() || document.getValue("unknown_key").value() != "keep me") {
        return fail("ConfigFileDocument should preserve and read unknown key values.");
    }

    return EXIT_SUCCESS;
}

int assertEditingOneKeyChangesOnlyThatLine() {
    const std::string original =
        "# heading\n"
        "keep_weapons=2\n"
        "sell_weapons=true\n"
        "unknown_key=abc\n";

    auto document = erdup::ConfigFileDocument::parse(original);
    if (!document.setValue("keep_weapons", "3")) {
        return fail("Editing an existing key should report a change.");
    }

    const std::string expected =
        "# heading\n"
        "keep_weapons=3\n"
        "sell_weapons=true\n"
        "unknown_key=abc\n";
    if (document.serialize() != expected) {
        std::cerr << "Actual:\n" << document.serialize() << "\n";
        return fail("Editing one key should change only that key line.");
    }

    return EXIT_SUCCESS;
}

int assertDuplicateKeyUsesLastActiveLine() {
    const std::string original =
        "keep_weapons=1\n"
        "# keep_weapons=ignored\n"
        "keep_weapons=2 ; active last value\n";

    auto document = erdup::ConfigFileDocument::parse(original);
    if (!document.getValue("KEEP_WEAPONS").has_value() || document.getValue("KEEP_WEAPONS").value() != "2") {
        return fail("ConfigFileDocument should use the last active value for duplicate keys.");
    }
    document.setValue("keep_weapons", "4");

    const std::string expected =
        "keep_weapons=1\n"
        "# keep_weapons=ignored\n"
        "keep_weapons=4 ; active last value\n";
    if (document.serialize() != expected) {
        return fail("Editing a duplicate key should change the last active line only.");
    }

    return EXIT_SUCCESS;
}

int assertMissingKnownKeyIsAddedNearGroup() {
    const std::string original =
        "# Metadata section\n"
        "metadata_iteminfo_path=\n"
        "metadata_weapon_names_path=C:\\mods\\weapon.txt\n"
        "metadata_goods_names_path=\n"
        "\n"
        "inventory_read_enabled=false\n";

    auto document = erdup::ConfigFileDocument::parse(original);
    if (!document.setValue("sell_value_path", "C:\\mods\\values.txt")) {
        return fail("Adding a missing known key should report a change.");
    }

    const std::string expected =
        "# Metadata section\n"
        "metadata_iteminfo_path=\n"
        "metadata_weapon_names_path=C:\\mods\\weapon.txt\n"
        "metadata_goods_names_path=\n"
        "sell_value_path=C:\\mods\\values.txt\n"
        "\n"
        "inventory_read_enabled=false\n";
    if (document.serialize() != expected) {
        std::cerr << "Actual:\n" << document.serialize() << "\n";
        return fail("Missing sell_value_path should be inserted with metadata/value settings.");
    }

    return EXIT_SUCCESS;
}

int assertDefinitionsExposeRequiredGroups() {
    const auto& definitions = erdup::configSettingDefinitions();
    const std::string groups = [&]() {
        std::string all;
        for (const auto& setting : definitions) {
            all += setting.group + "\n";
        }
        return all;
    }();

    if (!contains(groups, "Policy keep counts") ||
        !contains(groups, "Policy category toggles") ||
        !contains(groups, "Metadata and value paths") ||
        !contains(groups, "Inventory offsets") ||
        !contains(groups, "Diagnostics")) {
        return fail("Config setting definitions should expose the required editor groups.");
    }
    const auto* dryRun = erdup::findConfigSettingDefinition("dry_run");
    if (dryRun == nullptr || dryRun->editable) {
        return fail("dry_run should be visible but read-only in the settings editor.");
    }
    if (erdup::findConfigSettingDefinition("sell_value_path") == nullptr) {
        return fail("sell_value_path should be listed in the settings editor definitions.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertRoundTripPreservesBytes() != EXIT_SUCCESS ||
        assertEditingOneKeyChangesOnlyThatLine() != EXIT_SUCCESS ||
        assertDuplicateKeyUsesLastActiveLine() != EXIT_SUCCESS ||
        assertMissingKnownKeyIsAddedNearGroup() != EXIT_SUCCESS ||
        assertDefinitionsExposeRequiredGroups() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Config file editor core test passed.\n";
    return EXIT_SUCCESS;
}
