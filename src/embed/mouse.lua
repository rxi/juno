--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


local position = { x = 0, y = 0 }
local buttonsDown = {}
local buttonsPressed = {}


function juno.mouse._onEvent(e)
  if e.type == "mousemove" then
    position.x, position.y = e.x, e.y
  elseif e.type == "mousebuttondown" then
    buttonsDown[e.button] = true
    buttonsPressed[e.button] = true
  elseif e.type == "mousebuttonup" then
    buttonsDown[e.button] = nil
  end
end


function juno.mouse.reset()
  for k in pairs(buttonsPressed) do
    buttonsPressed[k] = nil
  end
end


function juno.mouse.isDown(...)
  for i = 1, select("#", ...) do
    local b = select(i, ...)
    if buttonsDown[b] then
      return true
    end
  end
  return false
end


function juno.mouse.wasPressed(...)
  for i = 1, select("#", ...) do
    local b = select(i, ...)
    if buttonsPressed[b] then
      return true
    end
  end
  return false
end


function juno.mouse.getPosition()
  return position.x, position.y
end


function juno.mouse.getX()
  return position.x
end


function juno.mouse.getY()
  return position.y
end
