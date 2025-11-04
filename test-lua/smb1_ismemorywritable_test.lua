-- SMB1 ismemorywritable() Test Script
-- Shows ismemorywritable() working by checking writability of addresses

function script()
    -- Test various memory addresses
    local tests = {
        {0x0000, true, "RAM start"},
        {0x07DE, true, "RAM (score)"},
        {0x075A, true, "RAM (lives)"},
        {0x1FFF, true, "RAM end"},
        {0x2000, true, "PPU start"},
        {0x2007, true, "PPU data"},
        {0x3FFF, true, "PPU end"},
        {0x4000, true, "APU start"},
        {0x401F, true, "APU end"},
        {0x4020, false, "UNKNOWN start"},
        {0x6000, false, "Save RAM area"},
        {0x7FFF, false, "Before ROM"},
        {0x8000, false, "ROM start"},
        {0xC000, false, "ROM"},
        {0xFFFF, false, "ROM end"}
    }
    
    local allCorrect = true
    local y = 100
    
    drawtext(4, y, "ismemorywritable() Test", 0x2E)
    y = y + 8
    
    for i, test in ipairs(tests) do
        local addr = test[1]
        local expected = test[2]
        local desc = test[3]
        local actual = ismemorywritable(addr)
        local correct = (actual == expected)
        
        if not correct then
            allCorrect = false
        end
        
        local color = correct and 0x2E or 0x26
        local resultStr = actual and "true" or "false"
        local expectedStr = expected and "true" or "false"
        drawtext(4, y, string.format("0x%04X (%s): %s (expected %s)", 
            addr, desc, resultStr, expectedStr), color)
        y = y + 8
        
        if y > 220 then break end
    end
    
    if allCorrect then
        drawtext(4, y, "ismemorywritable() WORKING!", 0x2E)
    else
        drawtext(4, y, "ismemorywritable() FAILED", 0x26)
    end
    
    -- Test validation before write
    local addr = 0x07DE
    if ismemorywritable(addr) then
        local before = readbyte(addr)
        writebyte(addr, 99)
        local after = readbyte(addr)
        drawtext(4, y + 8, string.format("Write test: %d->%d", before, after), 0x2A)
    end
end

