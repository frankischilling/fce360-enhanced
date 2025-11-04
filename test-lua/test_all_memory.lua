function script()
  local fps = getfps()
  
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "All Memory Functions Test", 0x3F)
  
  -- Test 1: writebyte
  writebyte(0x0040, 42)
  local b1 = readbyte(0x0040)
  drawtext(4, 24, string.format("writebyte: wrote 42, read %d", b1), b1 == 42 and 0x28 or 0x16)
  
  -- Test 2: writeword (little-endian)
  writeword(0x0050, 0xABCD)
  local wLow = readbyte(0x0050)
  local wHigh = readbyte(0x0051)
  local wValue = wLow + (wHigh * 256)
  drawtext(4, 32, string.format("writeword: wrote 0xABCD, read 0x%04X", wValue), wValue == 0xABCD and 0x28 or 0x16)
  
  -- Test 3: writebytes (multiple)
  writebytes(0x0060, 1, 2, 3, 4, 5)
  local bytes = {readbyte(0x0060), readbyte(0x0061), readbyte(0x0062), readbyte(0x0063), readbyte(0x0064)}
  local bytesMatch = (bytes[1] == 1 and bytes[2] == 2 and bytes[3] == 3 and bytes[4] == 4 and bytes[5] == 5)
  drawtext(4, 40, string.format("writebytes: wrote 1,2,3,4,5"), 0x20)
  drawtext(4, 48, string.format("read: %d,%d,%d,%d,%d", bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]), bytesMatch and 0x28 or 0x16)
  
  -- Test 4: Combined operations
  writebytes(0x0070, 0xFF, 0xFE)
  writeword(0x0072, 256)  -- Should write 0x00, 0x01 (little-endian)
  local combo1 = readbyte(0x0070)
  local combo2 = readbyte(0x0071)
  local combo3 = readbyte(0x0072)
  local combo4 = readbyte(0x0073)
  drawtext(4, 60, string.format("Combined: %02X %02X %02X %02X", combo1, combo2, combo3, combo4), 0x2E)
  
  -- Summary
  if b1 == 42 and wValue == 0xABCD and bytesMatch then
    drawtext(4, 72, "ALL TESTS PASSED!", 0x28)
  else
    drawtext(4, 72, "Some tests failed", 0x16)
  end
end

