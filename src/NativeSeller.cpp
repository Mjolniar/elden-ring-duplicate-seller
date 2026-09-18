#include "NativeSeller.h"
#include "InventoryReader.h"
#include "InventoryReport.h"
#include "NativeInventoryIdentity.h"
#include "Logger.h"
#include "MemoryReader.h"
#include "PatternScanner.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>
#include <tuple>

namespace erdup {
namespace {
bool faulted = false;
template<class T> T read(std::uintptr_t address) {
    T value{};
    if (!readCommittedMemory(address, &value, sizeof(value))) throw std::runtime_error("Game memory is not readable");
    return value;
}
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
bool match(std::uintptr_t address, const char* pattern) {
    auto parsed = parseBytePattern(pattern);
    std::vector<std::uint8_t> bytes(parsed.pattern.size());
    if (!readCommittedMemory(address, bytes.data(), bytes.size())) return false;
    return !findPattern(bytes.data(), bytes.size(), parsed.pattern).empty();
}
std::uintptr_t playerFor(const ModuleInfo& module) {
    const auto manager = read<std::uintptr_t>(module.baseAddress + 0x3d61f98);
    require(manager != 0, "Game data not loaded");
    auto player = read<std::uintptr_t>(manager + 8);
    require(player != 0, "Load a character before selling");
    return player;
}
InventoryReadReport capture(std::uintptr_t player) {
    InventoryReadLayout layout;
    layout.instanceIdFromSlot = true;
    layout.arrayOffset = 0x418; layout.countOffset = 0x420; layout.capacityOffset = 0x414;
    layout.entryStride = 0x18; layout.handleOffset = 0; layout.itemIdOffset = 4;
    layout.quantityOffset = 8; layout.maxEntries = 4096;
    auto report = readInventoryAtBase(0, player, layout);
    require(report.errors.empty() && report.entryBlockRead && report.rawCapacity == 2688,
        "Inventory is not ready for selling");
    return report;
}
std::array<int, 22> equipment(std::uintptr_t player, const InventoryReadReport& report) {
    const auto indices = read<std::array<int, 22>>(player + 0x2B8);
    for (int index : indices) {
        require(index == -1 || (index >= 0 && index < 3072), "Unexpected equipped-item index");
        if (index >= 384) {
            const auto& slot = report.slots.at(index - 384);
            require(slot.fieldsReadable && !slot.empty, "Equipped item does not match inventory");
        }
    }
    return indices;
}
void markEquipped(std::uintptr_t player, InventoryReadReport& report) {
    markNativeEquipped(report, equipment(player, report));
}
// Small SEH wrappers contain no C++ objects requiring stack unwinding.
bool callRemove(std::uintptr_t function, std::uintptr_t controller, unsigned index) {
    __try {
        reinterpret_cast<int(__fastcall*)(std::uintptr_t, unsigned, int)>(function)(controller, index, 1);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool callRunes(std::uintptr_t function, std::uintptr_t player, int amount, int* awarded) {
    __try {
        *awarded = reinterpret_cast<int(__fastcall*)(std::uintptr_t, int)>(function)(player, amount);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool requestSave(std::uintptr_t gameManager) {
    __try {
        *reinterpret_cast<volatile unsigned char*>(gameManager + 0xb72) = 1;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

std::string runNativeSale(const ModuleInfo& module, const Config& config, const MetadataIndex& metadata,
                         const SellValueIndex& values, const std::filesystem::path& directory) {
    int sold = 0, runes = 0;
    bool mutationStarted = false;
    try {
        require(!faulted, "Previous sale fault: restart the game before retrying");
        require(config.enabled && config.nativeSaleEnabled, "Native selling is disabled");
        require(module.fileVersion == "2.7.1.0" && config.supportedGameVersion == "2.7.1.0" &&
            module.imageSize == 98621952, "Native seller requires the validated 2.7.1.0 executable");
        require(match(module.baseAddress + 0x256560, "48 8B 05 31 BA B0 03 48 85 C0 74 05 48 8B 40 58 C3 C3") &&
            match(module.baseAddress + 0x24d1e0, "48 89 5C 24 20 89 54 24 10 55 56 57 41 56 41 57 48 83 EC 40 8B F2 48 8B E9 45 85 C0 74 50") &&
            match(module.baseAddress + 0x25e0e0, "44 8B 49 6C 45 33 DB 44 89 5C 24 10 45 8D 04 11") &&
            match(module.baseAddress + 0x67aba0, "48 8B 05 E1 2D 6F 03 80 B8 88 0D 00 00 0D 0F 94 C0 C3"),
            "Native function bytes differ from validated build");
        setSaleIndicator("Autosell: checking inventory...", true);
        const auto player = playerFor(module);
        const auto gameManager = read<std::uintptr_t>(module.baseAddress + 0x3d6d988);
        const auto saveSlot = read<int>(gameManager + 0xac0);
        require(saveSlot >= 0 && saveSlot < 10, "No active save slot");
        auto inventory = capture(player);
        markEquipped(player, inventory);
        // Equipment and Ashes of War only. Consumables are always preserved.
        auto policy = config;
        policy.keepWeapons = std::max(2, policy.keepWeapons);
        policy.keepShields = std::max(2, policy.keepShields);
        policy.keepArmor = std::max(1, policy.keepArmor);
        policy.keepTalismans = std::max(1, policy.keepTalismans);
        policy.keepAshesOfWar = std::max(2, policy.keepAshesOfWar);
        policy.itemStockRules.clear();
        policy.sellSpells = false; policy.sellConsumables = false;
        auto plan = buildInventoryReport(inventory.items, policy, &metadata, &values);
        std::ofstream audit(directory / ("native_sale_" + std::to_string(GetTickCount64()) + ".txt"));
        require(static_cast<bool>(audit), "Cannot open sale audit file");
        audit << formatInventoryReport(plan) << '\n'; audit.flush();
        logInfo("Native sale plan: " + std::to_string(plan.totalItemsToSell) + " items, " +
            std::to_string(plan.totalRunes) + " runes");
        if (config.dryRun) {
            const auto message = "Preview: " + std::to_string(plan.totalItemsToSell) + " duplicates, " +
                std::to_string(plan.totalRunes) + " runes (dry run)";
            setSaleIndicator(message); return message;
        }
        // Validate every candidate's price before touching any item.
        for (const auto& row : plan.rows) if (row.selectedForSale) {
            require(row.sellValueResolution == SellValueResolution::Resolved && row.item.sellValue > 0,
                "A duplicate has no verified trader price; no items sold");
            require(row.quantityToSell == 1 && row.item.quantity == 1,
                "Stacked equipment is not supported by this provisional adapter");
        }
        for (const auto& row : plan.rows) {
            if (!row.selectedForSale) continue;
            if (config.nativeSaleMaxItems > 0 && sold >= config.nativeSaleMaxItems) break;
            require(playerFor(module) == player, "Character changed during sale");
            auto before = capture(player);
            markEquipped(player, before);
            auto freshPlan = buildInventoryReport(before.items, policy, &metadata, &values);
            const auto eligible = std::find_if(freshPlan.rows.begin(), freshPlan.rows.end(), [&](const auto& fresh) {
                return fresh.item.instanceId == row.item.instanceId && fresh.selectedForSale &&
                    !fresh.item.equipped && fresh.item.baseItemId == row.item.baseItemId;
            });
            require(eligible != freshPlan.rows.end(), "Inventory or equipment changed during sale");
            const auto slot = std::find_if(before.slots.begin(), before.slots.end(), [&](const auto& entry) {
                return !entry.empty && entry.index + 1 == row.item.instanceId;
            });
            require(slot != before.slots.end(), "Sale item disappeared");
            const auto controller = read<std::uintptr_t>(player + 0x5d0);
            require(controller == player + 0x408 && read<unsigned>(controller + 0x1c) == 384 &&
                read<std::uintptr_t>(controller + 0x40) == before.arrayAddress,
                "Inventory controller no longer matches validated layout");
            const auto equippedBefore = equipment(player, before);
            unsigned balance = read<unsigned>(player + 0x6c);
            require(balance <= 999999999u && static_cast<unsigned>(row.runeValue) <= 999999999u - balance,
                "Rune limit would prevent full trader payout");
            setSaleIndicator("Autosell running: " + std::to_string(sold + 1) + "/" +
                std::to_string(config.nativeSaleMaxItems > 0 ? std::min(config.nativeSaleMaxItems, plan.totalItemsToSell) : plan.totalItemsToSell) +
                "  |  " + row.displayName, true);
            audit << "INTENT handle=" << slot->handle << " id=" << slot->rawItemId << " slot=" << slot->index
                << " count=" << before.rawCount << " balance=" << balance << " price=" << row.runeValue << '\n';
            audit.flush();
            mutationStarted = true;
            require(callRemove(module.baseAddress + 0x24d1e0, controller, static_cast<unsigned>(slot->index) + 384),
                "Native removal fault; inspect audit before retrying");
            auto after = capture(player);
            auto expected = nativeInventoryIdentities(before);
            expected.erase(slot->index);
            require(nativeInventoryIdentities(after) == expected && before.rawCount == after.rawCount + 1,
                "Native removal result differs from expected single item");
            require(equipment(player, after) == equippedBefore, "Equipment changed during removal");
            ++sold;
            require(read<unsigned>(player + 0x6c) == balance, "Rune balance changed during removal");
            int awarded = 0;
            require(callRunes(module.baseAddress + 0x25e0e0, player, row.runeValue, &awarded) &&
                awarded == row.runeValue && read<unsigned>(player + 0x6c) == balance + row.runeValue,
                "Rune payout verification failed; inspect audit before retrying");
            runes += row.runeValue;
            audit << "COMPLETED handle=" << slot->handle << " awarded=" << awarded
                << " balance=" << balance + row.runeValue << '\n'; audit.flush();
            logInfo("Native sale confirmed: " + row.displayName + " for " + std::to_string(awarded) + " runes");
            Sleep(80);
        }
        if (sold > 0) {
            require(playerFor(module) == player && read<int>(gameManager + 0xac0) == saveSlot,
                "Character changed before save request");
            require(requestSave(gameManager), "Sale completed but save request failed");
            audit << "SAVE_REQUEST slot=" << saveSlot << '\n'; audit.flush();
            logInfo("Native sale: requested game-owned save");
        }
        const auto message = "Sold " + std::to_string(sold) + " duplicates for " + std::to_string(runes) + " runes";
        setSaleIndicator(message); logInfo(message); return message;
    } catch (const std::exception& error) {
        if (mutationStarted) faulted = true;
        auto message = std::string("Autosell stopped: ") + error.what() + " (confirmed " +
            std::to_string(sold) + " removals, " + std::to_string(runes) + " runes)";
        logError(message); setSaleIndicator(message); return message;
    }
}
}
