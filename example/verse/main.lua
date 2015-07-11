

function juno.onLoad()
  G.screen = juno.graphics.screen
  G.elapsed = 983
  G.stars = {}
  for i = 1, 10000 do
    local x = {
      s = math.random() * .5,
      r = math.random() * 200,
      z = math.random() * 1
    }
    table.insert(G.stars, x)
  end
end


function juno.onUpdate(dt)
  G.elapsed = G.elapsed + dt
end


function juno.onDraw()
  G.screen:setBlend("add")
  G.screen:setColor(.4, .4, .7)
  for i, x in ipairs(G.stars) do
    local px = math.cos(x.s * G.elapsed + .3) * x.r
    local py = math.cos(x.s * G.elapsed + 1) * x.r
    px = px * x.z
    py = py * x.z
    G.screen:drawPixel(G.width / 2 + px, G.height / 2 + py)
  end
end

