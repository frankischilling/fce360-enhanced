function gui()
  -- Just draw text to verify script is running
  drawtext(10, 10, "LUA IS RUNNING", 0x3F)
  drawtext(10, 20, "If you see this", 0x20)
  drawtext(10, 30, "script is loaded", 0x20)
  
  -- Test getfps
  local fps = getfps()
  drawtext(10, 40, string.format("FPS: %.1f", fps), 0x2E)
end

