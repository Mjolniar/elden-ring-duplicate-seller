#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct InventorySnapshotFile {
    std::filesystem::path path;
    std::filesystem::file_time_type writeTime{};
};

struct InventorySnapshotCleanupReport {
    std::size_t discovered = 0;
    std::size_t retained = 0;
    std::size_t deleted = 0;
    std::vector<std::filesystem::path> deletedPaths;
    std::vector<std::string> warnings;
};

bool isInventorySnapshotFilename(const std::filesystem::path& path);
std::vector<std::filesystem::path> planInventorySnapshotCleanup(
    std::vector<InventorySnapshotFile> files,
    std::size_t retainNewest
);
InventorySnapshotCleanupReport cleanupInventorySnapshots(
    const std::filesystem::path& directory,
    std::size_t retainNewest
);
std::string formatInventorySnapshotCleanupReport(const InventorySnapshotCleanupReport& report);

} // namespace erdup
