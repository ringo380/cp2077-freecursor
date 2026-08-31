# FreeCursor - in-game acceptance checklist

**Result, 2026-08-31: every item below passed**, on build 65,536-byte
FreeCursor.dll (pre-symbols), across roughly 45 minutes of play. That
includes the two steps that could have sent code back for changes:
**step 4**, the refcount probe, and **step 9**, the fall-through UI states.
`ForceCursor` sets rather than counts, and the fall-through states needed no
fix. Re-run this list after any change to the plugin.

A separate matter, not a FreeCursor finding: the session ended in a game
crash while loading a save in the netrunner shop (quest `sq_q001_tbug`,
objective `01_equip_ping`, Kabuki), which recurred on reload. FreeCursor was
loaded but never armed in the crashing session - its log holds only
`FreeCursor loaded` and no `ForceCursor` lines. Attribution is open; the
sibling `cp2077-tooling` repo's README covers reading the crash dump.

Run in **borderless windowed**, with the CET console open, and check
`red4ext/logs/` afterwards.

**Escape hatch, before you start:** keyboard input is never swallowed. Esc and
the toggle hotkey always reach the game, in every state. You cannot get locked
out by this mod without also being able to press the key that undoes it.

---

## 1. Install and load

- [ ] Import `dist/FreeCursor-0.1.0.zip` through Vortex and deploy.
      **Reimport whenever the DLL is rebuilt**, and confirm the deployed
      `red4ext/plugins/FreeCursor/FreeCursor.dll` matches the staged one by
      size and hash. A crash dump resolved against a `.map` from a different
      build names the wrong function and reads entirely plausible.
- [ ] Launch, bind the hotkey in CET's Bindings tab (**there is no default
      binding** - the mod does nothing until you set one), load a save.
- [ ] `red4ext/logs/` contains `FreeCursor loaded`.
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

## 4. Refcount probe - the most important unverified assumption

- [ ] Detach in gameplay.
- [ ] Cross from gameplay into a menu and back at least five times (open/close inventory or phone).
- [ ] Press the hotkey **once**.
- [ ] The pointer must fully re-lock.

If it stays free, the underlying native is counting rather than setting, and
that needs a fix. This is the single highest-value check here.

## 5. Menu / phone detach

- [ ] Open the phone, detach.
- [ ] Clicks work.
- [ ] The wheel does **not** scroll the menu.
- [ ] **Ctrl+Alt+wheel still zooms Magnifier - check this inside a menu
      specifically**, not just in gameplay.
- [ ] Reattach.

## 6. Clicking after crossing into a menu

- [ ] Detach in gameplay, cross into a menu, click something. Clicks must work.

## 7. Crossing while detached

- [ ] Cursor stays free throughout.
- [ ] Camera holds still only in gameplay, moves normally in menus.

## 8. Auto-reattach - one per trigger

While detached, trigger each and confirm it reattaches with no residual free
pointer:

- [ ] a scene / cutscene
- [ ] photo mode
- [ ] a braindance
- [ ] loading a save
- [ ] quitting to main menu

## 9. Fall-through UI states - known gap, needs your observation

These screens are none of gameplay/menu/blocked, so detaching there frees the
cursor with a **live camera**. Nobody guessed at a fix, because blanket
swallowing would break device-screen clicks. Record what actually happens:

- [ ] scanner / quickhack - camera moves? clicks work?
- [ ] inside a vehicle - camera moves? clicks work?
- [ ] a computer / device terminal - camera moves? clicks work?

Reading a shard via the scanner is plausibly a core use case, so this one
matters. Your answers decide the fix.

## 10. Interruptions

- [ ] Alt-tab away while detached in gameplay, return. No stuck swallow;
      hotkey still works.
- [ ] Open the CET overlay while detached in gameplay. Note whether the overlay
      responds to the mouse. Confirm the escape route works: close the overlay
      with the keyboard, then press the hotkey.
- [ ] CET `/reload` while detached. Cursor and swallow should clear themselves
      **immediately**, with no keypress needed.
- [ ] Exit the game while detached. Pointer is normal on the desktop.

## 11. Logs

- [ ] `red4ext/logs/` shows paired `ForceCursor(true)` / `ForceCursor(false)`
      transitions and no repeating spam.
