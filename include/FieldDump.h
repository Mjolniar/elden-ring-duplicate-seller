#pragma once

#include "PointerScanner.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

constexpr std::size_t kMaxFieldDumpBytes = 0x4000;

struct FieldDumpSlot {
    std::size_t offset = 0;
    bool slotRead = false;
    std::uint64_t qword = 0;
    std::uint32_t low32 = 0;
    bool pointerReadable = false;
    std::intptr_t pointerModuleRelative = 0;
    std::string skippedReason;
};

struct FieldDumpReport {
    PointerChain chain;
    bool chainResolved = false;
    std::uintptr_t baseAddress = 0;
    std::intptr_t baseModuleRelative = 0;
    std::size_t requestedBytes = 0;
    std::size_t bytesToRead = 0;
    std::size_t stride = 0;
    bool capped = false;
    std::vector<FieldDumpSlot> slots;
    std::vector<std::string> errors;
};

FieldDumpReport dumpStructFieldsAt(
    std::uintptr_t moduleBase,
    std::uintptr_t structBase,
    const PointerChain& chain,
    std::size_t requestedBytes,
    std::size_t stride
);

FieldDumpReport runFieldDumpLive(
    std::uintptr_t moduleBase,
    const PointerChain& chain,
    std::size_t requestedBytes,
    std::size_t stride
);

std::string formatFieldDumpSummary(const FieldDumpReport& report);
std::string formatFieldDumpSlot(const FieldDumpSlot& slot);

} // namespace erdup
