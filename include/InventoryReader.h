#pragma once

#include "GameAdapter.h"
#include "PointerScanner.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

constexpr std::size_t kMaxInventoryReadEntries = 4096;

struct InventoryReadLayout {
    // Native handles can be shared by talisman/goods copies. Slot identities are
    // one-based and unique within a captured normal inventory allocation.
    bool instanceIdFromSlot = false;
    PointerChain baseChain;
    std::size_t arrayOffset = 0;
    std::size_t countOffset = 0;
    // Optional normal-array capacity, not the combined normal/key capacity.
    // When supplied, scan the bounded allocation rather than assuming count
    // describes the last occupied slot in a sparse inventory.
    std::size_t capacityOffset = 0;
    std::size_t entryStride = 0;
    std::size_t handleOffset = 0;
    std::size_t itemIdOffset = 0;
    std::size_t quantityOffset = 0;
    std::size_t maxEntries = 0;
    bool entryDiagnosticsEnabled = false;
    std::size_t entryDiagnosticsMax = 0;
    bool entryDiagnosticsIncludeConsumables = false;
    std::size_t headerDumpOffset = 0;
    std::size_t headerDumpBytes = 0;
    std::size_t focusDumpStart = 0;
    std::size_t focusDumpCount = 0;
};

struct InventoryReadSkip {
    std::size_t index = 0;
    std::uintptr_t entryAddress = 0;
    std::intptr_t entryModuleRelative = 0;
    std::string reason;
};

struct InventoryEntryDiagnostic {
    std::size_t index = 0;
    std::uintptr_t entryAddress = 0;
    std::intptr_t entryModuleRelative = 0;
    std::uint32_t handle = 0;
    std::uint32_t rawItemId = 0;
    std::uint32_t baseItemId = 0;
    ItemCategory category = ItemCategory::Unknown;
    std::uint32_t quantity = 0;
    int upgradeLevel = 0;
    bool modified = false;
    std::vector<std::uint32_t> u32Words;
    bool rawComplete = false;
};

struct InventoryReadSlot {
    std::size_t index = 0;
    bool fieldsReadable = false;
    bool empty = false;
    std::uint32_t handle = 0;
    std::uint32_t rawItemId = 0xffffffffu;
    std::uint32_t quantity = 0;
};

struct InventoryHeaderDumpWord {
    std::size_t offset = 0;
    std::uintptr_t address = 0;
    std::intptr_t moduleRelative = 0;
    bool readable = false;
    std::uint64_t value = 0;
};

struct InventoryFocusDumpRow {
    std::size_t index = 0;
    std::uintptr_t entryAddress = 0;
    std::intptr_t entryModuleRelative = 0;
    bool rawComplete = false;
    std::vector<std::uint32_t> u32Words;
};

struct InventoryReadReport {
    bool baseResolved = false;
    std::uintptr_t baseAddress = 0;
    std::intptr_t baseModuleRelative = 0;
    bool arrayRead = false;
    std::uintptr_t arrayAddress = 0;
    std::intptr_t arrayModuleRelative = 0;
    bool countRead = false;
    std::uint32_t rawCount = 0;
    std::uint32_t rawCapacity = 0;
    bool capacityRead = false;
    std::size_t entriesToRead = 0;
    bool countCapped = false;
    bool entryBlockRead = false;
    std::size_t entryBlockBytes = 0;
    bool upgradeModifiedDecoded = false;
    std::vector<ItemInstance> items;
    std::vector<InventoryReadSlot> slots;
    std::vector<InventoryReadSkip> skipped;
    std::vector<InventoryEntryDiagnostic> entryDiagnostics;
    std::vector<InventoryHeaderDumpWord> headerDump;
    std::vector<InventoryFocusDumpRow> focusDump;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

InventoryReadReport readInventoryAtBase(
    std::uintptr_t moduleBase,
    std::uintptr_t playerDataBase,
    const InventoryReadLayout& layout
);

InventoryReadReport readInventoryLive(
    std::uintptr_t moduleBase,
    const InventoryReadLayout& layout
);

std::string formatInventoryReadSummary(const InventoryReadReport& report);
bool shouldExportInventorySnapshot(const InventoryReadReport& report);
std::string formatInventoryReadSkip(const InventoryReadSkip& skip);
std::string formatInventoryReadSkipSummary(
    const std::vector<InventoryReadSkip>& skips,
    std::size_t loggedCount
);
std::string formatInventoryEntryDiagnostic(const InventoryEntryDiagnostic& diagnostic);
std::string formatInventorySnapshotFixture(const InventoryReadReport& report);

} // namespace erdup
