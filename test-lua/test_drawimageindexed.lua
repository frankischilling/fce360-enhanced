-- Test script for drawimageindexed function
-- Draws sprites using indexed palette mapping

-- Define a 4-color palette
local palette1 = {0x20, 0x2E, 0x16, 0x3F}  -- White, Yellow/green, Red/orange, Bright

-- 8x8 sprite data using palette indices (1-4, Lua 1-based)
-- Index 1 = white, Index 2 = yellow/green, Index 3 = red/orange, Index 4 = bright
local spriteData = {
    1, 2, 1, 2, 1, 2, 1, 2,  -- Row 1
    2, 1, 2, 1, 2, 1, 2, 1,  -- Row 2
    1, 2, 1, 2, 1, 2, 1, 2,  -- Row 3
    2, 1, 2, 1, 2, 1, 2, 1,  -- Row 4
    1, 2, 1, 2, 1, 2, 1, 2,  -- Row 5
    2, 1, 2, 1, 2, 1, 2, 1,  -- Row 6
    1, 2, 1, 2, 1, 2, 1, 2,  -- Row 7
    2, 1, 2, 1, 2, 1, 2, 1   -- Row 8
}

-- Different palette for same sprite data
local palette2 = {0x0F, 0x16, 0x26, 0x37}  -- Different color scheme

function gui()
    -- Draw sprite with palette1
    drawimageindexed(10, 10, spriteData, palette1, 8, 8)
    
    -- Draw same sprite data with palette2 (different colors)
    drawimageindexed(100, 10, spriteData, palette2, 8, 8)
    
    -- Draw with a different palette
    local palette3 = {0x00, 0x10, 0x20, 0x3F}
    drawimageindexed(190, 10, spriteData, palette3, 8, 8)
    
    -- Test with a 4x4 sprite and 2-color palette
    local smallPalette = {0x20, 0x16}
    local smallSprite = {
        1, 2, 1, 2,
        2, 1, 2, 1,
        1, 2, 1, 2,
        2, 1, 2, 1
    }
    drawimageindexed(10, 100, smallSprite, smallPalette, 4, 4)
    
    -- Show FPS for reference
    local fps = getfps()
    drawtext(4, 4, string.format("drawimageindexed test - FPS: %.1f", fps), 0x2E)
end

