#pragma once

#include <filesystem>
#include <cstdint>
#include <map>
#include <string>

namespace erdup {

struct Config {
    bool enabled = true;
    int keepCount = 1;
    int keepWeapons = 2;
    int keepShields = 1;
    int keepArmor = 1;
    int keepTalismans = 1;
    int keepAshesOfWar = 1;
    int keepSpells = 1;
    int keepConsumables = 0;
    int hotkeyVirtualKey = 0x77; // F8
    bool sellWeapons = true;
    bool sellShields = true;
    bool sellArmor = true;
    bool sellTalismans = true;
    bool sellAshesOfWar = true;
    bool sellSpells = false;
    bool sellConsumables = false;
    std::map<std::uint32_t, int> itemStockRules;
    bool requireUnupgraded = true;
    bool requireUnmodified = true;
    bool dryRun = true;
    bool nativeSaleEnabled = false;
    int nativeSaleMaxItems = 0; // 0 = all eligible duplicates; 1 = single-sale validation.
    std::string supportedGameVersion = "2.6.2.0";
    bool memoryScanEnabled = false;
    int memoryScanHotkeyVirtualKey = 0;
    int memoryScanValueSize = 4;
    // Scan budget. Defaults are sized for a large game process so a single
    // pass does not truncate before reaching the target value. Expressed in
    // megabytes for readability; converted to bytes when the scan runs.
    int memoryScanMaxBytesMb = 3072;
    int memoryScanMaxRegions = 65536;
    int memoryScanMaxCandidates = 65536;
    // Read-only pointer-path scan. Runs on the current value-scan candidates to find a
    // static [module + rva] -> +offsets chain that is reproducible across launches.
    int pointerScanHotkeyVirtualKey = 0;
    int pointerScanMaxDepth = 3;
    int pointerScanOffsetWindow = 0x800;
    // Hands-free automation: watch memory_scan_target.txt and, on each new value, scan and
    // narrow, then auto-run the pointer scan once candidates drop to the threshold. Removes
    // any dependence on hotkeys.
    bool autoScanEnabled = false;
    int autoScanPointerThreshold = 8;
    // On load, resolve each chain in pointer_chains.txt live and log the value it lands on.
    // Used to confirm a discovered chain survives across launches.
    bool pointerChainVerifyEnabled = false;
    // Read-only struct field classifier. Triggered by the same watched-file path as
    // pointer-chain verification and intended to locate candidate inventory-container
    // members inside an already verified base struct. Disabled by default.
    bool fieldDumpEnabled = false;
    std::string fieldDumpChain;
    int fieldDumpBytes = 0x400;
    int fieldDumpStride = 8;
    // Read-only live inventory decode diagnostic. Disabled by default and
    // separate from probe readiness and the stub adapter.
    bool inventoryReadEnabled = false;
    std::string inventoryBaseChain;
    int inventoryArrayOffset = 0;
    int inventoryCountOffset = 0;
    int inventoryCapacityOffset = 0;
    int inventoryEntryStride = 0;
    int inventoryHandleOffset = 0;
    int inventoryItemIdOffset = 0;
    int inventoryQuantityOffset = 0;
    int inventoryMaxEntries = 512;
    bool inventorySnapshotExportEnabled = false;
    int inventorySnapshotAutoIntervalSeconds = 5;
    bool inventorySnapshotAutoRunOnStart = true;
    bool inventorySnapshotAutoDryRunReportEnabled = false;
    int inventorySnapshotRetentionCount = 24;
    int inventorySnapshotHeaderOffset = 0x400;
    int inventorySnapshotHeaderBytes = 0x40;
    int inventorySnapshotFocusStart = 288;
    int inventorySnapshotFocusCount = 32;
    bool inventoryEntryDiagnosticsEnabled = false;
    int inventoryEntryDiagnosticsMax = 64;
    bool inventoryEntryDiagnosticsIncludeConsumables = false;
    bool inventoryHandleDiagnosticsEnabled = false;
    int inventoryHandleDiagnosticsMaxHandles = 32;
    int inventoryHandleDiagnosticsMaxMatchesPerHandle = 8;
    int inventoryHandleDiagnosticsMaxBytesMb = 1024;
    bool inventoryHandleDiagnosticsIncludeConsumables = false;
    bool inventoryCompactRowDiagnosticsEnabled = false;
    int inventoryCompactRowDiagnosticsMaxTargets = 128;
    int inventoryCompactRowDiagnosticsMaxRows = 512;
    int inventoryCompactRowDiagnosticsMaxBytesMb = 1024;
    int inventoryCompactRowDiagnosticsMinRunLength = 4;
    int inventoryCompactRowDiagnosticsMaxRuns = 32;
    bool inventoryCompactRowDiagnosticsIncludeConsumables = false;
    bool inventoryCompactRowDumpEnabled = false;
    int inventoryCompactRowDumpMaxRuns = 1;
    int inventoryCompactRowDumpMaxRowsPerRun = 16;
    int inventoryCompactRowDumpBacktrackBytes = 0x10;
    int inventoryCompactRowDumpMaxStrideBytes = 0x100;

    // Optional metadata source paths. Empty means "not configured" and the
    // source is skipped. See docs/READ_ONLY_ADAPTER.md for example paths
    // pointed at a randomizer output folder.
    std::string metadataItemInfoPath;
    std::string metadataWeaponNamesPath;
    std::string metadataProtectorNamesPath;
    std::string metadataAccessoryNamesPath;
    std::string metadataGoodsNamesPath;
    std::string sellValuePath;
};

Config loadConfig(const std::filesystem::path& path);
std::string describeConfig(const Config& config);

} // namespace erdup
