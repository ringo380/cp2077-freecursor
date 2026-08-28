-- FreeCursor CET wiring: joins the RED4ext natives (Task 1/2) with the pure
-- state machine (Task 3, state.lua) via a hotkey and GameUI context
-- observation. This file is the only place that touches CET/game APIs;
-- state.lua stays pure and untouched.
--
-- Vendored dependency: External/GameUI.lua is psiberx's GameUI v1.2.3
-- (44,157 bytes, SHA-256 A72647FAF49B2CBC...), copied byte-for-byte from
-- AerialTrafficSurge per the task-4 addendum. Do not edit that file here;
-- if it needs a change, that's an upstream concern.

local GameUI = require("External/GameUI")
local state = require("state")

-- mod.recovering is set when a teardown (reattach) attempt could not
-- confirm the cursor native actually released, and cleared once it does.
-- While true, every apply() call -- hotkey or automatic context change --
-- is forced down the teardown path regardless of what state.lua's decide()
-- computes, so a stuck cursor can never cause swallow/wheel to be silently
-- re-armed behind the player's back. See teardown()/apply() below.
local mod = { s = state.new(), ready = false, recovering = false }

-- GameUI.IsDetached() means "no active game session" (e.g. at the main menu
-- before loading a save) -- a naming collision with this mod's own notion of
-- a "detached cursor" (self.detached in state.lua). It is unrelated and is
-- folded into ctx.isBlocked below purely as one more reason to refuse/auto-
-- reattach, never as a signal about the cursor itself.
local function context()
  return {
    isDefault = GameUI.IsDefault(),
    isMenu    = GameUI.IsMenu(),
    isBlocked = GameUI.IsLoading() or GameUI.IsScene() or GameUI.IsBraindance()
                or GameUI.IsPhoto() or GameUI.IsDetached(),
  }
end

-- Drive toward the fully-attached state. This is the ONLY path that ever
-- clears swallow/wheel, and it never early-returns: wheel, swallow, and
-- cursor are each attempted independently and unconditionally, so a
-- reattach failure can never leave the player with swallowed mouse input
-- and no recovery lever. Order is wheel -> swallow -> cursor, which
-- guarantees input is un-swallowed no later than the cursor is reattached
-- (rather than "shouldn't matter" -- it does matter, and this order is why
-- it's safe).
--
-- Bookkeeping: we only believe the teardown succeeded if the cursor native
-- itself confirms it. If it doesn't, we deliberately keep mod.s.detached
-- true (the real native-visible state, overriding whatever state.lua's
-- decide() just set it to) and set mod.recovering, so the next hotkey
-- press or context-change event calls back in here and keeps retrying the
-- full teardown -- instead of the mod quietly believing the job is done
-- while input stays eaten.
local function teardown()
  if not Game.FreeCursor_SetWheelBlock(false) then
    print("[FreeCursor] Failed to clear wheel-block state; the mouse wheel may still be blocked.")
  end

  -- SetInputSwallow returns false on exactly one condition: the window hook is
  -- not installed. An uninstalled hook is not in the WndProc chain and so
  -- swallows nothing -- "returned false" and "input is actually eaten" are
  -- mutually exclusive by construction. So this branch must NOT alarm the
  -- player about captured input; that message could only ever be wrong.
  if not Game.FreeCursor_SetInputSwallow(false) then
    print("[FreeCursor] Input swallowing is not active (window hook not installed); your mouse is not captured.")
  end

  local cursorOk = Game.FreeCursor_SetCursorForced(false)
  if not cursorOk then
    print("[FreeCursor] Failed to reattach the cursor; will keep retrying on the next toggle press or context change.")
  end

  mod.s.detached = not cursorOk
  mod.recovering = not cursorOk
end

-- Apply an action returned by state.lua. All three natives are applied
-- independently (per addendum: never derive one from another, never skip a
-- call because another one was made).
local function apply(action)
  if action.refused then
    print("[FreeCursor] Can't detach the cursor in this context.")
    return
  end

  if mod.recovering then
    print("[FreeCursor] FreeCursor can't detach: the cursor native did not confirm a previous reattach, so detaching is disabled until it does. It keeps retrying on every toggle press and context change; if this never clears, restart the game.")
  end

  if mod.recovering or not action.cursor then
    -- Disarm direction, OR we're still recovering from a previous failed
    -- teardown: always drive toward the safe (attached) state regardless
    -- of what state.lua's ctx-based decision computed, until the cursor
    -- native actually confirms it let go. This is what stops an automatic
    -- context-change event from re-arming swallow/wheel while stuck.
    teardown()
    return
  end

  -- Arm direction: detach the cursor, then independently arm swallow/wheel.
  -- Cursor first: if it fails we never arm swallow/wheel, so the player is
  -- never left with a dead camera / eaten input and an attached cursor.
  if not Game.FreeCursor_SetCursorForced(true) then
    print("[FreeCursor] Failed to set cursor state; the plugin may need updating for this game version.")
    mod.s.detached = false
    return
  end

  -- Fail closed, precisely scoped on action.swallow being TRUE. In gameplay the
  -- swallow is what holds the camera still; without it the cursor would be
  -- detached with a live camera following pointer motion, which is exactly the
  -- state the spec says we must never leave the player in. So roll the cursor
  -- back rather than continue. A menu detach legitimately asks for
  -- swallow = false, and must never be aborted by the same benign false return
  -- (which only ever means "the window hook isn't up yet").
  if action.swallow and not Game.FreeCursor_SetInputSwallow(true) then
    Game.FreeCursor_SetCursorForced(false)
    mod.s.detached = false
    print("[FreeCursor] Couldn't hold the camera still; the cursor stays attached. Try again in a moment.")
    return
  end

  if not action.swallow and not Game.FreeCursor_SetInputSwallow(false) then
    print("[FreeCursor] Input swallowing is not active (window hook not installed); mouse input passes through as normal.")
  end

  if not Game.FreeCursor_SetWheelBlock(action.wheel) then
    print("[FreeCursor] Failed to set wheel-block state; the mouse wheel may scroll menus instead of driving Magnifier zoom.")
  end
end

registerForEvent("onInit", function()
  -- Partial-install check: the natives are RTTI globals registered by the
  -- RED4ext plugin. If any is missing, the plugin either isn't installed,
  -- didn't load for this game version, or a hook failed -- report plainly
  -- and refuse to arm the hotkey rather than let the player half-detach.
  local missing = {}
  for _, name in ipairs({
    "FreeCursor_SetCursorForced",
    "FreeCursor_SetInputSwallow",
    "FreeCursor_SetWheelBlock",
  }) do
    if type(Game[name]) ~= "function" then
      table.insert(missing, name)
    end
  end

  if #missing > 0 then
    print(("[FreeCursor] RED4ext plugin not found or incomplete (missing: %s). Install/update FreeCursor.dll in red4ext/plugins/FreeCursor/."):format(table.concat(missing, ", ")))
    return
  end

  -- The C++ flags outlive the Lua side: a CET `/reload` builds a fresh `mod`
  -- table with detached = false while the cursor may still be forced and
  -- swallow still armed, so the mod's belief and the natives' actual state
  -- diverge with no way back. Force all three to the safe state up front,
  -- mirroring onShutdown. Return values are ignored for the same reason they
  -- are there: this is unconditional best-effort, not something to retry.
  --
  -- Safe only because ApplyCursorForced dedups: clearing a reason that was
  -- never armed is a no-op rather than an unbalanced release of a refcount key.
  Game.FreeCursor_SetWheelBlock(false)
  Game.FreeCursor_SetInputSwallow(false)
  Game.FreeCursor_SetCursorForced(false)

  mod.ready = true

  -- Route every UI-context change through the same decision function used
  -- by the hotkey, so that: entering a blocked context (loading, scene,
  -- braindance, photo mode, no session) while detached auto-reattaches all
  -- three natives; crossing gameplay<->menu while detached keeps the cursor
  -- free and only re-evaluates swallow, with wheel staying true throughout.
  GameUI.Observe(function()
    if mod.ready then
      apply(state.onContextChange(mod.s, context()))
    end
  end)
end)

-- registerHotkey callbacks only fire while the CET overlay is closed -- this
-- is what makes "Toggle free cursor" usable as a real gameplay hotkey rather
-- than something that only works with the overlay open.
registerHotkey("freecursor_toggle", "Toggle free cursor", function()
  if not mod.ready then
    print("[FreeCursor] Not initialized; plugin missing or incomplete.")
    return
  end
  apply(state.toggle(mod.s, context()))
end)

-- Fail closed on shutdown: always force-clear all three natives regardless
-- of what state.lua currently thinks, so a script reload / game exit never
-- leaves the player with a stray detached cursor or swallowed input. Return
-- values are intentionally ignored here -- there is no user to report to
-- and nothing left to retry from once the mod is shutting down, so this is
-- deliberately best-effort rather than routed through teardown()'s
-- retry/recovery bookkeeping.
registerForEvent("onShutdown", function()
  if mod.ready then
    Game.FreeCursor_SetWheelBlock(false)
    Game.FreeCursor_SetInputSwallow(false)
    Game.FreeCursor_SetCursorForced(false)
  end
end)

return mod
