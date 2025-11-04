-- SMB1 getmemorytype() Test Script
-- Shows getmemorytype() working by identifying memory regions

function script()
    -- Test various memory addresses
    local tests = {
        {0x0000, "RAM"},
        {0x07DE, "RAM"},  -- SMB1 score (in RAM)
        {0x075A, "RAM"},  -- SMB1 lives (in RAM)
        {0x1FFF, "RAM"},  -- End of RAM
        {0x2000, "PPU"},  -- PPU registers
        {0x2007, "PPU"},  -- PPU data
        {0x3FFF, "PPU"},  -- PPU mirror end
        {0x4000, "APU"},  -- APU registers
        {0x401F, "APU"},  -- End of APU
        {0x4020, "UNKNOWN"},  -- Expansion ROM area
        {0x6000, "UNKNOWN"},  -- Save RAM area
        {0x7FFF, "UNKNOWN"},  -- Before ROM
        {0x8000, "ROM"},  -- Program ROM start
        {0xC000, "ROM"},  -- Program ROM
        {0xFFFF, "ROM"}   -- End of ROM
    }
    
    local allCorrect = true
    local y = 100
    
    drawtext(4, y, "getmemorytype() Test", 0x2E)
    y = y + 8
    
    for i, test in ipairs(tests) do
        local addr = test[1]
        local expected = test[2]
        local actual = getmemorytype(addr)
        local correct = (actual == expected)
        
        if not correct then
            allCorrect = false
        end
        
        local color = correct and 0x2E or 0x26
        drawtext(4, y, string.format("0x%04X: %s (expected %s)", addr, actual, expected), color)
        y = y + 8
        
        if y > 220 then break end  -- Prevent overflow
    end
    
    if allCorrect then
        drawtext(4, y, "getmemorytype() WORKING!", 0x2E)
    else
        drawtext(4, y, "getmemorytype() FAILED", 0x26)
    end
end

