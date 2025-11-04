-- SMB1 backupbytes() Test Script
-- Shows backupbytes() working by backing up and restoring game values

function script()
    -- Test 1: Backup score
    local backup = backupbytes(0x07DE, 3)
    local score = readbytes(0x07DE, 3)
    
    -- Verify backup matches original
    local matches = true
    for i = 1, 3 do
        if backup[i] ~= score[i] then
            matches = false
            break
        end
    end
    
    -- Test 2: Modify original, then restore from backup
    writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
    local modified = readbytes(0x07DE, 3)
    
    -- Restore using writebytes with backup table
    writebytes(0x07DE, backup[1], backup[2], backup[3])
    local restored = readbytes(0x07DE, 3)
    
    -- Verify restore worked
    local restoredMatches = true
    for i = 1, 3 do
        if backup[i] ~= restored[i] then
            restoredMatches = false
            break
        end
    end
    
    -- Display on screen
    drawtext(4, 100, "backupbytes() Test", 0x2E)
    drawtext(4, 108, string.format("Original: %d,%d,%d", score[1], score[2], score[3]), 0x20)
    drawtext(4, 116, string.format("Backup:   %d,%d,%d", backup[1], backup[2], backup[3]), 0x2A)
    drawtext(4, 124, string.format("Modified: %d,%d,%d", modified[1], modified[2], modified[3]), 0x37)
    drawtext(4, 132, string.format("Restored: %d,%d,%d", restored[1], restored[2], restored[3]), 0x37)
    
    if matches and restoredMatches then
        drawtext(4, 140, "backupbytes() WORKING!", 0x2E)
    else
        drawtext(4, 140, "backupbytes() FAILED", 0x26)
    end
end

