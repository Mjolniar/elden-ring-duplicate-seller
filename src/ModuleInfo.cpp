#include "ModuleInfo.h"

#include <Windows.h>

#include <array>
#include <sstream>

namespace erdup {

namespace {

std::filesystem::path currentModulePath() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(buffer.data());
}

std::size_t imageSizeFromBase(HMODULE module) {
    const auto base = reinterpret_cast<const std::uint8_t*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    return nt->OptionalHeader.SizeOfImage;
}

std::string fileVersionString(const std::filesystem::path& path) {
    const auto widePath = path.wstring();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(widePath.c_str(), &handle);
    if (size == 0) {
        return {};
    }

    std::vector<std::byte> data(size);
    if (!GetFileVersionInfoW(widePath.c_str(), 0, size, data.data())) {
        return {};
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || info == nullptr) {
        return {};
    }

    std::ostringstream out;
    out << HIWORD(info->dwFileVersionMS) << "."
        << LOWORD(info->dwFileVersionMS) << "."
        << HIWORD(info->dwFileVersionLS) << "."
        << LOWORD(info->dwFileVersionLS);
    return out.str();
}

} // namespace

ModuleInfo getCurrentProcessModuleInfo() {
    ModuleInfo module;
    const HMODULE handle = GetModuleHandleW(nullptr);
    if (handle == nullptr) {
        module.warnings.push_back("GetModuleHandleW(nullptr) failed.");
        return module;
    }

    module.loaded = true;
    module.path = currentModulePath();
    module.baseAddress = reinterpret_cast<std::uintptr_t>(handle);
    module.imageSize = imageSizeFromBase(handle);
    module.fileVersion = fileVersionString(module.path);

    if (module.path.empty()) {
        module.warnings.push_back("Could not resolve process module path.");
    }
    if (module.imageSize == 0) {
        module.warnings.push_back("Could not resolve process image size from PE headers.");
    }
    if (module.fileVersion.empty()) {
        module.warnings.push_back("Could not resolve process file version.");
    }

    return module;
}

std::string describeModuleInfo(const ModuleInfo& module) {
    std::ostringstream out;
    out << "loaded=" << module.loaded
        << ", path=\"" << module.path.string() << "\""
        << ", base=0x" << std::hex << module.baseAddress << std::dec
        << ", image_size=" << module.imageSize
        << ", file_version=\"" << module.fileVersion << "\"";

    for (const auto& warning : module.warnings) {
        out << ", warning=\"" << warning << "\"";
    }
    return out.str();
}

} // namespace erdup
