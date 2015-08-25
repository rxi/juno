

function juno.onLoad()
  G.screen = juno.Buffer.fromBlank(juno.graphics:getSize())
  G.elapsed = 983
  G.timer = 0
  G.timer2 = 0
end


function juno.onKeyDown(k)
  if k == "f12" then
    juno.debug.setVisible(not juno.debug.getVisible())
  end
end


function juno.onUpdate(dt)
  -- Draw
  G.timer = G.timer - dt
  while G.timer <= 0 do
    local step = 0.001
    G.screen:setAlpha(.2)
    G.screen:setBlend("add")
    G.screen:setColor(math.abs(math.cos(G.elapsed * .2)) * .8, .2, .5)
    G.timer = G.timer + step
    G.elapsed = G.elapsed + step
    local x = (G.width + G.width * math.cos(G.elapsed)) / 2
    local y = (G.height + G.height * math.sin(G.elapsed * .7)) / 2
    local x1, y1 = math.cos(6 * G.elapsed), math.sin(6 * G.elapsed)
    local x2, y2 = math.cos(5 * G.elapsed), math.sin(5 * G.elapsed)
    local r = 60 + math.sin(2 * G.elapsed) * 50
    G.screen:drawLine(x + x1 * r, y + y1 * r, x + x2 * r, y + y2 * r)
  end
  -- Fade
  G.timer2 = G.timer2 - dt
  while G.timer2 <= 0 do
    G.timer2 = G.timer2 + .1
    G.screen:setAlpha(.02)
    G.screen:setBlend("alpha")
    G.screen:setColor(0, 0, 0)
    G.screen:drawRect(0, 0, G.screen:getSize())
  end
end


function juno.onDraw()
  juno.graphics.draw(G.screen)
end

