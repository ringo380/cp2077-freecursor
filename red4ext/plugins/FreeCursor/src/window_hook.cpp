#include "window_hook.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
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
freecursor::WindowHook::LogFn g_log = nullptr;

void Logf(const char* aFmt, ...)
{
    if (!g_log)
        return;
    char    line[256];
    va_list args;
    va_start(args, aFmt);
    std::vsnprintf(line, sizeof(line), aFmt, args);
    va_end(args);
    g_log(line);
}

// Diagnostic: window activation and focus traffic, with the flags as they
// stood when the message arrived. These are rare messages (alt-tab, click-in,
// overlay open), so this cannot spam the log; it is what makes an alt-tab
// timeline readable after the fact.
void LogFocusMessage(UINT aMsg, WPARAM awParam, bool aSwallow, bool aWheelBlock)
{
    const char* name = nullptr;
    switch (aMsg)
    {
    case WM_ACTIVATE:    name = "WM_ACTIVATE";    break;
    case WM_ACTIVATEAPP: name = "WM_ACTIVATEAPP"; break;
    case WM_SETFOCUS:    name = "WM_SETFOCUS";    break;
    case WM_KILLFOCUS:   name = "WM_KILLFOCUS";   break;
    default:             return;
    }
    Logf("%s wParam=%llu swallow=%d wheelBlock=%d", name, static_cast<unsigned long long>(awParam),
         aSwallow ? 1 : 0, aWheelBlock ? 1 : 0);
}

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
constexpr USHORT kRawWheelFlags = RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL;
// The five button DOWN flags occupy the even bits; each UP flag is the next
// bit up, so `up >> 1` lines up with the matching DOWN bit.
constexpr USHORT kRawButtonDownFlags = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN |
                                       RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_BUTTON_4_DOWN |
                                       RI_MOUSE_BUTTON_5_DOWN;
constexpr USHORT kRawButtonUpFlags = kRawButtonDownFlags << 1;

// Buttons whose raw DOWN packet we swallowed and whose UP the game/CET
// therefore must not see either (see PassesSwallowedButtonUp). Only touched
// from the window thread; cleared whenever swallow is disarmed.
std::atomic<USHORT> g_swallowedDowns{0};

// Windows Magnifier zooms on Ctrl+Alt+wheel. A wheel packet carries no motion,
// so it can never move the camera; the only reason to eat it is to stop the
// game scrolling/zooming underneath a Magnifier zoom gesture. Everything else
// (weapon cycling, scrolling a texting thread) is wanted.
bool CtrlAltHeld()
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 && (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

// Ctrl and Alt are the one keyboard exception. Magnifier's zoom gesture is
// Ctrl+Alt+wheel, and the game binds Ctrl (crouch / dodge) and Alt (switch
// item) on their own, so the first half of the gesture fires a game action
// before the wheel ever arrives. While the gameplay swallow is armed, the two
// modifiers are eaten - every other key still passes, so Esc, the toggle key
// and dialogue keys keep working. Magnifier is unaffected: it reads the
// modifiers through its own low-level hook, ahead of this WndProc, and
// CtrlAltHeld() above reads the async key state for the same reason.
//
// Press/release balance follows the mouse-button rule: a release is eaten
// only when its press was eaten too, so a Ctrl held across the toggle press
// (game saw the DOWN) still gets its UP, and never leaves a stuck crouch.
// Left and right keys are tracked separately, and the raw and legacy streams
// each keep their own mask, since both carry the same physical keystroke.
constexpr int kModSlotNone = -1;

int ModifierSlot(USHORT aVKey, bool aExtended)
{
    switch (aVKey)
    {
    case VK_CONTROL:  return aExtended ? 1 : 0;
    case VK_LCONTROL: return 0;
    case VK_RCONTROL: return 1;
    case VK_MENU:     return aExtended ? 3 : 2;
    case VK_LMENU:    return 2;
    case VK_RMENU:    return 3;
    default:          return kModSlotNone;
    }
}

std::atomic<unsigned> g_swallowedModsRaw{0};
std::atomic<unsigned> g_swallowedModsLegacy{0};

// Returns true if this modifier event must be eaten; updates the mask.
bool SwallowModifierEvent(std::atomic<unsigned>& aMask, int aSlot, bool aIsUp)
{
    const unsigned bit  = 1u << aSlot;
    unsigned       seen = aMask.load(std::memory_order_relaxed);
    if (aIsUp)
    {
        if ((seen & bit) == 0)
            return false; // press went to the game, so must the release
        aMask.store(seen & ~bit, std::memory_order_relaxed);
        return true;
    }
    aMask.store(seen | bit, std::memory_order_relaxed);
    return true;
}

bool IsLegacyKeyMessage(UINT aMsg)
{
    return aMsg == WM_KEYDOWN || aMsg == WM_KEYUP || aMsg == WM_SYSKEYDOWN || aMsg == WM_SYSKEYUP;
}

bool ShouldSwallowRawInput(DWORD aType, USHORT aButtonFlags, bool aSwallow, bool aWheelBlock, bool aCtrlAlt)
{
    if (aType != RIM_TYPEMOUSE)
        return false; // keyboard / HID: decided elsewhere (modifiers) or never eaten

    // The game's wheel comes from the raw stream, not just WM_MOUSEWHEEL, so
    // decide it here. Wheel packets are decided BEFORE the swallow check: they
    // cannot move the camera, and while detached the player still wants plain
    // scrolling to reach the game.
    if (aButtonFlags & kRawWheelFlags)
        return aWheelBlock && aCtrlAlt;

    return aSwallow; // gameplay: camera must hold still
}

// CET reads its hotkeys from this same raw stream, fires them on key UP, and
// matches the whole set of keys it believes are held - mouse buttons included.
// If CET saw a button go down (before swallow was armed, say RMB held to aim
// while pressing the toggle) and we then ate the UP, CET thinks the button is
// still held, every later press is a combo, and neither the toggle nor CET's
// own overlay key matches anything until CET happens to see that button
// released. Whether this bites depends on which of us hooked the window last,
// which is a launch-time race. So: a raw UP whose DOWN we did not swallow is
// always passed through. A stray release is harmless to the game; a missing
// one is a stuck hotkey. Returns true if the packet must pass for that reason.
bool PassesSwallowedButtonUp(USHORT aButtonFlags)
{
    const USHORT downs = static_cast<USHORT>(aButtonFlags & kRawButtonDownFlags);
    const USHORT ups   = static_cast<USHORT>(aButtonFlags & kRawButtonUpFlags);
    USHORT       seen  = g_swallowedDowns.load(std::memory_order_relaxed);

    const bool mustPass = (static_cast<USHORT>(ups >> 1) & ~seen) != 0;
    if (mustPass)
        return true;

    seen = static_cast<USHORT>((seen | downs) & ~static_cast<USHORT>(ups >> 1));
    g_swallowedDowns.store(seen, std::memory_order_relaxed);
    return false;
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

    if (raw.header.dwType == RIM_TYPEKEYBOARD)
    {
        if (!aSwallow)
            return false;
        const int slot = ModifierSlot(raw.data.keyboard.VKey, (raw.data.keyboard.Flags & RI_KEY_E0) != 0);
        if (slot == kModSlotNone)
            return false;
        return SwallowModifierEvent(g_swallowedModsRaw, slot, (raw.data.keyboard.Flags & RI_KEY_BREAK) != 0);
    }

    // HID packets are variable-length and may not fit a plain RAWINPUT; they
    // never reach the mouse branch anyway (the header is all we read for them).
    const USHORT buttonFlags = raw.header.dwType == RIM_TYPEMOUSE ? raw.data.mouse.usButtonFlags : 0;
    if (!ShouldSwallowRawInput(raw.header.dwType, buttonFlags, aSwallow, aWheelBlock, CtrlAltHeld()))
        return false;

    return !PassesSwallowedButtonUp(buttonFlags);
}

LRESULT APIENTRY HookedWndProc(HWND ahWnd, UINT auMsg, WPARAM awParam, LPARAM alParam)
{
    const bool swallow    = g_swallow.load(std::memory_order_relaxed);
    const bool wheelBlock = g_wheelBlock.load(std::memory_order_relaxed);

    LogFocusMessage(auMsg, awParam, swallow, wheelBlock);

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

    // Legacy Ctrl/Alt key messages, same rule and balance as the raw path.
    // Returning without DefWindowProc also stops an Alt release turning into
    // WM_SYSCOMMAND/SC_KEYMENU. No other key message is ever touched.
    if (swallow && IsLegacyKeyMessage(auMsg))
    {
        const int slot = ModifierSlot(static_cast<USHORT>(awParam), (alParam & (1 << 24)) != 0);
        if (slot != kModSlotNone &&
            SwallowModifierEvent(g_swallowedModsLegacy, slot, auMsg == WM_KEYUP || auMsg == WM_SYSKEYUP))
            return 0;
    }

    // Legacy mouse messages. Two independent flags: SetSwallow covers the full
    // mouse range, SetWheelBlock covers only the wheel. The wheel is decided
    // first, with the same rule as the raw path: eaten only while both flags
    // agree it is a Magnifier zoom gesture. Other keyboard messages never
    // match either check, so the toggle key always reaches the game.
    if (IsWheelMessage(auMsg))
        return (wheelBlock && CtrlAltHeld()) ? 1 : CallWindowProc(g_originalProc, ahWnd, auMsg, awParam, alParam);

    if (swallow && IsLegacyMouseMessage(auMsg))
        return 1; // consumed: the game never sees it

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

void freecursor::WindowHook::Install(LogFn aLog)
{
    g_log = aLog;
    g_shouldStop.store(false, std::memory_order_release);

    g_pollThread = std::thread(
        [aLog]
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

            // CET subclasses this same window from its own 50 ms poll, so who
            // sees input first is a per-launch race. Record which module the
            // procedure we displaced belongs to: cyber_engine_tweaks.asi means
            // we are AHEAD of CET (it only sees what we forward);
            // Cyberpunk2077.exe means CET had not hooked yet and will end up
            // ahead of us. This line is what lets a log settle the question.
            if (aLog)
            {
                char    line[512];
                HMODULE owner = nullptr;
                wchar_t path[MAX_PATH] = {};
                if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       reinterpret_cast<LPCWSTR>(g_originalProc), &owner) &&
                    GetModuleFileNameW(owner, path, MAX_PATH) != 0)
                {
                    const wchar_t* base = std::wcsrchr(path, L'\\');
                    base                = base ? base + 1 : path;
                    std::snprintf(line, sizeof(line), "Window hook installed; previous WndProc belongs to %ls", base);
                }
                else
                {
                    std::snprintf(line, sizeof(line), "Window hook installed; previous WndProc owner unknown");
                }
                aLog(line);
            }
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
    g_swallowedDowns.store(0, std::memory_order_release);
    g_swallowedModsRaw.store(0, std::memory_order_release);
    g_swallowedModsLegacy.store(0, std::memory_order_release);
}

bool freecursor::WindowHook::SetSwallow(bool aEnabled)
{
    if (!g_installed.load(std::memory_order_acquire))
        return false;

    // A fresh arm starts with no swallowed downs: any button held right now
    // went down in the clear, so its release must be forwarded. Disarming
    // clears for the same reason in reverse - once packets flow again, a
    // release the game never saw the press for is simply ignored by it.
    g_swallowedDowns.store(0, std::memory_order_release);
    g_swallowedModsRaw.store(0, std::memory_order_release);
    g_swallowedModsLegacy.store(0, std::memory_order_release);
    if (g_swallow.exchange(aEnabled, std::memory_order_acq_rel) != aEnabled)
        Logf("swallow -> %d", aEnabled ? 1 : 0);
    return true;
}

bool freecursor::WindowHook::SetWheelBlock(bool aEnabled)
{
    if (!g_installed.load(std::memory_order_acquire))
        return false;

    if (g_wheelBlock.exchange(aEnabled, std::memory_order_acq_rel) != aEnabled)
        Logf("wheelBlock -> %d", aEnabled ? 1 : 0);
    return true;
}
