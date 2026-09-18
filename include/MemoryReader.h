#pragma once

#include <cstddef>
#include <cstdint>

namespace erdup {

struct SafeMemoryRange {
    std::uintptr_t baseAddress = 0;
    std::size_t size = 0;
};

bool isRangeContained(SafeMemoryRange safeRange, std::uintptr_t address, std::size_t length);
bool isAddressRangeCommittedReadable(std::uintptr_t address, std::size_t length);
bool readCommittedMemory(std::uintptr_t address, void* output, std::size_t length);
bool readMemory(SafeMemoryRange safeRange, std::uintptr_t address, void* output, std::size_t length);

} // namespace erdup
