--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


local function checkArg(idx, cond, msg)
  if not cond then
    error("bad argument #" .. idx .. ", " .. msg, 3)
  end
end


function juno.Buffer:getSize()
  return self:getWidth(), self:getHeight()
end


local defaultFont = juno.Font.fromEmbedded()

local fontTexCache = {}
setmetatable(fontTexCache, { 
  __index = function(t, k) 
    fontTexCache[k] = {}
    return fontTexCache[k]
  end,
  __mode = "v",
})

function juno.Buffer:drawText(font, text, x, y, width)
  if type(font) ~= "userdata" then
    return self:drawText(defaultFont, font, text, x, y, width)
  end
  checkArg(3, x == nil or type(x) == "number", "expected number")
  checkArg(4, y == nil or type(y) == "number", "expected number")
  checkArg(5, width == nil or type(width) == "number", "expected number")
  text = tostring(text)
  if width then
    -- Word wrapped multi line
    local height = font:getHeight()
    local line
    for word in text:gmatch("%S+") do
      local tmp = (line and (line .. " ") or "") .. word
      if font:getWidth(tmp) > width then
        juno.graphics.drawText(font, line, x, y)
        y = y + height
        line = word
      else
        line = tmp
      end
    end
    self:drawText(font, line, x, y)
  elseif text:find("\n") then
    -- Multi line
    local height = font:getHeight()
    for line in (text.."\n"):gmatch("(.-)\n") do
      self:drawText(font, line, x, y)
      y = y + height
    end
  else
    -- Single line
    local tex = fontTexCache[font][text]
    if not tex then
      tex = font:render(text)
      fontTexCache[font][text] = tex
    end
    self:drawBuffer(tex, x, y)
  end
end
