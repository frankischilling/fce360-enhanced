-- Test script for Row 0 - Dark Colors (0x00-0x0F)
-- Displays the 16 dark colors from the first row of the NES palette

function gui()
    -- Palette grid settings
    local x0, y0 = 8, 8
    local w, h = 14, 14
    local spacing = 2
    
    -- Draw Row 0 colors (0x00-0x0F)
    for col = 0, 15 do
        local idx = col  -- Colors 0x00 to 0x0F
        local x = x0 + col * (w + spacing)
        local y = y0
        
        -- Draw filled rectangle for each color
        fillrect(x, y, w, h, idx)
        
        -- Draw border around each color swatch
        drawrect(x, y, w, h, 0x20)  -- White border for visibility
    end
    
    -- Title text below the grid
    local gridBottom = y0 + h + 10
    drawtext(8, gridBottom, "Row 0 - Dark Colors (0x00-0x0F)", 0x20)
    
    -- Color labels (show hex values)
    local labelY = gridBottom + 12
    for col = 0, 15 do
        local x = x0 + col * (w + spacing) + 2
        local hexStr = string.format("%02X", col)
        drawtext(x, labelY, hexStr, 0x20)
    end
    
    -- Color descriptions
    local descY = labelY + 12
    drawtext(8, descY, "0x00=dark gray  0x01=deep blue  0x02=dark blue  0x03=navy blue", 0x20)
    local descY2 = descY + 8
    drawtext(8, descY2, "0x04=dark purple  0x05=dark red-purple  0x06=maroon  0x07=very dark red", 0x20)
    local descY3 = descY2 + 8
    drawtext(8, descY3, "0x08=brown  0x09=deep green  0x0A=dark green  0x0B=teal-green", 0x20)
    local descY4 = descY3 + 8
    drawtext(8, descY4, "0x0C=dark cyan-blue  0x0D-0x0F=black (transparent)", 0x20)
end

