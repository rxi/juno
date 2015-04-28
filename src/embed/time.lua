--
-- Copyright (c) 2015 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--


local last = 0
local delta = 0
local average = 0
local avgTimer = 0
local avgAcc = 1
local avgCount = 1


function juno.time.step()
  local now = juno.time.getTime()
  if last == 0 then last = now end
  delta = now - last
  last = now
  avgTimer = avgTimer - delta
  avgAcc = avgAcc + delta
  avgCount = avgCount + 1
  if avgTimer <= 0 then
    average = avgAcc / avgCount
    avgTimer = avgTimer + 1
    avgCount = 0
    avgAcc = 0
  end
end


function juno.time.getDelta()
  return delta
end


function juno.time.getAverage()
  return average
end


function juno.time.getFps()
  return math.floor(1 / average + .5)
end

