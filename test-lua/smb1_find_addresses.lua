function gui()
  drawtext(4, 4, "SMB1 Address Finder", 0x3F)
  
  -- Try multiple known addresses for lives
  -- Different ROM versions may use different addresses
  local addr1 = readbyte(0x075A)  -- Common US address
  local addr2 = readbyte(0x001E)  -- Alternative address
  local addr3 = readbyte(0x0765)  -- Another possibility
  
  drawtext(4, 16, string.format("0x075A = %d", addr1), 0x20)
  drawtext(4, 24, string.format("0x001E = %d", addr2), 0x20)
  drawtext(4, 32, string.format("0x0765 = %d", addr3), 0x20)
  
  -- Scan RAM area (0x0000-0x07FF) for value 3 (current lives)
  drawtext(4, 44, "Scanning for value 3...", 0x2E)
  local found = {}
  local count = 0
  for i = 0, 0x7FF, 1 do
    local val = readbyte(i)
    if val == 3 and count < 10 then
      found[count + 1] = string.format("0x%04X = 3", i)
      count = count + 1
    end
  end
  
  -- Show first few matches
  for i = 1, math.min(5, count) do
    drawtext(4, 52 + (i * 8), found[i], 0x37)
  end
  
  if count == 0 then
    drawtext(4, 52, "No value 3 found", 0x16)
  end
end

