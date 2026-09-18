#include "HandleDiagnostics.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t handle,
    std::uint32_t baseItemId,
    erdup::ItemCategory category,
    int quantity = 1
) {
    erdup::ItemInstance instance;
    instance.instanceId = handle;
    instance.baseItemId = baseItemId;
    instance.category = category;
    instance.quantity = quantity;
    return instance;
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

bool contains(const std::string& text, const std::string& expected) {
    return text.find(expected) != std::string::npos;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

int assertHandleReferenceScanFindsBoundedContext() {
    std::vector<std::byte> regionBytes(256);
    writeU32(regionBytes, 0x40 - 8, 0x11111111);
    writeU32(regionBytes, 0x40 - 4, 0x22222222);
    writeU32(regionBytes, 0x40, 0x80800097);
    writeU32(regionBytes, 0x40 + 4, 0x33333333);
    writeU32(regionBytes, 0x40 + 8, 0x44444444);
    writeU32(regionBytes, 0x80, 0x80800097);
    writeU32(regionBytes, 0xc0, 0x40800001);

    const std::vector<erdup::ItemInstance> items = {
        item(0x80800097, 34020000, erdup::ItemCategory::Weapon),
        item(0x40800001, 115, erdup::ItemCategory::Consumable),
    };

    erdup::HandleDiagnosticLimits limits;
    limits.maxHandles = 8;
    limits.maxMatchesPerHandle = 1;
    limits.contextWordsBefore = 2;
    limits.contextWordsAfter = 2;
    limits.includeConsumables = false;

    const auto base = reinterpret_cast<std::uintptr_t>(regionBytes.data());
    const auto report = erdup::scanHandleReferencesInRegions(
        {{base, regionBytes.size()}},
        base,
        items,
        limits
    );

    if (report.targetsConsidered != 1 || report.matches.size() != 1 || report.truncated) {
        return fail("Expected one non-consumable target and one capped handle match.");
    }

    const auto& match = report.matches.front();
    if (match.target.handle != 0x80800097 ||
        match.target.baseItemId != 34020000 ||
        match.matchWordIndex != 2 ||
        match.contextWords.size() != 5 ||
        !match.contextComplete) {
        return fail("Handle match did not preserve target fields and bounded context.");
    }
    if (match.contextWords[0] != 0x11111111 ||
        match.contextWords[1] != 0x22222222 ||
        match.contextWords[2] != 0x80800097 ||
        match.contextWords[3] != 0x33333333 ||
        match.contextWords[4] != 0x44444444) {
        return fail("Handle match context words were not captured in order.");
    }

    const auto summary = erdup::formatHandleDiagnosticSummary(report);
    const auto formatted = erdup::formatHandleDiagnosticMatch(match);
    if (!contains(summary, "handle_ref_scan:") ||
        !contains(summary, "matches=1") ||
        !contains(formatted, "handle_ref_match:") ||
        !contains(formatted, "handle=0x80800097") ||
        !contains(formatted, "match_word=2") ||
        !contains(formatted, "u32[2]=0x80800097")) {
        return fail("Formatted handle diagnostics are missing expected fields.");
    }

    return EXIT_SUCCESS;
}

int assertConsumableTargetsCanBeIncluded() {
    std::vector<std::byte> regionBytes(128);
    writeU32(regionBytes, 0x20, 0x40800001);

    const std::vector<erdup::ItemInstance> items = {
        item(0x40800001, 115, erdup::ItemCategory::Consumable),
    };

    erdup::HandleDiagnosticLimits limits;
    limits.includeConsumables = true;
    limits.maxHandles = 4;
    limits.maxMatchesPerHandle = 4;
    limits.contextWordsBefore = 1;
    limits.contextWordsAfter = 1;

    const auto base = reinterpret_cast<std::uintptr_t>(regionBytes.data());
    const auto report = erdup::scanHandleReferencesInRegions(
        {{base, regionBytes.size()}},
        base,
        items,
        limits
    );

    if (report.targetsConsidered != 1 || report.matches.size() != 1 ||
        report.matches.front().target.category != erdup::ItemCategory::Consumable) {
        return fail("Consumable handle target should be scanned only when explicitly included.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertHandleReferenceScanFindsBoundedContext() != EXIT_SUCCESS ||
        assertConsumableTargetsCanBeIncluded() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Handle diagnostics test passed.\n";
    return EXIT_SUCCESS;
}
