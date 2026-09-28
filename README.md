# FreeCursor

An accessibility mod for Cyberpunk 2077. Press a hotkey and your Windows
mouse pointer comes free of the game, so you can aim **Windows Magnifier**
at the text you need to read. The camera holds still while you read. Press
the hotkey again and you are back in control.

## Why this exists

The game locks the mouse pointer to the centre of the screen during play
and in most menus. Magnifier's "follow the mouse pointer" mode follows that
pointer, so it stays stuck on the middle of the screen and can never reach
a dialogue choice, an item description, or a subtitle. For a player who
relies on magnification, that makes much of the game's text unreadable.

FreeCursor frees the pointer on demand, without pausing the game or opening
a menu.

## What it does

- **Hotkey to detach and reattach.** You choose the key (see Install).
- **In gameplay, the camera holds still** while detached: mouse movement
  and clicks go to Magnifier, not the game.
- **In menus and the phone, the mouse still works** as normal while
  detached, so you can read and click.
- **The keyboard keeps working**, so you can pick a timed dialogue choice
  with Up/Down and Enter (or `F`, `R`, `1`, `2`) while you read it. The one
  exception is below.
- **Its own pointer, in the game's style.** While detached, the pointer is
  a cyan arrowhead like the game's menu pointer, drawn at your Windows
  pointer size (Settings > Accessibility > Mouse pointer and touch), and
  the game's own menu pointer is hidden so you never see two. Your Windows
  pointer colour does not apply while detached.
- **Magnifier's zoom gesture works.** Ctrl+Alt+mouse wheel zooms Magnifier
  and does not also scroll or zoom the game underneath.
- **Automatic detach in the phone and messages**, on by default, and
  optionally in menus.
- **You cannot get stranded.** A loading screen, cutscene, braindance,
  photo mode, or quitting to the main menu reattaches you automatically.

It does not start or configure Magnifier. Turn Magnifier on however you
normally do (Win + `+`) and set it to follow the mouse pointer.

## Requirements

- Cyberpunk 2077 2.x. Tested on **patch 2.31**. The plugin is not tied to
  one game version, so it keeps loading after a game update; if an update
  ever breaks it, the CET console says so (see Troubleshooting).
- [**RED4ext**](https://www.nexusmods.com/cyberpunk2077/mods/2380).
- [**Cyber Engine Tweaks**](https://www.nexusmods.com/cyberpunk2077/mods/107)
  (CET). Tested with 1.37.1.
- Optional: [**Native Settings UI**](https://www.nexusmods.com/cyberpunk2077/mods/3518)
  for the in-game settings page.
- **Borderless windowed** display mode is recommended. Magnifier may not
  track correctly over exclusive fullscreen.
- No controller support; this is for mouse and keyboard.

## Install

Install with Vortex or Mod Organizer 2, or extract the archive into your
game folder. It contains:

```
red4ext/plugins/FreeCursor/FreeCursor.dll
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/init.lua
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/state.lua
bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/External/GameUI.lua
```

**Then bind the hotkey. The mod does nothing until you do.** CET mods
cannot ship a default key.

1. Start the game and open the CET overlay (the key you chose when you
   first installed CET, often `~`).
2. Open **Bindings**, find **Toggle free cursor** under FreeCursor, and
   press the key you want.
3. **Numpad 9 is recommended.** The game does not use the numpad, and it
   is easy to find by touch.

Do not use a combination with Ctrl or Alt; see Keyboard below.

## Using it

Press your hotkey to detach, move the pointer (and Magnifier with it) to
the text, read, then press the hotkey again to reattach.

| Where you are | Pointer | Mouse input to the game |
|---|---|---|
| Normal gameplay | free | held back, so the camera stays still |
| A menu, the phone, or messages | free | passes through (clicks and wheel work) |
| Loading, a cutscene, braindance, photo mode, main menu | detach is refused | unchanged |

The game keeps running while you are detached; nothing pauses.

**Keyboard.** Every key reaches the game while detached, except **Ctrl and
Alt during gameplay**. Those two start Magnifier's zoom gesture, and the
game would otherwise crouch, dodge or switch items on them. A Ctrl or Alt
you were already holding when you detached still releases normally, so
crouch never sticks.

**Mouse wheel.** A plain wheel always reaches the game (scroll a message
thread, switch weapons). Only while Ctrl and Alt are both held is the wheel
kept from the game, so Magnifier's Ctrl+Alt+wheel zoom does not also act
on the game.

## Settings

With Native Settings UI installed, FreeCursor has a page under
**Settings > Mods** (and in Mod Configuration Menu if you use it). Two
switches:

- **Detach in the phone** (on by default): the pointer frees itself when the phone or
  messages open, and locks again when they close. Phone calls do not
  count; you keep full control during a call.
- **Detach in menus** (off by default): the same for the inventory, map, journal, pause
  menu, vendors, stash and other full-screen menus.

An automatic detach only undoes itself. If you detached with the hotkey
before opening the phone, closing the phone leaves you detached. Dialogue
choices happen in gameplay, not in a menu, so they use the hotkey.

Without Native Settings UI the mod still works with the defaults: phone
on, menus off.
Settings are saved in `settings.json` in the mod's CET folder.

## Troubleshooting

CET's messages appear in the CET overlay's console and in
`bin/x64/plugins/cyber_engine_tweaks/scripting.log`. The plugin writes
`red4ext/logs/freecursor-<date>.log`. Both paths are in the game folder.

**The hotkey does nothing.**
1. Check that you bound it (Install, step 2).
2. Look for `[FreeCursor] RED4ext plugin not found or incomplete` in the
   CET console. That means `FreeCursor.dll` did not load: check it is at
   exactly `red4ext/plugins/FreeCursor/FreeCursor.dll` and that RED4ext
   itself is working (it writes `red4ext/logs/red4ext-<date>.log`).
3. `Failed to set cursor state; the plugin may need updating for this game
   version` means a game update changed something FreeCursor relies on.
   Remove the mod until an updated version is out.

**The pointer will not reattach.** FreeCursor retries on every hotkey
press and every time the screen changes (entering or leaving a menu), so
press the hotkey again. If the CET console keeps reporting a failure,
restart the game.

**I opened the CET overlay while detached and its mouse does not work.**
Close the overlay with the keyboard (the same key that opened it), then
press your hotkey to reattach. CET ignores mod hotkeys while its overlay is
open, which is why the hotkey does nothing until you close it.

**The hotkey and CET's overlay key both stop responding.** Alt-tab out of
the game and back; CET resets its key state when the window loses focus.
Please report it (see below) with your `freecursor-<date>.log`.

**Reporting a problem.** Open an issue on GitHub
([ringo380/cp2077-freecursor](https://github.com/ringo380/cp2077-freecursor/issues))
or post on the Nexus page, and attach `red4ext/logs/freecursor-<date>.log`
and CET's `scripting.log` from the session where it happened.

## Building from source

`red4ext/plugins/FreeCursor/` is a CMake project that fetches the RED4ext
SDK at a pinned commit. From that folder:

```
cmake -B build
cmake --build build --config Release
```

Needs Visual Studio 2022 Build Tools (MSVC x64) and a Windows SDK. The
Release build also writes `FreeCursor.pdb` and `FreeCursor.map` beside the
DLL; keep them if you ever need to read a crash dump.

The Lua state machine (`state.lua`) has no game dependencies and is tested
with LuaJIT, which matches CET's runtime. From `tests/`:

```
luajit test_state.lua
```

### How it works

`init.lua` is the only file that talks to CET and the game. It calls three
natives the plugin registers:

| Native | Effect |
|---|---|
| `FreeCursor_SetCursorForced(Bool) -> Bool` | frees or re-locks the pointer through the game's own `ForceCursor` |
| `FreeCursor_SetInputSwallow(Bool) -> Bool` | holds back mouse input, plus Ctrl and Alt, at the game window (gameplay only) |
| `FreeCursor_SetWheelBlock(Bool) -> Bool` | holds back the wheel while Ctrl and Alt are held (whenever detached) |

The input hook sits on the game window's procedure and sees both the Raw
Input stream (`WM_INPUT`, which the game reads for mouselook) and the
legacy `WM_*BUTTON*` messages. It only ever holds back mouse packets, and
Ctrl/Alt keyboard packets while the gameplay hold is on; every other key
passes. On both paths a release is held back only if its press was, so no
button or key is ever left stuck, in the game or in CET. That matters for
CET in particular: it reads hotkeys from the same raw stream, and its
overlay takes the mouse capture on a button press and lets go on the
release.

CET hooks the same window at about the same moment, so which of the two
sees input first varies per launch. The plugin log line `Window hook
installed; previous WndProc belongs to <module>` records it.

The phone does not count as a menu to the game's UI state, so "Detach in
the phone" polls `PhoneSystem.IsPhoneOpened()` ten times a second while it
is on.

## License and credits

MIT, see `LICENSE`.

`External/GameUI.lua` is psiberx's GameUI helper (version 1.2.3), included
unchanged under its author's terms and not covered by this repository's
license.
