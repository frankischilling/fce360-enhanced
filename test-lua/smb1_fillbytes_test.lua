-- SMB1 fillbytes() Test Script
-- Shows fillbytes() working by manipulating visible game values

function script()
    -- Read current score before filling
    local scoreBefore = readbytes(0x07DE, 3)
    local scoreBeforeVal = scoreBefore[1] * 10000 + scoreBefore[2] * 100 + scoreBefore[3]
    
    -- Test: Fill score with 0 (clear it)
    fillbytes(0x07DE, 3, 0)
    
    -- Read score after filling
    local scoreAfter = readbytes(0x07DE, 3)
    local scoreAfterVal = scoreAfter[1] * 10000 + scoreAfter[2] * 100 + scoreAfter[3]
    
    -- Display on screen
    drawtext(4, 4, "fillbytes() Test", 0x2E)
    drawtext(4, 12, string.format("Score Before: %05d", scoreBeforeVal), 0x20)
    drawtext(4, 20, string.format("Score After:  %05d", scoreAfterVal), 0x37)
    
    if scoreAfterVal == 0 then
        drawtext(4, 28, "fillbytes() WORKING!", 0x2E)
    else
        drawtext(4, 28, "fillbytes() FAILED", 0x26)
    end
    
    -- Print to console
    print("fillbytes() Test:")
    print(string.format("  Before: %d,%d,%d = %05d", scoreBefore[1], scoreBefore[2], scoreBefore[3], scoreBeforeVal))
    print(string.format("  After:  %d,%d,%d = %05d", scoreAfter[1], scoreAfter[2], scoreAfter[3], scoreAfterVal))
    
    -- Test 2: Fill with specific value (0xFF), then clear again
    if scoreAfterVal == 0 then
        -- Fill score area with 0xFF
        fillbytes(0x07DE, 3, 0xFF)
        local test2 = readbytes(0x07DE, 3)
        print(string.format("  Test 2 - Filled with 0xFF: %d,%d,%d", test2[1], test2[2], test2[3]))
        
        -- Clear it again with fillbytes
        fillbytes(0x07DE, 3, 0)
        local cleared = readbytes(0x07DE, 3)
        print(string.format("  Test 2 - Cleared to 0: %d,%d,%d", cleared[1], cleared[2], cleared[3]))
    end
end

