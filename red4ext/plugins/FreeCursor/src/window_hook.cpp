#include "window_hook.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cwchar>
#include <thread>

namespace
{
HWND    g_hWnd        = nullptr;
WNDPROC g_originalProc = nullptr;
std::atomic<bool> g_swallow{false};
std::atomic<bool> g_wheelBlock{false};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_shouldStop{false};
std::thread       g_pollThread;

bool IsLegacyMouseMessage(UINT aMsg)
{
    // Keyboard messages (WM_KEYDOWN etc.) never fall in this range, so the
    // legacy path never touches them. Raw input is handled separately below.
    return aMsg >= WM_MOUSEFIRST && aMsg <= WM_MOUSELAST;
}

bool IsWheelMessage(UINT aMsg)
{
    return aMsg == WM_MOUSEWHEEL || aMsg == WM_MOUSEHWHEEL;
}

// The game reads its input through Raw Input (Cyberpunk2077.exe imports
// RegisterRawInputDevices/GetRawInputData), and a WM_INPUT message carries raw
// KEYBOARD events as well as raw mouse motion. Swallowing every WM_INPUT
// therefore eats keyboard too -- which is exactly why dialog choices, the
// phone hold-key and every other key went dead in gameplay while detached.
// Decide per packet instead: mouse packets follow the swallow/wheel flags,
// keyboard and HID packets always pass through. The pure decision is split
// out so the truth table can be read on its own.
bool ShouldSwallowRawInput(DWORD aType, USHORT aButtonFlags, bool aSwallow, bool aWheelBlock)
{
    if (aType != RIM_TYPEMOUSE)
        return false; // keyboard / HID: never ours to eat

    if (aSwallow)
        return true; // gameplay: camera must hold still

    // Menu mode: the game's wheel comes from the raw stream, not just
    // WM_MOUSEWHEEL, so block it here too or Ctrl+Alt+wheel Magnifier zoom
    // also scrolls the texting UI underneath.
    return aWheelBlock && (aButtonFlags & (RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL)) != 0;
}

bool ShouldSwallowWmInput(LPARAM alParam, bool aSwallow, bool aWheelBlock)
{
    RAWINPUT raw{};
    UINT     size = sizeof(raw);
    const UINT got = GetRawInputData(reinterpret_cast<HRAWINPUT>(alParam), RID_INPUT, &raw, &size,
                                     sizeof(RAWINPUTHEADER));
    if (got == static_cast<UINT>(-1) || got < sizeof(RAWINPUTHEADER))
    {
        // Unreadable packet. While swallowing, keep the "camera never moves
        // while detached" guarantee and eat it; otherwise let it through.
        return aSwallow;
    }

    // HID packets are variable-length and may not fit a plain RAWINPUT; they
    // never reach the mouse branch anyway (the header is all we read for them).
    const USHORT buttonFlags = raw.header.dwType == RIM_TYPEMOUSE ? raw.data.mouse.usButtonFlags : 0;
    return ShouldSwallowRawInput(raw.header.dwType, buttonFlags, aSwallow, aWheelBlock);
}

LRESULT APIENTRY HookedWndProc(HWND ahWnd, UINT auMsg, WPARAM awParam, LPARAM alParam)
{
    const bool swallow    = g_swallow.load(std::memory_order_relaxed);
    const bool wheelBlock = g_wheelBlock.load(std::memory_order_relaxed);

    if (auMsg == WM_INPUT)
    {
        if ((swallow || wheelBlock) && ShouldSwallowWmInput(alParam, swallow, wheelBlock))
        {
            // MSDN requires an application that handles WM_INPUT to pass it to
            // DefWindowProc so the raw-input buffer is cleaned up. The game
            // still never sees it -- which is the whole point of swallowing it
            // -- but we must not simply drop it on the floor.
            return DefWindowProc(ahWnd, auMsg, awParam, alParam);
        }
        return CallWindowProc(g_originalProc, ahWnd, auMsg, awParam, alParam);
    }

    // Legacy mouse messages. Two independent flags: SetSwallow covers the full
    // mouse range (which already includes the wheel), SetWheelBlock covers only
    // the wheel. A message is swallowed if either flag says so. Keyboard
    // messages never match either check, so the toggle key always reaches the
    // game.
    if (swallow && IsLegacyMouseMessage(auMsg))
        return 1; // consumed: the game never sees it

    if (wheelBlock && IsWheelMessage(auMsg))
        return 1; // consumed: blocks game scroll/zoom while cursor is detached

    return CallWindowProc(g_originalProc, ahWnd, auMsg, awParam, alParam);
}

// REDengine's window class, unchanged since The Witcher 2. Established from the
// install itself rather than from documentation: the UTF-16 literal appears
// exactly once in Cyberpunk2077.exe, and the same literal appears in CET's
// cyber_engine_tweaks.asi, which locates the game window at runtime.
constexpr wchar_t kGameWindowClass[] = L"W2ViewportClass";

BOOL CALLBACK EnumWindowsProc(HWND ahWnd, LPARAM alParam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(ahWnd, &pid);

    if (pid != GetCurrentProcessId() || !IsWindowVisible(ahWnd) || GetWindow(ahWnd, GW_OWNER))
        return TRUE; // keep looking

    // Without this, the poll latches onto the first visible unowned window in
    // the process -- a splash or transient window would set g_installed while
    // nothing is actually hooked, which is the one path that can make the mod
    // believe swallow is armed when it is not. If a future patch renames the
    // class we simply never install, and the Lua side fails closed.
    wchar_t className[64] = {};
    if (GetClassNameW(ahWnd, className, ARRAYSIZE(className)) == 0 ||
        std::wcscmp(className, kGameWindowClass) != 0)
        return TRUE; // keep looking

    *reinterpret_cast<HWND*>(alParam) = ahWnd;
    return FALSE; // found it, stop enumerating
}
} // namespace

void freecursor::WindowHook::Install()
{
    g_shouldStop.store(false, std::memory_order_release);

    g_pollThread = std::thread(
        []
        {
            HWND found = nullptr;
            while (!found && !g_shouldStop.load(std::memory_order_relaxed))
            {
                EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&found));
                if (found || g_shouldStop.load(std::memory_order_relaxed))
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (!found || g_shouldStop.load(std::memory_order_relaxed))
                return; // asked to stop before a window was found: leave everything uninstalled

            g_hWnd         = found;
            g_originalProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(found, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
            g_installed.store(true, std::memory_order_release);
        });
}

void freecursor::WindowHook::Uninstall()
{
    g_shouldStop.store(true, std::memory_order_release);
    if (g_pollThread.joinable())
        g_pollThread.join();

    if (g_hWnd && g_originalProc)
    {
        SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalProc));
        g_hWnd         = nullptr;
        g_originalProc = nullptr;
    }
    g_installed.store(false, std::memory_order_release);
    g_swallow.store(false, std::memory_order_release);
    g_wheelBlock.store(false, std::memory_order_release);
}

bool freecursor::WindowHook::SetSwallow(bool aEnabled)
{
    if (!g_installed.load(std::memory_order_acquire))
        return false;

    g_swallow.store(aEnabled, std::memory_order_release);
    return true;
}

bool freecursor::WindowHook::SetWheelBlock(bool aEnabled)
{
    if (!g_installed.load(std::memory_order_acquire))
        return false;

    g_wheelBlock.store(aEnabled, std::memory_order_release);
    return true;
}
