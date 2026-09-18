#include "MemoryReader.h"

#include <Windows.h>

#include <cstring>
#include <limits>

namespace erdup {

namespace {

bool addWouldOverflow(std::uintptr_t value, std::size_t addend) {
    return addend > std::numeric_limits<std::uintptr_t>::max() - value;
}

bool isReadableProtect(DWORD protect) {
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) {
        return false;
    }

    const DWORD baseProtect = protect & 0xff;
    return baseProtect == PAGE_READONLY ||
        baseProtect == PAGE_READWRITE ||
        baseProtect == PAGE_WRITECOPY ||
        baseProtect == PAGE_EXECUTE_READ ||
        baseProtect == PAGE_EXECUTE_READWRITE ||
        baseProtect == PAGE_EXECUTE_WRITECOPY;
}

bool guardedCopy(void* output, const void* input, std::size_t length) noexcept {
#if defined(_MSC_VER)
    __try {
        std::memcpy(output, input, length);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(output, input, length);
    return true;
#endif
}

} // namespace

bool isRangeContained(SafeMemoryRange safeRange, std::uintptr_t address, std::size_t length) {
    if (safeRange.baseAddress == 0 || addWouldOverflow(safeRange.baseAddress, safeRange.size)) {
        return false;
    }

    const auto rangeEnd = safeRange.baseAddress + safeRange.size;
    if (length == 0) {
        return address >= safeRange.baseAddress && address <= rangeEnd;
    }

    if (addWouldOverflow(address, length)) {
        return false;
    }

    const auto readEnd = address + length;
    return address >= safeRange.baseAddress && readEnd <= rangeEnd;
}

bool isAddressRangeCommittedReadable(std::uintptr_t address, std::size_t length) {
    if (length == 0) {
        return true;
    }
    if (address == 0 || addWouldOverflow(address, length)) {
        return false;
    }

    const auto end = address + length;
    auto cursor = address;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) {
            return false;
        }

        const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (addWouldOverflow(regionBase, info.RegionSize)) {
            return false;
        }
        const auto regionEnd = regionBase + info.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }

        if (info.State != MEM_COMMIT || !isReadableProtect(info.Protect)) {
            return false;
        }

        cursor = regionEnd < end ? regionEnd : end;
    }

    return true;
}

bool readCommittedMemory(std::uintptr_t address, void* output, std::size_t length) {
    if (length == 0) {
        return true;
    }
    if (output == nullptr || !isAddressRangeCommittedReadable(address, length)) {
        return false;
    }

    return guardedCopy(output, reinterpret_cast<const void*>(address), length);
}

bool readMemory(SafeMemoryRange safeRange, std::uintptr_t address, void* output, std::size_t length) {
    if (!isRangeContained(safeRange, address, length)) {
        return false;
    }
    return readCommittedMemory(address, output, length);
}

} // namespace erdup
