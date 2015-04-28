--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


-- Override juno.graphics.init function
local init = juno.graphics.init

juno.graphics.init = function(...)
  -- Do init
  local screen = init(...)
  juno.graphics.screen = screen
  -- Bind the screen buffer's methods to the graphics module
  for k, v in pairs(juno.Buffer) do
    if not juno.graphics[k] then
      juno.graphics[k] = function(...)
        return v(screen, ...)
      end
    end
  end
  -- Unbind Buffer constructors (which make no sense bound)
  juno.graphics.fromBlank  = nil
  juno.graphics.fromFile   = nil
  juno.graphics.fromString = nil
  -- Override juno.graphics.clear() to use _clearColor if available
  local clear = juno.graphics.clear
  function juno.graphics.clear(r, g, b, a)
    local c = juno.graphics._clearColor
    r = r or (c and c[1])
    g = g or (c and c[2])
    b = b or (c and c[3])
    clear(r, g, b, 1)
  end
  -- Return main screen buffer
  return screen
end


function juno.graphics.setClearColor(...)
  juno.graphics._clearColor = { ... }
end


