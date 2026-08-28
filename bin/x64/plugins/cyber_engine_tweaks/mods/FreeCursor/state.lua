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
--               game, even though other mouse input passes through in menus)
--   detached -> mirrors self.detached, handed back for the caller's own state
--   refused  -> true only when a detach attempt was rejected outright
--               (blocked context); everything else is left unchanged

local state = {}

-- A factory, not a shared constant: callers may inspect/mutate the returned
-- action, so each call needs its own table.
local function attached()
  return { cursor = false, swallow = false, detached = false, refused = false, wheel = false }
end

function state.new()
  return { detached = false }
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

  self.detached = not self.detached
  return state.decide(self, ctx)
end

function state.onContextChange(self, ctx)
  return state.decide(self, ctx)
end

return state
