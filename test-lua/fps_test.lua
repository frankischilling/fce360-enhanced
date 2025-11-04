-- Enhanced test script to verify all Lua drawing functions
-- Demonstrates: drawtext, drawpixel, drawline, drawrect, fillrect, clearrect, drawtextwh

function gui()
  local fps = getfps()
  
  -- Clear previous frame area (optional - only if you want to redraw everything)
  -- clearrect(0, 0, 256, 240)  -- Uncomment to clear entire screen each frame
  
  -- ===== BASIC TEXT =====
  drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
  drawtext(4, 12, "Drawing API Test", 0x3F)
  
  -- ===== PIXELS =====
  -- Draw some pixels in a pattern
  for i = 0, 10 do
    drawpixel(4 + i * 2, 24 + i, 0x2E)  -- Diagonal line of pixels
  end
  
  -- ===== LINES =====
  -- Draw a crosshair at screen center
  drawline(128, 100, 128, 140, 0x3F)  -- Vertical line
  drawline(108, 120, 148, 120, 0x3F)  -- Horizontal line
  
  -- Draw a box using lines
  drawline(200, 30, 250, 30, 0x2E)    -- Top
  drawline(250, 30, 250, 80, 0x2E)    -- Right
  drawline(250, 80, 200, 80, 0x2E)    -- Bottom
  drawline(200, 80, 200, 30, 0x2E)    -- Left
  
  -- ===== RECTANGLES =====
  -- Draw rectangle outlines
  drawrect(10, 50, 60, 40, 0x20)      -- White outline
  drawrect(80, 50, 60, 40, 0x2E)       -- Yellow/green outline
  drawrect(150, 50, 50, 30, 0x3F)      -- Bright outline
  
  -- ===== FILLED RECTANGLES =====
  -- Draw filled rectangles (useful for bars, backgrounds)
  local barWidth = math.floor((fps / 60.0) * 100)  -- Scale FPS to bar width
  if barWidth > 100 then barWidth = 100 end
  fillrect(10, 100, barWidth, 8, 0x2E)  -- FPS bar that scales with FPS
  drawrect(10, 100, 100, 8, 0x3F)        -- Border around FPS bar
  
  -- Draw a background panel
  fillrect(5, 115, 120, 60, 0x10)       -- Dark background
  drawrect(5, 115, 120, 60, 0x20)        -- White border around panel
  
  -- ===== ENHANCED TEXT WITH BORDER =====
  -- Text with border for better visibility
  drawtextwh(10, 120, "Enhanced Text", 0x2E, 110, 16, 1)  -- Border level 1
  drawtextwh(10, 135, "With Border!", 0x3F, 110, 16, 2)   -- Border level 2 (thicker)
  
  -- ===== MORE VISUAL DEMOS =====
  -- Draw a simple graph/pattern using pixels
  for i = 0, 50 do
    local y = 180 + math.floor(math.sin(i * 0.2) * 15)
    drawpixel(10 + i, y, 0x2E)
  end
  
  -- Draw multiple small rectangles
  for i = 0, 7 do
    local x = 180 + (i % 4) * 18
    local y = 170 + math.floor(i / 4) * 18
    fillrect(x, y, 15, 15, 0x20 + i)  -- Varying colors
    drawrect(x, y, 15, 15, 0x3F)       -- Border on each
  end
  
  -- ===== CLEAR RECT TEST =====
  -- Note: Uncomment to see clearrect in action (will clear a region each frame)
  -- clearrect(200, 100, 50, 40)  -- This would clear a rectangle area
  
  -- Status text at bottom
  drawtext(4, 232, "All Drawing Functions Active", 0x20)
end
