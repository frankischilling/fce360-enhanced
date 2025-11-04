function gui()
  local fps = getfps()
  
  -- Display FPS
  
  -- Test with just a simple triangle first
  drawpolygon(50, 30, 100, 60, 20, 60, 0x2E)
  
  -- Status text
  drawtext(4, 200, "drawpolygon() test", 0x20)
end
