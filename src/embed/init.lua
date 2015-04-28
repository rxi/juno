--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


local function call(fn, ...)
  if fn then return fn(...) end
end

local function merge(...)
  local res = {}
  for i = 1, select("#", ...) do
    local t = select(i, ...)
    if t then
      for k, v in pairs(t) do
        res[k] = v
      end
    end
  end
  return res
end

local function push(arr, ...)
  for i = 1, select("#", ...) do
    table.insert(arr, (select(i, ...)))
  end
end


local doneOnError = false
local exit = os.exit
local traceback = debug.traceback

local function onError(msg)
  if not doneOnError then
    doneOnError = true
    juno.onError(msg, traceback())
  else
    print("\n" .. msg .. "\n" .. traceback())
    exit(1)
  end
end

-------------------------------------------------------------------------------
-- Init callbacks
-------------------------------------------------------------------------------

local eventHandlers = {
  keydown = function(e)
    call(juno.keyboard._onEvent, e)
    call(juno.debug._onEvent, e)
    call(juno.onKeyDown, e.key, e.char)
    end,
  keyup = function(e)
    call(juno.keyboard._onEvent, e)
    call(juno.onKeyUp, e.key)
    end,
  mousemove = function(e)
    call(juno.mouse._onEvent, e)
    call(juno.onMouseMove, e.x, e.y)
    end,
  mousebuttondown = function(e)
    call(juno.mouse._onEvent, e)
    call(juno.onMouseDown, e.x, e.y, e.button)
    end,
  mousebuttonup = function(e)
    call(juno.mouse._onEvent, e)
    call(juno.onMouseUp, e.x, e.y, e.button)
    end,
  quit = function(e)
    call(juno.onQuit)
    os.exit()
    end,
}

local function onStepMain()
  for i, e in ipairs(juno.system.poll()) do
    call(eventHandlers[e.type], e)
  end
  call(juno.time.step)
  call(juno.onUpdate, call(juno.time.getDelta))
  call(juno.graphics.clear)
  call(juno.onDraw)
  call(juno.debug._draw)
  call(juno.keyboard.reset)
  call(juno.mouse.reset)
end

function juno._onStep()
  xpcall(onStepMain, onError)
end

function juno._onAudio(...)
  local res
  local args = { ... }
  xpcall(function()
    res = call(juno.onAudio, unpack(args))
  end, onError)
  return res
end

local pcallFunc
local pcallArgs = {}
local pcallWrap = function()
  return pcallFunc(unpack(pcallArgs))
end

function juno._pcall(fn, ...)
  pcallFunc = fn
  -- Fill argument table with new arguments, discard old args
  local n = select("#", ...)
  for i = 1, n do
    pcallArgs[i] = select(i, ...)
  end
  if #pcallArgs > n then
    for i = n + 1, #pcallArgs do
      pcallArgs[i] = nil
    end
  end
  -- Do call
  return xpcall(pcallWrap, onError)
end

function juno.onError(msg, stacktrace)
  -- Create and print error string
  local tab = "    "
  local str = 
    msg:gsub("\t", tab):gsub("\n+$", "") .. "\n\n" ..
    stacktrace:gsub("\t", tab)
  print("Error:\n" .. str)
  -- Override event handlers
  eventHandlers = {
    quit = function() os.exit() end,
    keydown = function(e) if e.key == "escape" then os.exit() end end,
  }
  -- Disable debug
  call(juno.debug.setVisible, false)
  call(juno.mouse.setVisible, true)

  --  Init error state
  local font, bigfont
  local done = false 
  local alpha = 0

  function juno.onUpdate()
    -- The initialisation of the error state's graphics is defered to the
    -- first onUpdate() call in case the error occurs in the audio thread in
    -- which case it won't be able to change the openGL state 
    juno.graphics.reset()
    juno.graphics.setClearColor(.15, .16, .2)
    font = juno.Font.fromEmbedded(14)
    bigfont = juno.Font.fromEmbedded(40)
    -- Init update function
    function juno.onUpdate(dt)
      if alpha == 1 then
        done = true
      else
        alpha = math.min(alpha + dt / .5, 1)
      end
    end
  end

  function juno.onAudio() end
   
  function juno.onDraw()
    juno.graphics.setAlpha(alpha)
    juno.graphics.drawText(bigfont, "Error", 40, 40)
    juno.graphics.drawText(font, str, 40, 120)
    -- As this screen won't change after its faded in we can sleep here to
    -- avoid having to redraw too often -- this will reduce CPU usage
    if done then
      juno.time.sleep(.1)
    end
  end
end


-------------------------------------------------------------------------------
-- Init filesystem
-------------------------------------------------------------------------------

-- Mount project paths
if juno._argv[2] then
  -- Try to mount all arguments as package
  for i = 2, #juno._argv do
    juno.fs.mount(juno._argv[i])
  end
else
  -- Try to mount default packages (pak0, pak1, etc.)
  local dirs = { juno.system.info("exedir") }
  if juno.system.info("os") == "osx" then
    table.insert(dirs, juno.system.info("exedir") .. "/../Resources")
  end
  for _, dir in ipairs(dirs) do
    local idx = 0
    while juno.fs.mount(dir .. "/pak" .. idx) do
      idx = idx + 1
    end
    if idx ~= 0 then break end
  end
end

-- Add filesystem-compatible package loader
table.insert(package.loaders, 1, function(modname)
  modname = modname:gsub("%.", "/")
  for x in package.path:gmatch("[^;]+") do
    local filename = x:gsub("?", modname)
    if juno.fs.exists(filename) then
      return assert(loadstring(juno.fs.read(filename), "=" .. filename))
    end
  end
end)

-- Add extra package paths
package.path = package.path .. ";?/init.lua"



-------------------------------------------------------------------------------
-- Init config    
-------------------------------------------------------------------------------

local c = {}
if juno.fs.exists("config.lua") then
  c = call(require, "config")
end

local config = merge({
  title       = "Juno " .. juno.getVersion(),
  width       = 500,
  height      = 500,
  maxfps      = 60,
  samplerate  = 44100,
  buffersize  = 2048,
  fullscreen  = false,
  resizable   = false,
}, c)

if not config.identity then
  config.identity = config.title:gsub("[^%w]", ""):lower()
end


-------------------------------------------------------------------------------
-- Init filesystem write path
-------------------------------------------------------------------------------

local appdata = juno.system.info("appdata")
local path = appdata .. "/juno/" .. config.identity

juno.fs.setWritePath(path)
juno.fs.mount(path)


-------------------------------------------------------------------------------
-- Init modules   
-------------------------------------------------------------------------------

juno.graphics.init(config.width, config.height, config.title,
                   config.fullscreen, config.resizable)
juno.graphics.setMaxFps(config.maxfps)
juno.graphics.setClearColor(0, 0, 0)
juno.audio.init(config.samplerate, config.buffersize)


-------------------------------------------------------------------------------
-- Init project   
-------------------------------------------------------------------------------

if juno.fs.exists("main.lua") then
  -- Load project file
  xpcall(function() require "main" end, onError)
else
  -- No project file -- init "no project loaded" screen
  local w, h = juno.graphics.getSize()
  local txt = juno.Font.fromEmbedded(14):render("No project loaded")
  local txtPost = txt:clone()
  local txtMask = txt:clone()
  local particles = {}

  function juno.onLoad()
    juno.graphics.setClearColor(0.15, 0.15, 0.15)
    for i = 1, 30 do 
      local p = {
        x = 0,
        y = (i / 30) * 100,
        z = 0,
        r = (i / 30) * 2,
      }
      table.insert(particles, p) 
    end
  end

  function juno.onUpdate(dt)
    local n = juno.time.getTime()
    for _, p in ipairs(particles) do
      p.x = math.cos(n * p.r) * 100
      p.z = math.sin(n * p.r)
    end
  end

  function juno.onKeyDown(k)
    if k == "escape" then
      os.exit()
    end
  end

  function juno.onDraw()
    -- Draw particles
    juno.graphics.setBlend("add")
    local lastx, lasty
    for _, p in ipairs(particles) do
      local x, y = (p.x * p.z) + w / 2, (p.y * p.z) + w / 2
      juno.graphics.setAlpha(p.a)
      juno.graphics.drawPixel(x, y)
      if lastx then
        juno.graphics.setAlpha(.3)
        juno.graphics.drawLine(x, y, lastx, lasty)
      end
      lastx, lasty = x, y
    end
    -- Draw text
    local n = juno.time.getTime() * 2
    local x = (1 + math.sin(n)) * txtMask:getWidth() / 2
    txtPost:copyPixels(txt)
    txtMask:clear(1, 1, 1, .5)
    txtMask:drawRect(x - 10, 0, 20, 100, 1, 1, 1, .6)
    txtMask:drawRect(x -  5, 0, 10, 100, 1, 1, 1, 1)
    juno.bufferfx.mask(txtPost, txtMask)
    local tx, ty = (h - txt:getWidth()) / 2, (h - txt:getHeight()) / 2
    juno.graphics.reset()
    juno.graphics.draw(txtPost, tx, ty + 130)
  end

end

xpcall(function() call(juno.onLoad) end, onError)

