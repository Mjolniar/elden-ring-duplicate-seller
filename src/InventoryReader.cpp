#include "InventoryReader.h"

#include "MemoryReader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace erdup {

namespace {

constexpr std::size_t kMaxDiagnosticEntryBytes = 0x40;
constexpr std::size_t kMaxHeaderDumpBytes = 0x400;
constexpr std::size_t kMaxFocusDumpRows = 256;
constexpr std::uint32_t kMinimumWeaponParamId = 100000;
constexpr std::uint32_t kWeaponAffinityStep = 100;
constexpr std::uint32_t kMaxKnownWeaponAffinitySuffix = 1200;
constexpr std::uint32_t kMaxKnownWeaponUpgradeLevel = 25;

bool addWouldOverflow(std::uintptr_t value, std::size_t addend) {
    return addend > std::numeric_limits<std::uintptr_t>::max() - value;
}

std::intptr_t relativeToModule(std::uintptr_t address, std::uintptr_t moduleBase) {
    if (address >= moduleBase) {
        return static_cast<std::intptr_t>(address - moduleBase);
    }
    return -static_cast<std::intptr_t>(moduleBase - address);
}

std::string formatRelativeAddress(std::intptr_t relativeAddress) {
    std::ostringstream out;
    if (relativeAddress < 0) {
        out << "-0x" << std::hex << (static_cast<std::uintptr_t>(-(relativeAddress + 1)) + 1);
    } else {
        out << "0x" << std::hex << static_cast<std::uintptr_t>(relativeAddress);
    }
    return out.str();
}

std::string formatOffset(std::size_t offset) {
    std::ostringstream out;
    out << "0x" << std::hex << offset;
    return out.str();
}

std::string formatHex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << value;
    return out.str();
}

bool addOffset(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) {
    if (addWouldOverflow(base, offset)) {
        return false;
    }
    output = base + offset;
    return true;
}

template <typename T>
bool readField(std::uintptr_t base, std::size_t offset, T& output) {
    std::uintptr_t address = 0;
    if (!addOffset(base, offset, address)) {
        return false;
    }
    return readCommittedMemory(address, &output, sizeof(output));
}

template <typename T>
bool readFieldFromBlock(
    const std::vector<std::byte>& block,
    std::size_t index,
    std::size_t stride,
    std::size_t offset,
    T& output
) {
    if (stride == 0 || index > (std::numeric_limits<std::size_t>::max() / stride)) {
        return false;
    }
    if (offset > stride || sizeof(output) > stride - offset) {
        return false;
    }

    const auto entryOffset = index * stride;
    if (entryOffset > block.size() || stride > block.size() - entryOffset) {
        return false;
    }

    std::memcpy(&output, block.data() + entryOffset + offset, sizeof(output));
    return true;
}

std::vector<std::byte> readEntryBlock(
    InventoryReadReport& report,
    std::uintptr_t arrayPointer,
    const InventoryReadLayout& layout
) {
    std::vector<std::byte> block;
    if (report.entriesToRead == 0 || layout.entryStride == 0) {
        return block;
    }
    if (report.entriesToRead > (std::numeric_limits<std::size_t>::max() / layout.entryStride)) {
        return block;
    }

    const auto bytesToRead = report.entriesToRead * layout.entryStride;
    block.resize(bytesToRead);
    if (!readCommittedMemory(arrayPointer, block.data(), block.size())) {
        block.clear();
        return block;
    }

    report.entryBlockRead = true;
    report.entryBlockBytes = block.size();
    return block;
}

bool canReadEntryFromBlock(
    const InventoryReadReport& report,
    const std::vector<std::byte>& block,
    std::size_t index,
    std::size_t stride
) {
    if (!report.entryBlockRead || stride == 0) {
        return false;
    }
    if (index > (std::numeric_limits<std::size_t>::max() / stride)) {
        return false;
    }
    const auto entryOffset = index * stride;
    return entryOffset <= block.size() && stride <= block.size() - entryOffset;
}

ItemCategory categoryFromInventoryItemId(std::uint32_t itemId) {
    switch ((itemId >> 28) & 0xfu) {
    case 0x0: return ItemCategory::Weapon;
    case 0x1: return ItemCategory::Armor;
    case 0x2: return ItemCategory::Talisman;
    case 0x4: return ItemCategory::Consumable;
    case 0x8: return ItemCategory::AshOfWar;
    default: return ItemCategory::Unknown;
    }
}

bool isWeaponFamilyCategory(ItemCategory category) {
    return category == ItemCategory::Weapon || category == ItemCategory::Shield;
}

bool isLikelyModifiedWeaponParamId(std::uint32_t baseItemId, ItemCategory category) {
    if (!isWeaponFamilyCategory(category) || baseItemId < kMinimumWeaponParamId) {
        return false;
    }

    const auto suffix = baseItemId % 10000u;
    const auto upgradeLevel = suffix % 100u;
    const auto affinitySuffix = suffix - upgradeLevel;
    return affinitySuffix >= kWeaponAffinityStep &&
        affinitySuffix <= kMaxKnownWeaponAffinitySuffix &&
        (affinitySuffix % kWeaponAffinityStep) == 0;
}

int decodeWeaponUpgradeLevel(std::uint32_t baseItemId, ItemCategory category) {
    if (!isWeaponFamilyCategory(category) || baseItemId < kMinimumWeaponParamId) {
        return 0;
    }

    const auto upgradeLevel = baseItemId % 100u;
    if (upgradeLevel > kMaxKnownWeaponUpgradeLevel) {
        return 0;
    }
    return static_cast<int>(upgradeLevel);
}

std::string fieldReadReason(
    bool handleRead,
    bool itemIdRead,
    bool quantityRead
) {
    if (!handleRead && !itemIdRead && !quantityRead) {
        return "entry fields unreadable";
    }
    if (!handleRead) {
        return "handle unreadable";
    }
    if (!itemIdRead) {
        return "item id unreadable";
    }
    if (!quantityRead) {
        return "quantity unreadable";
    }
    return "unknown read failure";
}

void addSkip(
    InventoryReadReport& report,
    std::size_t index,
    std::uintptr_t entryAddress,
    std::uintptr_t moduleBase,
    std::string reason
) {
    report.skipped.push_back({
        index,
        entryAddress,
        relativeToModule(entryAddress, moduleBase),
        std::move(reason),
    });
}

bool shouldCollectDiagnostic(
    const InventoryReadLayout& layout,
    const InventoryReadReport& report,
    ItemCategory category
) {
    if (!layout.entryDiagnosticsEnabled) {
        return false;
    }
    if (report.entryDiagnostics.size() >= layout.entryDiagnosticsMax) {
        return false;
    }
    return layout.entryDiagnosticsIncludeConsumables || category != ItemCategory::Consumable;
}

InventoryEntryDiagnostic buildEntryDiagnostic(
    std::size_t index,
    std::uintptr_t entryAddress,
    std::uintptr_t moduleBase,
    const InventoryReadLayout& layout,
    std::uint32_t handle,
    std::uint32_t rawItemId,
    std::uint32_t quantity
) {
    InventoryEntryDiagnostic diagnostic;
    diagnostic.index = index;
    diagnostic.entryAddress = entryAddress;
    diagnostic.entryModuleRelative = relativeToModule(entryAddress, moduleBase);
    diagnostic.handle = handle;
    diagnostic.rawItemId = rawItemId;
    diagnostic.baseItemId = rawItemId & 0x0fffffffu;
    diagnostic.category = categoryFromInventoryItemId(rawItemId);
    diagnostic.quantity = quantity;
    diagnostic.upgradeLevel = decodeWeaponUpgradeLevel(diagnostic.baseItemId, diagnostic.category);
    diagnostic.modified = isLikelyModifiedWeaponParamId(diagnostic.baseItemId, diagnostic.category);

    const auto bytesToRead = std::min(layout.entryStride, kMaxDiagnosticEntryBytes);
    const auto wordsToRead = bytesToRead / sizeof(std::uint32_t);
    diagnostic.u32Words.reserve(wordsToRead);

    diagnostic.rawComplete = true;
    for (std::size_t wordIndex = 0; wordIndex < wordsToRead; ++wordIndex) {
        std::uint32_t word = 0;
        if (!readField(entryAddress, wordIndex * sizeof(std::uint32_t), word)) {
            diagnostic.rawComplete = false;
            break;
        }
        diagnostic.u32Words.push_back(word);
    }

    return diagnostic;
}

void collectHeaderDump(
    InventoryReadReport& report,
    std::uintptr_t moduleBase,
    std::uintptr_t playerDataBase,
    const InventoryReadLayout& layout
) {
    const auto bytesToDump = std::min(layout.headerDumpBytes, kMaxHeaderDumpBytes);
    const auto qwordsToDump = bytesToDump / sizeof(std::uint64_t);
    for (std::size_t i = 0; i < qwordsToDump; ++i) {
        const auto offset = layout.headerDumpOffset + (i * sizeof(std::uint64_t));
        std::uintptr_t address = 0;
        InventoryHeaderDumpWord word;
        word.offset = offset;
        if (!addOffset(playerDataBase, offset, address)) {
            word.moduleRelative = relativeToModule(playerDataBase, moduleBase);
            report.headerDump.push_back(word);
            continue;
        }

        word.address = address;
        word.moduleRelative = relativeToModule(address, moduleBase);
        word.readable = readCommittedMemory(address, &word.value, sizeof(word.value));
        report.headerDump.push_back(word);
    }
}

void collectFocusDump(
    InventoryReadReport& report,
    std::uintptr_t moduleBase,
    std::uintptr_t arrayPointer,
    const InventoryReadLayout& layout
) {
    const auto rowsToDump = std::min(layout.focusDumpCount, kMaxFocusDumpRows);
    const auto bytesToRead = std::min(layout.entryStride, kMaxDiagnosticEntryBytes);
    const auto wordsToRead = bytesToRead / sizeof(std::uint32_t);

    for (std::size_t row = 0; row < rowsToDump; ++row) {
        const auto index = layout.focusDumpStart + row;
        InventoryFocusDumpRow dump;
        dump.index = index;

        if (index > (std::numeric_limits<std::size_t>::max() / layout.entryStride)) {
            dump.entryAddress = arrayPointer;
            dump.entryModuleRelative = relativeToModule(arrayPointer, moduleBase);
            report.focusDump.push_back(std::move(dump));
            continue;
        }

        const auto entryOffset = index * layout.entryStride;
        std::uintptr_t entryAddress = 0;
        if (!addOffset(arrayPointer, entryOffset, entryAddress)) {
            dump.entryAddress = arrayPointer;
            dump.entryModuleRelative = relativeToModule(arrayPointer, moduleBase);
            report.focusDump.push_back(std::move(dump));
            continue;
        }

        dump.entryAddress = entryAddress;
        dump.entryModuleRelative = relativeToModule(entryAddress, moduleBase);
        dump.rawComplete = true;
        dump.u32Words.reserve(wordsToRead);
        for (std::size_t wordIndex = 0; wordIndex < wordsToRead; ++wordIndex) {
            std::uint32_t value = 0;
            if (!readField(entryAddress, wordIndex * sizeof(std::uint32_t), value)) {
                dump.rawComplete = false;
                break;
            }
            dump.u32Words.push_back(value);
        }
        report.focusDump.push_back(std::move(dump));
    }
}

} // namespace

InventoryReadReport readInventoryAtBase(
    std::uintptr_t moduleBase,
    std::uintptr_t playerDataBase,
    const InventoryReadLayout& layout
) {
    InventoryReadReport report;
    report.warnings.push_back(
        "upgrade/modified state is decoded conservatively from weapon param ID suffixes; mutation remains disabled."
    );

    if (playerDataBase == 0) {
        report.errors.push_back("Inventory base address is null.");
        return report;
    }
    if (layout.entryStride == 0) {
        report.errors.push_back("inventory_entry_stride must be greater than zero.");
        return report;
    }

    const auto effectiveMaxEntries = std::min(
        layout.maxEntries == 0 ? kMaxInventoryReadEntries : layout.maxEntries,
        kMaxInventoryReadEntries
    );
    if (effectiveMaxEntries == 0) {
        report.errors.push_back("inventory_max_entries must be greater than zero.");
        return report;
    }

    report.baseResolved = true;
    report.baseAddress = playerDataBase;
    report.baseModuleRelative = relativeToModule(playerDataBase, moduleBase);
    collectHeaderDump(report, moduleBase, playerDataBase, layout);

    std::uintptr_t arrayPointer = 0;
    if (!readField(playerDataBase, layout.arrayOffset, arrayPointer)) {
        report.errors.push_back("Could not read inventory array pointer.");
        return report;
    }
    report.arrayRead = true;
    report.arrayAddress = arrayPointer;
    report.arrayModuleRelative = relativeToModule(arrayPointer, moduleBase);

    std::uint32_t rawCount = 0;
    if (!readField(playerDataBase, layout.countOffset, rawCount)) {
        report.errors.push_back("Could not read inventory item count.");
        return report;
    }
    report.countRead = true;
    report.rawCount = rawCount;
    report.entriesToRead = std::min<std::size_t>(rawCount, effectiveMaxEntries);
    report.countCapped = static_cast<std::size_t>(rawCount) > effectiveMaxEntries;

    if (layout.capacityOffset != 0) {
        if (!readField(playerDataBase, layout.capacityOffset, report.rawCapacity)) {
            report.errors.push_back("Could not read inventory array capacity.");
            return report;
        }
        report.capacityRead = true;
        if (report.rawCapacity == 0 || report.rawCapacity > kMaxInventoryReadEntries ||
            rawCount > report.rawCapacity) {
            report.errors.push_back("Inventory count/capacity is implausible.");
            return report;
        }
        report.entriesToRead = std::min<std::size_t>(report.rawCapacity, effectiveMaxEntries);
        if (report.rawCapacity > effectiveMaxEntries) {
            report.warnings.push_back("inventory capacity scan truncated by max_entries; report may omit items");
        }
    }

    if ((report.entriesToRead > 0 || layout.focusDumpCount > 0) && arrayPointer == 0) {
        report.errors.push_back("Inventory array pointer is null.");
        return report;
    }
    if ((report.entriesToRead > 0 || layout.focusDumpCount > 0) &&
        !isAddressRangeCommittedReadable(arrayPointer, 1)) {
        report.errors.push_back("Inventory array pointer is not readable.");
        return report;
    }

    collectFocusDump(report, moduleBase, arrayPointer, layout);

    if (report.entriesToRead == 0) {
        return report;
    }

    report.upgradeModifiedDecoded = true;
    const auto entryBlock = readEntryBlock(report, arrayPointer, layout);

    for (std::size_t i = 0; i < report.entriesToRead; ++i) {
        if (i > (std::numeric_limits<std::size_t>::max() / layout.entryStride)) {
            addSkip(report, i, arrayPointer, moduleBase, "entry address overflow");
            continue;
        }
        const auto entryOffset = i * layout.entryStride;
        std::uintptr_t entryAddress = 0;
        if (!addOffset(arrayPointer, entryOffset, entryAddress)) {
            addSkip(report, i, arrayPointer, moduleBase, "entry address overflow");
            continue;
        }

        std::uint32_t handle = 0;
        std::uint32_t itemId = 0;
        std::uint32_t quantity = 0;
        const auto readFromBlock = canReadEntryFromBlock(report, entryBlock, i, layout.entryStride);
        const bool handleRead = readFromBlock
            ? readFieldFromBlock(entryBlock, i, layout.entryStride, layout.handleOffset, handle)
            : readField(entryAddress, layout.handleOffset, handle);
        const bool itemIdRead = readFromBlock
            ? readFieldFromBlock(entryBlock, i, layout.entryStride, layout.itemIdOffset, itemId)
            : readField(entryAddress, layout.itemIdOffset, itemId);
        const bool quantityRead = readFromBlock
            ? readFieldFromBlock(entryBlock, i, layout.entryStride, layout.quantityOffset, quantity)
            : readField(entryAddress, layout.quantityOffset, quantity);

        InventoryReadSlot rawSlot;
        rawSlot.index = i;
        rawSlot.handle = handle;
        rawSlot.rawItemId = itemId;
        rawSlot.quantity = quantity;
        rawSlot.fieldsReadable = handleRead && itemIdRead && quantityRead;

        if (!handleRead || !itemIdRead || !quantityRead) {
            report.slots.push_back(rawSlot);
            addSkip(report, i, entryAddress, moduleBase, fieldReadReason(handleRead, itemIdRead, quantityRead));
            continue;
        }
        if (handle == 0 || itemId == 0xffffffffu) {
            rawSlot.empty = true;
            report.slots.push_back(rawSlot);
            addSkip(report, i, entryAddress, moduleBase, "empty slot");
            if (!report.capacityRead && report.items.size() < rawCount && report.entriesToRead < effectiveMaxEntries) {
                ++report.entriesToRead;
            }
            continue;
        }
        report.slots.push_back(rawSlot);

        ItemInstance item;
        item.instanceId = layout.instanceIdFromSlot ? static_cast<std::uint32_t>(i + 1) : handle;
        item.baseItemId = itemId & 0x0fffffffu;
        item.category = categoryFromInventoryItemId(itemId);
        item.quantity = quantity > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(quantity);
        item.upgradeLevel = decodeWeaponUpgradeLevel(item.baseItemId, item.category);
        item.modified = isLikelyModifiedWeaponParamId(item.baseItemId, item.category);
        item.sellValue = 0;
        report.items.push_back(item);

        if (shouldCollectDiagnostic(layout, report, item.category)) {
            report.entryDiagnostics.push_back(buildEntryDiagnostic(
                i,
                entryAddress,
                moduleBase,
                layout,
                handle,
                itemId,
                quantity
            ));
        }

        if (!report.capacityRead && report.items.size() >= rawCount) {
            break;
        }
    }

    if (!report.capacityRead && report.items.size() < rawCount && report.entriesToRead >= effectiveMaxEntries) {
        report.warnings.push_back(
            "inventory scan reached max_entries before decoding raw_count non-empty rows"
        );
    }

    return report;
}

InventoryReadReport readInventoryLive(
    std::uintptr_t moduleBase,
    const InventoryReadLayout& layout
) {
    const auto resolved = verifyPointerChainLive(moduleBase, layout.baseChain, sizeof(std::uintptr_t));
    if (!resolved.resolved) {
        InventoryReadReport report;
        report.warnings.push_back(
            "upgrade/modified decode did not run because the inventory base chain did not resolve."
        );
        report.errors.push_back("Inventory base chain unresolved.");
        return report;
    }

    return readInventoryAtBase(moduleBase, resolved.resolvedAddress, layout);
}

std::string formatInventoryReadSummary(const InventoryReadReport& report) {
    std::ostringstream out;
    out << "inventory_read:"
        << " base_resolved=" << report.baseResolved
        << ", array_read=" << report.arrayRead
        << ", count_read=" << report.countRead
        << ", raw_count=" << report.rawCount
        << ", capacity_read=" << report.capacityRead
        << ", raw_capacity=" << report.rawCapacity
        << ", entries_to_read=" << report.entriesToRead
        << ", decoded=" << report.items.size()
        << ", snapshot_slots=" << report.slots.size()
        << ", skipped=" << report.skipped.size()
        << ", entry_diagnostics=" << report.entryDiagnostics.size()
        << ", header_dump_qwords=" << report.headerDump.size()
        << ", focus_dump_rows=" << report.focusDump.size()
        << ", count_capped=" << report.countCapped
        << ", entry_block_read=" << report.entryBlockRead
        << ", entry_block_bytes=" << report.entryBlockBytes
        << ", upgrade_modified_decoded=" << report.upgradeModifiedDecoded;

    if (report.baseResolved) {
        out << ", base_rva=" << formatRelativeAddress(report.baseModuleRelative);
    }
    if (report.arrayRead) {
        out << ", array_rva=" << formatRelativeAddress(report.arrayModuleRelative);
    }
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }

    return out.str();
}

bool shouldExportInventorySnapshot(const InventoryReadReport& report) {
    return report.baseResolved &&
        report.arrayRead &&
        report.countRead &&
        report.rawCount > 0 &&
        !report.slots.empty();
}

std::string formatInventoryReadSkip(const InventoryReadSkip& skip) {
    std::ostringstream out;
    out << "inventory_read_skip:"
        << " index=" << skip.index
        << ", entry_rva=" << formatRelativeAddress(skip.entryModuleRelative)
        << ", reason=\"" << skip.reason << "\"";
    return out.str();
}

std::string formatInventoryReadSkipSummary(
    const std::vector<InventoryReadSkip>& skips,
    std::size_t loggedCount
) {
    const auto logged = std::min(loggedCount, skips.size());
    std::map<std::string, std::size_t> reasonCounts;
    for (const auto& skip : skips) {
        ++reasonCounts[skip.reason];
    }

    std::ostringstream out;
    out << "inventory_read_skip_summary:"
        << " total=" << skips.size()
        << ", logged=" << logged
        << ", omitted=" << (skips.size() - logged);

    for (const auto& [reason, count] : reasonCounts) {
        out << ", reason=\"" << reason << "\":" << count;
    }

    return out.str();
}

std::string formatInventoryEntryDiagnostic(const InventoryEntryDiagnostic& diagnostic) {
    std::ostringstream out;
    out << "inventory_entry_diag:"
        << " index=" << diagnostic.index
        << ", entry_rva=" << formatRelativeAddress(diagnostic.entryModuleRelative)
        << ", handle=0x" << std::hex << diagnostic.handle
        << ", raw_item_id=0x" << diagnostic.rawItemId
        << std::dec
        << ", base_item_id=" << diagnostic.baseItemId
        << ", category=";

    switch (diagnostic.category) {
    case ItemCategory::Weapon: out << "weapon"; break;
    case ItemCategory::Shield: out << "shield"; break;
    case ItemCategory::Armor: out << "armor"; break;
    case ItemCategory::Talisman: out << "talisman"; break;
    case ItemCategory::AshOfWar: out << "ash_of_war"; break;
    case ItemCategory::Spell: out << "spell"; break;
    case ItemCategory::Consumable: out << "consumable"; break;
    case ItemCategory::Material: out << "material"; break;
    case ItemCategory::KeyItem: out << "keyitem"; break;
    case ItemCategory::Unknown: out << "unknown"; break;
    }

    out << ", quantity=" << diagnostic.quantity
        << ", upgrade_level=" << diagnostic.upgradeLevel
        << ", modified_id_variant=" << diagnostic.modified
        << ", raw_complete=" << diagnostic.rawComplete;

    for (std::size_t i = 0; i < diagnostic.u32Words.size(); ++i) {
        out << ", u32[" << i << "]=0x" << std::hex << diagnostic.u32Words[i] << std::dec;
    }

    return out.str();
}

std::string formatInventorySnapshotFixture(const InventoryReadReport& report) {
    std::ostringstream out;
    out << "# ERDuplicateSeller inventory snapshot fixture\n"
        << "# source=inventory_read"
        << " raw_count=" << report.rawCount
        << " entries_to_read=" << report.entriesToRead
        << " decoded=" << report.items.size()
        << " skipped=" << report.skipped.size()
        << " count_capped=" << (report.countCapped ? 1 : 0)
        << "\n";
    if (report.countCapped) {
        out << "# WARNING: count was capped; this snapshot is incomplete for before/after comparison.\n";
    }
    if (!report.countRead) {
        out << "# WARNING: count was not readable; this snapshot is diagnostic-only.\n";
    }
    if (!report.headerDump.empty()) {
        out << "# header_dump_qwords=" << report.headerDump.size() << "\n";
        for (const auto& word : report.headerDump) {
            out << "# header_qword=offset:" << formatOffset(word.offset)
                << ",rva:" << formatRelativeAddress(word.moduleRelative)
                << ",readable:" << (word.readable ? "true" : "false");
            if (word.readable) {
                out << ",value:0x" << std::hex << word.value << std::dec;
            }
            out << "\n";
        }
    }
    if (!report.focusDump.empty()) {
        out << "# focus_dump_rows=" << report.focusDump.size() << "\n";
        for (const auto& row : report.focusDump) {
            out << "# focus_row=index:" << row.index
                << ",rva:" << formatRelativeAddress(row.entryModuleRelative)
                << ",raw_complete:" << (row.rawComplete ? "true" : "false");
            for (std::size_t i = 0; i < row.u32Words.size(); ++i) {
                out << ",u32[" << i << "]:0x" << std::hex << row.u32Words[i] << std::dec;
            }
            out << "\n";
        }
    }

    out << "count=" << report.rawCount << "\n";
    for (const auto& slot : report.slots) {
        out << "slot=index:" << slot.index << ",";
        if (!slot.fieldsReadable) {
            out << "unreadable:true\n";
            continue;
        }
        if (slot.empty) {
            out << "empty:true\n";
            continue;
        }
        out << "handle:" << formatHex32(slot.handle)
            << ",raw:" << formatHex32(slot.rawItemId)
            << ",qty:" << slot.quantity
            << "\n";
    }

    return out.str();
}

} // namespace erdup
