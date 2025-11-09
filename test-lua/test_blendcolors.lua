-- Test script for blendcolors() function
-- Demonstrates color blending and gradient effects

local initialized = false
local frameCounter = 0

-- Test color pairs for blending
local colorPairs = {
    {name = "Red to White", c1 = 0x16, c2 = 0x20},
    {name = "Green to Yellow", c1 = 0x29, c2 = 0x37},
    {name = "Blue to Cyan", c1 = 0x21, c2 = 0x2C},
    {name = "Black to White", c1 = 0x00, c2 = 0x20},
    {name = "Red to Green", c1 = 0x16, c2 = 0x29},
}

local currentPair = 1
local ratio = 0.0
local ratioDirection = 1  -- 1 for increasing, -1 for decreasing

function gui()
    frameCounter = frameCounter + 1
    
    -- Print initial message once
    if not initialized then
        print("=== blendcolors() Test ===")
        print("Blends two palette colors and returns closest matching color index")
        print("")
        
        -- Test basic blending
        print("Testing basic color blending:")
        local test1 = blendcolors(0x16, 0x20, 0.0)  -- 100% red
        local test2 = blendcolors(0x16, 0x20, 1.0)  -- 100% white
        local test3 = blendcolors(0x16, 0x20, 0.5)  -- 50/50 blend
        
        print(string.format("blendcolors(0x16, 0x20, 0.0) = %02X (should be 0x16)", test1))
        print(string.format("blendcolors(0x16, 0x20, 1.0) = %02X (should be 0x20)", test2))
        print(string.format("blendcolors(0x16, 0x20, 0.5) = %02X (50/50 blend)", test3))
        print("")
        
        initialized = true
    end
    
    -- Animate ratio for current color pair
    ratio = ratio + (0.01 * ratioDirection)
    if ratio >= 1.0 then
        ratio = 1.0
        ratioDirection = -1
    elseif ratio <= 0.0 then
        ratio = 0.0
        ratioDirection = 1
    end
    
    -- Change color pair every 300 frames
    if frameCounter % 300 == 0 then
        currentPair = (math.floor(frameCounter / 300) % #colorPairs) + 1
    end
    
    local pair = colorPairs[currentPair]
    
    -- Get blended color
    local blendedColor = blendcolors(pair.c1, pair.c2, ratio)
    
    -- Display header
    drawtext(4, 4, "blendcolors() Test", 0x20)
    drawtext(4, 14, string.format("Pair: %s", pair.name), 0x29)
    drawtext(4, 24, string.format("Ratio: %.2f", ratio), 0x37)
    
    -- Display color information
    local rgb1 = getcolorrgb(pair.c1)
    local rgb2 = getcolorrgb(pair.c2)
    local rgbBlended = getcolorrgb(blendedColor)
    
    drawtext(4, 34, string.format("Color1 (0x%02X): RGB(%3d,%3d,%3d)", 
          pair.c1, rgb1[1], rgb1[2], rgb1[3]), 0x29)
    fillrect(180, 34, 24, 12, pair.c1)
    
    drawtext(4, 46, string.format("Color2 (0x%02X): RGB(%3d,%3d,%3d)", 
          pair.c2, rgb2[1], rgb2[2], rgb2[3]), 0x37)
    fillrect(180, 46, 24, 12, pair.c2)
    
    drawtext(4, 58, string.format("Blended (0x%02X): RGB(%3d,%3d,%3d)", 
          blendedColor, rgbBlended[1], rgbBlended[2], rgbBlended[3]), 0x2E)
    fillrect(180, 58, 24, 12, blendedColor)
    
    -- Show gradient effect
    drawtext(4, 74, "Gradient (0.0 to 1.0):", 0x2E)
    local gradientY = 84
    for i = 0, 15 do
        local gradRatio = i / 15.0
        local gradColor = blendcolors(pair.c1, pair.c2, gradRatio)
        fillrect(4 + (i * 16), gradientY, 14, 14, gradColor)
    end
    
    -- Show ratio slider visualization with actual blended colors
    drawtext(4, 104, "Blend Gradient (showing actual blended colors):", 0x2E)
    local sliderX = 4 + math.floor(ratio * 240)
    -- Draw gradient of actual blended colors
    for i = 0, 239 do
        local gradRatio = i / 239.0
        local gradColor = blendcolors(pair.c1, pair.c2, gradRatio)
        fillrect(4 + i, 114, 1, 16, gradColor)
    end
    -- Draw indicator line
    fillrect(sliderX, 112, 1, 20, 0x20)  -- White indicator line
    -- Draw indicator box
    fillrect(sliderX - 3, 110, 7, 4, 0x20)  -- Top indicator
    fillrect(sliderX - 3, 128, 7, 4, 0x20)  -- Bottom indicator
    
    -- Display multiple blend ratios
    drawtext(4, 136, "Blend Ratios:", 0x2E)
    local testRatios = {0.0, 0.25, 0.5, 0.75, 1.0}
    local yPos = 146
    for i, testRatio in ipairs(testRatios) do
        local testBlend = blendcolors(pair.c1, pair.c2, testRatio)
        drawtext(4, yPos, string.format("%.2f -> %02X", testRatio, testBlend), 0x29)
        fillrect(80, yPos, 16, 10, testBlend)
        yPos = yPos + 12
    end
    
    -- Display frame counter
    drawtext(4, 230, string.format("Frame: %d", frameCounter), 0x2E)
    
    -- Test error handling (only once, at start)
    if frameCounter == 1 then
        print("=== Error Handling Tests ===")
        
        -- Test invalid color indices
        local success, err = pcall(function()
            blendcolors(-1, 0x20, 0.5)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid color1 -1")
        else
            print("✗ ERROR: Should have failed for color1 -1")
        end
        
        success, err = pcall(function()
            blendcolors(0x20, 64, 0.5)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid color2 64")
        else
            print("✗ ERROR: Should have failed for color2 64")
        end
        
        -- Test invalid ratio
        success, err = pcall(function()
            blendcolors(0x16, 0x20, 1.5)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid ratio 1.5")
        else
            print("✗ ERROR: Should have failed for ratio 1.5")
        end
        
        -- Test valid boundary cases
        success, err = pcall(function()
            local result = blendcolors(0x16, 0x20, 0.0)
            print("✓ Valid blend (ratio 0.0): " .. string.format("%02X", result))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for ratio 0.0")
        end
        
        success, err = pcall(function()
            local result = blendcolors(0x16, 0x20, 1.0)
            print("✓ Valid blend (ratio 1.0): " .. string.format("%02X", result))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for ratio 1.0")
        end
        
        print("")
    end
end

print("blendcolors() test script loaded")

