function gui()
  -- Basic test - verify script loads
  drawtext(10, 10, "SMB1 HUD SCRIPT", 0x3F)
  
  local fps = getfps()
  drawtext(10, 20, string.format("FPS: %.1f", fps), 0x2E)
  
  -- Lives at 0x075A (stored as lives-1, so add 1)
  local livesRaw = readbyte(0x075A)
  local lives = livesRaw + 1  -- Add 1 because memory stores (lives - 1)
  drawtext(10, 30, string.format("Lives: %d (raw=%d)", lives, livesRaw), 0x20)
  
  -- Try reading from RAM (always safe)
  local ram0 = readbyte(0x0000)
  drawtext(10, 40, string.format("RAM[0]=%d", ram0), 0x20)
  
  -- Try SMB1 addresses
  local coins = readbyte(0x075E)
  drawtext(10, 50, string.format("Coins: %d", coins), 0x37)
  
  local worldLevel = readbyte(0x075F)
  local world = (worldLevel >> 4) + 1
  local level = (worldLevel & 0x0F) + 1
  drawtext(10, 60, string.format("World %d-%d", world, level), 0x2E)
end

