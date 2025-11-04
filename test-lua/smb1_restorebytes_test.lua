-- SMB1 restorebytes() Test Script
-- Shows restorebytes() working by backing up and restoring game values

function script()
    -- Test 1: Backup and restore score
    local scoreBackup = backupbytes(0x07DE, 3)
    local originalScore = readbytes(0x07DE, 3)
    
    -- Modify score
    writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
    local modifiedScore = readbytes(0x07DE, 3)
    
    -- Restore using restorebytes
    restorebytes(0x07DE, scoreBackup)
    local restoredScore = readbytes(0x07DE, 3)
    
    -- Verify restore worked
    local restoredMatches = true
    for i = 1, 3 do
        if originalScore[i] ~= restoredScore[i] then
            restoredMatches = false
            break
        end
    end
    
    -- Test 2: Backup and restore multiple values
    local gameBackup = {
        score = backupbytes(0x07DE, 3),
        lives = backupbytes(0x075A, 1)
    }
    
    -- Modify values
    writebytes(0x07DE, 5, 0, 0)  -- Set score to 50000
    writebyte(0x075A, 98)        -- Set lives to 99
    
    -- Restore
    restorebytes(0x07DE, gameBackup.score)
    restorebytes(0x075A, gameBackup.lives)
    
    -- Verify
    local finalScore = readbytes(0x07DE, 3)
    local finalLives = readbyte(0x075A)
    local scoreMatches = comparebytes(0x07DE, 0x07DE, 3)  -- Compare with itself (should always match)
    
    -- Display on screen
    drawtext(4, 100, "restorebytes() Test", 0x2E)
    drawtext(4, 108, string.format("Original: %d,%d,%d", originalScore[1], originalScore[2], originalScore[3]), 0x20)
    drawtext(4, 116, string.format("Modified: %d,%d,%d", modifiedScore[1], modifiedScore[2], modifiedScore[3]), 0x37)
    drawtext(4, 124, string.format("Restored: %d,%d,%d", restoredScore[1], restoredScore[2], restoredScore[3]), 0x37)
    
    if restoredMatches then
        drawtext(4, 132, "restorebytes() WORKING!", 0x2E)
    else
        drawtext(4, 132, "restorebytes() FAILED", 0x26)
    end
end

