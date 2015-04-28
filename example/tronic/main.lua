

function juno.onLoad(dt)
  G.field = juno.Buffer.fromBlank(G.width, G.height)
  G.field:drawBox(0, 0, G.width, G.height)
  G.tickTimer = 0
  -- Initialise player
  G.player = {
    x = 20,
    y = G.height / 2,
    direction = "down",
    color = { 1, 1, 1 },
  }
  -- Initialise AIs
  G.ai = {}
  for i = 1, 3 do
    table.insert(G.ai, {
      x = ((G.width - 40) / 3) * i + 20,
      y = G.height / 2,
      dead = false,
      direction = "down",
      turnTimer = 0,
      turnRate = 10 + math.random(20),
      color = ({ { 1, 0, 0 }, { 0, 1, 1 }, { 1, 1, 0 } })[i],
    })
  end
end


function juno.onKeyDown(k)
  -- Handle player movement keys
  if k == "left" or k == "up" or k == "down" or k == "right" then
    G.player.direction = k
  end
  -- Handle game restart key
  if k == "r" then
    juno.onLoad()
  end
end


local function isPixelBlack(x, y)
  local r, g, b = G.field:getPixel(x, y)
  return r == 0 and g == 0 and b == 0
end


local function nextPosition(bike, steps)
  steps = steps or 1
  return ({
    left  = function() return bike.x - steps, bike.y end,
    right = function() return bike.x + steps, bike.y end,
    up    = function() return bike.x, bike.y - steps end,
    down  = function() return bike.x, bike.y + steps end,
  })[bike.direction]()
end


local function randomDirection()
  return ({ "left", "right", "up", "down" })[math.random(4)]
end


local function updateAi(ai, dt)
  -- Do random turn timer and random turn
  ai.turnTimer = ai.turnTimer - 1
  if ai.turnTimer <= 0 then
    ai.turnTimer = math.random(ai.turnRate)
    ai.direction = randomDirection()
  end
  -- Do obstacle avoidance
  for lookahead = 4, 1, -1 do 
    for i = 1, 8 do
      if not isPixelBlack(nextPosition(ai, lookahead)) then
        ai.direction = randomDirection()
      end
    end
  end
end


local function updateBike(bike)
  -- Don't update the bike if its dead
  if bike.dead then
    return
  end
  -- Move bike
  bike.x, bike.y = nextPosition(bike)
  -- Kill the bike if it collided with something
  if not isPixelBlack(bike.x, bike.y) then
    bike.dead = true
    return
  end
  -- Draw bike
  G.field:setPixel(bike.x, bike.y, unpack(bike.color))
end


local function onTick()
  -- Update AIs
  for i, ai in ipairs(G.ai) do
    updateAi(ai)
    updateBike(ai)
  end
  -- Update player
  updateBike(G.player)
end


function juno.onUpdate(dt)
  -- Update tick timer
  G.tickTimer = G.tickTimer - dt
  while G.tickTimer <= 0 do
    onTick()
    G.tickTimer = G.tickTimer + .03
  end
  -- Player is dead? Restart the game
  if G.player.dead then
    juno.onLoad()
  end
end


function juno.onDraw()
  juno.graphics.copyPixels(G.field, 0, 0, nil, G.scale)
end
