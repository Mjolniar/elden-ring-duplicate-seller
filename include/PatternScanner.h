#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace erdup {

using BytePattern = std::vector<std::optional<std::uint8_t>>;

struct PatternParseResult {
    BytePattern pattern;
    std::vector<std::string> errors;
};

PatternParseResult parseBytePattern(const std::string& patternText);
std::vector<std::size_t> findPattern(const std::uint8_t* data, std::size_t size, const BytePattern& pattern);

} // namespace erdup
