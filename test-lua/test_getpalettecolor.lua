-- Test script for getpalettecolor() function
-- Displays palette RAM color indices and demonstrates palette reading

local initialized = false
local testIndices = {0, 1, 4, 8, 12, 16, 20, 24, 28, 31}  -- Test various palette RAM indices
local paletteData = {}
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Print initial message once
    if not initialized then
        print("=== getpalettecolor() Test ===")
        print("Gets palette color index from PALRAM (0-31)")
        print("Each entry contains a color index (0-63)")
        print("")
        
        -- Test all indices and store results
        for i = 0, 31 do
            local colorIndex = getpalettecolor(i)
            paletteData[i] = colorIndex
            if i % 8 == 0 then
                print(string.format("PALRAM[%02d]: Color index %02X (%d)", 
                      i, colorIndex, colorIndex))
            end
        end
        print("")
        initialized = true
    end
    
    -- Display header
    drawtext(4, 4, "getpalettecolor() Test", 0x20)
    drawtext(4, 14, "Palette RAM (PALRAM):", 0x2E)
    
    -- Display palette RAM entries and their color indices
    local yPos = 24
    for i, index in ipairs(testIndices) do
        local colorIndex = getpalettecolor(index)
        
        -- Display PALRAM index and color index
        local text = string.format("PALRAM[%2d] = %02X (%2d)", index, colorIndex, colorIndex)
        drawtext(4, yPos, text, 0x29)
        
        -- Get RGB for the color index to show what color it actually is
        local rgb = getcolorrgb(colorIndex)
        drawtext(100, yPos, string.format("RGB(%3d,%3d,%3d)", 
              rgb[1], rgb[2], rgb[3]), 0x37)
        
        -- Draw a color swatch using the actual color
        fillrect(200, yPos, 16, 8, colorIndex)
        
        yPos = yPos + 10
        if yPos > 220 then break end  -- Don't overflow screen
    end
    
    -- Display background palette entries (0x00-0x0F)
    yPos = yPos + 10
    drawtext(4, yPos, "Background Palettes:", 0x2E)
    yPos = yPos + 10
    
    -- Background palette 0: PALRAM[0x01-0x03]
    local bgPal0 = {
        {idx = 0x00, name = "Universal"},
        {idx = 0x01, name = "BG Pal 0-1"},
        {idx = 0x02, name = "BG Pal 0-2"},
        {idx = 0x03, name = "BG Pal 0-3"}
    }
    
    for i, entry in ipairs(bgPal0) do
        local colorIndex = getpalettecolor(entry.idx)
        local rgb = getcolorrgb(colorIndex)
        local text = string.format("%02X %-12s -> %02X RGB(%3d,%3d,%3d)", 
              entry.idx, entry.name, colorIndex, rgb[1], rgb[2], rgb[3])
        drawtext(4, yPos, text, 0x29)
        fillrect(200, yPos, 16, 8, colorIndex)
        yPos = yPos + 10
        if yPos > 220 then break end
    end
    
    -- Display sprite palette entries (0x10-0x1F)
    if yPos < 200 then
        drawtext(4, yPos, "Sprite Palettes:", 0x2E)
        yPos = yPos + 10
        
        -- Sprite palette 0: PALRAM[0x11-0x13]
        local sprPal0 = {
            {idx = 0x10, name = "Universal"},
            {idx = 0x11, name = "SP Pal 0-1"},
            {idx = 0x12, name = "SP Pal 0-2"},
            {idx = 0x13, name = "SP Pal 0-3"}
        }
        
        for i, entry in ipairs(sprPal0) do
            local colorIndex = getpalettecolor(entry.idx)
            local rgb = getcolorrgb(colorIndex)
            local text = string.format("%02X %-12s -> %02X RGB(%3d,%3d,%3d)", 
                  entry.idx, entry.name, colorIndex, rgb[1], rgb[2], rgb[3])
            drawtext(4, yPos, text, 0x29)
            fillrect(200, yPos, 16, 8, colorIndex)
            yPos = yPos + 10
            if yPos > 220 then break end
        end
    end
    
    -- Display frame counter
    drawtext(4, 230, string.format("Frame: %d", frameCounter), 0x2E)
    
    -- Test error handling (only once, at start)
    if frameCounter == 1 then
        print("=== Error Handling Tests ===")
        
        -- Test invalid indices (should error)
        local success, err = pcall(function()
            getpalettecolor(-1)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index -1")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index -1")
        end
        
        success, err = pcall(function()
            getpalettecolor(32)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index 32")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index 32")
        end
        
        -- Test valid boundary indices (should succeed)
        success, err = pcall(function()
            local color = getpalettecolor(0)
            print("✓ Valid index 0: Color " .. string.format("%02X", color))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 0: " .. tostring(err))
        end
        
        success, err = pcall(function()
            local color = getpalettecolor(31)
            print("✓ Valid index 31: Color " .. string.format("%02X", color))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 31: " .. tostring(err))
        end
        
        print("")
    end
    
    -- Display current palette RAM index being analyzed (cycles through)
    local currentIndex = math.floor(frameCounter / 60) % 32  -- Change every 60 frames
    local currentColor = getpalettecolor(currentIndex)
    local currentRGB = getcolorrgb(currentColor)
    drawtext(150, 4, string.format("PALRAM[%02d]", currentIndex), 0x20)
    drawtext(150, 14, string.format("Color: %02X", currentColor), 0x29)
    drawtext(150, 24, string.format("RGB: %d,%d,%d", 
          currentRGB[1], currentRGB[2], currentRGB[3]), 0x37)
    fillrect(150, 34, 32, 16, currentColor)
end

print("getpalettecolor() test script loaded")

