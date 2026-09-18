#include "NativeSeller.h"
#include "MemoryReader.h"
#include "Logger.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <mutex>
#include <thread>

namespace erdup {
namespace {
std::mutex statusMutex;
std::string statusText = "Autosell ready";
bool busy = false;
ULONGLONG statusUntil = 0;
std::string readyText;
constexpr int indicatorWidth = 280;
constexpr int indicatorHeight = 24;
// CSFeMan's documented frontend mode: 1 = pause menu, 2 = popup/submenu,
// 3 = gameplay, 0 = hidden HUD. Read only; never simulate menu key presses.
// See docs/MENU_INDICATOR.md for the pinned anchors and live observations.
bool menuIsVisible(const ModuleInfo& module) {
    std::uintptr_t frontend = 0, menu = 0, frontendMenu = 0;
    std::uintptr_t data = 0, player = 0, inventory = 0;
    unsigned char mode = 0;
    return readCommittedMemory(module.baseAddress + 0x3d6f8f0, &frontend, sizeof(frontend)) && frontend &&
        readCommittedMemory(module.baseAddress + 0x3d6f820, &menu, sizeof(menu)) && menu &&
        readCommittedMemory(frontend + 0x18, &frontendMenu, sizeof(frontendMenu)) && frontendMenu == menu &&
        readCommittedMemory(module.baseAddress + 0x3d61f98, &data, sizeof(data)) && data &&
        readCommittedMemory(data + 8, &player, sizeof(player)) && player &&
        readCommittedMemory(player + 0x418, &inventory, sizeof(inventory)) && inventory &&
        readCommittedMemory(frontend + 0x78, &mode, sizeof(mode)) && (mode == 1 || mode == 2);
}

bool menuLayoutMatches(const ModuleInfo& module) {
    constexpr std::array<unsigned char, 14> menuAnchor{
        0x48,0x8b,0x05,0x73,0x69,0x61,0x03,0x33,0xdb,0x48,0x89,0x74,0x24,0x40};
    constexpr std::array<unsigned char, 10> frontendAnchor{
        0x48,0x8b,0x0d,0xd6,0x86,0x60,0x03,0x48,0x85,0xc9};
    std::array<unsigned char, menuAnchor.size()> menuBytes{};
    std::array<unsigned char, frontendAnchor.size()> frontendBytes{};
    return module.fileVersion == "2.7.1.0" && module.imageSize == 98621952 &&
        readCommittedMemory(module.baseAddress + 0x758ea6, menuBytes.data(), menuBytes.size()) && menuBytes == menuAnchor &&
        readCommittedMemory(module.baseAddress + 0x767213, frontendBytes.data(), frontendBytes.size()) && frontendBytes == frontendAnchor;
}
LRESULT CALLBACK indicatorProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT rect{}; GetClientRect(window, &rect);
        std::lock_guard lock(statusMutex);
        auto brush = CreateSolidBrush(RGB(18, 20, 20));
        FillRect(dc, &rect, brush); DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, busy ? RGB(226, 208, 156) : RGB(194, 190, 176));
        auto font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        auto oldFont = SelectObject(dc, font);
        rect.left += 8; rect.right -= 8;
        DrawTextA(dc, statusText.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, oldFont); DeleteObject(font);
        EndPaint(window, &paint); return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
}
void setSaleIndicator(const std::string& text, bool running) {
    std::lock_guard lock(statusMutex);
    statusText = text; busy = running; statusUntil = GetTickCount64() + 1500;
}
void startSaleIndicator(int virtualKey) {
    char keyName[64]{};
    GetKeyNameTextA(static_cast<LONG>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC) << 16), keyName, 64);
    readyText = std::string(keyName) + " | Sell duplicates";
    setSaleIndicator(readyText);
    std::thread([] {
        const auto module = getCurrentProcessModuleInfo();
        if (!menuLayoutMatches(module)) {
            logError("Menu indicator hidden: unsupported frontend layout");
            return;
        }
        logInfo("Menu indicator: 280x24, menu-only, result timeout 1500ms");
        WNDCLASSW wc{}; wc.lpfnWndProc = indicatorProc;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"ERDuplicateSellerIndicator";
        RegisterClassW(&wc);
        HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST |
            WS_EX_LAYERED | WS_EX_TRANSPARENT, wc.lpszClassName, L"Autosell status", WS_POPUP,
            30, 30, indicatorWidth, indicatorHeight, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window) return;
        SetLayeredWindowAttributes(window, 0, 205, LWA_ALPHA);
        for (;;) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            auto game = GetForegroundWindow(); DWORD process = 0;
            GetWindowThreadProcessId(game, &process);
            if (process == GetCurrentProcessId() && game != window && menuIsVisible(module)) {
                RECT bounds{}; GetClientRect(game, &bounds);
                POINT origin{std::max(0L, bounds.right - indicatorWidth - 16), 12}; ClientToScreen(game, &origin);
                SetWindowPos(window, HWND_TOPMOST, origin.x, origin.y, indicatorWidth, indicatorHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            } else ShowWindow(window, SW_HIDE);
            {
                std::lock_guard lock(statusMutex);
                if (!busy && GetTickCount64() > statusUntil) statusText = readyText;
            }
            InvalidateRect(window, nullptr, FALSE); UpdateWindow(window);
            Sleep(100);
        }
    }).detach();
}
}
