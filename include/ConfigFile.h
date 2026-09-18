#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace erdup {

struct ConfigSettingDefinition {
    std::string key;
    std::string group;
    std::string label;
    std::string note;
    bool editable = true;
};

class ConfigFileDocument {
public:
    struct Line {
        std::string content;
        std::string newline;
        bool activeSetting = false;
        std::string key;
        std::size_t valueBegin = 0;
        std::size_t valueEnd = 0;
    };

    static ConfigFileDocument parse(const std::string& text);
    static ConfigFileDocument load(const std::filesystem::path& path);

    std::optional<std::string> getValue(const std::string& key) const;
    bool setValue(const std::string& key, const std::string& value);

    std::string serialize() const;
    void save(const std::filesystem::path& path) const;

private:
    std::vector<Line> lines_;

    std::optional<std::size_t> findLastActiveKey(const std::string& normalizedKey) const;
    std::optional<std::size_t> findFirstActiveKey(const std::string& normalizedKey) const;
    std::size_t insertionIndexForMissingKey(const std::string& normalizedKey) const;
    std::string preferredNewlineForInsertion(std::size_t index) const;
};

std::string normalizeConfigKey(std::string key);
const std::vector<ConfigSettingDefinition>& configSettingDefinitions();
const ConfigSettingDefinition* findConfigSettingDefinition(const std::string& key);

} // namespace erdup
