-- Pure decision logic for FreeCursor. Deliberately free of game/CET calls so
-- it can be unit-tested outside the game; the caller applies the returned
-- action via the RED4ext plugin.
--
-- Action shape: { cursor, swallow, detached, refused, wheel } -- all bool.
--   cursor   -> Game.FreeCursor_SetCursorForced(Bool)
--   swallow  -> Game.FreeCursor_SetInputSwallow(Bool) (gameplay only)
--   wheel    -> Game.FreeCursor_SetWheelBlock(Bool)   (whenever detached,
--               gameplay and menu alike -- see addendum: Ctrl+Alt+wheel
--               drives Windows Magnifier zoom and must not also scroll the
--               game. The native only eats the wheel while Ctrl+Alt are
--               held; a plain wheel always reaches the game.)
--   detached -> mirrors self.detached, handed back for the caller's own state
--   refused  -> true only when a detach attempt was rejected outright
--               (blocked context); everything else is left unchanged

local state = {}

-- A factory, not a shared constant: callers may inspect/mutate the returned
-- action, so each call needs its own table.
local function attached()
  return { cursor = false, swallow = false, detached = false, refused = false, wheel = false }
end

-- `auto` records that the current detach was started by an automatic trigger
-- (phone opened, menu entered) rather than the hotkey. Only an auto detach is
-- ever undone automatically; a manual detach outlives the trigger.
function state.new()
  return { detached = false, auto = false }
end

-- Given the desired intent and the current context, produce the action to apply.
function state.decide(self, ctx)
  if not self.detached then
    return attached()
  end

  -- Blocked contexts (loading, scene, braindance, photo mode) never hold a
  -- detached cursor: auto-reattach rather than strand the player.
  if ctx.isBlocked then
    self.detached = false
    self.auto = false
    return attached()
  end

  -- Gameplay: free the cursor and swallow mouse input so the camera holds still.
  -- Menu: free the cursor but pass input through so the UI stays usable --
  -- except the mouse wheel, which stays blocked in both modes so it can't
  -- also scroll/zoom the game while the player is using it to drive the
  -- Windows Magnifier.
  return {
    cursor   = true,
    swallow  = ctx.isDefault and not ctx.isMenu,
    detached = true,
    refused  = false,
    wheel    = true,
  }
end

function state.toggle(self, ctx)
  if not self.detached and ctx.isBlocked then
    local action = attached()
    action.refused = true
    return action
  end

  -- A hotkey press always takes ownership: reattaching cancels a pending
  -- auto-reattach, and detaching by hand is never undone by a trigger ending.
  self.detached = not self.detached
  self.auto = false
  return state.decide(self, ctx)
end

function state.onContextChange(self, ctx)
  return state.decide(self, ctx)
end

-- Automatic trigger: `wanted` is true while any enabled auto-detach
-- condition holds (phone open, menu open). Rising edge detaches only if the
-- cursor is attached; falling edge reattaches only if the detach was ours.
-- Everything else is a plain context re-evaluation, identical to
-- onContextChange, so this can be the single entry point for both.
function state.setAuto(self, wanted, ctx)
  if wanted and not self.detached and not ctx.isBlocked then
    self.detached = true
    self.auto = true
  elseif not wanted and self.auto then
    self.detached = false
    self.auto = false
  end
  return state.decide(self, ctx)
end

return state
