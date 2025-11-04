-- Test script for drawtriangle and filltriangle with 10 colors from README palette

function gui()
    local startY = 20
    local triangleSize = 18
    local spacing = 45
    local startX = 15
    local outlineOffset = 25  -- Offset between outline and filled versions
    
    -- Row 1: Dark colors (Row 0)
    local y1 = startY
    -- Triangle 1: 0x00 - dark gray (outline and filled)
    drawtriangle(startX, y1, startX + triangleSize, y1 + triangleSize, startX - triangleSize/2, y1 + triangleSize, 0x00)
    filltriangle(startX + outlineOffset, y1, startX + outlineOffset + triangleSize, y1 + triangleSize, startX + outlineOffset - triangleSize/2, y1 + triangleSize, 0x00)
    
    -- Triangle 2: 0x06 - maroon
    drawtriangle(startX + spacing, y1, startX + spacing + triangleSize, y1 + triangleSize, startX + spacing - triangleSize/2, y1 + triangleSize, 0x06)
    filltriangle(startX + spacing + outlineOffset, y1, startX + spacing + outlineOffset + triangleSize, y1 + triangleSize, startX + spacing + outlineOffset - triangleSize/2, y1 + triangleSize, 0x06)
    
    -- Triangle 3: 0x0B - teal-green
    drawtriangle(startX + spacing * 2, y1, startX + spacing * 2 + triangleSize, y1 + triangleSize, startX + spacing * 2 - triangleSize/2, y1 + triangleSize, 0x0B)
    filltriangle(startX + spacing * 2 + outlineOffset, y1, startX + spacing * 2 + outlineOffset + triangleSize, y1 + triangleSize, startX + spacing * 2 + outlineOffset - triangleSize/2, y1 + triangleSize, 0x0B)
    
    -- Row 2: Medium-Dark colors (Row 1)
    local y2 = startY + 50
    -- Triangle 4: 0x10 - light gray
    drawtriangle(startX, y2, startX + triangleSize, y2 + triangleSize, startX - triangleSize/2, y2 + triangleSize, 0x10)
    filltriangle(startX + outlineOffset, y2, startX + outlineOffset + triangleSize, y2 + triangleSize, startX + outlineOffset - triangleSize/2, y2 + triangleSize, 0x10)
    
    -- Triangle 5: 0x16 - red / orange-red
    drawtriangle(startX + spacing, y2, startX + spacing + triangleSize, y2 + triangleSize, startX + spacing - triangleSize/2, y2 + triangleSize, 0x16)
    filltriangle(startX + spacing + outlineOffset, y2, startX + spacing + outlineOffset + triangleSize, y2 + triangleSize, startX + spacing + outlineOffset - triangleSize/2, y2 + triangleSize, 0x16)
    
    -- Triangle 6: 0x1C - cyan
    drawtriangle(startX + spacing * 2, y2, startX + spacing * 2 + triangleSize, y2 + triangleSize, startX + spacing * 2 - triangleSize/2, y2 + triangleSize, 0x1C)
    filltriangle(startX + spacing * 2 + outlineOffset, y2, startX + spacing * 2 + outlineOffset + triangleSize, y2 + triangleSize, startX + spacing * 2 + outlineOffset - triangleSize/2, y2 + triangleSize, 0x1C)
    
    -- Row 3: Medium-Bright colors (Row 2)
    local y3 = startY + 100
    -- Triangle 7: 0x20 - bright white
    drawtriangle(startX, y3, startX + triangleSize, y3 + triangleSize, startX - triangleSize/2, y3 + triangleSize, 0x20)
    filltriangle(startX + outlineOffset, y3, startX + outlineOffset + triangleSize, y3 + triangleSize, startX + outlineOffset - triangleSize/2, y3 + triangleSize, 0x20)
    
    -- Triangle 8: 0x26 - coral red
    drawtriangle(startX + spacing, y3, startX + spacing + triangleSize, y3 + triangleSize, startX + spacing - triangleSize/2, y3 + triangleSize, 0x26)
    filltriangle(startX + spacing + outlineOffset, y3, startX + spacing + outlineOffset + triangleSize, y3 + triangleSize, startX + spacing + outlineOffset - triangleSize/2, y3 + triangleSize, 0x26)
    
    -- Triangle 9: 0x29 - medium bright green
    drawtriangle(startX + spacing * 2, y3, startX + spacing * 2 + triangleSize, y3 + triangleSize, startX + spacing * 2 - triangleSize/2, y3 + triangleSize, 0x29)
    filltriangle(startX + spacing * 2 + outlineOffset, y3, startX + spacing * 2 + outlineOffset + triangleSize, y3 + triangleSize, startX + spacing * 2 + outlineOffset - triangleSize/2, y3 + triangleSize, 0x29)
    
    -- Row 4: Bright colors (Row 3)
    local y4 = startY + 150
    -- Triangle 10: 0x37 - bright yellow
    drawtriangle(startX, y4, startX + triangleSize, y4 + triangleSize, startX - triangleSize/2, y4 + triangleSize, 0x37)
    filltriangle(startX + outlineOffset, y4, startX + outlineOffset + triangleSize, y4 + triangleSize, startX + outlineOffset - triangleSize/2, y4 + triangleSize, 0x37)
end

