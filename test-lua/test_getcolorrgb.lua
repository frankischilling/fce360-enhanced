-- Test script for getcolorrgb() function
-- Displays RGB values for palette colors and demonstrates color analysis

local initialized = false
local testIndices = {0, 1, 16, 32, 48, 63}  -- Test various palette indices
local colorData = {}
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Print initial message once
    if not initialized then
        print("=== getcolorrgb() Test ===")
        print("Gets RGB values for palette colors (0-63)")
        print("")
        
        -- Test all indices and store results
        for i = 0, 63 do
            local rgb = getcolorrgb(i)
            colorData[i] = {r = rgb[1], g = rgb[2], b = rgb[3]}
            if i % 16 == 0 then
                print(string.format("Palette %02X: RGB(%d, %d, %d)", 
                      i, rgb[1], rgb[2], rgb[3]))
            end
        end
        print("")
        initialized = true
    end
    
    -- Display header
    drawtext(4, 4, "getcolorrgb() Test", 0x20)
    drawtext(4, 14, "Palette RGB Values:", 0x2E)
    
    -- Display RGB values for test indices
    local yPos = 24
    for i, index in ipairs(testIndices) do
        local rgb = getcolorrgb(index)
        
        -- Display palette index and RGB values
        local text = string.format("%02X: RGB(%3d, %3d, %3d)", index, rgb[1], rgb[2], rgb[3])
        drawtext(4, yPos, text, 0x29)
        
        -- Draw a color swatch using the palette color
        local swatchX = 120
        local swatchY = yPos
        fillrect(swatchX, swatchY, 16, 8, index)
        drawtext(swatchX + 18, swatchY, string.format("0x%02X", index), 0x37)
        
        yPos = yPos + 10
    end
    
    -- Display some common palette colors with descriptions
    yPos = yPos + 10
    drawtext(4, yPos, "Common Colors:", 0x2E)
    yPos = yPos + 10
    
    local commonColors = {
        {idx = 0x00, name = "Dark Gray"},
        {idx = 0x10, name = "Light Gray"},
        {idx = 0x20, name = "Bright White"},
        {idx = 0x16, name = "Red/Orange"},
        {idx = 0x29, name = "Green"},
        {idx = 0x37, name = "Yellow"},
        {idx = 0x3D, name = "Silver"}
    }
    
    for i, color in ipairs(commonColors) do
        local rgb = getcolorrgb(color.idx)
        local text = string.format("%02X %-12s RGB(%3d,%3d,%3d)", 
              color.idx, color.name, rgb[1], rgb[2], rgb[3])
        drawtext(4, yPos, text, 0x29)
        
        -- Color swatch
        fillrect(140, yPos, 16, 8, color.idx)
        
        yPos = yPos + 10
        if yPos > 220 then break end  -- Don't overflow screen
    end
    
    -- Display frame counter
    drawtext(4, 230, string.format("Frame: %d", frameCounter), 0x2E)
    
    -- Test error handling (only once, at start)
    if frameCounter == 1 then
        print("=== Error Handling Tests ===")
        
        -- Test invalid indices (should error)
        local success, err = pcall(function()
            getcolorrgb(-1)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index -1")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index -1")
        end
        
        success, err = pcall(function()
            getcolorrgb(64)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index 64")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index 64")
        end
        
        -- Test valid boundary indices (should succeed)
        success, err = pcall(function()
            local rgb = getcolorrgb(0)
            print("✓ Valid index 0: RGB(" .. rgb[1] .. ", " .. rgb[2] .. ", " .. rgb[3] .. ")")
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 0: " .. tostring(err))
        end
        
        success, err = pcall(function()
            local rgb = getcolorrgb(63)
            print("✓ Valid index 63: RGB(" .. rgb[1] .. ", " .. rgb[2] .. ", " .. rgb[3] .. ")")
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 63: " .. tostring(err))
        end
        
        print("")
    end
    
    -- Display current palette index being analyzed (cycles through)
    local currentIndex = math.floor(frameCounter / 60) % 64  -- Change every 60 frames
    local currentRGB = getcolorrgb(currentIndex)
    drawtext(150, 4, string.format("Index: 0x%02X", currentIndex), 0x20)
    drawtext(150, 14, string.format("RGB: %d,%d,%d", 
          currentRGB[1], currentRGB[2], currentRGB[3]), 0x29)
    fillrect(150, 24, 32, 16, currentIndex)
end

print("getcolorrgb() test script loaded")

