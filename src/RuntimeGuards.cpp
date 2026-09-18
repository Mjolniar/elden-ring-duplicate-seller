#include "RuntimeGuards.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <sstream>

namespace erdup {

namespace {

std::filesystem::path currentProcessPath() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(buffer.data());
}

bool moduleLoaded(const wchar_t* moduleName) {
    return GetModuleHandleW(moduleName) != nullptr;
}

std::string narrowAscii(std::wstring value) {
    std::string result;
    result.reserve(value.size());
    for (const auto ch : value) {
        result.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return result;
}

} // namespace

RuntimeGuardReport evaluateRuntimeGuards() {
    RuntimeGuardReport report;
    report.processPath = currentProcessPath();

    const auto fileName = report.processPath.filename().wstring();
    std::wstring lowered = fileName;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });

    if (lowered != L"eldenring.exe") {
        report.reasons.push_back("Process is not eldenring.exe: " + narrowAscii(fileName));
    }

    if (moduleLoaded(L"EasyAntiCheat_x64.dll") || moduleLoaded(L"easyanticheat_x64.dll")) {
        report.reasons.push_back("Known Easy Anti-Cheat module is loaded.");
    }

    report.allowed = report.reasons.empty();
    return report;
}

std::string describeRuntimeGuardReport(const RuntimeGuardReport& report) {
    std::ostringstream out;
    out << "process=\"" << report.processPath.string() << "\"";
    if (report.allowed) {
        out << ", allowed=true";
        return out.str();
    }

    out << ", allowed=false";
    for (const auto& reason : report.reasons) {
        out << ", reason=\"" << reason << "\"";
    }
    return out.str();
}

} // namespace erdup
