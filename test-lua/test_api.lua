-- Simple Lua API Test Script
function gui()
  local fps = getfps()
  
  -- Basic text (no borders)
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "Clean text", 0x20)
  
  -- Text with borders
  drawtextwh(4, 20, "Border=0", 0x2E, 100, 16, 0)
  drawtextwh(4, 28, "Border=1", 0x2E, 100, 16, 1)
  drawtextwh(4, 36, "Border=2", 0x2E, 100, 16, 2)
  
  -- Simple shapes
  drawline(10, 50, 50, 90, 0x3F)
  drawrect(60, 50, 40, 40, 0x20)
  fillrect(110, 50, 40, 40, 0x2E)
  
  -- Clear and redraw test
  clearrect(10, 100, 80, 20)
  drawtext(12, 102, "Cleared area", 0x20)
end
