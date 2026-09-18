#include "PatternScanner.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

} // namespace

int main() {
    const auto parsed = erdup::parseBytePattern("48 8B ?? 10");
    if (!parsed.errors.empty()) {
        return fail("Expected valid wildcard pattern.");
    }
    if (parsed.pattern.size() != 4 || parsed.pattern[2].has_value()) {
        return fail("Wildcard token was not parsed correctly.");
    }

    const std::vector<std::uint8_t> data = {
        0x90, 0x48, 0x8B, 0x01, 0x10,
        0x48, 0x8B, 0xFF, 0x10, 0xC3,
    };

    const auto matches = erdup::findPattern(data.data(), data.size(), parsed.pattern);
    if (matches.size() != 2 || matches[0] != 1 || matches[1] != 5) {
        return fail("Pattern scanner did not find expected matches.");
    }

    const auto invalid = erdup::parseBytePattern("48 GG 10");
    if (invalid.errors.empty()) {
        return fail("Invalid pattern token was accepted.");
    }

    std::cout << "Pattern scanner smoke test passed.\n";
    return EXIT_SUCCESS;
}
