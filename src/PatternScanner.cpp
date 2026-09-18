#include "PatternScanner.h"

#include <cctype>
#include <sstream>

namespace erdup {

namespace {

bool isWildcardToken(const std::string& token) {
    if (token == "?" || token == "??") {
        return true;
    }
    for (const auto ch : token) {
        if (ch != '?') {
            return false;
        }
    }
    return !token.empty();
}

bool isHexByteToken(const std::string& token) {
    if (token.size() != 2) {
        return false;
    }
    for (const auto ch : token) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

std::uint8_t parseHexByte(const std::string& token) {
    return static_cast<std::uint8_t>(std::stoul(token, nullptr, 16));
}

} // namespace

PatternParseResult parseBytePattern(const std::string& patternText) {
    PatternParseResult result;
    std::istringstream input(patternText);
    std::string token;
    std::size_t index = 0;

    while (input >> token) {
        if (isWildcardToken(token)) {
            result.pattern.push_back(std::nullopt);
        } else if (isHexByteToken(token)) {
            result.pattern.push_back(parseHexByte(token));
        } else {
            std::ostringstream error;
            error << "Invalid pattern token at index " << index << ": " << token;
            result.errors.push_back(error.str());
        }
        ++index;
    }

    if (result.pattern.empty()) {
        result.errors.push_back("Pattern is empty.");
    }

    return result;
}

std::vector<std::size_t> findPattern(const std::uint8_t* data, std::size_t size, const BytePattern& pattern) {
    std::vector<std::size_t> matches;
    if (data == nullptr || pattern.empty() || pattern.size() > size) {
        return matches;
    }

    for (std::size_t offset = 0; offset <= size - pattern.size(); ++offset) {
        bool matched = true;
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i].has_value() && data[offset + i] != pattern[i].value()) {
                matched = false;
                break;
            }
        }

        if (matched) {
            matches.push_back(offset);
        }
    }

    return matches;
}

} // namespace erdup
