function script()
  local fps = getfps()
  
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "SMB1 Advanced Memory", 0x3F)
  
  -- Set lives to 99 using writebyte
  writebyte(0x075A, 98)
  
  -- Set coins to 99 using writebyte
  writebyte(0x075E, 99)
  
  -- Set score using writebytes (3 bytes: high, mid, low)
  -- Score of 50000 = (5 * 10000) + (0 * 100) + (0)
  writebytes(0x07DE, 5, 0, 0)
  
  -- Read back and display
  local livesRaw = readbyte(0x075A)
  local lives = livesRaw + 1
  local coins = readbyte(0x075E)
  
  local scoreH = readbyte(0x07DE)
  local scoreM = readbyte(0x07DF)
  local scoreL = readbyte(0x07E0)
  local score = scoreH * 10000 + scoreM * 100 + scoreL
  
  drawtext(4, 24, string.format("Lives: %d", lives), 0x20)
  drawtext(4, 32, string.format("Coins: %d", coins), 0x37)
  drawtext(4, 40, string.format("Score: %d", score), 0x29)
  
  -- Test writeword on a safe RAM address
  writeword(0x0100, 12345)
  local wordValue = readbyte(0x0100) + (readbyte(0x0101) * 256)
  drawtext(4, 52, string.format("Word test: %d", wordValue), 0x2E)
end

