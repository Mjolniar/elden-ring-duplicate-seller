#include "InventorySnapshotRetention.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

int assertFilenameFilter() {
    if (!erdup::isInventorySnapshotFilename("inventory_snapshot_20260714_120000_000.txt") ||
        erdup::isInventorySnapshotFilename("inventory_snapshot_20260714_120000.txt") ||
        erdup::isInventorySnapshotFilename("inventory_snapshot_old.txt") ||
        erdup::isInventorySnapshotFilename("inventory_snapshot_manual_baseline.txt") ||
        erdup::isInventorySnapshotFilename("inventory_snapshot_notes.md") ||
        erdup::isInventorySnapshotFilename("other_inventory_snapshot_20260714.txt")) {
        return fail("Snapshot filename filter should match only generated timestamped snapshot text files.");
    }
    return EXIT_SUCCESS;
}

int assertCleanupPlanKeepsNewest() {
    using clock = std::filesystem::file_time_type::clock;
    const auto now = clock::now();
    std::vector<erdup::InventorySnapshotFile> files = {
        {"inventory_snapshot_old.txt", now - std::chrono::seconds(20)},
        {"inventory_snapshot_new.txt", now},
        {"inventory_snapshot_middle.txt", now - std::chrono::seconds(10)},
    };

    const auto toDelete = erdup::planInventorySnapshotCleanup(files, 2);
    if (toDelete.size() != 1 || toDelete[0].filename().string() != "inventory_snapshot_old.txt") {
        return fail("Cleanup plan should delete only files older than the retained newest set.");
    }
    if (!erdup::planInventorySnapshotCleanup(files, 0).empty()) {
        return fail("Retention count zero should disable cleanup.");
    }

    return EXIT_SUCCESS;
}

int assertCleanupDeletesOnlyOldSnapshots() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("erdup_snapshot_retention_" + std::to_string(std::rand()));
    std::filesystem::create_directories(directory);

    const auto oldSnapshot = directory / "inventory_snapshot_20260714_120000_000.txt";
    const auto midSnapshot = directory / "inventory_snapshot_20260714_120005_000.txt";
    const auto newSnapshot = directory / "inventory_snapshot_20260714_120010_000.txt";
    const auto manualBaseline = directory / "inventory_snapshot_manual_baseline.txt";
    const auto unrelated = directory / "notes.txt";

    writeFile(oldSnapshot, "old");
    writeFile(midSnapshot, "middle");
    writeFile(newSnapshot, "new");
    writeFile(manualBaseline, "manual");
    writeFile(unrelated, "keep");

    using clock = std::filesystem::file_time_type::clock;
    const auto now = clock::now();
    std::filesystem::last_write_time(oldSnapshot, now - std::chrono::seconds(20));
    std::filesystem::last_write_time(midSnapshot, now - std::chrono::seconds(10));
    std::filesystem::last_write_time(newSnapshot, now);

    const auto report = erdup::cleanupInventorySnapshots(directory, 2);
    const auto formatted = erdup::formatInventorySnapshotCleanupReport(report);

    const bool passed = report.discovered == 3 &&
        report.retained == 2 &&
        report.deleted == 1 &&
        !std::filesystem::exists(oldSnapshot) &&
        std::filesystem::exists(midSnapshot) &&
        std::filesystem::exists(newSnapshot) &&
        std::filesystem::exists(manualBaseline) &&
        std::filesystem::exists(unrelated) &&
        formatted.find("deleted=1") != std::string::npos;

    std::filesystem::remove_all(directory);

    if (!passed) {
        return fail("Snapshot cleanup should delete only the oldest matching snapshot files.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertFilenameFilter() != EXIT_SUCCESS ||
        assertCleanupPlanKeepsNewest() != EXIT_SUCCESS ||
        assertCleanupDeletesOnlyOldSnapshots() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Inventory snapshot retention test passed.\n";
    return EXIT_SUCCESS;
}
