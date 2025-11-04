-- findpattern() Test Script
-- Tests pattern matching with and without wildcards

function script()
    local y = 4
    drawtext(4, y, "findpattern() Test", 0x2E)
    y = y + 8
    
    -- Setup: Write known patterns to RAM for testing
    -- Pattern 1: {0xAA, 0xBB, 0xCC} at 0x0200
    writebytes(0x0200, 0xAA, 0xBB, 0xCC)
    
    -- Pattern 2: {0xAA, 0xDD, 0xCC} at 0x0210 (middle byte different)
    writebytes(0x0210, 0xAA, 0xDD, 0xCC)
    
    -- Pattern 3: {0xAA, 0xEE, 0xCC} at 0x0220 (middle byte different)
    writebytes(0x0220, 0xAA, 0xEE, 0xCC)
    
    -- Pattern 4: {0x11, 0x22, 0x33, 0x44} at 0x0230
    writebytes(0x0230, 0x11, 0x22, 0x33, 0x44)
    
    -- Test 1: Find exact pattern without mask
    drawtext(4, y, "Test 1: Exact pattern", 0x20)
    y = y + 8
    
    local pattern1 = {0xAA, 0xBB, 0xCC}
    local results1 = findpattern(pattern1, 0x0200, 0x02FF)
    local count1 = #results1
    drawtext(4, y, string.format("Pattern {0xAA,0xBB,0xCC}: %d found", count1), count1 >= 1 and 0x28 or 0x16)
    y = y + 8
    
    if count1 > 0 then
        drawtext(4, y, string.format("  Found at: 0x%04X", results1[1]), 0x20)
        y = y + 8
    end
    
    -- Test 2: Find pattern with wildcard in middle (using mask)
    y = y + 4
    drawtext(4, y, "Test 2: Pattern with wildcard", 0x20)
    y = y + 8
    
    local pattern2 = {0xAA, 0x00, 0xCC}  -- Middle byte doesn't matter
    local mask2 = {1, 0, 1}  -- 1 = match, 0 = wildcard
    local results2 = findpattern(pattern2, 0x0200, 0x02FF, mask2)
    local count2 = #results2
    drawtext(4, y, string.format("Pattern {0xAA,??,0xCC}: %d found", count2), count2 >= 3 and 0x28 or 0x16)
    y = y + 8
    
    if count2 > 0 then
        for i = 1, math.min(count2, 3) do
            drawtext(4, y, string.format("  Found at: 0x%04X", results2[i]), 0x20)
            y = y + 8
        end
    end
    
    -- Test 3: Find pattern with no wildcards (should match scanbytes behavior)
    y = y + 4
    drawtext(4, y, "Test 3: No mask (like scanbytes)", 0x20)
    y = y + 8
    
    local pattern3 = {0x11, 0x22, 0x33, 0x44}
    local results3 = findpattern(pattern3, 0x0200, 0x02FF)
    local count3 = #results3
    drawtext(4, y, string.format("Pattern {0x11,0x22,0x33,0x44}: %d found", count3), count3 >= 1 and 0x28 or 0x16)
    y = y + 8
    
    if count3 > 0 then
        drawtext(4, y, string.format("  Found at: 0x%04X", results3[1]), 0x20)
        y = y + 8
    end
    
    -- Test 4: Pattern that shouldn't exist
    y = y + 4
    drawtext(4, y, "Test 4: Non-existent pattern", 0x20)
    y = y + 8
    
    local pattern4 = {0xFF, 0xFF, 0xFF, 0xFF}
    local results4 = findpattern(pattern4, 0x0200, 0x02FF)
    local count4 = #results4
    drawtext(4, y, string.format("Pattern {0xFF,0xFF,0xFF,0xFF}: %d found", count4), count4 == 0 and 0x28 or 0x16)
    y = y + 8
    
    -- Test 5: Pattern with multiple wildcards
    y = y + 4
    drawtext(4, y, "Test 5: Multiple wildcards", 0x20)
    y = y + 8
    
    local pattern5 = {0xAA, 0x00, 0x00, 0xCC}
    local mask5 = {1, 0, 0, 1}  -- First and last must match, middle two wildcards
    -- Write a matching pattern
    writebytes(0x0240, 0xAA, 0x99, 0x88, 0xCC)
    local results5 = findpattern(pattern5, 0x0200, 0x02FF, mask5)
    local count5 = #results5
    drawtext(4, y, string.format("Pattern {0xAA,??,??,0xCC}: %d found", count5), count5 >= 1 and 0x28 or 0x16)
    y = y + 8
    
    if count5 > 0 then
        drawtext(4, y, string.format("  Found at: 0x%04X", results5[1]), 0x20)
        y = y + 8
    end
    
    -- Summary
    y = y + 4
    if count1 >= 1 and count2 >= 3 and count3 >= 1 and count4 == 0 and count5 >= 1 then
        drawtext(4, y, "ALL TESTS PASSED!", 0x28)
    else
        drawtext(4, y, "Some tests failed", 0x16)
    end
    
    -- Print to console
    print("findpattern() Test Results:")
    print(string.format("  Test 1 (exact): %d matches", count1))
    print(string.format("  Test 2 (wildcard): %d matches", count2))
    print(string.format("  Test 3 (no mask): %d matches", count3))
    print(string.format("  Test 4 (non-existent): %d matches", count4))
    print(string.format("  Test 5 (multi-wildcard): %d matches", count5))
end

