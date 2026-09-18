#pragma once

#include <filesystem>
#include <string>

namespace erdup {

void initializeLogger(const std::filesystem::path& logPath);
void logInfo(const std::string& message);
void logError(const std::string& message);

} // namespace erdup
