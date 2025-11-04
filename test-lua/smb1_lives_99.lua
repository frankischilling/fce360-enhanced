function gui()
  local fps = getfps()
  
  -- Display FPS
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "SMB1 Lives: 99 Test", 0x3F)
  
  -- SMB1 stores lives as (displayed_lives - 1) at 0x075A
  -- To display 99 lives, write 98 (0x62) to 0x075A
  local targetLives = 99
  local valueToWrite = targetLives - 1  -- 98 (0x62)
  
  -- Read current lives
  local livesRaw = readbyte(0x075A)
  local displayedLives = livesRaw + 1
  
  -- Only write if not already at target (more efficient)
  if displayedLives ~= targetLives then
    writebyte(0x075A, valueToWrite)
    -- Read back to verify
    livesRaw = readbyte(0x075A)
    displayedLives = livesRaw + 1
  end
  
  -- Display results
  drawtext(4, 24, string.format("Target: %d lives", targetLives), 0x20)
  drawtext(4, 32, string.format("Memory: 0x075A = %d (0x%02X)", livesRaw, livesRaw), 0x2E)
  drawtext(4, 40, string.format("Displayed: %d lives", displayedLives), 0x37)
  
  -- Show current game state
  local coins = readbyte(0x075E)
  local worldLevel = readbyte(0x075F)
  local world = (worldLevel >> 4) + 1
  local level = (worldLevel & 0x0F) + 1
  
  drawtext(4, 52, string.format("Coins: %d", coins), 0x29)
  drawtext(4, 60, string.format("World %d-%d", world, level), 0x2E)
  
  -- Status indicator
  if displayedLives == targetLives then
    drawtext(4, 72, "Status: 99 LIVES ACTIVE!", 0x28)
    -- Draw a small indicator
    fillrect(4, 82, 100, 4, 0x28)
  else
    drawtext(4, 72, string.format("Status: Setting to %d...", targetLives), 0x16)
  end
end

