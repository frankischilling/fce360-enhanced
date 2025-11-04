-- SMB1 readram() Test Script
-- Shows readram() working by reading from RAM region

function script()
    -- Test 1: Read from RAM (SMB1 score is in RAM at 0x07DE)
    local ramData = readram(0x07DE, 3)
    local allData = readbytes(0x07DE, 3)
    
    -- Verify they match (since 0x07DE is in RAM)
    local matches = true
    for i = 1, 3 do
        if ramData[i] ~= allData[i] then
            matches = false
            break
        end
    end
    
    -- Test 2: Try to read from start of RAM
    local startRam = readram(0x0000, 8)
    
    -- Test 3: Try to read from end of RAM
    local endRam = readram(0x1FF8, 8)  -- Read last 8 bytes of RAM
    
    -- Display on screen
    drawtext(4, 100, "readram() Test", 0x2E)
    drawtext(4, 108, string.format("Score (RAM): %d,%d,%d", ramData[1], ramData[2], ramData[3]), 0x20)
    drawtext(4, 116, string.format("Score (all): %d,%d,%d", allData[1], allData[2], allData[3]), 0x20)
    drawtext(4, 124, string.format("RAM Start: %d bytes read", #startRam), 0x2A)
    drawtext(4, 132, string.format("RAM End: %d bytes read", #endRam), 0x2A)
    
    if matches then
        drawtext(4, 140, "readram() WORKING!", 0x2E)
    else
        drawtext(4, 140, "readram() FAILED", 0x26)
    end
    
    -- Test 4: Verify validation (this would error if called)
    -- readram(0x8000, 1)  -- This should error (outside RAM)
end

