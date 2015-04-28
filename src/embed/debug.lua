--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


juno.debug = juno.debug or {}

local font
local inited = false
local enabled = false
local focused = false
local indicators = {}
local lines = {}
local inputbuf = ""

-- Override print
local _print = print
print = function(...)
  _print(...)
  -- Convert all arguments to string and store in table
  local t = {}
  for i = 1, select("#", ...) do
    t[#t + 1] = tostring(select(i, ...))
  end
  local str = table.concat(t, " ")
  -- Multiple lines? Split and insert, else just insert the string
  if str:match("\n") then
    for line in (str .. "\n"):gmatch("(.-)\n") do
      table.insert(lines, line)
    end
  else
    table.insert(lines, str) 
  end
  while #lines > 6 do
    table.remove(lines, 1)
  end
end


local indicatorIdx
local textRegionWidth = 20

local function newIndicator(fn, min, max)
  min = min or 0
  max = max or 0
  local trueMin, trueMax = min, max
  -- Get idx
  indicatorIdx = indicatorIdx and (indicatorIdx + 1) or 0
  -- Init
  local pad = 8
  local height = 26
  local maxBars = 16
  local barUpdatePeriod = 1
  local yoffset = pad + height * indicatorIdx
  local lastUpdate = juno.time.getNow()
  local bars = {}
  local lastMin = min
  local lastMax = max
  -- Fill bars with zeros
  for i = 1, maxBars do
    bars[i] = 0
  end
  -- Return draw function
  return function()
    local txt, val = fn()
    -- Resize text region?
    textRegionWidth = math.max(font:getWidth(txt) + 8, textRegionWidth)
    -- Update bars?
    if juno.time.getNow() > lastUpdate + barUpdatePeriod then
      table.remove(bars)
      table.insert(bars, 1, val)
      min = math.min(trueMin, unpack(bars))
      max = math.max(trueMax, unpack(bars))
      lastUpdate = juno.time.getNow()
    end
    -- Draw text
    local w = textRegionWidth
    juno.graphics.drawRect(pad / 2, yoffset - (pad / 2),
                           w, height - 1, 0, 0, 0, .8)
    juno.graphics.drawText(font, txt, pad, yoffset)
    -- Draw bars
    juno.graphics.drawRect(pad / 2 + w + 1, yoffset - (pad / 2),
                           73, height - 1, 0, 0, 0, .8)
    for i, v in ipairs(bars) do
      local x = math.floor((bars[i] - min) / (max - min) * 16)
      juno.graphics.drawRect(pad / 2 + w + 1 + (i - 1) * 4 + 5,
                             yoffset + 16 - x, 3, x,
                             nil, nil, nil, (i == 1) and 1 or .4)
    end
  end
end


local function draw()
  -- Not enabled? Don't draw
  if not enabled then
    return
  end
  -- Draw
  juno.graphics.reset()
  -- Draw indicators
  for i, v in ipairs(indicators) do
    v()
  end
  -- Draw console input text
  local w = 300
  if focused then
    local h = font:getHeight()
    local y = juno.graphics.getHeight() - 8 - h
    local caret = (juno.time.getTime() % .6 < .3) and "_" or ""
    w = math.max(w, font:getWidth(inputbuf .. "_"))
    juno.graphics.drawRect(4, juno.graphics.getHeight() - h - 12,
                           w + 8, h + 8,
                           0, 0, 0, .8)
    juno.graphics.drawText(font, inputbuf .. caret, 8, y)
  end
  -- Draw console output text
  if #lines > 0 then
    local h = font:getHeight()
    local rh = #lines * h + 8
    local oy = focused and (h + 9) or 0
    for i, v in ipairs(lines) do
      w = math.max(w, font:getWidth(v))
    end
    juno.graphics.drawRect(4, juno.graphics.getHeight() - 4 - rh - oy,
                           w + 8, rh,
                           0, 0, 0, .8)
    for i, v in ipairs(lines) do
      local y = juno.graphics.getHeight() - 8 - (#lines - i + 1) * h
      juno.graphics.drawText(font, v, 8, y - oy)
    end
  end
end 


local function init()
  -- Init font
  font = juno.Font.fromEmbedded(12)
  -- Init indicators
  juno.debug.addIndicator(function()
    local r = juno.time.getFps()
    return r .. "fps", r
  end)
  juno.debug.addIndicator(function()
    local r = collectgarbage("count")
    return string.format("%.2fmb", r / 1024), r
  end)
  -- Override present function to draw the debug information before calling the
  -- proper present function
  local present = juno.graphics.present
  juno.graphics.present = function(...)
    draw()
    present(...)
  end
  -- Set init flag
  inited = true
end


local onError = function(msg)
  print("error: " .. msg:match("[^\n]+"))
end

function juno.debug._onEvent(e)
  -- Handle console's keyboard input
  if e.type == "keydown" and enabled and focused then
    if e.key == "backspace" then
      inputbuf = inputbuf:sub(1, #inputbuf - 1)
    elseif e.key == "return" then
      local fn, err = loadstring(inputbuf, "=input")
      if fn then
        xpcall(fn, onError)
      else
        onError(err)
      end
      inputbuf = ""
    elseif e.char then
      inputbuf = inputbuf .. e.char
    end
  end
end


function juno.debug._draw()
  draw()
end


function juno.debug.setVisible(x)
  enabled = x and true or false
  if enabled and not inited then
    init()
  end
end

function juno.debug.getVisible(x)
  return enabled
end


function juno.debug.setFocused(x)
  focused = x and true or false
end

function juno.debug.getFocused(x)
  return focused
end


function juno.debug.clear()
  while #lines > 0 do
    table.remove(lines)
  end
end


function juno.debug.addIndicator(fn, min, max)
  -- Error check
  local str, num = fn()
  if type(str) ~= "string" or type(num) ~= "number" then
    error("expected indicator function to return string and number", 2)
  end
  if min and type(min) ~= "number" then
    error("expected `min` to be a number", 2)
  end
  if max and type(max) ~= "number" then
    error("expected `max` to be a number", 2)
  end
  -- Create, add and return
  local indicator = newIndicator(fn, min, max)
  table.insert(indicators, indicator)
  return indicator
end


function juno.debug.removeIndicator(indicator)
  for i, v in ipairs(indicators) do
    if v == indicator then
      table.remove(indicators, i)
      return
    end
  end
end
