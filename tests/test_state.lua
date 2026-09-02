package.path = package.path .. ";../bin/x64/plugins/cyber_engine_tweaks/mods/FreeCursor/?.lua"

local state = require("state")

local failures = 0

local function check(label, got, want)
  if got ~= want then
    print(string.format("FAIL %s: got %s, want %s", label, tostring(got), tostring(want)))
    failures = failures + 1
  end
end

local GAMEPLAY = { isDefault = true,  isMenu = false, isBlocked = false }
local MENU     = { isDefault = false, isMenu = true,  isBlocked = false }
local BLOCKED  = { isDefault = false, isMenu = false, isBlocked = true  }

-- Detaching in gameplay frees the cursor and swallows mouse input; wheel is
-- also blocked (addendum case 1).
local s = state.new()
local a = state.toggle(s, GAMEPLAY)
check("gameplay detach cursor", a.cursor, true)
check("gameplay detach swallow", a.swallow, true)
check("gameplay detach detached", a.detached, true)
check("gameplay detach wheel", a.wheel, true)

-- Toggling again reattaches fully; wheel drops with everything else.
a = state.toggle(s, GAMEPLAY)
check("gameplay reattach cursor", a.cursor, false)
check("gameplay reattach swallow", a.swallow, false)
check("gameplay reattach detached", a.detached, false)
check("gameplay reattach wheel", a.wheel, false)

-- Detaching in a menu frees the cursor but leaves input alone -- except the
-- wheel, which is still blocked (addendum case 2, the case the addendum
-- exists for).
s = state.new()
a = state.toggle(s, MENU)
check("menu detach cursor", a.cursor, true)
check("menu detach swallow", a.swallow, false)
check("menu detach detached", a.detached, true)
check("menu detach wheel", a.wheel, true)

-- Toggling again reattaches from menu mode too (addendum case 5, the menu half).
a = state.toggle(s, MENU)
check("menu reattach cursor", a.cursor, false)
check("menu reattach swallow", a.swallow, false)
check("menu reattach detached", a.detached, false)
check("menu reattach wheel", a.wheel, false)

-- Blocked contexts refuse to detach and change nothing (addendum case 6).
s = state.new()
a = state.toggle(s, BLOCKED)
check("blocked refused", a.refused, true)
check("blocked cursor", a.cursor, false)
check("blocked detached", a.detached, false)
check("blocked swallow", a.swallow, false)
check("blocked wheel", a.wheel, false)

-- Crossing gameplay -> menu while detached keeps the cursor free, drops
-- swallow, and keeps the wheel blocked (addendum case 3).
s = state.new()
state.toggle(s, GAMEPLAY)
a = state.onContextChange(s, MENU)
check("cross to menu cursor", a.cursor, true)
check("cross to menu swallow", a.swallow, false)
check("cross to menu detached", a.detached, true)
check("cross to menu wheel", a.wheel, true)

-- Crossing menu -> gameplay while detached re-engages swallow and keeps the
-- wheel blocked throughout (addendum case 4).
s = state.new()
state.toggle(s, MENU)
a = state.onContextChange(s, GAMEPLAY)
check("cross to gameplay cursor", a.cursor, true)
check("cross to gameplay swallow", a.swallow, true)
check("cross to gameplay detached", a.detached, true)
check("cross to gameplay wheel", a.wheel, true)

-- Crossing into a blocked context while detached auto-reattaches, dropping
-- the wheel block too (addendum case 7).
s = state.new()
state.toggle(s, GAMEPLAY)
a = state.onContextChange(s, BLOCKED)
check("cross to blocked cursor", a.cursor, false)
check("cross to blocked swallow", a.swallow, false)
check("cross to blocked detached", a.detached, false)
check("cross to blocked wheel", a.wheel, false)

-- Pressing the hotkey while ALREADY detached in a blocked context must
-- reattach, not refuse. This pins the interaction between toggle's refusal
-- guard and decide's auto-reattach: the guard is scoped to `not self.detached`,
-- so it deliberately does not fire here, and decide's isBlocked branch then
-- reattaches. Refusing instead would strand a detached cursor in exactly the
-- context that must never hold one.
s = state.new()
state.toggle(s, GAMEPLAY)
check("toggle-in-blocked precondition", s.detached, true)
a = state.toggle(s, BLOCKED)
check("toggle while detached in blocked refused", a.refused, false)
check("toggle while detached in blocked cursor", a.cursor, false)
check("toggle while detached in blocked swallow", a.swallow, false)
check("toggle while detached in blocked detached", a.detached, false)
check("toggle while detached in blocked wheel", a.wheel, false)
check("toggle while detached in blocked self", s.detached, false)

-- ...and the guard DOES fire on the next press, now that we are attached:
-- pressing again in the same blocked context refuses rather than detaching.
a = state.toggle(s, BLOCKED)
check("re-toggle in blocked refused", a.refused, true)
check("re-toggle in blocked detached", a.detached, false)
check("re-toggle in blocked self", s.detached, false)

-- Context changes while attached must not spontaneously detach.
s = state.new()
a = state.onContextChange(s, MENU)
check("attached stays attached", a.detached, false)
check("attached no cursor", a.cursor, false)
check("attached no wheel", a.wheel, false)

-- Automatic detach (0.4.0). The trigger rising while attached detaches and
-- marks the detach as auto-owned; falling reattaches.
s = state.new()
a = state.setAuto(s, true, MENU)
check("auto detach cursor", a.cursor, true)
check("auto detach swallow", a.swallow, false)
check("auto detach detached", a.detached, true)
check("auto detach owner", s.auto, true)
a = state.setAuto(s, false, GAMEPLAY)
check("auto reattach cursor", a.cursor, false)
check("auto reattach detached", a.detached, false)
check("auto reattach owner", s.auto, false)

-- A manual detach outlives the trigger: the phone opening and closing
-- around a hand-detached cursor changes nothing.
s = state.new()
state.toggle(s, GAMEPLAY)
a = state.setAuto(s, true, MENU)
check("manual then auto rise detached", a.detached, true)
check("manual then auto rise owner", s.auto, false)
a = state.setAuto(s, false, GAMEPLAY)
check("manual then auto fall detached", a.detached, true)
check("manual then auto fall cursor", a.cursor, true)
check("manual then auto fall swallow", a.swallow, true)

-- The hotkey inside an auto detach reattaches AND takes ownership, so the
-- trigger ending later is a no-op rather than a second reattach.
s = state.new()
state.setAuto(s, true, MENU)
a = state.toggle(s, MENU)
check("hotkey inside auto cursor", a.cursor, false)
check("hotkey inside auto detached", a.detached, false)
check("hotkey inside auto owner", s.auto, false)
a = state.setAuto(s, false, GAMEPLAY)
check("trigger end after hotkey detached", a.detached, false)
check("trigger end after hotkey cursor", a.cursor, false)

-- Steady state is a plain re-evaluation: wanted stays true across a
-- gameplay/menu crossing and swallow follows the context.
s = state.new()
state.setAuto(s, true, MENU)
a = state.setAuto(s, true, GAMEPLAY)
check("auto steady cursor", a.cursor, true)
check("auto steady swallow", a.swallow, true)
check("auto steady owner", s.auto, true)

-- A blocked context never starts an auto detach, and ends one.
s = state.new()
a = state.setAuto(s, true, BLOCKED)
check("auto in blocked detached", a.detached, false)
check("auto in blocked owner", s.auto, false)
s = state.new()
state.setAuto(s, true, MENU)
a = state.setAuto(s, true, BLOCKED)
check("auto crossing to blocked detached", a.detached, false)
check("auto crossing to blocked owner", s.auto, false)
a = state.setAuto(s, false, GAMEPLAY)
check("auto fall after blocked detached", a.detached, false)

if failures == 0 then
  print("ok - all state tests passed")
  os.exit(0)
else
  print(string.format("%d test(s) failed", failures))
  os.exit(1)
end
