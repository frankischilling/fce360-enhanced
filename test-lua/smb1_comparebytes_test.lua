-- SMB1 comparebytes() Test Script
-- Shows comparebytes() working by comparing memory regions

function script()
    -- Test 1: Compare identical regions (score to itself)
    local result1 = comparebytes(0x07DE, 0x07DE, 3)
    
    -- Test 2: Copy score to backup and compare
    copybytes(0x07DE, 0x0600, 3)
    local result2 = comparebytes(0x07DE, 0x0600, 3)
    
    -- Test 3: Modify backup and compare (should be different)
    writebyte(0x0600, 99)
    local result3 = comparebytes(0x07DE, 0x0600, 3)
    
    -- Display on screen
    drawtext(4, 4, "comparebytes() Test", 0x2E)
    drawtext(4, 12, string.format("Same region: %s", result1 and "true" or "false"), 0x20)
    drawtext(4, 20, string.format("After copy:  %s", result2 and "true" or "false"), 0x2A)
    drawtext(4, 28, string.format("After modify: %s", result3 and "true" or "false"), 0x37)
    
    if result1 and result2 and not result3 then
        drawtext(4, 36, "comparebytes() WORKING!", 0x2E)
    else
        drawtext(4, 36, "comparebytes() FAILED", 0x26)
    end
    
    -- Test 4: Compare different regions
    local score = readbytes(0x07DE, 3)
    local lives = readbyte(0x075A)
    local result4 = comparebytes(0x07DE, 0x075A, 1)
    drawtext(4, 44, string.format("Score vs Lives: %s", result4 and "true" or "false"), 0x2A)
end

