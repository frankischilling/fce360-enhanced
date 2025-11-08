-- Test script for drawtextrotated function
-- Demonstrates text drawing at various rotation angles
-- Angle range: 0-360 degrees

-- Frame counter for animation (persists between calls)
local frameCount = 0

function gui()
  local centerX = 128
  local centerY = 120
  
  -- Increment frame counter each call
  frameCount = frameCount + 1
  
  -- Title
  drawtext(4, 4, "drawtextrotated Test", 0x3F)
  
  -- Draw text at cardinal directions
  drawtextrotated(centerX, centerY - 40, "0", 0x2E, 0)      -- 0 degrees (normal)
  drawtextrotated(centerX + 40, centerY, "90", 0x39, 90)     -- 90 degrees (down)
  drawtextrotated(centerX, centerY + 40, "180", 0x0F, 180)  -- 180 degrees (upside down)
  drawtextrotated(centerX - 40, centerY, "270", 0x3C, 270)   -- 270 degrees (up)
  
  -- Draw at 45 degree increments
  drawtextrotated(centerX + 30, centerY - 30, "45", 0x2A, 45)
  drawtextrotated(centerX + 30, centerY + 30, "135", 0x2A, 135)
  drawtextrotated(centerX - 30, centerY + 30, "225", 0x2A, 225)
  drawtextrotated(centerX - 30, centerY - 30, "315", 0x2A, 315)
  
  -- Animated rotating text in center
  -- Rotate 1 degree per frame (360 frames = full rotation)
  local angle = frameCount % 360
  drawtextrotated(centerX, centerY, "SPIN", 0x0F, angle)
  
  -- Draw a reference line pointing right (0 degrees) for comparison
  drawline(centerX, centerY, centerX + 20, centerY, 0x0F)
  
  -- Status
  drawtext(4, 232, string.format("Angle: %d", angle), 0x20)
end

