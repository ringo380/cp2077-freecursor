# FreeCursor

An accessibility mod for Cyberpunk 2077. It lets you detach your Windows mouse
pointer from the game on a hotkey, so you can aim **Windows Magnifier** at
on-screen text and read it, then reattach and keep playing.

## Why this exists

Cyberpunk 2077 locks the OS mouse pointer to the center of the screen during
normal play, and across most menus too. Windows Magnifier's "follow the mouse
pointer" mode tracks that OS pointer - so if the game never lets it move,
Magnifier stays stuck magnifying the middle of the screen and can never be
pointed at the dialogue, item description, or menu text you actually need to
read. This is a real barrier for a player with a vision impairment who relies
on Magnifier.

FreeCursor frees the OS pointer on demand so you can move it wherever you need
to read, without pausing the game, opening a menu, or leaving your session.

## What it does - and does not do

- Press a hotkey (you choose the key - see Install below) to **detach**: the
  OS pointer is released so Magnifier can follow it. Press the same hotkey
  again to **reattach**.
- It does not open any menu, does not pause the game, and does not touch
  Windows Magnifier itself. Magnifier is still a separate, user-driven tool -
  turn it on however you normally do (e.g. Win + `+`). FreeCursor only makes
  sure the OS pointer is free to move once Magnifier is following it.
- Optionally, it can detach **automatically** when the phone opens or when
  you enter a menu, and reattach when you leave - see Settings below. Both
  options are off by default.
- There is no configurable keybind default and no controller support. The
  hotkey must be bound by hand (below) before the mod does anything.

## Requirements

- Cyberpunk 2077 **patch 2.31** (game file version `3.0.80.51928`). The
  plugin declares this exact runtime to RED4ext. On any other game version,
  **RED4ext silently refuses to load the plugin** - nothing crashes, nothing
  errors, the mod just does nothing. This is the single most likely reason
  the hotkey appears to do nothing; see Troubleshooting.
- **Cyber Engine Tweaks (CET) v1.37.1** or a compatible version.
- **RED4ext**, matching the game version above.
- **Borderless windowed** display mode. Windows Magnifier does not reliably
  composite over an exclusive-fullscreen DX12 swapchain - if you're in true
  fullscreen, Magnifier may not track correctly even with the cursor free.

## Install

FreeCursor has two parts that both need to be in place - a native RED4ext
plugin and a CET Lua mod. Copy these into your game install (paths below are
relative to your Cyberpunk 2077 folder):

```
red4ext/plugins/FreeCursor/FreeCursor.dll
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/init.lua
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/state.lua
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/External/GameUI.lua
```

If you manage mods with **Vortex**, stage this same layout into a folder and
import/zip it for Vortex rather than copying files into the game install
directly - a manual copy will get clobbered on Vortex's next deployment.

`stage-mod.ps1` in the sibling `cp2077-tooling` repo does the filtering for
you: run `pwsh -File ..\cp2077-tooling\stage-mod.ps1 -Name FreeCursor` from
this repo's root and it copies `bin/` and `red4ext/` into `dist/FreeCursor`,
dropping `src/`, `build/` and `CMakeLists.txt`. It
stages source only, so copy the built `FreeCursor.dll` into the staged
`red4ext/plugins/FreeCursor/` yourself before zipping.

**After installing, you must bind the hotkey yourself:** open CET's overlay
(default `~`), go to **Bindings**, find **"Toggle free cursor"** under the
FreeCursor mod, and assign a key. **Semicolon (`;`) is the recommended
binding** - it is unused by the game and sits under the right hand. CET
cannot ship a default binding (its hotkey API has no such parameter, and
bindings live in CET's own `bindings.json`), so until you set one the mod
loads but the hotkey does nothing.

## Usage

Press your bound hotkey to detach the cursor, move Magnifier wherever you
need to read, then press it again to reattach. Behavior depends on what's on
screen:

| Context | Cursor | Mouse input to the game |
|---|---|---|
| Normal gameplay | freed | swallowed - your camera holds still while you read |
| Menu or phone/texting UI | freed | passes through normally (clicks and wheel work) |
| Loading, a scene, braindance, photo mode, or the main menu (no active session) | detach is refused | unchanged |

The game keeps running in real time while detached - nothing pauses.

**The keyboard always works while detached, in every context.** Only mouse
input is ever swallowed. So during normal gameplay you can still pick a
dialogue choice with **Up/Down + Enter** (or `F`, or the direct-pick keys
`F`/`R`/`1`/`2`), **hold `T`** for half a second to open the phone, open the
inventory with `I`, and so on - the camera stays still while you read the
options through Magnifier, and the keys still land. Timed dialogue choices
are the case this is for.

**The mouse wheel: plain scrolling works, Ctrl+Alt+wheel is Magnifier's.**
Magnifier zooms with **Ctrl+Alt+mouse wheel**, and it receives that wheel
input through a low-level hook that fires before the game ever sees it. If
the game were also allowed to see it, the same gesture would scroll or zoom
whatever is on screen underneath. So while detached, FreeCursor eats the
wheel from the game **only while Ctrl and Alt are both held**. A plain wheel
reaches the game as normal in every context - it scrolls a texting thread,
cycles weapons - and never moves the camera, because a wheel event carries
no motion.

**You can't get stranded.** If you detach and then a loading screen, cutscene,
braindance, or photo mode starts - or your session ends back to the main
menu - FreeCursor reattaches automatically. You never need to remember to
reattach before those moments.

## Settings

FreeCursor registers a **FreeCursor** tab with Native Settings UI, so it
shows up in **Mod Configuration Menu (MCM)** if you use it, and under the
vanilla **Settings > Mods** page either way. If Native Settings UI is not
installed, the mod still runs with whatever is in `settings.json` (defaults
if the file is absent) and prints one line to the CET console saying so.

Two switches, both **off** by default:

- **Detach in the phone** - the cursor frees itself when the phone or
  messenger opens and locks again when it closes. A phone call does not
  count: you keep full first-person control during a call, so nothing
  detaches.
- **Detach in menus** - the same for the inventory, map, journal, pause menu,
  vendors, stash and other full-screen menus.

An automatic detach only ever undoes itself. If you detached with the hotkey
before the phone opened, closing the phone leaves you detached. If the phone
detached you and you press the hotkey while it is open, you reattach and the
phone closing later does nothing. Dialogue choices happen during gameplay,
not in a menu, so they stay hotkey-only.

Settings persist in `settings.json` next to `init.lua`.

## Troubleshooting

**The hotkey does nothing at all.**
1. Check that you actually bound it - see Install above. Without a binding,
   there is nothing to press.
2. Check the CET console/log for a line like:
   `[FreeCursor] RED4ext plugin not found or incomplete (missing: ...)`.
   That means `FreeCursor.dll` isn't installed, didn't load, or RED4ext
   couldn't find it - reinstall the native plugin at
   `red4ext/plugins/FreeCursor/FreeCursor.dll` and confirm RED4ext itself is
   installed and loading (check `red4ext/logs/`).
3. **Check your game version.** The plugin only declares support for patch
   2.31 (`3.0.80.51928`). If your game has updated past that, RED4ext will
   silently skip loading the plugin - no error dialog, no crash, it just
   isn't there. This is the most common way the mod appears to "do nothing."
   Check `red4ext/logs/` for FreeCursor's own log entries; if there are none
   at all for this session, the plugin never loaded.

**The cursor won't reattach, or the camera won't hold still.**
FreeCursor is built to fail toward "stuck attached, in control" rather than
"stuck detached, input eaten." If a reattach can't be confirmed, it keeps
retrying automatically on every hotkey press and every context change - press
the hotkey again. If the RED4ext plugin becomes unable to reach the game
(for example after an unexpected patch mid-session), the hotkey stops being
able to detach for the rest of that session; the CET console will keep
reporting the failure so it isn't silent, but there's currently no
in-session fix beyond restarting.

**I opened the CET overlay while detached and the overlay's mouse is dead.**
While you are detached during normal gameplay, mouse input is being swallowed
before the game sees it, and the hook order between FreeCursor and CET is
undetermined - so opening the CET overlay in that state may leave the overlay
itself unable to see the mouse. CET also suppresses mod hotkeys while its
overlay is open, so pressing the toggle key will not help until the overlay is
closed. **The way out is the keyboard:** close the overlay with the keyboard
(the same key you opened it with, default `~`), then press your FreeCursor
toggle hotkey to reattach. FreeCursor never swallows keyboard input - that is
deliberate, and it is why this escape route always works.

**The hotkey stops toggling, and CET's overlay key goes dead too.** CET reads
its hotkeys from the same raw-input stream FreeCursor filters, fires them on
key release, and matches against every key it believes is held - mouse
buttons included. Versions before 0.3.0 could eat a button release CET was
waiting for (hold right-click to aim, press the toggle, let go), after which
every keypress looked like a combo to CET. 0.3.0 forwards any release whose
press it did not swallow. If it ever recurs: alt-tab out and back (CET resets
its key state on focus loss), or click once inside any menu.

**Where to look for logs.** RED4ext plugin messages (address resolution,
native call failures) go to RED4ext's own log output under `red4ext/logs/`.
Lua-side messages (missing plugin, per-toggle failures) print to CET's
console/log inside the CET overlay.

## Out of scope

There is no configurable default keybind, no controller support, and no
direct integration with Windows Magnifier beyond freeing the OS pointer for
it to follow. Magnifier itself is entirely up to you to run and configure.

---

## For developers: building from source

### RED4ext plugin

From `red4ext/plugins/FreeCursor/` in this repo:

```
"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -B build
"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Release
```

Output: `build/Release/FreeCursor.dll`. CMake fetches the pinned RED4ext SDK
commit automatically via `FetchContent`.

### Lua state-machine tests

The state machine in `state.lua` is pure Lua with no CET/game dependencies,
so it's tested standalone with LuaJIT. From `tests/` in this repo:

```
C:\Users\ringo\AppData\Local\Programs\LuaJIT\bin\luajit.exe test_state.lua
```

Assertions cover detach/reattach in gameplay and menus, refusal in blocked
contexts, toggling while already detached in a blocked context, crossing
between gameplay and menus while detached, and auto-reattach on entering a
blocked context.

### How it works, briefly

Three independent RED4ext RTTI globals, called from `init.lua`:

| Native | Effect |
|---|---|
| `FreeCursor_SetCursorForced(Bool) -> Bool` | frees/re-locks the OS pointer via the game's own `ForceCursor` |
| `FreeCursor_SetInputSwallow(Bool) -> Bool` | swallows all mouse input at the window level (gameplay only) |
| `FreeCursor_SetWheelBlock(Bool) -> Bool` | swallows the mouse wheel while Ctrl+Alt are held (whenever detached, gameplay and menus alike) |

The game reads input through Win32 Raw Input, so a `WM_INPUT` message can
carry a keyboard event as well as mouse motion. The hook reads each raw
packet's type and only ever swallows `RIM_TYPEMOUSE` packets; keyboard and
HID packets always pass through. The wheel block likewise checks the raw
packet's `RI_MOUSE_WHEEL` flag, not just `WM_MOUSEWHEEL` - the texting UI
scrolls from the raw stream, so blocking the legacy message alone was not
enough.

CET hooks the same window with the same 50 ms `EnumWindows` poll, so which of
the two sees input first is a per-launch race; the RED4ext log line
`Window hook installed; previous WndProc belongs to <module>` records the
outcome (`cyber_engine_tweaks.asi` = FreeCursor is ahead of CET). Because CET
takes its hotkeys from raw input and matches the full held-key set, the hook
never swallows a button release whose press it did not also swallow.

`state.lua` is a pure, unit-tested decision function with no engine
dependencies; `init.lua` is the only file that touches CET/game APIs, wiring
the hotkey, `GameUI.Observe` context changes, the phone poll and the settings
switches to those three natives. The phone is not a menu to GameUI (the
messenger overlay never raises the game's in-menu flag), so "detach in the
phone" polls the base game's `PhoneSystem.IsPhoneOpened()` ten times a
second while that option is on, and only counts it once GameUI reports a
non-default context, which excludes phone calls (full control, default
context). Auto-detach ownership (`auto` in
`state.lua`) is what stops a trigger ending from undoing a manual detach. See
`docs/superpowers/specs/2026-08-26-freecursor-design.md` in this repo for the
full design rationale.
