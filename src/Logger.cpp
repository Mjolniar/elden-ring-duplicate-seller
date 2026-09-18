#include "Logger.h"

#include <Windows.h>

#include <fstream>
#include <mutex>

namespace erdup {

namespace {

std::mutex g_logMutex;
std::filesystem::path g_logPath;

void writeLogLine(const std::string& prefix, const std::string& message) {
    const auto line = prefix + message + "\n";
    OutputDebugStringA(line.c_str());

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logPath.empty()) {
        std::ofstream file(g_logPath, std::ios::app);
        if (file) {
            file << line;
        }
    }
}

} // namespace

void initializeLogger(const std::filesystem::path& logPath) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logPath = logPath;
    std::ofstream file(g_logPath, std::ios::app);
    if (file) {
        file << "=== ERDuplicateSeller log start ===\n";
    }
}

void logInfo(const std::string& message) {
    writeLogLine("[ERDuplicateSeller] ", message);
}

void logError(const std::string& message) {
    writeLogLine("[ERDuplicateSeller:ERROR] ", message);
}

} // namespace erdup
