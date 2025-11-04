-- scanchanged() Test Script
-- Tests comparing two memory snapshots to detect changes

function script()
    local y = 4
    drawtext(4, y, "scanchanged() Test", 0x2E)
    y = y + 8
    
    -- Setup: Write known data to RAM for testing
    -- Write initial values to 0x0200-0x020F
    writebytes(0x0200, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00)
    
    -- Test 1: Take snapshot, modify some bytes, compare
    drawtext(4, y, "Test 1: Basic comparison", 0x20)
    y = y + 8
    
    local before1 = readbytes(0x0200, 10)
    
    -- Modify some bytes
    writebyte(0x0202, 0xFF)  -- Change byte at 0x0202
    writebyte(0x0205, 0x00)  -- Change byte at 0x0205
    writebyte(0x0207, 0xAA)  -- Change byte at 0x0207
    
    local after1 = readbytes(0x0200, 10)
    local changes1 = scanchanged(before1, after1, 0x0200)
    
    local count1 = 0
    for _ in pairs(changes1) do count1 = count1 + 1 end
    
    drawtext(4, y, string.format("Changed addresses: %d", count1), count1 == 3 and 0x28 or 0x16)
    y = y + 8
    
    if count1 > 0 then
        for addr, value in pairs(changes1) do
            drawtext(4, y, string.format("  0x%04X -> 0x%02X", addr, value), 0x20)
            y = y + 8
            if y > 200 then break end  -- Limit display
        end
    end
    
    -- Test 2: No changes (should return empty table)
    y = y + 4
    drawtext(4, y, "Test 2: No changes", 0x20)
    y = y + 8
    
    local before2 = readbytes(0x0210, 5)
    -- Don't modify anything
    local after2 = readbytes(0x0210, 5)
    local changes2 = scanchanged(before2, after2, 0x0210)
    
    local count2 = 0
    for _ in pairs(changes2) do count2 = count2 + 1 end
    
    drawtext(4, y, string.format("Changed addresses: %d", count2), count2 == 0 and 0x28 or 0x16)
    y = y + 8
    
    -- Test 3: All bytes changed
    y = y + 4
    drawtext(4, y, "Test 3: All bytes changed", 0x20)
    y = y + 8
    
    -- Write initial values
    writebytes(0x0220, 0x00, 0x00, 0x00, 0x00)
    local before3 = readbytes(0x0220, 4)
    
    -- Change all bytes
    writebytes(0x0220, 0xFF, 0xFF, 0xFF, 0xFF)
    local after3 = readbytes(0x0220, 4)
    local changes3 = scanchanged(before3, after3, 0x0220)
    
    local count3 = 0
    for _ in pairs(changes3) do count3 = count3 + 1 end
    
    drawtext(4, y, string.format("Changed addresses: %d", count3), count3 == 4 and 0x28 or 0x16)
    y = y + 8
    
    -- Test 4: Using backupbytes for snapshot
    y = y + 4
    drawtext(4, y, "Test 4: Using backupbytes", 0x20)
    y = y + 8
    
    -- Write initial data
    writebytes(0x0230, 0x11, 0x22, 0x33, 0x44, 0x55)
    local backup4 = backupbytes(0x0230, 5)
    
    -- Modify some bytes
    writebyte(0x0231, 0x99)
    writebyte(0x0233, 0x88)
    
    local current4 = readbytes(0x0230, 5)
    local changes4 = scanchanged(backup4, current4, 0x0230)
    
    local count4 = 0
    for _ in pairs(changes4) do count4 = count4 + 1 end
    
    drawtext(4, y, string.format("Changed addresses: %d", count4), count4 == 2 and 0x28 or 0x16)
    y = y + 8
    
    if count4 > 0 then
        for addr, value in pairs(changes4) do
            drawtext(4, y, string.format("  0x%04X -> 0x%02X", addr, value), 0x20)
            y = y + 8
            if y > 220 then break end
        end
    end
    
    -- Test 5: Verify new values are correct
    y = y + 4
    drawtext(4, y, "Test 5: Verify new values", 0x20)
    y = y + 8
    
    local verifyPass = true
    for addr, newValue in pairs(changes4) do
        local actualValue = readbyte(addr)
        if actualValue ~= newValue then
            verifyPass = false
            break
        end
    end
    
    drawtext(4, y, string.format("New values correct: %s", verifyPass and "YES" or "NO"), verifyPass and 0x28 or 0x16)
    y = y + 8
    
    -- Summary
    y = y + 4
    if count1 == 3 and count2 == 0 and count3 == 4 and count4 == 2 and verifyPass then
        drawtext(4, y, "ALL TESTS PASSED!", 0x28)
    else
        drawtext(4, y, "Some tests failed", 0x16)
    end
    
    -- Print to console
    print("scanchanged() Test Results:")
    print(string.format("  Test 1 (basic): %d changes", count1))
    print(string.format("  Test 2 (no changes): %d changes", count2))
    print(string.format("  Test 3 (all changed): %d changes", count3))
    print(string.format("  Test 4 (backupbytes): %d changes", count4))
    print(string.format("  Test 5 (verify values): %s", verifyPass and "PASS" or "FAIL"))
    
    if count1 > 0 then
        print("  Test 1 changes:")
        for addr, value in pairs(changes1) do
            print(string.format("    0x%04X -> 0x%02X", addr, value))
        end
    end
end

