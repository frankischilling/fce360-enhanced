-- Test script for setpalettecolor() function
-- Makes Mario in Super Mario Bros. 1 have crazy colors!
-- Cycles through different color schemes for Mario's sprite palette

local initialized = false
local frameCounter = 0
local colorCycle = 0

-- Mario's sprite palette uses PALRAM[0x10-0x13]
-- 0x10 = Universal sprite color (usually transparent/black)
-- 0x11 = Mario's main color (red)
-- 0x12 = Mario's secondary color (skin/white)
-- 0x13 = Mario's accent color (blue overalls)

-- Crazy color schemes for Mario
local colorSchemes = {
    -- Rainbow scheme
    {name = "Rainbow", colors = {0x00, 0x20, 0x37, 0x29}},
    -- Neon scheme
    {name = "Neon", colors = {0x00, 0x16, 0x2A, 0x3B}},
    -- Fire scheme
    {name = "Fire", colors = {0x00, 0x16, 0x27, 0x37}},
    -- Ice scheme
    {name = "Ice", colors = {0x00, 0x21, 0x2C, 0x20}},
    -- Green scheme
    {name = "Green", colors = {0x00, 0x29, 0x2A, 0x39}},
    -- Purple scheme
    {name = "Purple", colors = {0x00, 0x13, 0x24, 0x23}},
    -- Yellow scheme
    {name = "Yellow", colors = {0x00, 0x37, 0x28, 0x27}},
    -- Cyan scheme
    {name = "Cyan", colors = {0x00, 0x2C, 0x2B, 0x21}},
}

local currentScheme = 1
local schemeChangeCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Print initial message once
    if not initialized then
        print("=== setpalettecolor() Test - Crazy Mario Colors ===")
        print("Cycling through color schemes for Mario's sprite palette")
        print("Mario's palette: PALRAM[0x10-0x13]")
        print("")
        initialized = true
    end
    
    -- Change color scheme every 60 frames (~1 second at 60fps)
    if frameCounter % 60 == 0 then
        currentScheme = ((frameCounter / 60) % #colorSchemes) + 1
        schemeChangeCounter = schemeChangeCounter + 1
        
        local scheme = colorSchemes[currentScheme]
        print(string.format("Frame %d: Switching to %s scheme", frameCounter, scheme.name))
    end
    
    -- Apply current color scheme to Mario's sprite palette
    local scheme = colorSchemes[currentScheme]
    
    -- Set Mario's sprite palette colors
    setpalettecolor(0x10, scheme.colors[1])  -- Universal sprite color
    setpalettecolor(0x11, scheme.colors[2])  -- Main color
    setpalettecolor(0x12, scheme.colors[3])  -- Secondary color
    setpalettecolor(0x13, scheme.colors[4])  -- Accent color
    
    -- Display current scheme info on screen
    drawtext(4, 4, "Crazy Mario Colors!", 0x20)
    drawtext(4, 14, string.format("Scheme: %s", scheme.name), 0x29)
    drawtext(4, 24, string.format("Frame: %d", frameCounter), 0x37)
    
    -- Read back the palette colors AFTER setting them (to verify they were set)
    local pal0x10 = getpalettecolor(0x10)
    local pal0x11 = getpalettecolor(0x11)
    local pal0x12 = getpalettecolor(0x12)
    local pal0x13 = getpalettecolor(0x13)
    
    drawtext(4, 34, string.format("PAL[0x10]: %02X", pal0x10), 0x2E)
    drawtext(4, 44, string.format("PAL[0x11]: %02X", pal0x11), 0x2E)
    drawtext(4, 54, string.format("PAL[0x12]: %02X", pal0x12), 0x2E)
    drawtext(4, 64, string.format("PAL[0x13]: %02X", pal0x13), 0x2E)
    
    -- Show color swatches using the actual color indices
    fillrect(100, 34, 16, 8, pal0x10)
    fillrect(100, 44, 16, 8, pal0x11)
    fillrect(100, 54, 16, 8, pal0x12)
    fillrect(100, 64, 16, 8, pal0x13)
    
    -- Get RGB values for each color - read from actual PALRAM values
    local rgb0x11 = getcolorrgb(pal0x11)  -- Main color RGB (read from PALRAM)
    local rgb0x12 = getcolorrgb(pal0x12)  -- Secondary color RGB (read from PALRAM)
    local rgb0x13 = getcolorrgb(pal0x13)  -- Accent color RGB (read from PALRAM)
    
    -- Show RGB values for main color (the one that changes most)
    drawtext(4, 74, string.format("RGB[0x11]: %d,%d,%d", rgb0x11[1], rgb0x11[2], rgb0x11[3]), 0x37)
    drawtext(4, 84, string.format("RGB[0x12]: %d,%d,%d", rgb0x12[1], rgb0x12[2], rgb0x12[3]), 0x29)
    drawtext(4, 94, string.format("RGB[0x13]: %d,%d,%d", rgb0x13[1], rgb0x13[2], rgb0x13[3]), 0x2C)
    
    -- Display instructions
    drawtext(4, 220, "Mario should be cycling colors!", 0x20)
    drawtext(4, 230, string.format("Changes: %d", schemeChangeCounter), 0x2E)
    
    -- Optional: Also cycle background palette for extra craziness
    -- Uncomment these lines to make the background colors change too
    --[[
    local bgColor = ((frameCounter // 30) % 64)
    setpalettecolor(0x01, bgColor)
    setpalettecolor(0x02, (bgColor + 16) % 64)
    setpalettecolor(0x03, (bgColor + 32) % 64)
    --]]
end

print("setpalettecolor() test script loaded - Mario will have crazy colors!")

