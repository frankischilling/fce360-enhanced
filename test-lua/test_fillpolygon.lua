function gui()
  local fps = getfps()
  
  -- Display FPS
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "FillPolygon Test", 0x3F)
  
  -- Test filled square
  fillpolygon(50, 50, 100, 50, 100, 100, 50, 100, 0x20)
  
  -- Test filled pentagon
  fillpolygon(128, 30, 148, 60, 128, 90, 108, 60, 118, 30, 0x2E)
  
  -- Test filled star (10 points)
  fillpolygon(
    128, 20,   -- Top
    132, 50,   -- Top-right outer
    160, 50,   -- Right outer
    138, 70,   -- Right inner
    148, 100,  -- Bottom-right outer
    128, 80,   -- Bottom
    108, 100,  -- Bottom-left outer
    118, 70,   -- Left inner
    96, 50,    -- Left outer
    124, 50,   -- Top-left outer
    0x37
  )
  
  -- Test filled hexagon
  local cx, cy, radius = 200, 120, 25
  fillpolygon(
    cx, cy - radius,                    -- Top
    cx + radius * 0.866, cy - radius * 0.5,  -- Top-right
    cx + radius * 0.866, cy + radius * 0.5,  -- Bottom-right
    cx, cy + radius,                    -- Bottom
    cx - radius * 0.866, cy + radius * 0.5,  -- Bottom-left
    cx - radius * 0.866, cy - radius * 0.5,   -- Top-left
    0x29
  )
  
  -- Status text
  drawtext(4, 220, "fillpolygon() test", 0x20)
end

