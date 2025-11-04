function gui()
  drawtext(4, 4, "SMB1 Lives Finder", 0x3F)
  
  -- Check addresses around 0x075A
  local y = 16
  for i = 0, 15 do
    local addr = 0x0750 + i
    local val = readbyte(addr)
    drawtext(4, y, string.format("0x%04X: %d", addr, val), 0x20)
    y = y + 8
    if y > 200 then break end
  end
  
  -- Check if 0x075A stores (lives - 1)
  local a1 = readbyte(0x075A)
  drawtext(150, 50, string.format("0x075A = %d", a1), 0x2E)
  drawtext(150, 60, "Formula: raw+1", 0x2E)
  drawtext(150, 70, string.format("Lives = %d", a1 + 1), 0x37)
  
  -- Test: if you have 3 lives, this should show 3
  -- If you have 1 life, this should show 1
end

