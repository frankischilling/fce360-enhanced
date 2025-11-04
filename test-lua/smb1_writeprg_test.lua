-- SMB1 writeprg() Test Script
-- Shows writeprg() attempting to write to program ROM
-- Note: Most ROM is read-only, so writes may be ignored by the mapper

function script()
    -- Test 1: Attempt to write to program ROM
    -- Read before write
    local before = readbyte(0x8000)
    
    -- Attempt to write (may be ignored if ROM is read-only)
    writeprg(0x8000, 0xFF)
    
    -- Read after write
    local after = readbyte(0x8000)
    
    -- Display results
    drawtext(4, 100, "writeprg() Test", 0x2E)
    drawtext(4, 108, string.format("ROM 0x8000 before: 0x%02X", before), 0x20)
    drawtext(4, 116, string.format("ROM 0x8000 after:  0x%02X", after), 0x20)
    
    if before == after then
        drawtext(4, 124, "ROM write ignored (read-only)", 0x2A)
    else
        drawtext(4, 124, "ROM write succeeded!", 0x2E)
    end
    
    -- Test 2: Try different ROM addresses
    writeprg(0xC000, 0xAA)
    writeprg(0xFFFF, 0x55)
    
    local addr1 = readbyte(0xC000)
    local addr2 = readbyte(0xFFFF)
    
    drawtext(4, 132, string.format("0xC000: 0x%02X, 0xFFFF: 0x%02X", addr1, addr2), 0x2A)
    
    -- Note: This function is for mapper-specific operations
    -- Most standard ROM is read-only, but some mappers support ROM writes
    drawtext(4, 140, "Note: ROM writes may be ignored", 0x37)
end

