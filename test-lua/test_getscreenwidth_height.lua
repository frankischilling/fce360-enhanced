-- Test script for getscreenwidth() and getscreenheight() functions
-- Demonstrates dynamic positioning and centering using screen dimensions

local initialized = false
local centerX = 0
local centerY = 0
local cornerPositions = {}

function gui()
    local width = getscreenwidth()
    local height = getscreenheight()
    
    -- Print initial message once
    if not initialized then
        print("=== getscreenwidth() and getscreenheight() Test ===")
        print(string.format("Screen dimensions: %d x %d pixels", width, height))
        print("")
        initialized = true
    end
    
    -- Calculate center position
    centerX = width / 2
    centerY = height / 2
    
    -- Calculate corner positions
    cornerPositions = {
        topLeft = {x = 0, y = 0},
        topRight = {x = width - 1, y = 0},
        bottomLeft = {x = 0, y = height - 1},
        bottomRight = {x = width - 1, y = height - 1},
        center = {x = centerX, y = centerY}
    }
    
    -- Display screen dimensions
    drawtext(4, 4, "Screen Dimensions:", 0x20)
    drawtext(4, 14, string.format("Width: %d px", width), 0x29)
    drawtext(4, 24, string.format("Height: %d px", height), 0x37)
    
    -- Display center position
    drawtext(4, 34, string.format("Center: (%d, %d)", centerX, centerY), 0x2E)
    
    -- Draw markers at corners and center
    -- Top-left corner
    drawtext(cornerPositions.topLeft.x, cornerPositions.topLeft.y, "TL", 0x2D)
    
    -- Top-right corner (adjust for text width)
    local trText = "TR"
    local trX = cornerPositions.topRight.x - gettextwidth(trText)
    drawtext(trX, cornerPositions.topRight.y, trText, 0x2D)
    
    -- Bottom-left corner
    drawtext(cornerPositions.bottomLeft.x, cornerPositions.bottomLeft.y - 8, "BL", 0x2D)
    
    -- Bottom-right corner
    local brText = "BR"
    local brX = cornerPositions.bottomRight.x - gettextwidth(brText)
    drawtext(brX, cornerPositions.bottomRight.y - 8, brText, 0x2D)
    
    -- Center marker
    local centerText = "C"
    local centerTextX = centerX - (gettextwidth(centerText) / 2)
    local centerTextY = centerY - 4
    drawtext(centerTextX, centerTextY, centerText, 0x2D)
    
    -- Draw a border around the screen edges
    -- Top edge
    for x = 0, width - 1, 4 do
        drawtext(x, 0, ".", 0x37)
    end
    
    -- Bottom edge
    for x = 0, width - 1, 4 do
        drawtext(x, height - 1, ".", 0x37)
    end
    
    -- Left edge
    for y = 0, height - 1, 4 do
        drawtext(0, y, ".", 0x37)
    end
    
    -- Right edge
    for x = 0, height - 1, 4 do
        drawtext(width - 1, x, ".", 0x37)
    end
    
    -- Display corner coordinates
    drawtext(4, 44, string.format("TL: (0, 0)", width, height), 0x2E)
    drawtext(4, 54, string.format("TR: (%d, 0)", width - 1), 0x2E)
    drawtext(4, 64, string.format("BL: (0, %d)", height - 1), 0x2E)
    drawtext(4, 74, string.format("BR: (%d, %d)", width - 1, height - 1), 0x2E)
    
    -- Example: Centered text
    local centeredText = "CENTERED"
    local textWidth = gettextwidth(centeredText)
    local textX = centerX - (textWidth / 2)
    drawtext(textX, centerY + 10, centeredText, 0x29)
    
    -- Example: Right-aligned text
    local rightText = "RIGHT"
    local rightTextX = width - gettextwidth(rightText) - 4
    drawtext(rightTextX, 4, rightText, 0x37)
    
    -- Example: Bottom-aligned text
    local bottomText = "BOTTOM"
    drawtext(4, height - 12, bottomText, 0x37)
    
    -- Show frame count for reference
    local frame = getframecount()
    drawtext(4, height - 22, string.format("Frame: %d", frame), 0x2E)
end

print("getscreenwidth() and getscreenheight() test script loaded")

