#include "core/OverlayInteraction.h"

#ifdef ISLE_COMPANION_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace isle {
namespace {

constexpr LONG_PTR kWsExTransparent = 0x00000020;
constexpr LONG_PTR kWsExLayered = 0x00080000;
constexpr LONG_PTR kWsExNoActivate = 0x08000000;
constexpr LONG_PTR kWsExToolWindow = 0x00000080;
constexpr LONG_PTR kWsExAppWindow = 0x00040000;
constexpr UINT kWdaExcludeFromCapture = 0x00000011;

} // namespace

bool applyWindowsOverlayInputStyle(WId windowId, bool /*interactable*/)
{
    if (windowId == 0) {
        return false;
    }
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // Keep layered for opacity, force a normal taskbar app window, and never
    // use click-through / no-activate (those hide the taskbar button).
    style |= kWsExLayered | kWsExAppWindow;
    style &= ~(kWsExTransparent | kWsExNoActivate | kWsExToolWindow);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
}

bool excludeWindowFromCapture(WId windowId)
{
    if (windowId == 0) {
        return false;
    }
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    using SetWindowDisplayAffinityFn = BOOL(WINAPI *)(HWND, DWORD);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) {
        return false;
    }
    auto fn = reinterpret_cast<SetWindowDisplayAffinityFn>(
        GetProcAddress(user32, "SetWindowDisplayAffinity"));
    if (fn == nullptr) {
        return false;
    }
    return fn(hwnd, kWdaExcludeFromCapture) == TRUE;
}

} // namespace isle
#else
namespace isle {
} // namespace isle
#endif
