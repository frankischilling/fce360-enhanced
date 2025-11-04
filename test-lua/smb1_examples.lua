-- Super Mario Bros 1 - Basic writebyte Examples
-- Uncomment the one you want to use

-- Example 1: Infinite Lives (keep at 99)
function script()
  writebyte(0x075A, 98)  -- 98 = 99 lives displayed
end

-- Example 2: Infinite Coins (keep at 99)
--[[
function script()
  writebyte(0x075E, 99)
end
--]]

-- Example 3: Set Lives to 5
--[[
function script()
  writebyte(0x075A, 4)  -- 4 = 5 lives displayed
end
--]]

-- Example 4: Set Coins to 50
--[[
function script()
  writebyte(0x075E, 50)
end
--]]

-- Example 5: Always Fire Power-up
--[[
function script()
  writebyte(0x0756, 2)  -- 2 = Fire, 1 = Super, 0 = Small
end
--]]

-- Example 6: Always Super Mario
--[[
function script()
  writebyte(0x0756, 1)  -- 1 = Super Mario
end
--]]

