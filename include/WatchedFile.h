#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace erdup {

struct WatchedFileState {
    bool readable = false;
    std::filesystem::file_time_type writeTime{};
    std::uintmax_t size = 0;
    std::uint64_t contentHash = 0;
    std::string error;
};

struct WatchedFileDecision {
    bool changed = false;
    std::string reason;
};

WatchedFileState readWatchedFileState(const std::filesystem::path& path);
WatchedFileDecision decideWatchedFileTrigger(
    bool havePrevious,
    const WatchedFileState& previous,
    const WatchedFileState& current
);
std::string formatWatchedFileState(const WatchedFileState& state);

} // namespace erdup
