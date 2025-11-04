function script()
  local fps = getfps()
  
  drawtext(4, 20, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 38, "writeword() Test", 0x3F)
  
  -- Test writeword: Write a 16-bit value
  -- Example: Write to a RAM address (using safe RAM area)
  local testValue = 0x1234
  writeword(0x0010, testValue)
  
  -- Read back to verify (little-endian)
  local low = readbyte(0x0010)
  local high = readbyte(0x0011)
  local readValue = low + (high * 256)
  
  drawtext(4, 50, string.format("Wrote: 0x%04X (%d)", testValue, testValue), 0x20)
  drawtext(4, 58, string.format("Low byte: 0x%02X (%d)", low, low), 0x2E)
  drawtext(4, 66, string.format("High byte: 0x%02X (%d)", high, high), 0x2E)
  drawtext(4, 74, string.format("Read back: 0x%04X", readValue), 0x37)
  
  if readValue == testValue then
    drawtext(4, 80, "SUCCESS: writeword works!", 0x28)
  else
    drawtext(4, 80, "ERROR: Values don't match", 0x16)
  end
  
  -- Test with a simple value
  writeword(0x0020, 42)
  local simple = readbyte(0x0020) + (readbyte(0x0021) * 256)
  drawtext(4, 92, string.format("Simple test (42): %d", simple), 0x20)
end

