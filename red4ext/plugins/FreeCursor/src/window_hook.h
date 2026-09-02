#pragma once

namespace freecursor::WindowHook
{
using LogFn = void (*)(const char* aMessage);

// Starts a background thread that polls for the CP2077 window and subclasses it.
// Safe to call once, at plugin load. aLog (may be null) receives one line once
// the hook is in place, naming the module that owned the WndProc we displaced
// - which is how a log tells whether we hooked ahead of or behind CET.
void Install(LogFn aLog);

// Restores the original WNDPROC. Safe to call even if Install() never completed.
// Clears both flags so a reload never leaves input eaten.
void Uninstall();

// Arms/disarms full mouse swallowing: all of WM_MOUSEFIRST..WM_MOUSELAST plus
// WM_INPUT packets whose raw type is MOUSE. Raw keyboard/HID packets and every
// WM_KEY* message always pass through, so every key keeps working while
// detached (dialog choices, phone hold-key, the toggle itself). Intended for
// gameplay mode, where the camera must hold still. Returns false if the hook
// is not installed yet, in which case the flag is NOT applied.
bool SetSwallow(bool aEnabled);

// Arms/disarms blocking of the wheel alone: WM_MOUSEWHEEL/WM_MOUSEHWHEEL and
// raw mouse packets carrying RI_MOUSE_WHEEL/HWHEEL - but only while Ctrl+Alt
// are held, i.e. only the Windows Magnifier zoom gesture. A plain wheel always
// reaches the game, in both modes, because a wheel packet carries no motion
// and cannot move the camera. Intended to be armed whenever the cursor is
// detached (gameplay AND menu/phone mode). Independent of SetSwallow.
// Returns false if the hook is not installed yet, in which case the flag is
// NOT applied.
bool SetWheelBlock(bool aEnabled);
} // namespace freecursor::WindowHook
