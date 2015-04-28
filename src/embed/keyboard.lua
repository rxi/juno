--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


-- The table is checked and created here if it does not exist, at the time of
-- writing this the juno.keyboard does not have a C module, and so the table
-- won't exist yet.
juno.keyboard = juno.keyboard or {}

local keysDown = {}
local keysPressed = {}


function juno.keyboard._onEvent(e)
  if e.type == "keydown" then
    keysDown[e.key] = true
    keysPressed[e.key] = true
  elseif e.type == "keyup" then
    keysDown[e.key] = nil
  end
end


function juno.keyboard.reset()
  for k in pairs(keysPressed) do
    keysPressed[k] = nil
  end
end


function juno.keyboard.isDown(...)
  for i = 1, select("#", ...) do
    local k = select(i, ...)
    if keysDown[k] then
      return true
    end
  end
  return false
end


function juno.keyboard.wasPressed(...)
  for i = 1, select("#", ...) do
    local k = select(i, ...)
    if keysPressed[k] then
      return true
    end
  end
  return false
end

