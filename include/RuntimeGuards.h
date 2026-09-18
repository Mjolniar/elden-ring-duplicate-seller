#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct RuntimeGuardReport {
    bool allowed = false;
    std::filesystem::path processPath;
    std::vector<std::string> reasons;
};

RuntimeGuardReport evaluateRuntimeGuards();
std::string describeRuntimeGuardReport(const RuntimeGuardReport& report);

} // namespace erdup
