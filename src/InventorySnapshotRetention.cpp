#include "InventorySnapshotRetention.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>

namespace erdup {

namespace {

constexpr const char* kSnapshotPrefix = "inventory_snapshot_";
constexpr const char* kSnapshotSuffix = ".txt";

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isDigitRange(const std::string& value, std::size_t offset, std::size_t count) {
    if (offset + count > value.size()) {
        return false;
    }
    for (std::size_t i = offset; i < offset + count; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

bool isInventorySnapshotFilename(const std::filesystem::path& path) {
    const auto filename = path.filename().string();
    constexpr std::size_t dateDigits = 8;
    constexpr std::size_t timeDigits = 6;
    constexpr std::size_t millisecondDigits = 3;
    const std::size_t prefixLength = std::string(kSnapshotPrefix).size();
    const std::size_t suffixLength = std::string(kSnapshotSuffix).size();
    const std::size_t expectedLength =
        prefixLength + dateDigits + 1 + timeDigits + 1 + millisecondDigits + suffixLength;

    if (filename.size() != expectedLength ||
        filename.rfind(kSnapshotPrefix, 0) != 0 ||
        !endsWith(filename, kSnapshotSuffix)) {
        return false;
    }

    const auto dateOffset = prefixLength;
    const auto firstSeparator = dateOffset + dateDigits;
    const auto timeOffset = firstSeparator + 1;
    const auto secondSeparator = timeOffset + timeDigits;
    const auto millisecondOffset = secondSeparator + 1;

    return filename[firstSeparator] == '_' &&
        filename[secondSeparator] == '_' &&
        isDigitRange(filename, dateOffset, dateDigits) &&
        isDigitRange(filename, timeOffset, timeDigits) &&
        isDigitRange(filename, millisecondOffset, millisecondDigits);
}

std::vector<std::filesystem::path> planInventorySnapshotCleanup(
    std::vector<InventorySnapshotFile> files,
    std::size_t retainNewest
) {
    if (retainNewest == 0 || files.size() <= retainNewest) {
        return {};
    }

    std::sort(files.begin(), files.end(), [](const InventorySnapshotFile& lhs, const InventorySnapshotFile& rhs) {
        if (lhs.writeTime != rhs.writeTime) {
            return lhs.writeTime > rhs.writeTime;
        }
        return lhs.path.filename().string() > rhs.path.filename().string();
    });

    std::vector<std::filesystem::path> toDelete;
    toDelete.reserve(files.size() - retainNewest);
    for (std::size_t i = retainNewest; i < files.size(); ++i) {
        toDelete.push_back(files[i].path);
    }
    return toDelete;
}

InventorySnapshotCleanupReport cleanupInventorySnapshots(
    const std::filesystem::path& directory,
    std::size_t retainNewest
) {
    InventorySnapshotCleanupReport report;
    if (retainNewest == 0) {
        return report;
    }

    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        if (ec) {
            report.warnings.push_back("could not check snapshot directory: " + ec.message());
        }
        return report;
    }

    std::vector<InventorySnapshotFile> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            report.warnings.push_back("could not enumerate snapshot directory: " + ec.message());
            break;
        }
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        const auto path = entry.path();
        if (!isInventorySnapshotFilename(path)) {
            continue;
        }
        const auto writeTime = entry.last_write_time(ec);
        if (ec) {
            report.warnings.push_back("could not read snapshot timestamp: " + path.string() + ": " + ec.message());
            ec.clear();
            continue;
        }
        files.push_back({path, writeTime});
    }

    report.discovered = files.size();
    report.retained = std::min(files.size(), retainNewest);

    for (const auto& path : planInventorySnapshotCleanup(std::move(files), retainNewest)) {
        std::error_code removeError;
        if (std::filesystem::remove(path, removeError)) {
            ++report.deleted;
            report.deletedPaths.push_back(path);
        } else if (removeError) {
            report.warnings.push_back("could not delete old snapshot: " + path.string() + ": " + removeError.message());
        }
    }

    return report;
}

std::string formatInventorySnapshotCleanupReport(const InventorySnapshotCleanupReport& report) {
    std::ostringstream out;
    out << "inventory_snapshot_cleanup:"
        << " discovered=" << report.discovered
        << ", retained=" << report.retained
        << ", deleted=" << report.deleted
        << ", warnings=" << report.warnings.size();
    for (const auto& warning : report.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    return out.str();
}

} // namespace erdup
