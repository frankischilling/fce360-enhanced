function gui()
  local fps = getfps()
  
  -- Display FPS
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "SMB1 Memory Test", 0x3F)
  
  -- Super Mario Bros 1 Memory Addresses
  -- Lives (0x075A)
  local lives = readbyte(0x075A)
  drawtext(4, 24, string.format("Lives: %d", lives), 0x20)
  
  -- Coins (0x075E)
  local coins = readbyte(0x075E)
  drawtext(4, 32, string.format("Coins: %d", coins), 0x37)
  
  -- World/Level (0x075F)
  -- Bits 0-3 = Level (1-4), Bits 4-7 = World (1-8)
  local worldLevel = readbyte(0x075F)
  local world = (worldLevel >> 4) + 1
  local level = (worldLevel & 0x0F) + 1
  drawtext(4, 40, string.format("World %d-%d", world, level), 0x2E)
  
  -- Score (0x07DE = tens of thousands, 0x07DF = thousands, 0x07E0 = hundreds)
  local scoreHigh = readbyte(0x07DE)
  local scoreMid = readbyte(0x07DF)
  local scoreLow = readbyte(0x07E0)
  local score = scoreHigh * 10000 + scoreMid * 100 + scoreLow
  drawtext(4, 48, string.format("Score: %05d", score), 0x29)
  
  -- Power-up state (0x0756)
  -- 0 = Small, 1 = Super, 2 = Fire, 3 = Starman
  local powerUp = readbyte(0x0756)
  local powerUpName = "Small"
  if powerUp == 1 then powerUpName = "Super"
  elseif powerUp == 2 then powerUpName = "Fire"
  elseif powerUp == 3 then powerUpName = "Starman"
  end
  drawtext(4, 56, string.format("Power: %s", powerUpName), 0x16)
  
  -- Player X position (0x006D)
  local playerX = readbyte(0x006D)
  drawtext(4, 64, string.format("Player X: %d", playerX), 0x20)
  
  -- Timer (0x07F8 = hundreds, 0x07F9 = tens, 0x07FA = ones)
  local timerH = readbyte(0x07F8)
  local timerT = readbyte(0x07F9)
  local timerO = readbyte(0x07FA)
  local timer = timerH * 100 + timerT * 10 + timerO
  drawtext(4, 72, string.format("Timer: %03d", timer), 0x26)
  
  -- Draw a simple visual indicator for power-up state
  if powerUp >= 2 then
    -- Draw a small fire icon if Fire or Starman
    fillcircle(240, 50, 3, 0x16)  -- Red circle
    fillcircle(240, 50, 2, 0x27)  -- Orange/yellow inner
  end
  
  -- Status text
  drawtext(4, 220, "readbyte() test - SMB1", 0x20)
end

