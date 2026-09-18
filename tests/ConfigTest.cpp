#include "Config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

std::filesystem::path writeTempConfig(const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() /
        ("erdup_config_test_" + std::to_string(std::rand()) + ".ini");
    std::ofstream file(path, std::ios::trunc);
    file << contents;
    return path;
}

bool contains(const std::string& text, const std::string& expected) {
    return text.find(expected) != std::string::npos;
}

int assertMalformedValuesFallbackSafely() {
    const auto path = writeTempConfig(
        "keep_count=bad\n"
        "keep_weapons=-2\n"
        "keep_armor=999999999999999999999999\n"
        "hotkey_vk=-1\n"
        "memory_scan_hotkey_vk=0\n"
        "pointer_scan_hotkey_vk=999\n"
        "memory_scan_value_size=nope\n"
        "memory_scan_max_bytes_mb=-4\n"
        "memory_scan_max_regions=garbage\n"
        "memory_scan_max_candidates=0\n"
        "pointer_scan_max_depth=0\n"
        "pointer_scan_offset_window=-5\n"
        "auto_scan_pointer_threshold=-2\n"
        "field_dump_bytes=0x9000\n"
        "field_dump_stride=0\n"
        "inventory_array_offset=-1\n"
        "inventory_count_offset=bad\n"
        "inventory_capacity_offset=0x414\n"
        "inventory_entry_stride=-24\n"
        "inventory_max_entries=999999\n"
        "inventory_snapshot_auto_interval_seconds=999999\n"
        "inventory_snapshot_retention_count=-1\n"
        "inventory_snapshot_header_offset=-1\n"
        "inventory_snapshot_header_bytes=999999\n"
        "inventory_snapshot_focus_start=-1\n"
        "inventory_snapshot_focus_count=999999\n"
        "inventory_entry_diagnostics_max=-5\n"
        "inventory_handle_diagnostics_max_handles=-1\n"
        "inventory_handle_diagnostics_max_matches_per_handle=999999\n"
        "inventory_handle_diagnostics_max_bytes_mb=-2\n"
        "inventory_compact_row_diagnostics_max_targets=-1\n"
        "inventory_compact_row_diagnostics_max_rows=999999\n"
        "inventory_compact_row_diagnostics_max_bytes_mb=-3\n"
        "inventory_compact_row_diagnostics_min_run_length=0\n"
        "inventory_compact_row_diagnostics_max_runs=-1\n"
        "inventory_compact_row_dump_max_runs=-1\n"
        "inventory_compact_row_dump_max_rows_per_run=999999\n"
        "inventory_compact_row_dump_backtrack_bytes=-1\n"
        "inventory_compact_row_dump_max_stride_bytes=1\n"
        "item_stock_rules=1000:5,bad,2000:nope,3000:-1,1000:7\n"
    );

    const auto config = erdup::loadConfig(path);
    std::filesystem::remove(path);

    if (config.keepCount != 1) {
        return fail("Malformed keep_count should fall back to default.");
    }
    if (config.keepWeapons != 0) {
        return fail("Negative keep_weapons should clamp safely to zero.");
    }
    if (config.keepArmor != 1) {
        return fail("Overflowing keep_armor should fall back to default.");
    }
    if (config.hotkeyVirtualKey != 0x77) {
        return fail("Invalid main hotkey should fall back to default.");
    }
    if (config.memoryScanHotkeyVirtualKey != 0) {
        return fail("Memory scan hotkey should allow zero as disabled.");
    }
    if (config.pointerScanHotkeyVirtualKey != 0) {
        return fail("Out-of-range pointer scan hotkey should fall back to disabled.");
    }
    if (config.memoryScanValueSize != 4 ||
        config.memoryScanMaxBytesMb != 1 ||
        config.memoryScanMaxRegions != 65536 ||
        config.memoryScanMaxCandidates != 1 ||
        config.pointerScanMaxDepth != 1 ||
        config.pointerScanOffsetWindow != 0 ||
        config.autoScanPointerThreshold != 1 ||
        config.fieldDumpBytes != 0x4000 ||
        config.fieldDumpStride != 1 ||
        config.inventoryArrayOffset != 0 ||
        config.inventoryCountOffset != 0 ||
        config.inventoryCapacityOffset != 0x414 ||
        config.inventoryEntryStride != 0 ||
        config.inventoryMaxEntries != 4096 ||
        config.inventorySnapshotAutoIntervalSeconds != 3600 ||
        config.inventorySnapshotRetentionCount != 0 ||
        config.inventorySnapshotHeaderOffset != 0 ||
        config.inventorySnapshotHeaderBytes != 0x4000 ||
        config.inventorySnapshotFocusStart != 0 ||
        config.inventorySnapshotFocusCount != 4096 ||
        config.inventoryEntryDiagnosticsMax != 0 ||
        config.inventoryHandleDiagnosticsMaxHandles != 0 ||
        config.inventoryHandleDiagnosticsMaxMatchesPerHandle != 4096 ||
        config.inventoryHandleDiagnosticsMaxBytesMb != 1 ||
        config.inventoryCompactRowDiagnosticsMaxTargets != 0 ||
        config.inventoryCompactRowDiagnosticsMaxRows != 4096 ||
        config.inventoryCompactRowDiagnosticsMaxBytesMb != 1 ||
        config.inventoryCompactRowDiagnosticsMinRunLength != 1 ||
        config.inventoryCompactRowDiagnosticsMaxRuns != 0 ||
        config.inventoryCompactRowDumpMaxRuns != 0 ||
        config.inventoryCompactRowDumpMaxRowsPerRun != 4096 ||
        config.inventoryCompactRowDumpBacktrackBytes != 0 ||
        config.inventoryCompactRowDumpMaxStrideBytes != 4) {
        return fail("Malformed numeric scan settings did not fall back or clamp safely.");
    }
    if (config.itemStockRules.size() != 1 || config.itemStockRules.at(1000) != 7) {
        return fail("Stock rules should ignore malformed entries and use last duplicate key.");
    }

    return EXIT_SUCCESS;
}

int assertDuplicateKeysUseLastValue() {
    const auto path = writeTempConfig(
        "dry_run=false\n"
        "dry_run=true\n"
        "keep_shields=3\n"
        "keep_shields=1\n"
        "sell_spells=true\n"
        "sell_spells=false\n"
        "metadata_goods_names_path=first.txt\n"
        "metadata_goods_names_path=second.txt\n"
        "sell_value_path=first_values.txt\n"
        "sell_value_path=second_values.txt\n"
        "field_dump_chain=first\n"
        "field_dump_chain=second\n"
        "inventory_base_chain=first\n"
        "inventory_base_chain=second\n"
        "inventory_snapshot_export_enabled=false\n"
        "inventory_snapshot_export_enabled=true\n"
        "inventory_snapshot_auto_interval_seconds=30\n"
        "inventory_snapshot_auto_interval_seconds=5\n"
        "inventory_snapshot_auto_run_on_start=false\n"
        "inventory_snapshot_auto_run_on_start=true\n"
        "inventory_snapshot_auto_dry_run_report_enabled=true\n"
        "inventory_snapshot_auto_dry_run_report_enabled=false\n"
        "inventory_snapshot_retention_count=60\n"
        "inventory_snapshot_retention_count=24\n"
        "inventory_snapshot_header_offset=0x10\n"
        "inventory_snapshot_header_offset=0x400\n"
        "inventory_snapshot_header_bytes=0x20\n"
        "inventory_snapshot_header_bytes=0x40\n"
        "inventory_snapshot_focus_start=100\n"
        "inventory_snapshot_focus_start=288\n"
        "inventory_snapshot_focus_count=4\n"
        "inventory_snapshot_focus_count=32\n"
        "inventory_entry_diagnostics_enabled=false\n"
        "inventory_entry_diagnostics_enabled=true\n"
        "inventory_entry_diagnostics_include_consumables=true\n"
        "inventory_entry_diagnostics_include_consumables=false\n"
        "inventory_handle_diagnostics_enabled=false\n"
        "inventory_handle_diagnostics_enabled=true\n"
        "inventory_handle_diagnostics_include_consumables=true\n"
        "inventory_handle_diagnostics_include_consumables=false\n"
        "inventory_compact_row_diagnostics_enabled=false\n"
        "inventory_compact_row_diagnostics_enabled=true\n"
        "inventory_compact_row_diagnostics_include_consumables=true\n"
        "inventory_compact_row_diagnostics_include_consumables=false\n"
        "inventory_compact_row_dump_enabled=false\n"
        "inventory_compact_row_dump_enabled=true\n"
    );

    const auto config = erdup::loadConfig(path);
    std::filesystem::remove(path);

    if (!config.dryRun || config.keepShields != 1 || config.sellSpells ||
        config.metadataGoodsNamesPath != "second.txt" ||
        config.sellValuePath != "second_values.txt" ||
        config.fieldDumpChain != "second" ||
        config.inventoryBaseChain != "second" ||
        !config.inventorySnapshotExportEnabled ||
        config.inventorySnapshotAutoIntervalSeconds != 5 ||
        !config.inventorySnapshotAutoRunOnStart ||
        config.inventorySnapshotAutoDryRunReportEnabled ||
        config.inventorySnapshotRetentionCount != 24 ||
        config.inventorySnapshotHeaderOffset != 0x400 ||
        config.inventorySnapshotHeaderBytes != 0x40 ||
        config.inventorySnapshotFocusStart != 288 ||
        config.inventorySnapshotFocusCount != 32 ||
        !config.inventoryEntryDiagnosticsEnabled ||
        config.inventoryEntryDiagnosticsIncludeConsumables ||
        !config.inventoryHandleDiagnosticsEnabled ||
        config.inventoryHandleDiagnosticsIncludeConsumables ||
        !config.inventoryCompactRowDiagnosticsEnabled ||
        config.inventoryCompactRowDiagnosticsIncludeConsumables ||
        !config.inventoryCompactRowDumpEnabled) {
        return fail("Duplicate config keys should use the last active value.");
    }

    return EXIT_SUCCESS;
}

int assertDescribeConfigMentionsEveryField() {
    erdup::Config config;
    config.metadataItemInfoPath = "iteminfo.txt";
    config.metadataWeaponNamesPath = "weapon.txt";
    config.metadataProtectorNamesPath = "protector.txt";
    config.metadataAccessoryNamesPath = "accessory.txt";
    config.metadataGoodsNamesPath = "goods.txt";
    config.sellValuePath = "values.txt";

    const auto text = erdup::describeConfig(config);
    const std::vector<std::string> expectedFragments = {
        "enabled=",
        "keep_count=",
        "keep_weapons=",
        "keep_shields=",
        "keep_armor=",
        "keep_talismans=",
        "keep_ashes_of_war=",
        "keep_spells=",
        "keep_consumables=",
        "hotkey_vk=",
        "sell_weapons=",
        "sell_shields=",
        "sell_armor=",
        "sell_talismans=",
        "sell_ashes_of_war=",
        "sell_spells=",
        "sell_consumables=",
        "item_stock_rules=",
        "require_unupgraded=",
        "require_unmodified=",
        "dry_run=",
        "supported_game_version=",
        "memory_scan_enabled=",
        "memory_scan_hotkey_vk=",
        "memory_scan_value_size=",
        "memory_scan_max_bytes_mb=",
        "memory_scan_max_regions=",
        "memory_scan_max_candidates=",
        "pointer_scan_hotkey_vk=",
        "pointer_scan_max_depth=",
        "pointer_scan_offset_window=",
        "auto_scan_enabled=",
        "auto_scan_pointer_threshold=",
        "pointer_chain_verify_enabled=",
        "field_dump_enabled=",
        "field_dump_chain=",
        "field_dump_bytes=",
        "field_dump_stride=",
        "inventory_read_enabled=",
        "inventory_base_chain=",
        "inventory_array_offset=",
        "inventory_count_offset=",
        "inventory_entry_stride=",
        "inventory_handle_offset=",
        "inventory_itemid_offset=",
        "inventory_quantity_offset=",
        "inventory_max_entries=",
        "inventory_snapshot_export_enabled=",
        "inventory_snapshot_auto_interval_seconds=",
        "inventory_snapshot_auto_run_on_start=",
        "inventory_snapshot_auto_dry_run_report_enabled=",
        "inventory_snapshot_retention_count=",
        "inventory_snapshot_header_offset=",
        "inventory_snapshot_header_bytes=",
        "inventory_snapshot_focus_start=",
        "inventory_snapshot_focus_count=",
        "inventory_entry_diagnostics_enabled=",
        "inventory_entry_diagnostics_max=",
        "inventory_entry_diagnostics_include_consumables=",
        "inventory_handle_diagnostics_enabled=",
        "inventory_handle_diagnostics_max_handles=",
        "inventory_handle_diagnostics_max_matches_per_handle=",
        "inventory_handle_diagnostics_max_bytes_mb=",
        "inventory_handle_diagnostics_include_consumables=",
        "inventory_compact_row_diagnostics_enabled=",
        "inventory_compact_row_diagnostics_max_targets=",
        "inventory_compact_row_diagnostics_max_rows=",
        "inventory_compact_row_diagnostics_max_bytes_mb=",
        "inventory_compact_row_diagnostics_min_run_length=",
        "inventory_compact_row_diagnostics_max_runs=",
        "inventory_compact_row_diagnostics_include_consumables=",
        "inventory_compact_row_dump_enabled=",
        "inventory_compact_row_dump_max_runs=",
        "inventory_compact_row_dump_max_rows_per_run=",
        "inventory_compact_row_dump_backtrack_bytes=",
        "inventory_compact_row_dump_max_stride_bytes=",
        "metadata_iteminfo_path=",
        "metadata_weapon_names_path=",
        "metadata_protector_names_path=",
        "metadata_accessory_names_path=",
        "metadata_goods_names_path=",
        "metadata_paths_configured=5",
        "sell_value_path=",
    };

    for (const auto& fragment : expectedFragments) {
        if (!contains(text, fragment)) {
            std::cerr << "describeConfig output: " << text << "\n";
            return fail(("describeConfig missing fragment: " + fragment).c_str());
        }
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertMalformedValuesFallbackSafely() != EXIT_SUCCESS ||
        assertDuplicateKeysUseLastValue() != EXIT_SUCCESS ||
        assertDescribeConfigMentionsEveryField() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Config robustness test passed.\n";
    return EXIT_SUCCESS;
}
