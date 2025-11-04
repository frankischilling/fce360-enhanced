function gui()
  local fps = getfps()
  
  -- Display FPS
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  
  -- Simple readbyte test - read a few memory addresses
  local ram0 = readbyte(0x0000)
  local ram1 = readbyte(0x0001)
  
  drawtext(4, 12, string.format("RAM[0x0000] = %d", ram0), 0x20)
  drawtext(4, 20, string.format("RAM[0x0001] = %d", ram1), 0x20)
end

