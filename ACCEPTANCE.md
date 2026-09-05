# FreeCursor - in-game acceptance checklist

**0.5.2 (2026-09-04): the 0.5.1 diagnostic logging removed.** Native and
Lua change, no behaviour change; the code is the 0.5.0 shape again. Import
`dist/FreeCursor-0.5.2.zip`, confirm the deployed DLL hash matches the
staged one, and that the RED4ext log reports version 0.5.2 with no
`cursor before force` lines. Step 3 is the regression set.

**0.5.1 (2026-09-03): diagnostic build for the cursor that stays visible
after reattaching - step 14 is new.** Logging only, no behaviour change.
Import `dist/FreeCursor-0.5.1.zip`; the RED4ext log must report version
0.5.1 and show `cursor before force(...)` lines around every ForceCursor
transition, and the CET console must print `[FreeCursor] hotkey: context=`
on every press.

**0.5.1 result (2026-09-04):** two logged sessions, 11 detach/reattach
pairs, the pointer hid within 100 ms of every reattach, including detaches
made inside the pause menu and inside a modal popup that were released in
gameplay. The report has not reproduced. Steps 8, 9 and 13 were run before
the 0.5.1 logging existed and are ticked on the tester's word; step 9's
per-state observations were not written down at the time.

**0.5.0 (2026-09-02): Ctrl and Alt held back from the game while detached
in gameplay - step 13 is new.** Native change. Import
`dist/FreeCursor-0.5.0.zip`, confirm the deployed DLL hash matches the
staged one, and that the RED4ext log reports version 0.5.0. Steps 3, 3b,
3c and 5 are the regression set for this build.

**0.4.0 (2026-09-02): settings menu and automatic detach - step 12 is new.**
Lua-only change; the DLL is rebuilt only for the version string. Confirm the
deployed `init.lua` contains `registerSettingsUi` and the RED4ext log
reports version 0.4.0.

**0.4.0 partial result (2026-09-02):** most steps re-run and passing,
including the settings menu and automatic detach; the remaining steps need
further play to reach their triggers. Not yet a full pass.

**0.3.1 result (2026-09-02):** reported as working well across a play
session; the alt-tab stuck-detach from 0.3.0 was not seen again. It is not
reproducible on demand, so the focus/flag logging stays in.

**0.3.0 (2026-09-01): needs a re-run.** Two changes on top of 0.2.0: a raw
button release whose press was not swallowed is now forwarded (the stuck
hotkey - **step 3c is the repro, and the point of this build**), and the
wheel now reaches the game unless Ctrl+Alt are held (step 5). Confirm the
deployed DLL is the new one first: it must contain the string
`previous WndProc belongs to` (0.2.0 does not).

**0.2.0 result (2026-09-01):** step 3b passed - keys work while detached, hold-T
opens the phone. Step 5's wheel check passed. New finding: after detaching
with right-click held, `;` stopped toggling and CET's overlay key went dead
until a right-click inside a menu. Log showed no `ForceCursor(false)` for the
whole stuck period, i.e. the Lua callback never ran. Root cause is in CET's
hotkey matcher (raw-input, key-up, full held-set match) plus a swallowed
button release; fixed in 0.3.0.

**0.2.0 build note:** the raw-input fix changes what the hook swallows, so
**step 3b was new in 0.2.0**. That DLL contains the string `GetRawInputData`
(0.1.0 does not, and is the **same byte size**, so size proves nothing).

**Result, 2026-08-31 (0.1.0): every item below passed**, on build
65,536-byte FreeCursor.dll (pre-symbols), across roughly 45 minutes of play.
That includes the two steps that could have sent code back for changes:
**step 4**, the refcount probe, and **step 9**, the fall-through UI states.
`ForceCursor` sets rather than counts, and the fall-through states needed no
fix. Re-run this list after any change to the plugin.

The 0.1.0 run also surfaced the bug 0.2.0 fixes: with the cursor detached in
gameplay, **no key did anything** - dialogue choices could not be picked - and
the wheel still scrolled the texting UI. Cause: the game reads input through
Raw Input, and the hook swallowed every `WM_INPUT`, keyboard included.

A separate matter, not a FreeCursor finding: the session ended in a game
crash while loading a save in the netrunner shop (quest `sq_q001_tbug`,
objective `01_equip_ping`, Kabuki), which recurred on reload. FreeCursor was
loaded but never armed in the crashing session - its log holds only
`FreeCursor loaded` and no `ForceCursor` lines. Attribution is open; the
sibling `cp2077-tooling` repo's README covers reading the crash dump.

Run in **borderless windowed**, with the CET console open, and check
`red4ext/logs/` afterwards.

**Escape hatch, before you start:** keyboard input is never swallowed, apart
from Ctrl and Alt while detached in gameplay (0.5.0). Esc and the toggle
hotkey always reach the game, in every state, as long as the toggle is not a
Ctrl or Alt combination. You cannot get locked out by this mod without also
being able to press the key that undoes it.

---

## 1. Install and load

- [ ] Import `dist/FreeCursor-0.5.2.zip` through Vortex and deploy.
      **Reimport whenever the DLL is rebuilt**, and confirm the deployed
      `red4ext/plugins/FreeCursor/FreeCursor.dll` matches the staged one by
      hash (not size). A crash dump resolved against a `.map` from a different
      build names the wrong function and reads entirely plausible.
- [ ] Launch, bind the hotkey in CET's Bindings tab to **`Numpad 9`** (**there is no
      default binding** - the mod does nothing until you set one), load a save.
- [ ] `red4ext/logs/` contains `FreeCursor loaded` and, a moment later,
      `Window hook installed; previous WndProc belongs to <module>`. Note the
      module: `cyber_engine_tweaks.asi` means FreeCursor is ahead of CET this
      launch (the order that exposed the stuck-hotkey bug);
      `Cyberpunk2077.exe` means CET is ahead. It varies per launch.
- [ ] The CET console does **not** show `RED4ext plugin not found`.

## 2. The startup race

- [ ] Mash the hotkey during the first ~3 seconds after a save finishes loading.

Expected: normal detach/reattach. If instead you see *"couldn't hold the camera
still; the cursor stays attached"* and the cursor does **not** detach - that is
the mod correctly refusing to half-work, not a failure. Note whether it happens.

## 3. Gameplay detach - the core case

- [ ] Press the hotkey during normal play.
- [ ] Pointer moves freely and Magnifier follows it.
- [ ] **The camera does not move.**
- [ ] Press again: pointer re-locks, camera responds normally.

## 3b. Keyboard while detached in gameplay - the 0.2.0 fix

All with the cursor detached during normal play (not in a menu):

- [ ] Walk up to an NPC conversation with choices. **Up/Down** moves the
      highlight, **Enter** (or `F`) picks it. Try a direct pick too (`R`, `1`).
- [ ] A **timed** choice: aim Magnifier at the options, read them, pick one
      with the keys before the timer runs out. This is the use case.
- [ ] **Hold `T`** for half a second: the phone opens. (A tap is the
      answer/notification key, not the phone - hold is correct.)
- [ ] `I` opens the inventory; `Esc` opens the pause menu.
- [ ] Throughout: **the camera never moves**, even while pressing keys and
      nudging the mouse.

## 3c. Toggle with a mouse button held - the 0.3.0 fix

Reproduced on 0.2.0 at 19:54 on 2026-09-01: detached with RMB held, stuck for
two minutes until a right-click inside a menu. Run it on a launch where the
log says `previous WndProc belongs to cyber_engine_tweaks.asi`; on the other
order it never reproduced and passing proves less.

- [ ] In gameplay, attached: **hold right-click** (aim), press `Numpad 9`, release
      right-click. Press `Numpad 9` again. It must reattach, immediately.
- [ ] Same with **left-click** held, and with the **middle button** held.
- [ ] Detach with nothing held, then click left and right a few times while
      detached (the game must not react - clicks are still swallowed in
      gameplay), then press `Numpad 9`. It must reattach.
- [ ] CET's overlay key still opens the overlay after each of the above.

## 4. Refcount probe - the most important unverified assumption

- [ ] Detach in gameplay.
- [ ] Cross from gameplay into a menu and back at least five times (open/close inventory or phone).
- [ ] Press the hotkey **once**.
- [ ] The pointer must fully re-lock.

If it stays free, the underlying native is counting rather than setting, and
that needs a fix. This is the single highest-value check here.

## 5. Menu / phone detach

- [ ] Open the phone, detach.
- [ ] Clicks work. Keys work (arrows move through contacts/messages).
- [ ] A **plain wheel scrolls the texting thread** (0.3.0: the wheel reaches
      the game unless Ctrl+Alt are held).
- [ ] **Ctrl+Alt+wheel zooms Magnifier and does NOT scroll the thread** -
      check this inside the texting thread specifically, not just in gameplay.
- [ ] In gameplay, detached: plain wheel cycles weapons as normal;
      Ctrl+Alt+wheel zooms Magnifier only. Camera never moves either way.
- [ ] Reattach.

## 6. Clicking after crossing into a menu

- [ ] Detach in gameplay, cross into a menu, click something. Clicks must work.

## 7. Crossing while detached

- [ ] Cursor stays free throughout.
- [ ] Camera holds still only in gameplay, moves normally in menus.

## 8. Auto-reattach - one per trigger

While detached, trigger each and confirm it reattaches with no residual free
pointer:

- [x] a scene / cutscene
- [x] photo mode
- [x] a braindance
- [x] loading a save
- [x] quitting to main menu

## 9. Fall-through UI states - known gap, needs your observation

These screens are none of gameplay/menu/blocked, so detaching there frees the
cursor with a **live camera**. Nobody guessed at a fix, because blanket
swallowing would break device-screen clicks. Record what actually happens:

- [x] scanner / quickhack - camera moves? clicks work?
- [x] inside a vehicle - camera moves? clicks work?
- [x] a computer / device terminal - camera moves? clicks work?

Reading a shard via the scanner is plausibly a core use case, so this one
matters. Your answers decide the fix. Run before 0.5.1; reported done on
2026-09-04 with no per-state notes, so the questions above are still open.

## 10. Interruptions

- [ ] Alt-tab away while detached in gameplay, return. No stuck swallow;
      hotkey still works.
- [ ] Open the CET overlay while detached in gameplay. Note whether the overlay
      responds to the mouse. Confirm the escape route works: close the overlay
      with the keyboard, then press the hotkey.
- [ ] CET `/reload` while detached. Cursor and swallow should clear themselves
      **immediately**, with no keypress needed.
- [ ] Exit the game while detached. Pointer is normal on the desktop.

## 12. Settings menu and automatic detach - new in 0.4.0

- [ ] Open Mod Configuration Menu (or Settings > Mods). A **FreeCursor** tab
      exists with an **Automatic detach** section and two switches, both off.
- [ ] Turn on **Detach in the phone**. Close the menu, hold `T`: the cursor
      frees itself within a moment of the phone opening. Clicks and the wheel
      work in the thread. Close the phone: the cursor locks and the camera
      responds. Repeat three times.
- [ ] Manual wins: detach with `Numpad 9` first, then open and close the phone. The
      cursor stays free the whole time; `Numpad 9` reattaches.
- [ ] Hotkey inside: let the phone detach you, press `Numpad 9` while it is open
      (locks), then close the phone. Nothing further happens.
- [ ] Turn on **Detach in menus**. Open the inventory: cursor frees. Close it:
      cursor locks. Open the map, then the journal, then the pause menu -
      each frees on entry and locks on exit. Open the phone from a menu and
      back: no flicker, still detached until both are closed.
- [ ] Turn a switch off while its trigger is active (phone open, or in the
      menu itself). The cursor locks immediately.
- [ ] Quit to desktop and relaunch: both switches are as you left them
      (`settings.json` in the mod folder holds them).
- [ ] With **Detach in the phone** on, take an incoming call during
      gameplay. The cursor must **not** free: a call keeps you in first
      person with full control, and the trigger requires the game to have
      left the default context. Open the messenger after hanging up: it
      detaches as normal.

## 13. Ctrl and Alt while detached in gameplay - new in 0.5.0

All in normal gameplay, standing, with the cursor detached via `Numpad 9`:

- [x] Tap and hold **Ctrl** alone. V does not crouch or dodge.
- [x] Tap **Alt** alone. No item switch.
- [x] **Ctrl+Alt+wheel**: Magnifier zooms, nothing happens in game.
- [x] Release everything, press `Numpad 9` to reattach. Ctrl now crouches and Alt
      now switches as normal; nothing is stuck.
- [x] Reverse order: **hold Ctrl first** (crouched), press `Numpad 9` while still
      holding it, then release Ctrl. V stands up (the release reached the
      game). Press `Numpad 9` to reattach.
- [x] Detach, hold Ctrl, press `Numpad 9` to reattach while still holding, then
      release. No stuck crouch either way.
- [x] In a menu, detached: Ctrl and Alt behave exactly as they do attached
      (menus are not affected by this change).
- [x] CET's overlay key still opens the overlay after each of the above.

## 14. Cursor still visible after reattach - diagnostic in 0.5.1, removed in 0.5.2

Reported 2026-09-03: the pointer sometimes stays on screen after the toggle
reattaches in gameplay. That day's logs show every reattach released all
three natives, and every detach began outside GameUI's default context (a
menu, a vehicle, the scanner or a popup), so the path under suspicion is
"detach outside gameplay, walk into gameplay, reattach there".

- [x] On foot, attached: press `Numpad 9` to detach, `Numpad 9` again to
      reattach. Note whether the pointer hides.
- [x] In a vehicle: same two presses. Note whether the pointer hides.
- [x] Open the inventory, press `Numpad 9` to detach, close the inventory, press
      `Numpad 9` in gameplay. Note whether the pointer hides.
- [x] Any time the pointer stays visible: does moving the mouse hide it?
      Does opening and closing a menu hide it? Note which.
- [x] Afterwards, keep the RED4ext log and the CET `scripting.log`. Under
      0.5.1 each `ForceCursor(false) applied` line was bracketed by cursor
      samples and the CET log named the GameUI context on every press.
      Result: 11 pairs, the pointer hid within 100 ms every time, nothing
      to fix. 0.5.2 removed the samples and the context prints; if the
      report comes back, the 0.5.1 diff is the diagnostic to reapply.

## 11. Logs

- [ ] `red4ext/logs/` shows paired `ForceCursor(true)` / `ForceCursor(false)`
      transitions and no repeating spam.
