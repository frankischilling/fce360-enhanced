-- Simple test script for blendcolors() function
-- Shows clear visual demonstration of color blending

local frameCounter = 0
local ratio = 0.0

-- Simple color pairs for clear blending demonstration
local color1 = 0x16  -- Red
local color2 = 0x29  -- Green

function gui()
    frameCounter = frameCounter + 1
    
    -- Animate ratio smoothly from 0.0 to 1.0 and back
    ratio = ratio + 0.01
    if ratio > 1.0 then
        ratio = 0.0
    end
    
    -- Get the blended color
    local blendedColor = blendcolors(color1, color2, ratio)
    
    -- Get RGB values for display
    local rgb1 = getcolorrgb(color1)
    local rgb2 = getcolorrgb(color2)
    local rgbBlended = getcolorrgb(blendedColor)
    
    -- Title
    drawtext(4, 4, "Simple Color Blending Test", 0x20)
    
    -- Show Color 1 (Red)
    drawtext(4, 20, "Color 1 (Red):", 0x29)
    fillrect(4, 30, 60, 60, color1)
    drawtext(4, 92, string.format("Index: 0x%02X", color1), 0x29)
    drawtext(4, 102, string.format("RGB: %d,%d,%d", rgb1[1], rgb1[2], rgb1[3]), 0x29)
    
    -- Show Blended Color (in the middle)
    drawtext(80, 20, string.format("Blended (ratio: %.2f)", ratio), 0x2E)
    fillrect(80, 30, 60, 60, blendedColor)
    drawtext(80, 92, string.format("Index: 0x%02X", blendedColor), 0x2E)
    drawtext(80, 102, string.format("RGB: %d,%d,%d", rgbBlended[1], rgbBlended[2], rgbBlended[3]), 0x2E)
    
    -- Show Color 2 (Green)
    drawtext(156, 20, "Color 2 (Green):", 0x37)
    fillrect(156, 30, 60, 60, color2)
    drawtext(156, 92, string.format("Index: 0x%02X", color2), 0x37)
    drawtext(156, 102, string.format("RGB: %d,%d,%d", rgb2[1], rgb2[2], rgb2[3]), 0x37)
    
    -- Show a gradient bar below
    drawtext(4, 120, "Gradient Bar (shows all blend ratios):", 0x2E)
    for i = 0, 255 do
        local gradRatio = i / 255.0
        local gradColor = blendcolors(color1, color2, gradRatio)
        fillrect(4 + i, 130, 1, 30, gradColor)
    end
    
    -- Show current position on gradient
    local markerX = 4 + math.floor(ratio * 255)
    fillrect(markerX - 2, 128, 5, 34, 0x20)  -- White marker
    
    -- Show ratio as percentage
    drawtext(4, 168, string.format("Blend Ratio: %.1f%% (%.2f)", ratio * 100, ratio), 0x2E)
    
    -- Show what happens at different ratios
    drawtext(4, 188, "Blend Examples:", 0x2E)
    local examples = {
        {ratio = 0.0, label = "0% (all Color 1)"},
        {ratio = 0.25, label = "25%"},
        {ratio = 0.5, label = "50% (halfway)"},
        {ratio = 0.75, label = "75%"},
        {ratio = 1.0, label = "100% (all Color 2)"}
    }
    
    local yPos = 198
    for i, ex in ipairs(examples) do
        local exColor = blendcolors(color1, color2, ex.ratio)
        drawtext(4, yPos, ex.label .. ":", 0x29)
        fillrect(100, yPos, 20, 12, exColor)
        drawtext(125, yPos, string.format("-> 0x%02X", exColor), 0x37)
        yPos = yPos + 14
    end
    
    -- Instructions
    drawtext(4, 230, "Watch the middle color change as it blends!", 0x2E)
end

print("Simple blendcolors() test loaded")
print("Watch the middle color blend between Red and Green")

