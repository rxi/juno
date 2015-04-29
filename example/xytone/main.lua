

function juno.onLoad()
  juno.debug.setVisible(true)

  vis = {}
  freq = 0
  gain = 0

  local delay = { idx = 0, max = 44100 * .4 }
  local phase = 0
  local xfreq = 0
  local xgain = 0

  juno.audio.master:setCallback(function(t)
    -- Clear visualisation table
    for i in ipairs(vis) do
      vis[i] = nil
    end
    -- Process audio                               
    for i = 1, #t, 2 do
      local dt = 1 / 44100
      -- Interp freq and gain (to avoid audible "steps" on high buffer size)
      xfreq = xfreq + (freq - xfreq) * .005
      xgain = xgain + (gain - xgain) * .005
      -- Increment phase
      phase = phase + dt * xfreq
      -- Generate sinewave
      local out = math.sin(phase * math.pi * 2)
      -- Apply gain
      out = out * xgain * .5
      -- Process delay
      out = out + (delay[delay.idx] or 0) * .5
      delay[delay.idx] = out
      delay.idx = (delay.idx + 1) % delay.max
      -- Write visualisation
      table.insert(vis, out)
      -- Write output
      t[i], t[i + 1] = out, out
    end
  end)
end


function juno.onMouseMove(x, y)
  gain = math.pow(1 - (y / juno.graphics.getWidth()), 1.8)
  freq = math.pow(x / juno.graphics.getHeight(), 2) * 3000 + 120
end


function juno.onDraw()
  -- Draw waveform
  juno.graphics.setColor(.7, .7, .7)
  local w, h = juno.graphics.getSize()
  local lastx, lasty
  for i, v in ipairs(vis) do
    local x, y = (i / #vis) * w, h / 2 + v * 100
    if i ~= 1 then
      juno.graphics.drawLine(lastx, lasty, x, y)
    end
    lastx, lasty = x, y
  end
  -- Draw x/y lines and info text
  juno.graphics.reset()
  local x, y = juno.mouse.getPosition()
  juno.graphics.drawLine(x, 0, x, juno.graphics.getHeight())
  juno.graphics.drawLine(0, y, juno.graphics.getWidth(), y)
  juno.graphics.drawText(string.format("freq: %.2fhz\ngain: %.2f",
                                       freq, gain), x + 10, y + 10)
end
