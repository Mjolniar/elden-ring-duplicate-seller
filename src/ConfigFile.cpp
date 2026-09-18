#include "ConfigFile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace erdup {

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool startsWithComment(const std::string& value) {
    const auto first = std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    return first != value.end() && (*first == '#' || *first == ';');
}

std::size_t firstCommentOffset(const std::string& value) {
    const auto comment = value.find_first_of("#;");
    return comment == std::string::npos ? value.size() : comment;
}

ConfigFileDocument::Line parseLine(std::string content, std::string newline) {
    ConfigFileDocument::Line line;
    line.content = std::move(content);
    line.newline = std::move(newline);

    if (trim(line.content).empty() || startsWithComment(line.content)) {
        return line;
    }

    const auto commentOffset = firstCommentOffset(line.content);
    const auto equals = line.content.find('=');
    if (equals == std::string::npos || equals > commentOffset) {
        return line;
    }

    const auto key = normalizeConfigKey(line.content.substr(0, equals));
    if (key.empty()) {
        return line;
    }

    line.activeSetting = true;
    line.key = key;
    line.valueBegin = equals + 1;
    line.valueEnd = commentOffset;
    while (line.valueBegin < line.valueEnd &&
           std::isspace(static_cast<unsigned char>(line.content[line.valueBegin]))) {
        ++line.valueBegin;
    }
    while (line.valueEnd > line.valueBegin &&
           std::isspace(static_cast<unsigned char>(line.content[line.valueEnd - 1]))) {
        --line.valueEnd;
    }

    return line;
}

std::string readWholeFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open config file: " + path.string());
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

const std::vector<ConfigSettingDefinition> kConfigSettings = {
    {"dry_run", "Safety", "Dry run", "Read-only safety flag. Keep true until mutation is implemented.", false},
    {"enabled", "Safety", "Enabled", "Enables planning/reporting only; this is not a selling toggle.", true},
    {"supported_game_version", "Safety", "Supported game version", "Version gate used by diagnostics.", true},
    {"hotkey_vk", "Safety", "Hotkey virtual key", "Windows virtual-key code for the duplicate-seller hotkey.", true},

    {"keep_count", "Policy keep counts", "Fallback keep count", "", true},
    {"keep_weapons", "Policy keep counts", "Weapons to keep", "Keep-best policy uses highest decoded upgrade first.", true},
    {"keep_shields", "Policy keep counts", "Shields to keep", "", true},
    {"keep_armor", "Policy keep counts", "Armor to keep", "", true},
    {"keep_talismans", "Policy keep counts", "Talismans to keep", "", true},
    {"keep_ashes_of_war", "Policy keep counts", "Ashes of War to keep", "", true},
    {"keep_spells", "Policy keep counts", "Spells to keep", "", true},
    {"keep_consumables", "Policy keep counts", "Consumables to keep", "Only used when consumable selling is enabled.", true},
    {"item_stock_rules", "Policy keep counts", "Item stock rules", "Comma-separated itemId:keepCount entries.", true},

    {"sell_weapons", "Policy category toggles", "Plan weapon duplicates", "", true},
    {"sell_shields", "Policy category toggles", "Plan shield duplicates", "", true},
    {"sell_armor", "Policy category toggles", "Plan armor duplicates", "", true},
    {"sell_talismans", "Policy category toggles", "Plan talisman duplicates", "", true},
    {"sell_ashes_of_war", "Policy category toggles", "Plan Ash of War duplicates", "", true},
    {"sell_spells", "Policy category toggles", "Plan spell duplicates", "", true},
    {"sell_consumables", "Policy category toggles", "Plan consumables", "Default false.", true},
    {"require_unupgraded", "Policy category toggles", "Require unupgraded non-weapons", "", true},
    {"require_unmodified", "Policy category toggles", "Require unmodified non-weapons", "", true},

    {"metadata_iteminfo_path", "Metadata and value paths", "iteminfo metadata path", "", true},
    {"metadata_weapon_names_path", "Metadata and value paths", "Weapon names path", "", true},
    {"metadata_protector_names_path", "Metadata and value paths", "Armor names path", "", true},
    {"metadata_accessory_names_path", "Metadata and value paths", "Talisman names path", "", true},
    {"metadata_goods_names_path", "Metadata and value paths", "Goods names path", "", true},
    {"sell_value_path", "Metadata and value paths", "Sell-value table path", "Optional read-only dry-run rune estimates.", true},

    {"memory_scan_enabled", "Diagnostics", "Memory scan enabled", "Read-only value scanner diagnostic.", true},
    {"memory_scan_hotkey_vk", "Diagnostics", "Memory scan hotkey", "", true},
    {"memory_scan_value_size", "Diagnostics", "Memory scan value size", "", true},
    {"memory_scan_max_bytes_mb", "Diagnostics", "Memory scan max MB", "", true},
    {"memory_scan_max_regions", "Diagnostics", "Memory scan max regions", "", true},
    {"memory_scan_max_candidates", "Diagnostics", "Memory scan max candidates", "", true},
    {"pointer_scan_hotkey_vk", "Diagnostics", "Pointer scan hotkey", "", true},
    {"pointer_scan_max_depth", "Diagnostics", "Pointer scan max depth", "", true},
    {"pointer_scan_offset_window", "Diagnostics", "Pointer scan offset window", "", true},
    {"auto_scan_enabled", "Diagnostics", "Auto scan enabled", "", true},
    {"auto_scan_pointer_threshold", "Diagnostics", "Auto scan pointer threshold", "", true},
    {"pointer_chain_verify_enabled", "Diagnostics", "Pointer chain verify enabled", "", true},
    {"field_dump_enabled", "Diagnostics", "Field dump enabled", "Read-only struct field diagnostic.", true},
    {"field_dump_chain", "Diagnostics", "Field dump chain", "", true},
    {"field_dump_bytes", "Diagnostics", "Field dump bytes", "", true},
    {"field_dump_stride", "Diagnostics", "Field dump stride", "", true},

    {"inventory_read_enabled", "Inventory offsets", "Inventory read enabled", "Read-only diagnostic inventory decode.", true},
    {"inventory_base_chain", "Inventory offsets", "Inventory base chain", "", true},
    {"inventory_array_offset", "Inventory offsets", "Inventory array offset", "", true},
    {"inventory_count_offset", "Inventory offsets", "Inventory count offset", "", true},
    {"inventory_entry_stride", "Inventory offsets", "Inventory entry stride", "", true},
    {"inventory_handle_offset", "Inventory offsets", "Inventory handle offset", "", true},
    {"inventory_itemid_offset", "Inventory offsets", "Inventory item id offset", "", true},
    {"inventory_quantity_offset", "Inventory offsets", "Inventory quantity offset", "", true},
    {"inventory_max_entries", "Inventory offsets", "Inventory max entries", "", true},
    {"inventory_snapshot_export_enabled", "Inventory offsets", "Snapshot export enabled", "Writes read-only snapshot fixture files beside the DLL.", true},
    {"inventory_snapshot_auto_interval_seconds", "Inventory offsets", "Snapshot auto interval", "Cooldown after each completed read-only snapshot. Zero disables automatic snapshots.", true},
    {"inventory_snapshot_auto_run_on_start", "Inventory offsets", "Snapshot run on start", "Runs one snapshot as soon as the schedule becomes active.", true},
    {"inventory_snapshot_retention_count", "Inventory offsets", "Snapshot retention count", "Keeps only the newest matching snapshot files. Zero disables cleanup.", true},
    {"inventory_snapshot_header_offset", "Inventory offsets", "Snapshot header offset", "Read-only header qword dump start offset for snapshot evidence.", true},
    {"inventory_snapshot_header_bytes", "Inventory offsets", "Snapshot header bytes", "Read-only header qword dump byte count.", true},
    {"inventory_snapshot_focus_start", "Inventory offsets", "Snapshot focus start", "First raw inventory row to dump into snapshot comments.", true},
    {"inventory_snapshot_focus_count", "Inventory offsets", "Snapshot focus count", "Number of raw inventory rows to dump into snapshot comments.", true},

    {"inventory_entry_diagnostics_enabled", "Diagnostics", "Entry diagnostics enabled", "", true},
    {"inventory_entry_diagnostics_max", "Diagnostics", "Entry diagnostics max rows", "", true},
    {"inventory_entry_diagnostics_include_consumables", "Diagnostics", "Entry diagnostics include consumables", "", true},
    {"inventory_handle_diagnostics_enabled", "Diagnostics", "Handle diagnostics enabled", "", true},
    {"inventory_handle_diagnostics_max_handles", "Diagnostics", "Handle diagnostics max handles", "", true},
    {"inventory_handle_diagnostics_max_matches_per_handle", "Diagnostics", "Handle diagnostics matches per handle", "", true},
    {"inventory_handle_diagnostics_max_bytes_mb", "Diagnostics", "Handle diagnostics max MB", "", true},
    {"inventory_handle_diagnostics_include_consumables", "Diagnostics", "Handle diagnostics include consumables", "", true},
    {"inventory_compact_row_diagnostics_enabled", "Diagnostics", "Compact row diagnostics enabled", "", true},
    {"inventory_compact_row_diagnostics_max_targets", "Diagnostics", "Compact row diagnostics max targets", "", true},
    {"inventory_compact_row_diagnostics_max_rows", "Diagnostics", "Compact row diagnostics max rows", "", true},
    {"inventory_compact_row_diagnostics_max_bytes_mb", "Diagnostics", "Compact row diagnostics max MB", "", true},
    {"inventory_compact_row_diagnostics_min_run_length", "Diagnostics", "Compact row diagnostics min run", "", true},
    {"inventory_compact_row_diagnostics_max_runs", "Diagnostics", "Compact row diagnostics max runs", "", true},
    {"inventory_compact_row_diagnostics_include_consumables", "Diagnostics", "Compact row diagnostics include consumables", "", true},
    {"inventory_compact_row_dump_enabled", "Diagnostics", "Compact row dump enabled", "", true},
    {"inventory_compact_row_dump_max_runs", "Diagnostics", "Compact row dump max runs", "", true},
    {"inventory_compact_row_dump_max_rows_per_run", "Diagnostics", "Compact row dump max rows per run", "", true},
    {"inventory_compact_row_dump_backtrack_bytes", "Diagnostics", "Compact row dump backtrack bytes", "", true},
    {"inventory_compact_row_dump_max_stride_bytes", "Diagnostics", "Compact row dump max stride bytes", "", true},
};

} // namespace

std::string normalizeConfigKey(std::string key) {
    key = trim(std::move(key));
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

const std::vector<ConfigSettingDefinition>& configSettingDefinitions() {
    return kConfigSettings;
}

const ConfigSettingDefinition* findConfigSettingDefinition(const std::string& key) {
    const auto normalized = normalizeConfigKey(key);
    for (const auto& setting : kConfigSettings) {
        if (setting.key == normalized) {
            return &setting;
        }
    }
    return nullptr;
}

ConfigFileDocument ConfigFileDocument::parse(const std::string& text) {
    ConfigFileDocument document;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto newlineOffset = text.find('\n', offset);
        const auto lineEnd = newlineOffset == std::string::npos ? text.size() : newlineOffset;
        std::string content = text.substr(offset, lineEnd - offset);
        std::string newline;
        if (!content.empty() && content.back() == '\r') {
            content.pop_back();
            newline = "\r\n";
        } else if (newlineOffset != std::string::npos) {
            newline = "\n";
        }

        document.lines_.push_back(parseLine(std::move(content), std::move(newline)));
        if (newlineOffset == std::string::npos) {
            break;
        }
        offset = newlineOffset + 1;
    }

    return document;
}

ConfigFileDocument ConfigFileDocument::load(const std::filesystem::path& path) {
    return parse(readWholeFile(path));
}

std::optional<std::string> ConfigFileDocument::getValue(const std::string& key) const {
    const auto normalized = normalizeConfigKey(key);
    const auto lineIndex = findLastActiveKey(normalized);
    if (!lineIndex.has_value()) {
        return std::nullopt;
    }

    const auto& line = lines_[lineIndex.value()];
    return trim(line.content.substr(line.valueBegin, line.valueEnd - line.valueBegin));
}

bool ConfigFileDocument::setValue(const std::string& key, const std::string& value) {
    const auto normalized = normalizeConfigKey(key);
    if (normalized.empty()) {
        return false;
    }

    if (const auto lineIndex = findLastActiveKey(normalized)) {
        auto& line = lines_[lineIndex.value()];
        if (line.content.substr(line.valueBegin, line.valueEnd - line.valueBegin) == value) {
            return false;
        }

        line.content.replace(line.valueBegin, line.valueEnd - line.valueBegin, value);
        line = parseLine(std::move(line.content), std::move(line.newline));
        return true;
    }

    const auto insertion = insertionIndexForMissingKey(normalized);
    const auto newline = preferredNewlineForInsertion(insertion);
    if (!lines_.empty() && insertion == lines_.size() && lines_.back().newline.empty()) {
        lines_.back().newline = newline;
    }

    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(insertion),
        parseLine(normalized + "=" + value, newline));
    return true;
}

std::string ConfigFileDocument::serialize() const {
    std::string text;
    for (const auto& line : lines_) {
        text += line.content;
        text += line.newline;
    }
    return text;
}

void ConfigFileDocument::save(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Could not write config file: " + path.string());
    }
    file << serialize();
}

std::optional<std::size_t> ConfigFileDocument::findLastActiveKey(const std::string& normalizedKey) const {
    for (std::size_t i = lines_.size(); i > 0; --i) {
        if (lines_[i - 1].activeSetting && lines_[i - 1].key == normalizedKey) {
            return i - 1;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ConfigFileDocument::findFirstActiveKey(const std::string& normalizedKey) const {
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].activeSetting && lines_[i].key == normalizedKey) {
            return i;
        }
    }
    return std::nullopt;
}

std::size_t ConfigFileDocument::insertionIndexForMissingKey(const std::string& normalizedKey) const {
    const auto* setting = findConfigSettingDefinition(normalizedKey);
    if (setting == nullptr) {
        return lines_.size();
    }

    const auto& settings = configSettingDefinitions();
    const auto current = std::find_if(settings.begin(), settings.end(), [&](const ConfigSettingDefinition& candidate) {
        return candidate.key == normalizedKey;
    });
    if (current == settings.end()) {
        return lines_.size();
    }

    for (auto it = std::make_reverse_iterator(current); it != settings.rend(); ++it) {
        if (it->group == setting->group) {
            if (const auto found = findLastActiveKey(it->key)) {
                return found.value() + 1;
            }
        }
    }

    for (auto it = current + 1; it != settings.end(); ++it) {
        if (it->group == setting->group) {
            if (const auto found = findFirstActiveKey(it->key)) {
                return found.value();
            }
        }
    }

    return lines_.size();
}

std::string ConfigFileDocument::preferredNewlineForInsertion(std::size_t index) const {
    if (index < lines_.size() && !lines_[index].newline.empty()) {
        return lines_[index].newline;
    }
    if (index > 0 && !lines_[index - 1].newline.empty()) {
        return lines_[index - 1].newline;
    }
    for (const auto& line : lines_) {
        if (!line.newline.empty()) {
            return line.newline;
        }
    }
    return "\n";
}

} // namespace erdup
