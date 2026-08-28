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

bool IsMouseMessage(UINT aMsg)
{
    // WM_INPUT carries raw mouse motion, which is what actually drives the
    // camera. Keyboard is deliberately never swallowed, so the toggle key
    // always gets back out.
    return (aMsg >= WM_MOUSEFIRST && aMsg <= WM_MOUSELAST) || aMsg == WM_INPUT;
}

bool IsWheelMessage(UINT aMsg)
{
    return aMsg == WM_MOUSEWHEEL || aMsg == WM_MOUSEHWHEEL;
}

LRESULT APIENTRY HookedWndProc(HWND ahWnd, UINT auMsg, WPARAM awParam, LPARAM alParam)
{
    // Two independent flags: SetSwallow covers the full mouse range (which
    // already includes the wheel), SetWheelBlock covers only the wheel. A
    // message is swallowed if either flag says so. Keyboard messages never
    // match either check, so the toggle key always reaches the game.
    if (g_swallow.load(std::memory_order_relaxed) && IsMouseMessage(auMsg))
    {
        // MSDN requires an application that handles WM_INPUT to pass it to
        // DefWindowProc so the raw-input buffer is cleaned up. The game still
        // never sees it -- which is the whole point of swallowing it -- but we
        // must not simply drop it on the floor.
        if (auMsg == WM_INPUT)
            return DefWindowProc(ahWnd, auMsg, awParam, alParam);

        return 1; // consumed: the game never sees it
    }

    if (g_wheelBlock.load(std::memory_order_relaxed) && IsWheelMessage(auMsg))
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
