#include "WatchedFile.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& text, const std::string& expected) {
    return text.find(expected) != std::string::npos;
}

int assertTriggerDecisionUsesSizeAndHash() {
    erdup::WatchedFileState previous;
    previous.readable = true;
    previous.size = 4;
    previous.contentHash = 0x100;

    const auto initial = erdup::decideWatchedFileTrigger(false, {}, previous);
    if (!initial.changed || initial.reason != "initial readable state") {
        return fail("Initial readable watched-file state should trigger.");
    }

    auto current = previous;
    auto unchanged = erdup::decideWatchedFileTrigger(true, previous, current);
    if (unchanged.changed || unchanged.reason != "unchanged") {
        return fail("Identical watched-file state should not trigger.");
    }

    current = previous;
    current.size = 5;
    const auto sizeChanged = erdup::decideWatchedFileTrigger(true, previous, current);
    if (!sizeChanged.changed || !contains(sizeChanged.reason, "size changed")) {
        return fail("Size changes must trigger even when mtime is unchanged.");
    }

    current = previous;
    current.contentHash = 0x200;
    const auto hashChanged = erdup::decideWatchedFileTrigger(true, previous, current);
    if (!hashChanged.changed || !contains(hashChanged.reason, "content hash changed")) {
        return fail("Content hash changes must trigger even when mtime and size are unchanged.");
    }

    current = previous;
    current.readable = false;
    current.error = "sharing violation";
    const auto blocked = erdup::decideWatchedFileTrigger(true, previous, current);
    if (blocked.changed || !contains(blocked.reason, "sharing violation")) {
        return fail("Unreadable watched-file state should block without updating the trigger.");
    }

    return EXIT_SUCCESS;
}

int assertReadsAndFormatsFileState() {
    const auto path = std::filesystem::temp_directory_path() /
        ("erdup_watch_test_" + std::to_string(std::rand()) + ".txt");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "12345";
    }

    const auto state = erdup::readWatchedFileState(path);
    std::filesystem::remove(path);

    if (!state.readable || state.size != 5 || state.contentHash == 0) {
        return fail("Watched-file state should read size and content hash from a local file.");
    }
    const auto formatted = erdup::formatWatchedFileState(state);
    if (!contains(formatted, "readable=1") ||
        !contains(formatted, "size=5") ||
        !contains(formatted, "hash=0x")) {
        return fail("Formatted watched-file state should include readable, size, and hash.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertTriggerDecisionUsesSizeAndHash() != EXIT_SUCCESS ||
        assertReadsAndFormatsFileState() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Watched file test passed.\n";
    return EXIT_SUCCESS;
}
