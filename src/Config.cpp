#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>

namespace erdup {

namespace {

constexpr int kMaxFieldDumpBytes = 0x4000;
constexpr int kMaxInventoryConfigEntries = 4096;
constexpr int kMaxInventorySnapshotIntervalSeconds = 3600;

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parseBool(const std::string& value, bool fallback) {
    const auto normalized = lower(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

int parseInt(const std::string& value, int fallback) {
    try {
        return std::stoi(trim(value), nullptr, 0);
    } catch (...) {
        return fallback;
    }
}

int parseVirtualKey(const std::string& value, int fallback, bool allowZero) {
    const auto parsed = parseInt(value, fallback);
    if (allowZero && parsed == 0) {
        return 0;
    }
    if (parsed <= 0 || parsed > 0xff) {
        return fallback;
    }
    return parsed;
}

int parseClampedInt(const std::string& value, int fallback, int minimum, int maximum) {
    const auto parsed = parseInt(value, fallback);
    return std::clamp(parsed, minimum, maximum);
}

void parseStockRules(Config& config, const std::string& value) {
    std::istringstream input(value);
    std::string rule;
    while (std::getline(input, rule, ',')) {
        rule = trim(rule);
        if (rule.empty()) {
            continue;
        }

        const auto separator = rule.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        const auto itemId = parseInt(rule.substr(0, separator), -1);
        const auto keepCount = parseInt(rule.substr(separator + 1), -1);
        if (itemId < 0 || keepCount < 0) {
            continue;
        }

        config.itemStockRules[static_cast<std::uint32_t>(itemId)] = keepCount;
    }
}

void applySetting(Config& config, const std::string& rawKey, const std::string& rawValue) {
    const auto key = lower(trim(rawKey));
    const auto value = trim(rawValue);

    if (key == "enabled") config.enabled = parseBool(value, config.enabled);
    else if (key == "keep_count") config.keepCount = std::max(0, parseInt(value, config.keepCount));
    else if (key == "keep_weapons") config.keepWeapons = std::max(0, parseInt(value, config.keepWeapons));
    else if (key == "keep_shields") config.keepShields = std::max(0, parseInt(value, config.keepShields));
    else if (key == "keep_armor") config.keepArmor = std::max(0, parseInt(value, config.keepArmor));
    else if (key == "keep_talismans") config.keepTalismans = std::max(0, parseInt(value, config.keepTalismans));
    else if (key == "keep_ashes_of_war") config.keepAshesOfWar = std::max(0, parseInt(value, config.keepAshesOfWar));
    else if (key == "keep_spells") config.keepSpells = std::max(0, parseInt(value, config.keepSpells));
    else if (key == "keep_consumables") config.keepConsumables = std::max(0, parseInt(value, config.keepConsumables));
    else if (key == "hotkey_vk") config.hotkeyVirtualKey = parseVirtualKey(value, config.hotkeyVirtualKey, false);
    else if (key == "sell_weapons") config.sellWeapons = parseBool(value, config.sellWeapons);
    else if (key == "sell_shields") config.sellShields = parseBool(value, config.sellShields);
    else if (key == "sell_armor") config.sellArmor = parseBool(value, config.sellArmor);
    else if (key == "sell_talismans") config.sellTalismans = parseBool(value, config.sellTalismans);
    else if (key == "sell_ashes_of_war") config.sellAshesOfWar = parseBool(value, config.sellAshesOfWar);
    else if (key == "sell_spells") config.sellSpells = parseBool(value, config.sellSpells);
    else if (key == "sell_consumables") config.sellConsumables = parseBool(value, config.sellConsumables);
    else if (key == "item_stock_rules") parseStockRules(config, value);
    else if (key == "require_unupgraded") config.requireUnupgraded = parseBool(value, config.requireUnupgraded);
    else if (key == "require_unmodified") config.requireUnmodified = parseBool(value, config.requireUnmodified);
    else if (key == "dry_run") config.dryRun = parseBool(value, config.dryRun);
    else if (key == "native_sale_enabled") config.nativeSaleEnabled = parseBool(value, config.nativeSaleEnabled);
    else if (key == "native_sale_max_items") config.nativeSaleMaxItems = std::clamp(parseInt(value, 0), 0, 4096);
    else if (key == "supported_game_version") config.supportedGameVersion = value;
    else if (key == "memory_scan_enabled") config.memoryScanEnabled = parseBool(value, config.memoryScanEnabled);
    else if (key == "memory_scan_hotkey_vk") config.memoryScanHotkeyVirtualKey = parseVirtualKey(value, config.memoryScanHotkeyVirtualKey, true);
    else if (key == "memory_scan_value_size") config.memoryScanValueSize = parseInt(value, config.memoryScanValueSize);
    else if (key == "memory_scan_max_bytes_mb") config.memoryScanMaxBytesMb = std::max(1, parseInt(value, config.memoryScanMaxBytesMb));
    else if (key == "memory_scan_max_regions") config.memoryScanMaxRegions = std::max(1, parseInt(value, config.memoryScanMaxRegions));
    else if (key == "memory_scan_max_candidates") config.memoryScanMaxCandidates = std::max(1, parseInt(value, config.memoryScanMaxCandidates));
    else if (key == "pointer_scan_hotkey_vk") config.pointerScanHotkeyVirtualKey = parseVirtualKey(value, config.pointerScanHotkeyVirtualKey, true);
    else if (key == "pointer_scan_max_depth") config.pointerScanMaxDepth = std::max(1, parseInt(value, config.pointerScanMaxDepth));
    else if (key == "pointer_scan_offset_window") config.pointerScanOffsetWindow = std::max(0, parseInt(value, config.pointerScanOffsetWindow));
    else if (key == "auto_scan_enabled") config.autoScanEnabled = parseBool(value, config.autoScanEnabled);
    else if (key == "auto_scan_pointer_threshold") config.autoScanPointerThreshold = std::max(1, parseInt(value, config.autoScanPointerThreshold));
    else if (key == "pointer_chain_verify_enabled") config.pointerChainVerifyEnabled = parseBool(value, config.pointerChainVerifyEnabled);
    else if (key == "field_dump_enabled") config.fieldDumpEnabled = parseBool(value, config.fieldDumpEnabled);
    else if (key == "field_dump_chain") config.fieldDumpChain = value;
    else if (key == "field_dump_bytes") config.fieldDumpBytes = parseClampedInt(value, config.fieldDumpBytes, 0, kMaxFieldDumpBytes);
    else if (key == "field_dump_stride") config.fieldDumpStride = parseClampedInt(value, config.fieldDumpStride, 1, kMaxFieldDumpBytes);
    else if (key == "inventory_read_enabled") config.inventoryReadEnabled = parseBool(value, config.inventoryReadEnabled);
    else if (key == "inventory_base_chain") config.inventoryBaseChain = value;
    else if (key == "inventory_array_offset") config.inventoryArrayOffset = std::max(0, parseInt(value, config.inventoryArrayOffset));
    else if (key == "inventory_count_offset") config.inventoryCountOffset = std::max(0, parseInt(value, config.inventoryCountOffset));
    else if (key == "inventory_capacity_offset") config.inventoryCapacityOffset = std::max(0, parseInt(value, config.inventoryCapacityOffset));
    else if (key == "inventory_entry_stride") config.inventoryEntryStride = std::max(0, parseInt(value, config.inventoryEntryStride));
    else if (key == "inventory_handle_offset") config.inventoryHandleOffset = std::max(0, parseInt(value, config.inventoryHandleOffset));
    else if (key == "inventory_itemid_offset") config.inventoryItemIdOffset = std::max(0, parseInt(value, config.inventoryItemIdOffset));
    else if (key == "inventory_quantity_offset") config.inventoryQuantityOffset = std::max(0, parseInt(value, config.inventoryQuantityOffset));
    else if (key == "inventory_max_entries") config.inventoryMaxEntries = parseClampedInt(value, config.inventoryMaxEntries, 1, kMaxInventoryConfigEntries);
    else if (key == "inventory_snapshot_export_enabled") config.inventorySnapshotExportEnabled = parseBool(value, config.inventorySnapshotExportEnabled);
    else if (key == "inventory_snapshot_auto_interval_seconds") config.inventorySnapshotAutoIntervalSeconds = parseClampedInt(value, config.inventorySnapshotAutoIntervalSeconds, 0, kMaxInventorySnapshotIntervalSeconds);
    else if (key == "inventory_snapshot_auto_run_on_start") config.inventorySnapshotAutoRunOnStart = parseBool(value, config.inventorySnapshotAutoRunOnStart);
    else if (key == "inventory_snapshot_auto_dry_run_report_enabled") config.inventorySnapshotAutoDryRunReportEnabled = parseBool(value, config.inventorySnapshotAutoDryRunReportEnabled);
    else if (key == "inventory_snapshot_retention_count") config.inventorySnapshotRetentionCount = parseClampedInt(value, config.inventorySnapshotRetentionCount, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_snapshot_header_offset") config.inventorySnapshotHeaderOffset = parseClampedInt(value, config.inventorySnapshotHeaderOffset, 0, kMaxFieldDumpBytes);
    else if (key == "inventory_snapshot_header_bytes") config.inventorySnapshotHeaderBytes = parseClampedInt(value, config.inventorySnapshotHeaderBytes, 0, kMaxFieldDumpBytes);
    else if (key == "inventory_snapshot_focus_start") config.inventorySnapshotFocusStart = parseClampedInt(value, config.inventorySnapshotFocusStart, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_snapshot_focus_count") config.inventorySnapshotFocusCount = parseClampedInt(value, config.inventorySnapshotFocusCount, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_entry_diagnostics_enabled") config.inventoryEntryDiagnosticsEnabled = parseBool(value, config.inventoryEntryDiagnosticsEnabled);
    else if (key == "inventory_entry_diagnostics_max") config.inventoryEntryDiagnosticsMax = parseClampedInt(value, config.inventoryEntryDiagnosticsMax, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_entry_diagnostics_include_consumables") config.inventoryEntryDiagnosticsIncludeConsumables = parseBool(value, config.inventoryEntryDiagnosticsIncludeConsumables);
    else if (key == "inventory_handle_diagnostics_enabled") config.inventoryHandleDiagnosticsEnabled = parseBool(value, config.inventoryHandleDiagnosticsEnabled);
    else if (key == "inventory_handle_diagnostics_max_handles") config.inventoryHandleDiagnosticsMaxHandles = parseClampedInt(value, config.inventoryHandleDiagnosticsMaxHandles, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_handle_diagnostics_max_matches_per_handle") config.inventoryHandleDiagnosticsMaxMatchesPerHandle = parseClampedInt(value, config.inventoryHandleDiagnosticsMaxMatchesPerHandle, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_handle_diagnostics_max_bytes_mb") config.inventoryHandleDiagnosticsMaxBytesMb = std::max(1, parseInt(value, config.inventoryHandleDiagnosticsMaxBytesMb));
    else if (key == "inventory_handle_diagnostics_include_consumables") config.inventoryHandleDiagnosticsIncludeConsumables = parseBool(value, config.inventoryHandleDiagnosticsIncludeConsumables);
    else if (key == "inventory_compact_row_diagnostics_enabled") config.inventoryCompactRowDiagnosticsEnabled = parseBool(value, config.inventoryCompactRowDiagnosticsEnabled);
    else if (key == "inventory_compact_row_diagnostics_max_targets") config.inventoryCompactRowDiagnosticsMaxTargets = parseClampedInt(value, config.inventoryCompactRowDiagnosticsMaxTargets, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_diagnostics_max_rows") config.inventoryCompactRowDiagnosticsMaxRows = parseClampedInt(value, config.inventoryCompactRowDiagnosticsMaxRows, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_diagnostics_max_bytes_mb") config.inventoryCompactRowDiagnosticsMaxBytesMb = std::max(1, parseInt(value, config.inventoryCompactRowDiagnosticsMaxBytesMb));
    else if (key == "inventory_compact_row_diagnostics_min_run_length") config.inventoryCompactRowDiagnosticsMinRunLength = parseClampedInt(value, config.inventoryCompactRowDiagnosticsMinRunLength, 1, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_diagnostics_max_runs") config.inventoryCompactRowDiagnosticsMaxRuns = parseClampedInt(value, config.inventoryCompactRowDiagnosticsMaxRuns, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_diagnostics_include_consumables") config.inventoryCompactRowDiagnosticsIncludeConsumables = parseBool(value, config.inventoryCompactRowDiagnosticsIncludeConsumables);
    else if (key == "inventory_compact_row_dump_enabled") config.inventoryCompactRowDumpEnabled = parseBool(value, config.inventoryCompactRowDumpEnabled);
    else if (key == "inventory_compact_row_dump_max_runs") config.inventoryCompactRowDumpMaxRuns = parseClampedInt(value, config.inventoryCompactRowDumpMaxRuns, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_dump_max_rows_per_run") config.inventoryCompactRowDumpMaxRowsPerRun = parseClampedInt(value, config.inventoryCompactRowDumpMaxRowsPerRun, 0, kMaxInventoryConfigEntries);
    else if (key == "inventory_compact_row_dump_backtrack_bytes") config.inventoryCompactRowDumpBacktrackBytes = parseClampedInt(value, config.inventoryCompactRowDumpBacktrackBytes, 0, kMaxFieldDumpBytes);
    else if (key == "inventory_compact_row_dump_max_stride_bytes") config.inventoryCompactRowDumpMaxStrideBytes = parseClampedInt(value, config.inventoryCompactRowDumpMaxStrideBytes, static_cast<int>(sizeof(std::uint32_t)), kMaxFieldDumpBytes);
    else if (key == "metadata_iteminfo_path") config.metadataItemInfoPath = value;
    else if (key == "metadata_weapon_names_path") config.metadataWeaponNamesPath = value;
    else if (key == "metadata_protector_names_path") config.metadataProtectorNamesPath = value;
    else if (key == "metadata_accessory_names_path") config.metadataAccessoryNamesPath = value;
    else if (key == "metadata_goods_names_path") config.metadataGoodsNamesPath = value;
    else if (key == "sell_value_path") config.sellValuePath = value;
}

} // namespace

Config loadConfig(const std::filesystem::path& path) {
    Config config;
    std::ifstream file(path);
    if (!file) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        applySetting(config, line.substr(0, separator), line.substr(separator + 1));
    }

    return config;
}

std::string describeConfig(const Config& config) {
    const int configuredMetadataPaths =
        (config.metadataItemInfoPath.empty() ? 0 : 1) +
        (config.metadataWeaponNamesPath.empty() ? 0 : 1) +
        (config.metadataProtectorNamesPath.empty() ? 0 : 1) +
        (config.metadataAccessoryNamesPath.empty() ? 0 : 1) +
        (config.metadataGoodsNamesPath.empty() ? 0 : 1);

    std::ostringstream out;
    out << "enabled=" << config.enabled
        << ", keep_count=" << config.keepCount
        << ", keep_weapons=" << config.keepWeapons
        << ", keep_shields=" << config.keepShields
        << ", keep_armor=" << config.keepArmor
        << ", keep_talismans=" << config.keepTalismans
        << ", keep_ashes_of_war=" << config.keepAshesOfWar
        << ", keep_spells=" << config.keepSpells
        << ", keep_consumables=" << config.keepConsumables
        << ", hotkey_vk=" << config.hotkeyVirtualKey
        << ", sell_weapons=" << config.sellWeapons
        << ", sell_shields=" << config.sellShields
        << ", sell_armor=" << config.sellArmor
        << ", sell_talismans=" << config.sellTalismans
        << ", sell_ashes_of_war=" << config.sellAshesOfWar
        << ", sell_spells=" << config.sellSpells
        << ", sell_consumables=" << config.sellConsumables
        << ", item_stock_rules=" << config.itemStockRules.size()
        << ", dry_run=" << config.dryRun
        << ", native_sale_enabled=" << config.nativeSaleEnabled
        << ", native_sale_max_items=" << config.nativeSaleMaxItems
        << ", supported_game_version=\"" << config.supportedGameVersion << "\""
        << ", memory_scan_enabled=" << config.memoryScanEnabled
        << ", memory_scan_hotkey_vk=" << config.memoryScanHotkeyVirtualKey
        << ", memory_scan_value_size=" << config.memoryScanValueSize
        << ", memory_scan_max_bytes_mb=" << config.memoryScanMaxBytesMb
        << ", memory_scan_max_regions=" << config.memoryScanMaxRegions
        << ", memory_scan_max_candidates=" << config.memoryScanMaxCandidates
        << ", pointer_scan_hotkey_vk=" << config.pointerScanHotkeyVirtualKey
        << ", pointer_scan_max_depth=" << config.pointerScanMaxDepth
        << ", pointer_scan_offset_window=" << config.pointerScanOffsetWindow
        << ", auto_scan_enabled=" << config.autoScanEnabled
        << ", auto_scan_pointer_threshold=" << config.autoScanPointerThreshold
        << ", pointer_chain_verify_enabled=" << config.pointerChainVerifyEnabled
        << ", field_dump_enabled=" << config.fieldDumpEnabled
        << ", field_dump_chain=\"" << config.fieldDumpChain << "\""
        << ", field_dump_bytes=" << config.fieldDumpBytes
        << ", field_dump_stride=" << config.fieldDumpStride
        << ", inventory_read_enabled=" << config.inventoryReadEnabled
        << ", inventory_base_chain=\"" << config.inventoryBaseChain << "\""
        << ", inventory_array_offset=" << config.inventoryArrayOffset
        << ", inventory_count_offset=" << config.inventoryCountOffset
        << ", inventory_capacity_offset=" << config.inventoryCapacityOffset
        << ", inventory_entry_stride=" << config.inventoryEntryStride
        << ", inventory_handle_offset=" << config.inventoryHandleOffset
        << ", inventory_itemid_offset=" << config.inventoryItemIdOffset
        << ", inventory_quantity_offset=" << config.inventoryQuantityOffset
        << ", inventory_max_entries=" << config.inventoryMaxEntries
        << ", inventory_snapshot_export_enabled=" << config.inventorySnapshotExportEnabled
        << ", inventory_snapshot_auto_interval_seconds=" << config.inventorySnapshotAutoIntervalSeconds
        << ", inventory_snapshot_auto_run_on_start=" << config.inventorySnapshotAutoRunOnStart
        << ", inventory_snapshot_auto_dry_run_report_enabled=" << config.inventorySnapshotAutoDryRunReportEnabled
        << ", inventory_snapshot_retention_count=" << config.inventorySnapshotRetentionCount
        << ", inventory_snapshot_header_offset=" << config.inventorySnapshotHeaderOffset
        << ", inventory_snapshot_header_bytes=" << config.inventorySnapshotHeaderBytes
        << ", inventory_snapshot_focus_start=" << config.inventorySnapshotFocusStart
        << ", inventory_snapshot_focus_count=" << config.inventorySnapshotFocusCount
        << ", inventory_entry_diagnostics_enabled=" << config.inventoryEntryDiagnosticsEnabled
        << ", inventory_entry_diagnostics_max=" << config.inventoryEntryDiagnosticsMax
        << ", inventory_entry_diagnostics_include_consumables=" << config.inventoryEntryDiagnosticsIncludeConsumables
        << ", inventory_handle_diagnostics_enabled=" << config.inventoryHandleDiagnosticsEnabled
        << ", inventory_handle_diagnostics_max_handles=" << config.inventoryHandleDiagnosticsMaxHandles
        << ", inventory_handle_diagnostics_max_matches_per_handle=" << config.inventoryHandleDiagnosticsMaxMatchesPerHandle
        << ", inventory_handle_diagnostics_max_bytes_mb=" << config.inventoryHandleDiagnosticsMaxBytesMb
        << ", inventory_handle_diagnostics_include_consumables=" << config.inventoryHandleDiagnosticsIncludeConsumables
        << ", inventory_compact_row_diagnostics_enabled=" << config.inventoryCompactRowDiagnosticsEnabled
        << ", inventory_compact_row_diagnostics_max_targets=" << config.inventoryCompactRowDiagnosticsMaxTargets
        << ", inventory_compact_row_diagnostics_max_rows=" << config.inventoryCompactRowDiagnosticsMaxRows
        << ", inventory_compact_row_diagnostics_max_bytes_mb=" << config.inventoryCompactRowDiagnosticsMaxBytesMb
        << ", inventory_compact_row_diagnostics_min_run_length=" << config.inventoryCompactRowDiagnosticsMinRunLength
        << ", inventory_compact_row_diagnostics_max_runs=" << config.inventoryCompactRowDiagnosticsMaxRuns
        << ", inventory_compact_row_diagnostics_include_consumables=" << config.inventoryCompactRowDiagnosticsIncludeConsumables
        << ", inventory_compact_row_dump_enabled=" << config.inventoryCompactRowDumpEnabled
        << ", inventory_compact_row_dump_max_runs=" << config.inventoryCompactRowDumpMaxRuns
        << ", inventory_compact_row_dump_max_rows_per_run=" << config.inventoryCompactRowDumpMaxRowsPerRun
        << ", inventory_compact_row_dump_backtrack_bytes=" << config.inventoryCompactRowDumpBacktrackBytes
        << ", inventory_compact_row_dump_max_stride_bytes=" << config.inventoryCompactRowDumpMaxStrideBytes
        << ", require_unupgraded=" << config.requireUnupgraded
        << ", require_unmodified=" << config.requireUnmodified
        << ", metadata_iteminfo_path=\"" << config.metadataItemInfoPath << "\""
        << ", metadata_weapon_names_path=\"" << config.metadataWeaponNamesPath << "\""
        << ", metadata_protector_names_path=\"" << config.metadataProtectorNamesPath << "\""
        << ", metadata_accessory_names_path=\"" << config.metadataAccessoryNamesPath << "\""
        << ", metadata_goods_names_path=\"" << config.metadataGoodsNamesPath << "\""
        << ", metadata_paths_configured=" << configuredMetadataPaths
        << ", sell_value_path=\"" << config.sellValuePath << "\"";
    return out.str();
}

} // namespace erdup
