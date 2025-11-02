function gui()
  local x0, y0 = 8, 8
  local w, h = 8, 8
  local i = 0
  for row = 0, 3 do
    for col = 0, 15 do
      local idx = 0x80 + i  -- full overlay block
      local x = x0 + col * (w + 1)
      local y = y0 + row * (h + 1)
      fillrect(x, y, w, h, idx)
      i = i + 1
    end
  end
  drawtext(8, y0 + 4*(h+1) + 6, "Overlay palette 0x80–0xBF", 0x8F)
end
