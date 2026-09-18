#include "ConfigFile.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::filesystem::path defaultConfigPath() {
    const auto cwd = std::filesystem::current_path();
    const auto local = cwd / "duplicate_seller.ini";
    if (std::filesystem::exists(local)) {
        return local;
    }

    const auto repoConfig = cwd / "config" / "duplicate_seller.ini";
    if (std::filesystem::exists(repoConfig)) {
        return repoConfig;
    }

    return local;
}

std::string displayValue(const erdup::ConfigFileDocument& document, const std::string& key) {
    const auto value = document.getValue(key);
    if (!value.has_value()) {
        return "<missing>";
    }
    if (value.value().empty()) {
        return "<blank>";
    }
    return value.value();
}

void printSettings(
    const erdup::ConfigFileDocument& document,
    std::vector<const erdup::ConfigSettingDefinition*>& editableByNumber
) {
    editableByNumber.clear();
    std::string currentGroup;
    int number = 1;

    std::cout << "\nERDuplicateSeller Settings\n";
    std::cout << "This tool edits only duplicate_seller.ini. It does not touch game memory.\n";
    std::cout << "dry_run is intentionally read-only here; keep it true until mutation exists.\n";

    for (const auto& setting : erdup::configSettingDefinitions()) {
        if (setting.group != currentGroup) {
            currentGroup = setting.group;
            std::cout << "\n[" << currentGroup << "]\n";
        }

        if (setting.editable) {
            editableByNumber.push_back(&setting);
            std::cout << number++ << ". ";
        } else {
            std::cout << "-  ";
        }

        std::cout << setting.key << " = " << displayValue(document, setting.key);
        if (!setting.editable) {
            std::cout << " (read-only)";
        }
        if (!setting.note.empty()) {
            std::cout << " -- " << setting.note;
        }
        std::cout << "\n";
    }
}

bool editSetting(
    erdup::ConfigFileDocument& document,
    const std::vector<const erdup::ConfigSettingDefinition*>& editableByNumber,
    const std::string& command
) {
    try {
        std::size_t consumed = 0;
        const auto selected = std::stoul(command, &consumed, 10);
        if (consumed != command.size() || selected == 0 || selected > editableByNumber.size()) {
            std::cout << "Unknown selection.\n";
            return false;
        }

        const auto& setting = *editableByNumber[selected - 1];
        std::cout << setting.key << " current value: " << displayValue(document, setting.key) << "\n";
        if (!setting.note.empty()) {
            std::cout << setting.note << "\n";
        }
        std::cout << "New value (blank input cancels): ";
        std::string value;
        std::getline(std::cin, value);
        if (value.empty()) {
            std::cout << "Edit cancelled.\n";
            return false;
        }

        if (document.setValue(setting.key, value)) {
            std::cout << "Updated " << setting.key << ".\n";
            return true;
        }

        std::cout << setting.key << " already had that value.\n";
        return false;
    } catch (...) {
        std::cout << "Unknown selection.\n";
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto configPath = argc > 1 ? std::filesystem::path(argv[1]) : defaultConfigPath();
        auto document = erdup::ConfigFileDocument::load(configPath);
        bool dirty = false;

        std::cout << "Config file: " << configPath.string() << "\n";
        for (;;) {
            std::vector<const erdup::ConfigSettingDefinition*> editableByNumber;
            printSettings(document, editableByNumber);
            std::cout << "\nEnter a number to edit, s to save, q to quit: ";

            std::string command;
            std::getline(std::cin, command);
            if (command == "q" || command == "Q") {
                if (dirty) {
                    std::cout << "Unsaved changes remain. Use s to save or q again to discard: ";
                    std::getline(std::cin, command);
                    if (command != "q" && command != "Q") {
                        continue;
                    }
                }
                return 0;
            }
            if (command == "s" || command == "S") {
                document.save(configPath);
                dirty = false;
                std::cout << "Saved " << configPath.string() << ".\n";
                continue;
            }

            dirty = editSetting(document, editableByNumber, command) || dirty;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Config editor failed: " << ex.what() << "\n";
        return 1;
    }
}
