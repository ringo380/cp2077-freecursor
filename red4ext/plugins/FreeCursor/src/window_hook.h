#pragma once

namespace freecursor::WindowHook
{
// Starts a background thread that polls for the CP2077 window and subclasses it.
// Safe to call once, at plugin load.
void Install();

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
// raw mouse packets carrying RI_MOUSE_WHEEL/HWHEEL. Intended to be armed
// whenever the cursor is detached (gameplay AND menu/phone mode), so the game
// never scrolls in response to Ctrl+Alt+wheel Magnifier zoom. Independent of
// SetSwallow - either flag may be on without the other; a message is swallowed
// if either flag says to swallow it. Returns false if the hook is not
// installed yet, in which case the flag is NOT applied.
bool SetWheelBlock(bool aEnabled);
} // namespace freecursor::WindowHook
