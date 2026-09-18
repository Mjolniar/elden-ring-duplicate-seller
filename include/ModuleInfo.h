#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace erdup {

struct ModuleInfo {
    bool loaded = false;
    std::filesystem::path path;
    std::uintptr_t baseAddress = 0;
    std::size_t imageSize = 0;
    std::string fileVersion;
    std::vector<std::string> warnings;
};

ModuleInfo getCurrentProcessModuleInfo();
std::string describeModuleInfo(const ModuleInfo& module);

} // namespace erdup
