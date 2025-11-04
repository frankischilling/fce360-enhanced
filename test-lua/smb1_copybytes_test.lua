-- SMB1 copybytes() Test Script
-- Shows copybytes() working by copying game values

function script()
    -- Test 1: Copy score to backup location (non-overlapping)
    local score = readbytes(0x07DE, 3)
    local scoreVal = score[1] * 10000 + score[2] * 100 + score[3]
    
    -- Copy score to backup location (0x0600-0x0602)
    copybytes(0x07DE, 0x0600, 3)
    
    -- Verify backup was created
    local backup = readbytes(0x0600, 3)
    local backupVal = backup[1] * 10000 + backup[2] * 100 + backup[3]
    
    -- Test 2: Copy backup back (restore)
    copybytes(0x0600, 0x07DE, 3)
    
    -- Verify restore worked
    local restored = readbytes(0x07DE, 3)
    local restoredVal = restored[1] * 10000 + restored[2] * 100 + restored[3]
    
    -- Display on screen
    drawtext(4, 4, "copybytes() Test", 0x2E)
    drawtext(4, 12, string.format("Original: %05d", scoreVal), 0x20)
    drawtext(4, 20, string.format("Backup:   %05d", backupVal), 0x2A)
    drawtext(4, 28, string.format("Restored: %05d", restoredVal), 0x37)
    
    if scoreVal == backupVal and scoreVal == restoredVal then
        drawtext(4, 100, "copybytes() WORKING!", 0x2E)
    else
        drawtext(4, 100, "copybytes() FAILED", 0x26)
    end
    
    -- Test 3: Overlapping copy (copy score to itself shifted by 1 byte)
    -- This tests the overlapping region handling
    local testVal = readbyte(0x07DE)
    copybytes(0x07DE, 0x07DF, 2)  -- Copy 2 bytes forward (overlapping)
    local afterOverlap = readbytes(0x07DE, 3)
    drawtext(4, 44, string.format("Overlap: %d->%d->%d", afterOverlap[1], afterOverlap[2], afterOverlap[3]), 0x2A)
end

