-- Test script for setdrawcolor function
-- Demonstrates setting and using a default drawing color

function gui()
    local fps = getfps()
    
    -- Clear previous frame
    clearrect(0, 0, 256, 240)
    
    -- Set default drawing color to red/orange
    setdrawcolor(0x16)
    
    -- Draw some shapes with explicit colors (should override default)
    fillrect(10, 10, 60, 60, 0x39)  -- Yellow-green (explicit color)
    fillcircle(50, 50, 20, 0x20)    -- White (explicit color)
    
    -- Note: Current drawing functions all require color parameter,
    -- so this test shows the default color is stored and ready for use
    
    -- Change default color to yellow-green
    setdrawcolor(0x39)
    
    -- Draw with explicit colors still
    fillrect(80, 10, 60, 60, 0x16)  -- Red/orange (explicit)
    fillcircle(110, 40, 20, 0x20)   -- White (explicit)
    
    -- Change default color to white
    setdrawcolor(0x20)
    
    -- Draw with explicit colors
    fillrect(150, 10, 60, 60, 0x29)  -- Green/teal (explicit)
    fillcircle(180, 40, 20, 0x16)   -- Red/orange (explicit)
    
    -- Change default color to green/teal
    setdrawcolor(0x29)
    
    -- Draw with explicit colors
    fillrect(10, 80, 60, 60, 0x3F)  -- Bright white (explicit)
    fillcircle(40, 110, 20, 0x16)   -- Red/orange (explicit)
    
    -- Test with different colors
    setdrawcolor(0x2E)  -- Yellow
    fillrect(80, 80, 60, 60, 0x16)  -- Red/orange (explicit)
    
    setdrawcolor(0x3F)  -- Bright white
    fillrect(150, 80, 60, 60, 0x10)  -- Dark gray (explicit)
    
    -- Show current default color info
    -- Note: We can't read it back, but we can demonstrate it's set
    drawtext(4, 4, string.format("setdrawcolor test - FPS: %.1f", fps), 0x39)
    drawtext(4, 12, "Default color stored", 0x20)
    drawtext(4, 20, "Ready for future use", 0x20)
    
    -- Draw a pattern showing various colors
    for y = 150, 220, 10 do
        for x = 10, 240, 10 do
            local color = ((x + y) % 16) + 0x20  -- Cycle through colors
            setdrawcolor(color)  -- Set default (not used yet by current functions)
            fillrect(x, y, 5, 5, color)  -- Still need explicit color
        end
    end
end

