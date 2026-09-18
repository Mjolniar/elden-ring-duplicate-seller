#include "WatchedFile.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

namespace erdup {

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t hashFile(const std::filesystem::path& path, bool& ok, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        ok = false;
        error = "could not open watched file";
        return 0;
    }

    std::uint64_t hash = kFnvOffset;
    char buffer[4096]{};
    while (file) {
        file.read(buffer, sizeof(buffer));
        const auto read = file.gcount();
        for (std::streamsize i = 0; i < read; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= kFnvPrime;
        }
    }

    if (!file.eof()) {
        ok = false;
        error = "could not finish reading watched file";
        return 0;
    }

    ok = true;
    return hash;
}

} // namespace

WatchedFileState readWatchedFileState(const std::filesystem::path& path) {
    WatchedFileState state;

    std::error_code ec;
    state.writeTime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        state.error = "last_write_time failed: " + ec.message();
        return state;
    }

    state.size = std::filesystem::file_size(path, ec);
    if (ec) {
        state.error = "file_size failed: " + ec.message();
        return state;
    }

    bool hashOk = false;
    state.contentHash = hashFile(path, hashOk, state.error);
    state.readable = hashOk;
    return state;
}

WatchedFileDecision decideWatchedFileTrigger(
    bool havePrevious,
    const WatchedFileState& previous,
    const WatchedFileState& current
) {
    WatchedFileDecision decision;
    if (!current.readable) {
        decision.reason = current.error.empty() ? "watched file unreadable" : current.error;
        return decision;
    }
    if (!havePrevious) {
        decision.changed = true;
        decision.reason = "initial readable state";
        return decision;
    }
    if (current.writeTime != previous.writeTime) {
        decision.changed = true;
        decision.reason = "mtime changed";
        return decision;
    }
    if (current.size != previous.size) {
        decision.changed = true;
        decision.reason = "size changed with unchanged mtime";
        return decision;
    }
    if (current.contentHash != previous.contentHash) {
        decision.changed = true;
        decision.reason = "content hash changed with unchanged mtime";
        return decision;
    }

    decision.reason = "unchanged";
    return decision;
}

std::string formatWatchedFileState(const WatchedFileState& state) {
    std::ostringstream out;
    out << "readable=" << (state.readable ? 1 : 0)
        << " size=" << state.size
        << " hash=0x" << std::hex << state.contentHash << std::dec;
    if (!state.error.empty()) {
        out << " error=\"" << state.error << "\"";
    }
    return out.str();
}

} // namespace erdup
