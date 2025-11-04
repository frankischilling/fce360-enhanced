function script()
  local fps = getfps()
  
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "writebytes() Test", 0x3F)
  
  -- Test writebytes: Write multiple bytes
  -- Write 5 bytes starting at address 0x0030
  writebytes(0x0030, 10, 20, 30, 40, 50)
  
  -- Read back to verify
  local v1 = readbyte(0x0030)
  local v2 = readbyte(0x0031)
  local v3 = readbyte(0x0032)
  local v4 = readbyte(0x0033)
  local v5 = readbyte(0x0034)
  
  drawtext(4, 24, string.format("Wrote: 10, 20, 30, 40, 50"), 0x20)
  drawtext(4, 32, string.format("Read: %d, %d, %d, %d, %d", v1, v2, v3, v4, v5), 0x2E)
  
  local success = (v1 == 10 and v2 == 20 and v3 == 30 and v4 == 40 and v5 == 50)
  if success then
    drawtext(4, 44, "SUCCESS: writebytes works!", 0x28)
  else
    drawtext(4, 44, "ERROR: Values don't match", 0x16)
  end
  
  -- Test SMB1 score writing (3 bytes)
  writebytes(0x07DE, 0, 0, 75)
  local scoreH = readbyte(0x07DE)
  local scoreM = readbyte(0x07DF)
  local scoreL = readbyte(0x07E0)
  local score = scoreH * 10000 + scoreM * 100 + scoreL
  drawtext(4, 56, string.format("SMB1 Score test: %d", score), 0x37)
end

