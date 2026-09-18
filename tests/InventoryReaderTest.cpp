#include "Config.h"
#include "InventoryReader.h"
#include "InventoryReport.h"
#include "NativeInventoryIdentity.h"
#include "InventorySnapshotFixture.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kArrayOffset = 0x418;
constexpr std::size_t kCountOffset = 0x420;
constexpr std::size_t kEntryStride = 0x18;
constexpr std::size_t kHandleOffset = 0x00;
constexpr std::size_t kItemIdOffset = 0x04;
constexpr std::size_t kQuantityOffset = 0x08;

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

template <typename T>
void writeValue(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void writeEntry(
    std::vector<std::byte>& entries,
    std::size_t index,
    std::uint32_t handle,
    std::uint32_t itemId,
    std::uint32_t quantity
) {
    const auto base = index * kEntryStride;
    writeValue(entries, base + kHandleOffset, handle);
    writeValue(entries, base + kItemIdOffset, itemId);
    writeValue(entries, base + kQuantityOffset, quantity);
}

erdup::InventoryReadLayout defaultLayout(std::size_t maxEntries = 16) {
    erdup::InventoryReadLayout layout;
    layout.arrayOffset = kArrayOffset;
    layout.countOffset = kCountOffset;
    layout.entryStride = kEntryStride;
    layout.handleOffset = kHandleOffset;
    layout.itemIdOffset = kItemIdOffset;
    layout.quantityOffset = kQuantityOffset;
    layout.maxEntries = maxEntries;
    return layout;
}

bool containsError(const erdup::InventoryReadReport& report, const std::string& expected) {
    for (const auto& error : report.errors) {
        if (error.find(expected) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int assertDecodedEntriesAndEmptySlots() {
    auto layout = defaultLayout();
    layout.entryDiagnosticsEnabled = true;
    layout.entryDiagnosticsMax = 4;
    layout.entryDiagnosticsIncludeConsumables = false;

    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 10);

    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 8);

    writeEntry(entries, 0, 0x10, 0x00001234, 1);
    writeValue<std::uint32_t>(entries, 0 * kEntryStride + 0x0c, 0xaaaa0000);
    writeValue<std::uint32_t>(entries, 0 * kEntryStride + 0x10, 0xbbbb0000);
    writeValue<std::uint32_t>(entries, 0 * kEntryStride + 0x14, 0xcccc0000);
    writeEntry(entries, 1, 0x11, 0x10002000, 2);
    writeEntry(entries, 2, 0x12, 0x20003000, 3);
    writeEntry(entries, 3, 0x13, 0x40004000, 4);
    writeEntry(entries, 4, 0x14, 0x80005000, 5);
    writeEntry(entries, 5, 0x15, 0xf0006000, 6);
    writeEntry(entries, 6, 0x00, 0x00007000, 7);
    writeEntry(entries, 7, 0x17, 0xffffffff, 8);
    writeEntry(entries, 8, 0x18, 0x00007001, 9);
    writeEntry(entries, 9, 0x19, 0x10008002, 10);

    const auto moduleBase = reinterpret_cast<std::uintptr_t>(playerData.data());
    const auto report = erdup::readInventoryAtBase(
        moduleBase,
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );

    if (!report.baseResolved || !report.arrayRead || !report.countRead) {
        return fail("Expected inventory base, array pointer, and count to be readable.");
    }
    if (report.rawCount != 8 || report.entriesToRead != 10 || report.countCapped) {
        return fail("Expected raw count 8 with sparse scan extension and no cap.");
    }
    if (!report.entryBlockRead || report.entryBlockBytes != kEntryStride * 8) {
        return fail("Expected the initial count-sized read to use the bounded block read fast path.");
    }
    if (report.items.size() != 8 || report.skipped.size() != 2) {
        return fail("Expected eight decoded entries and two skipped empty slots after sparse extension.");
    }
    if (report.slots.size() != 10 ||
        !report.slots[0].fieldsReadable ||
        report.slots[0].empty ||
        report.slots[0].rawItemId != 0x00001234 ||
        !report.slots[6].fieldsReadable ||
        !report.slots[6].empty ||
        report.slots[7].rawItemId != 0xffffffffu ||
        report.slots[9].rawItemId != 0x10008002u) {
        return fail("Inventory read should retain sparse raw slots for snapshot export.");
    }

    const erdup::ItemCategory expectedCategories[] = {
        erdup::ItemCategory::Weapon,
        erdup::ItemCategory::Armor,
        erdup::ItemCategory::Talisman,
        erdup::ItemCategory::Consumable,
        erdup::ItemCategory::AshOfWar,
        erdup::ItemCategory::Unknown,
        erdup::ItemCategory::Weapon,
        erdup::ItemCategory::Armor,
    };
    const std::uint32_t expectedBaseIds[] = {
        0x1234,
        0x2000,
        0x3000,
        0x4000,
        0x5000,
        0x6000,
        0x7001,
        0x8002,
    };
    const std::uint32_t expectedHandles[] = {
        0x10,
        0x11,
        0x12,
        0x13,
        0x14,
        0x15,
        0x18,
        0x19,
    };
    const int expectedQuantities[] = {1, 2, 3, 4, 5, 6, 9, 10};

    for (std::size_t i = 0; i < report.items.size(); ++i) {
        const auto& item = report.items[i];
        if (item.instanceId != expectedHandles[i]) {
            return fail("Decoded instance ID did not match the entry handle.");
        }
        if (item.category != expectedCategories[i]) {
            return fail("Decoded category did not match the item ID high nibble.");
        }
        if (item.baseItemId != expectedBaseIds[i]) {
            return fail("Decoded base item ID did not mask off the category nibble.");
        }
        if (item.quantity != expectedQuantities[i]) {
            return fail("Decoded quantity did not match the entry quantity.");
        }
        if (item.upgradeLevel != 0 || item.modified) {
            return fail("Synthetic non-weapon-suffix rows should decode as unupgraded and unmodified.");
        }
    }

    if (report.warnings.empty() || !report.upgradeModifiedDecoded) {
        return fail("Expected upgrade/modified weapon ID suffix decoding to run after the entry array was readable.");
    }
    if (report.entryDiagnostics.size() != 4) {
        return fail("Expected bounded entry diagnostics for four non-consumable decoded entries.");
    }
    if (report.entryDiagnostics[0].index != 0 ||
        report.entryDiagnostics[0].baseItemId != 0x1234 ||
        report.entryDiagnostics[0].category != erdup::ItemCategory::Weapon ||
        report.entryDiagnostics[0].upgradeLevel != 0 ||
        report.entryDiagnostics[0].modified ||
        report.entryDiagnostics[0].u32Words.size() != 6 ||
        !report.entryDiagnostics[0].rawComplete) {
        return fail("First entry diagnostic did not capture expected decoded fields and raw words.");
    }
    if (report.entryDiagnostics[0].u32Words[3] != 0xaaaa0000 ||
        report.entryDiagnostics[0].u32Words[4] != 0xbbbb0000 ||
        report.entryDiagnostics[0].u32Words[5] != 0xcccc0000) {
        return fail("Entry diagnostic did not preserve raw u32 slots beyond handle/id/quantity.");
    }
    for (const auto& diagnostic : report.entryDiagnostics) {
        if (diagnostic.category == erdup::ItemCategory::Consumable) {
            return fail("Entry diagnostics should skip consumables unless explicitly enabled.");
        }
    }

    const auto formatted = erdup::formatInventoryEntryDiagnostic(report.entryDiagnostics[0]);
    if (formatted.find("inventory_entry_diag:") == std::string::npos ||
        formatted.find("raw_item_id=0x1234") == std::string::npos ||
        formatted.find("upgrade_level=0") == std::string::npos ||
        formatted.find("modified_id_variant=0") == std::string::npos ||
        formatted.find("u32[3]=0xaaaa0000") == std::string::npos) {
        return fail("Formatted entry diagnostic should include raw item ID and u32 slots.");
    }

    const auto summary = erdup::formatInventoryReadSummary(report);
    if (summary.find("entry_block_read=1") == std::string::npos ||
        summary.find("entry_block_bytes=192") == std::string::npos) {
        return fail("Inventory summary should expose the block-read fast path.");
    }

    const auto snapshotText = erdup::formatInventorySnapshotFixture(report);
    if (snapshotText.find("count=8") == std::string::npos ||
        snapshotText.find("slot=index:0,handle:0x10,raw:0x1234,qty:1") == std::string::npos ||
        snapshotText.find("slot=index:6,empty:true") == std::string::npos ||
        snapshotText.find("slot=index:9,handle:0x19,raw:0x10008002,qty:10") == std::string::npos) {
        return fail("Snapshot fixture export should include count, sparse decoded slots, and empty slots.");
    }
    const auto snapshot = erdup::parseInventorySnapshotFixture(snapshotText);
    if (!snapshot.errors.empty() ||
        snapshot.activeCount != 8 ||
        snapshot.slots.size() != 10 ||
        snapshot.slots[0].handle != 0x10 ||
        snapshot.slots[6].rawItemId != 0xffffffffu ||
        snapshot.slots[9].handle != 0x19) {
        return fail("Sparse snapshot fixture export should parse back into comparable slots.");
    }

    return EXIT_SUCCESS;
}

int assertSkipSummaryFormattingCountsReasons() {
    const std::vector<erdup::InventoryReadSkip> skips = {
        {1, 0, 0x10, "empty slot"},
        {2, 0, 0x20, "empty slot"},
        {3, 0, 0x30, "quantity unreadable"},
    };

    const auto text = erdup::formatInventoryReadSkipSummary(skips, 2);
    if (!contains(text, "inventory_read_skip_summary:") ||
        !contains(text, "total=3") ||
        !contains(text, "logged=2") ||
        !contains(text, "omitted=1") ||
        !contains(text, "reason=\"empty slot\":2") ||
        !contains(text, "reason=\"quantity unreadable\":1")) {
        return fail("Skip summary should expose total/logged/omitted counts and per-reason totals.");
    }

    return EXIT_SUCCESS;
}

int assertSnapshotExportRequiresResolvedInventoryRows() {
    erdup::InventoryReadReport empty;
    if (erdup::shouldExportInventorySnapshot(empty)) {
        return fail("Empty inventory read reports should not be snapshot-exportable.");
    }

    erdup::InventoryReadReport unresolved;
    unresolved.rawCount = 1;
    unresolved.slots.push_back({});
    if (erdup::shouldExportInventorySnapshot(unresolved)) {
        return fail("Unresolved inventory reads should not be snapshot-exportable.");
    }

    erdup::InventoryReadReport missingCount;
    missingCount.baseResolved = true;
    missingCount.arrayRead = true;
    missingCount.rawCount = 1;
    missingCount.slots.push_back({});
    if (erdup::shouldExportInventorySnapshot(missingCount)) {
        return fail("Inventory reads without a readable count should not be snapshot-exportable.");
    }

    erdup::InventoryReadReport zeroCount;
    zeroCount.baseResolved = true;
    zeroCount.arrayRead = true;
    zeroCount.countRead = true;
    zeroCount.rawCount = 0;
    zeroCount.slots.push_back({});
    if (erdup::shouldExportInventorySnapshot(zeroCount)) {
        return fail("Zero-count diagnostic inventory reads should not be snapshot-exportable.");
    }

    erdup::InventoryReadReport noRows;
    noRows.baseResolved = true;
    noRows.arrayRead = true;
    noRows.countRead = true;
    noRows.rawCount = 1;
    if (erdup::shouldExportInventorySnapshot(noRows)) {
        return fail("Inventory reads with no retained rows should not be snapshot-exportable.");
    }

    erdup::InventoryReadReport ready;
    ready.baseResolved = true;
    ready.arrayRead = true;
    ready.countRead = true;
    ready.rawCount = 1;
    ready.slots.push_back({});
    if (!erdup::shouldExportInventorySnapshot(ready)) {
        return fail("Resolved inventory reads with retained rows should be snapshot-exportable.");
    }

    return EXIT_SUCCESS;
}

int assertModifiedWeaponParamVariantDetected() {
    auto layout = defaultLayout();
    layout.entryDiagnosticsEnabled = true;
    layout.entryDiagnosticsMax = 4;

    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 4);

    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 4);

    writeEntry(entries, 0, 0x80800001, 16000000, 1);   // Short Spear
    writeEntry(entries, 1, 0x80800002, 16000002, 1);   // Short Spear +2
    writeEntry(entries, 2, 0x80800003, 16000100, 1);   // Heavy Short Spear
    writeEntry(entries, 3, 0x90800004, 0x100f42a4, 1); // Armor-category row should not use weapon suffix rules.

    const auto report = erdup::readInventoryAtBase(
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );

    if (report.items.size() != 4 || report.entryDiagnostics.size() != 4) {
        return fail("Expected four decoded items and diagnostics for weapon suffix test.");
    }
    if (report.items[0].upgradeLevel != 0 || report.items[0].modified) {
        return fail("Unmodified weapon param ID should be unupgraded and unmodified.");
    }
    if (report.items[1].upgradeLevel != 2 || report.items[1].modified ||
        report.entryDiagnostics[1].upgradeLevel != 2) {
        return fail("Upgraded weapon param ID should decode the +2 suffix without marking affinity modification.");
    }
    if (report.items[2].upgradeLevel != 0 ||
        !report.items[2].modified ||
        !report.entryDiagnostics[2].modified) {
        return fail("Weapon affinity variant ID should be marked modified.");
    }
    if (report.items[3].category != erdup::ItemCategory::Armor ||
        report.items[3].upgradeLevel != 0 ||
        report.items[3].modified) {
        return fail("Non-weapon categories should not be marked modified by weapon ID suffixes.");
    }

    const erdup::Config config;
    const auto dryRunReport = erdup::buildInventoryReport(report.items, config);
    bool foundUpgradedRetained = false;
    bool foundModifiedSold = false;
    for (const auto& row : dryRunReport.rows) {
        if (row.item.baseItemId == 16000002 && !row.selectedForSale && row.reason == "retained by keep count") {
            foundUpgradedRetained = true;
        }
        if (row.item.baseItemId == 16000100 && row.selectedForSale && row.reason == "duplicate above keep count") {
            foundModifiedSold = true;
        }
    }
    if (!foundUpgradedRetained) {
        return fail("Upgraded weapon variant should be retained as the highest-upgrade copy.");
    }
    if (!foundModifiedSold) {
        return fail("Lower-priority modified weapon variant should be selected as the duplicate.");
    }

    const auto upgradedFormatted = erdup::formatInventoryEntryDiagnostic(report.entryDiagnostics[1]);
    if (upgradedFormatted.find("base_item_id=16000002") == std::string::npos ||
        upgradedFormatted.find("upgrade_level=2") == std::string::npos ||
        upgradedFormatted.find("modified_id_variant=0") == std::string::npos) {
        return fail("Formatted diagnostic should expose upgraded weapon ID detection.");
    }

    const auto modifiedFormatted = erdup::formatInventoryEntryDiagnostic(report.entryDiagnostics[2]);
    if (modifiedFormatted.find("base_item_id=16000100") == std::string::npos ||
        modifiedFormatted.find("upgrade_level=0") == std::string::npos ||
        modifiedFormatted.find("modified_id_variant=1") == std::string::npos) {
        return fail("Formatted diagnostic should expose modified weapon ID detection.");
    }

    return EXIT_SUCCESS;
}

int assertMaxEntriesCap() {
    auto layout = defaultLayout(2);
    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 5);

    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 5);
    for (std::size_t i = 0; i < 5; ++i) {
        writeEntry(entries, i, static_cast<std::uint32_t>(0x20 + i), 0x00009000, 1);
    }

    const auto report = erdup::readInventoryAtBase(
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );

    if (!report.countCapped || report.entriesToRead != 2 || report.items.size() != 2) {
        return fail("Inventory reader should honor inventory_max_entries before traversal.");
    }

    return EXIT_SUCCESS;
}

int assertSnapshotRawDiagnosticsIncludeHeaderAndFocusRows() {
    auto layout = defaultLayout(4);
    layout.headerDumpOffset = 0x400;
    layout.headerDumpBytes = 0x40;
    layout.focusDumpStart = 2;
    layout.focusDumpCount = 2;

    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 4);

    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 4);

    writeEntry(entries, 0, 0x30, 0x00001000, 1);
    writeEntry(entries, 1, 0x31, 0x00001001, 1);
    writeEntry(entries, 2, 0x32, 0x00001002, 2);
    writeValue<std::uint32_t>(entries, 2 * kEntryStride + 0x0c, 0xfeed0002);
    writeEntry(entries, 3, 0x33, 0xffffffffu, 0);
    writeValue<std::uint32_t>(entries, 3 * kEntryStride + 0x0c, 0xfeed0003);

    const auto report = erdup::readInventoryAtBase(
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );

    if (report.headerDump.size() != 8 || report.focusDump.size() != 2) {
        return fail("Expected bounded header qword dump and focus row dump.");
    }
    if (!report.headerDump[3].readable ||
        report.headerDump[3].offset != kArrayOffset ||
        report.headerDump[3].value != reinterpret_cast<std::uintptr_t>(entries.data()) ||
        !report.headerDump[4].readable ||
        report.headerDump[4].offset != kCountOffset ||
        report.headerDump[4].value != 4) {
        return fail("Header dump should preserve the candidate array pointer and count qwords.");
    }
    if (report.focusDump[0].index != 2 ||
        !report.focusDump[0].rawComplete ||
        report.focusDump[0].u32Words.size() != 6 ||
        report.focusDump[0].u32Words[0] != 0x32 ||
        report.focusDump[0].u32Words[3] != 0xfeed0002 ||
        report.focusDump[1].index != 3 ||
        report.focusDump[1].u32Words[1] != 0xffffffffu) {
        return fail("Focus row dump should preserve raw u32 words for the requested row range.");
    }

    const auto snapshotText = erdup::formatInventorySnapshotFixture(report);
    if (snapshotText.find("# header_qword=offset:0x418") == std::string::npos ||
        snapshotText.find("# header_qword=offset:0x420") == std::string::npos ||
        snapshotText.find("# focus_row=index:2") == std::string::npos ||
        snapshotText.find("u32[3]:0xfeed0002") == std::string::npos) {
        return fail("Snapshot fixture should include raw diagnostic comment lines.");
    }
    const auto snapshot = erdup::parseInventorySnapshotFixture(snapshotText);
    if (!snapshot.errors.empty() || !snapshot.warnings.empty() || snapshot.slots.size() != 4) {
        return fail("Snapshot parser should ignore raw diagnostic comments and preserve slot rows.");
    }

    return EXIT_SUCCESS;
}

int assertUnreadableArrayPointerErrors() {
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    void* noAccessPage = VirtualAlloc(
        nullptr,
        static_cast<std::size_t>(systemInfo.dwPageSize),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_NOACCESS
    );
    if (noAccessPage == nullptr) {
        return fail("VirtualAlloc PAGE_NOACCESS failed.");
    }

    auto layout = defaultLayout();
    std::vector<std::byte> playerData(0x500);
    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(noAccessPage));
    writeValue<std::uint32_t>(playerData, kCountOffset, 1);

    const auto report = erdup::readInventoryAtBase(
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );
    VirtualFree(noAccessPage, 0, MEM_RELEASE);

    if (!containsError(report, "Inventory array pointer is not readable")) {
        return fail("Unreadable inventory array pointer should yield an error without crashing.");
    }
    if (!report.items.empty()) {
        return fail("Unreadable inventory array pointer should decode no items.");
    }

    return EXIT_SUCCESS;
}

int assertUnreadableEntrySkipped() {
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const auto pageSize = static_cast<std::size_t>(systemInfo.dwPageSize);

    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (page == nullptr) {
        return fail("VirtualAlloc PAGE_READWRITE failed.");
    }
    const auto pageAddress = reinterpret_cast<std::uintptr_t>(page);
    const auto entryAddress = pageAddress + pageSize - sizeof(std::uint32_t);
    *reinterpret_cast<std::uint32_t*>(entryAddress) = 0x99;

    auto layout = defaultLayout();
    std::vector<std::byte> playerData(0x500);
    writeValue(playerData, kArrayOffset, entryAddress);
    writeValue<std::uint32_t>(playerData, kCountOffset, 1);

    const auto report = erdup::readInventoryAtBase(
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        reinterpret_cast<std::uintptr_t>(playerData.data()),
        layout
    );
    VirtualFree(page, 0, MEM_RELEASE);

    if (!report.items.empty() || report.skipped.size() != 1) {
        return fail("Unreadable entry fields should skip the entry without decoding an item.");
    }
    if (report.slots.size() != 1 || report.slots[0].fieldsReadable) {
        return fail("Unreadable entry should retain an unreadable raw slot marker.");
    }
    if (report.entryBlockRead) {
        return fail("Unreadable entry test should fall back when the contiguous block read fails.");
    }
    if (report.skipped[0].reason.find("item id unreadable") == std::string::npos) {
        return fail("Unreadable entry skip should record the field-read reason.");
    }
    if (erdup::formatInventorySnapshotFixture(report).find("slot=index:0,unreadable:true") ==
        std::string::npos) {
        return fail("Snapshot fixture export should mark unreadable slots explicitly.");
    }

    return EXIT_SUCCESS;
}

int assertCapacityScanPreservesSparseTail() {
    auto layout = defaultLayout(16);
    layout.capacityOffset = 0x414;
    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 6);
    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 2);
    writeValue<std::uint32_t>(playerData, 0x414, 5);
    writeEntry(entries, 0, 1, 1000000, 1);
    writeEntry(entries, 1, 0, 0xffffffffu, 0);
    writeEntry(entries, 2, 2, 1000000, 1);
    writeEntry(entries, 3, 0, 0xffffffffu, 0);
    writeEntry(entries, 4, 3, 1000000, 1);
    // This deliberately plausible record is OUTSIDE the allocation's capacity.
    writeEntry(entries, 5, 4, 1000000, 1);
    const auto base = reinterpret_cast<std::uintptr_t>(playerData.data());
    auto report = erdup::readInventoryAtBase(base, base, layout);
    if (!report.errors.empty() || !report.capacityRead || report.rawCapacity != 5 ||
        report.entriesToRead != 5 || report.items.size() != 3 || report.slots.size() != 5 ||
        report.items.back().instanceId != 3 || report.entryBlockBytes != 5 * kEntryStride) {
        return fail("Capacity scan must include sparse tail entries and exclude adjacent storage.");
    }
    layout.maxEntries = 2;
    report = erdup::readInventoryAtBase(base, base, layout);
    bool truncated = false;
    for (const auto& warning : report.warnings) {
        truncated = truncated || contains(warning, "capacity scan truncated");
    }
    if (!truncated || report.slots.size() != 2) {
        return fail("A capped capacity scan must report incomplete inventory.");
    }
    writeValue<std::uint32_t>(playerData, 0x414, 1);
    report = erdup::readInventoryAtBase(base, base, layout);
    if (!containsError(report, "implausible") || !report.items.empty()) {
        return fail("Count exceeding capacity must fail before reading rows.");
    }
    writeValue<std::uint32_t>(playerData, 0x414, 0xffffffffu);
    report = erdup::readInventoryAtBase(base, base, layout);
    if (!containsError(report, "implausible") || !report.items.empty()) {
        return fail("Unreasonable capacity must fail before reading rows.");
    }
    return EXIT_SUCCESS;
}

int assertLiveChainAndReportPath() {
    auto layout = defaultLayout();
    std::vector<std::uintptr_t> moduleSlots(2, 0);
    std::vector<std::byte> playerData(0x500);
    std::vector<std::byte> entries(kEntryStride * 3);

    const auto moduleBase = reinterpret_cast<std::uintptr_t>(moduleSlots.data());
    moduleSlots[1] = reinterpret_cast<std::uintptr_t>(playerData.data());
    layout.baseChain.staticModuleRva = reinterpret_cast<std::uintptr_t>(&moduleSlots[1]) - moduleBase;
    layout.baseChain.offsets = {0};

    writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
    writeValue<std::uint32_t>(playerData, kCountOffset, 3);
    writeEntry(entries, 0, 1, 0x00001234, 1);
    writeEntry(entries, 1, 2, 0x00001234, 1);
    writeEntry(entries, 2, 3, 0x00001234, 1);

    const auto readReport = erdup::readInventoryLive(moduleBase, layout);
    if (!readReport.baseResolved || readReport.items.size() != 3) {
        return fail("Live synthetic chain should resolve and decode three inventory items.");
    }

    const erdup::Config config;
    const auto report = erdup::buildInventoryReport(readReport.items, config);
    if (report.totalItemsToSell != 1 || report.rows.size() != 3) {
        return fail("Decoded duplicate weapon set should produce one dry-run sale candidate.");
    }

    int sellRows = 0;
    for (const auto& row : report.rows) {
        if (row.selectedForSale) {
            ++sellRows;
        }
    }
    if (sellRows != 1) {
        return fail("Expected exactly one selected sale row in the decoded dry-run report.");
    }

    const auto formatted = erdup::formatInventoryReport(report);
    if (formatted.find("Inventory dry-run report: sell 1 item(s)") == std::string::npos ||
        formatted.find("SELL id=4660") == std::string::npos) {
        return fail("Formatted dry-run report should include the decoded duplicate sale candidate.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    {
        auto layout = defaultLayout();
        layout.instanceIdFromSlot = true;
        std::vector<std::byte> playerData(0x500);
        std::vector<std::byte> entries(kEntryStride * 3);
        writeValue(playerData, kArrayOffset, reinterpret_cast<std::uintptr_t>(entries.data()));
        writeValue<std::uint32_t>(playerData, kCountOffset, 3);
        for (std::size_t i = 0; i < 3; ++i) writeEntry(entries, i, 0xa00003e8u, 0x200003e8u, 1);
        const auto base = reinterpret_cast<std::uintptr_t>(playerData.data());
        auto captured = erdup::readInventoryAtBase(base, base, layout);
        std::array<int,22> equipped; equipped.fill(-1); equipped[0] = 386;
        erdup::markNativeEquipped(captured, equipped);
        const auto plan = erdup::buildInventoryReport(captured.items, erdup::Config{});
        if (plan.totalItemsToSell != 2 || !plan.rows[0].selectedForSale ||
            !plan.rows[1].selectedForSale || plan.rows[2].selectedForSale ||
            captured.items[0].equipped || !captured.items[2].equipped)
            return fail("Shared talisman handles must retain only the equipped slot and select two distinct extras.");
        auto expected = erdup::nativeInventoryIdentities(captured);
        expected.erase(0);
        writeEntry(entries, 0, 0, 0xffffffffu, 0);
        writeValue<std::uint32_t>(playerData, kCountOffset, 2);
        auto removed = erdup::readInventoryAtBase(base, base, layout);
        if (erdup::nativeInventoryIdentities(removed) != expected || expected.size() != 2)
            return fail("Removing one shared-handle copy must preserve the two other inventory slots.");
    }
    if (assertDecodedEntriesAndEmptySlots() != EXIT_SUCCESS ||
        assertSkipSummaryFormattingCountsReasons() != EXIT_SUCCESS ||
        assertSnapshotExportRequiresResolvedInventoryRows() != EXIT_SUCCESS ||
        assertModifiedWeaponParamVariantDetected() != EXIT_SUCCESS ||
        assertMaxEntriesCap() != EXIT_SUCCESS ||
        assertSnapshotRawDiagnosticsIncludeHeaderAndFocusRows() != EXIT_SUCCESS ||
        assertUnreadableArrayPointerErrors() != EXIT_SUCCESS ||
        assertUnreadableEntrySkipped() != EXIT_SUCCESS ||
        assertCapacityScanPreservesSparseTail() != EXIT_SUCCESS ||
        assertLiveChainAndReportPath() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Inventory reader test passed.\n";
    return EXIT_SUCCESS;
}
