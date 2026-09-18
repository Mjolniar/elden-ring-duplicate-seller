#include "FieldDump.h"

#include "MemoryReader.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace erdup {

namespace {

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

} // namespace

FieldDumpReport dumpStructFieldsAt(
    std::uintptr_t moduleBase,
    std::uintptr_t structBase,
    const PointerChain& chain,
    std::size_t requestedBytes,
    std::size_t stride
) {
    FieldDumpReport report;
    report.chain = chain;
    report.requestedBytes = requestedBytes;
    report.bytesToRead = std::min(requestedBytes, kMaxFieldDumpBytes);
    report.stride = stride;
    report.capped = requestedBytes > report.bytesToRead;

    if (structBase == 0) {
        report.errors.push_back("Field dump base address is null.");
        return report;
    }
    if (stride == 0) {
        report.errors.push_back("field_dump_stride must be greater than zero.");
        return report;
    }

    report.chainResolved = true;
    report.baseAddress = structBase;
    report.baseModuleRelative = relativeToModule(structBase, moduleBase);

    for (std::size_t offset = 0; offset + sizeof(std::uint64_t) <= report.bytesToRead; offset += stride) {
        FieldDumpSlot slot;
        slot.offset = offset;

        if (addWouldOverflow(structBase, offset)) {
            slot.skippedReason = "address overflow";
            report.slots.push_back(slot);
            continue;
        }

        const auto slotAddress = structBase + offset;
        std::uint64_t qword = 0;
        if (!readCommittedMemory(slotAddress, &qword, sizeof(qword))) {
            slot.skippedReason = "unreadable";
            report.slots.push_back(slot);
            continue;
        }

        slot.slotRead = true;
        slot.qword = qword;
        slot.low32 = static_cast<std::uint32_t>(qword & std::numeric_limits<std::uint32_t>::max());

        const auto candidatePointer = static_cast<std::uintptr_t>(qword);
        slot.pointerReadable = isAddressRangeCommittedReadable(candidatePointer, sizeof(std::uintptr_t));
        if (slot.pointerReadable) {
            slot.pointerModuleRelative = relativeToModule(candidatePointer, moduleBase);
        }

        report.slots.push_back(slot);
    }

    return report;
}

FieldDumpReport runFieldDumpLive(
    std::uintptr_t moduleBase,
    const PointerChain& chain,
    std::size_t requestedBytes,
    std::size_t stride
) {
    FieldDumpReport report;
    report.chain = chain;
    report.requestedBytes = requestedBytes;
    report.bytesToRead = std::min(requestedBytes, kMaxFieldDumpBytes);
    report.stride = stride;
    report.capped = requestedBytes > report.bytesToRead;

    const auto resolved = verifyPointerChainLive(moduleBase, chain, sizeof(std::uintptr_t));
    if (!resolved.resolved) {
        report.errors.push_back("Field dump chain unresolved.");
        return report;
    }

    return dumpStructFieldsAt(moduleBase, resolved.resolvedAddress, chain, requestedBytes, stride);
}

std::string formatFieldDumpSummary(const FieldDumpReport& report) {
    std::ostringstream out;
    out << "field_dump:"
        << " bytes_requested=" << report.requestedBytes
        << ", bytes_read=" << report.bytesToRead
        << ", stride=" << report.stride
        << ", capped=" << report.capped
        << ", resolved=" << report.chainResolved
        << ", slots=" << report.slots.size();

    if (report.chainResolved) {
        out << ", base_rva=" << formatRelativeAddress(report.baseModuleRelative);
    }
    for (const auto& error : report.errors) {
        out << ", error=\"" << error << "\"";
    }
    return out.str();
}

std::string formatFieldDumpSlot(const FieldDumpSlot& slot) {
    std::ostringstream out;
    out << "field_dump_slot: offset=" << formatOffset(slot.offset);
    if (!slot.slotRead) {
        out << ", skipped=\"" << slot.skippedReason << "\"";
        return out.str();
    }

    out << ", u32=" << slot.low32
        << ", pointer=" << (slot.pointerReadable ? "yes" : "no");
    if (slot.pointerReadable) {
        out << ", pointer_rva=" << formatRelativeAddress(slot.pointerModuleRelative);
    }
    return out.str();
}

} // namespace erdup
