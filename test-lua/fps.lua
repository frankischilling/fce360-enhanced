function gui()
  -- Try to get FPS (might be 0 at first)
  local fps = getfps()
  drawtext(6, 170, string.format("FPS: %.1f", fps), 0x2E)
end