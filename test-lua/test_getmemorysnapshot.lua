-- getmemorysnapshot() Test Script
-- Tests creating memory snapshots indexed by address

function script()
    local y = 4
    drawtext(4, y, "getmemorysnapshot() Test", 0x2E)
    y = y + 8
    
    -- Test 1: Create snapshot of a small RAM region
    drawtext(4, y, "Test 1: Small region snapshot", 0x20)
    y = y + 8
    
    -- Write some test data
    writebytes(0x0200, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE)
    
    local snapshot1 = getmemorysnapshot(0x0200, 0x0204)
    local count1 = 0
    for _ in pairs(snapshot1) do count1 = count1 + 1 end
    
    drawtext(4, y, string.format("Snapshots: %d addresses", count1), count1 == 5 and 0x28 or 0x16)
    y = y + 8
    
    -- Verify values
    local test1Pass = (snapshot1[0x0200] == 0xAA and 
                       snapshot1[0x0201] == 0xBB and 
                       snapshot1[0x0202] == 0xCC and
                       snapshot1[0x0203] == 0xDD and
                       snapshot1[0x0204] == 0xEE)
    drawtext(4, y, string.format("Values correct: %s", test1Pass and "YES" or "NO"), test1Pass and 0x28 or 0x16)
    y = y + 8
    
    -- Test 2: Compare snapshots over time
    y = y + 4
    drawtext(4, y, "Test 2: Snapshot comparison", 0x20)
    y = y + 8
    
    -- Write known initial values first
    writebytes(0x0210, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66)
    
    -- Take initial snapshot
    local beforeSnapshot = getmemorysnapshot(0x0210, 0x0215)
    
    -- Modify some bytes (write different values)
    writebyte(0x0212, 0xFF)  -- Change 0x33 to 0xFF
    writebyte(0x0214, 0x00)  -- Change 0x55 to 0x00
    
    -- Take new snapshot
    local afterSnapshot = getmemorysnapshot(0x0210, 0x0215)
    
    -- Compare snapshots
    local changes = {}
    for addr, newValue in pairs(afterSnapshot) do
        local oldValue = beforeSnapshot[addr]
        if oldValue ~= newValue then
            table.insert(changes, {addr = addr, old = oldValue, new = newValue})
        end
    end
    
    drawtext(4, y, string.format("Changes detected: %d", #changes), #changes == 2 and 0x28 or 0x16)
    y = y + 8
    
    if #changes > 0 then
        for i, change in ipairs(changes) do
            drawtext(4, y, string.format("  0x%04X: 0x%02X -> 0x%02X", change.addr, change.old, change.new), 0x20)
            y = y + 8
            if y > 200 then break end
        end
    end
    
    -- Test 3: Snapshot of entire RAM region
    y = y + 4
    drawtext(4, y, "Test 3: Large region snapshot", 0x20)
    y = y + 8
    
    local startTime = os.clock()
    local ramSnapshot = getmemorysnapshot(0x0000, 0x07FF)
    local endTime = os.clock()
    local elapsed = (endTime - startTime) * 1000  -- Convert to milliseconds
    
    local ramCount = 0
    for _ in pairs(ramSnapshot) do ramCount = ramCount + 1 end
    
    drawtext(4, y, string.format("RAM snapshot: %d addresses", ramCount), ramCount == 0x0800 and 0x28 or 0x20)
    y = y + 8
    drawtext(4, y, string.format("Time: %.2f ms", elapsed), 0x20)
    y = y + 8
    
    -- Test 4: Verify address-indexed access
    y = y + 4
    drawtext(4, y, "Test 4: Address-indexed access", 0x20)
    y = y + 8
    
    -- Write known values first
    writebytes(0x0300, 0xAA, 0xBB, 0xCC)
    local testSnapshot = getmemorysnapshot(0x0300, 0x0302)
    
    -- Now write different values (snapshot should still have old values)
    writebytes(0x0300, 0x11, 0x22, 0x33)
    
    -- Access by address (not array index) - snapshot should have old values
    local addr300 = testSnapshot[0x0300]
    local addr301 = testSnapshot[0x0301]
    local addr302 = testSnapshot[0x0302]
    local addr303 = testSnapshot[0x0303]
    
    local test4Pass = (addr300 ~= nil and addr300 == 0xAA and
                      addr301 ~= nil and addr301 == 0xBB and
                      addr302 ~= nil and addr302 == 0xCC and
                      addr303 == nil)  -- Should not exist
    
    drawtext(4, y, string.format("Address keys: %s", test4Pass and "CORRECT" or "WRONG"), test4Pass and 0x28 or 0x16)
    y = y + 8
    if not test4Pass then
        drawtext(4, y, string.format("  [0x0300]=%s [0x0301]=%s [0x0302]=%s", 
              tostring(addr300), tostring(addr301), tostring(addr302)), 0x16)
        y = y + 8
    end
    
    -- Test 5: Use with scanchanged pattern
    y = y + 4
    drawtext(4, y, "Test 5: Snapshot + scanchanged", 0x20)
    y = y + 8
    
    -- Write known initial value
    writebyte(0x0221, 0x55)
    local snap1 = getmemorysnapshot(0x0220, 0x0222)
    
    -- Write different value
    writebyte(0x0221, 0x99)
    local snap2 = getmemorysnapshot(0x0220, 0x0222)
    
    -- Check if snapshots captured different values
    local val1 = snap1[0x0221]
    local val2 = snap2[0x0221]
    local test5Pass = (val1 ~= nil and val2 ~= nil and val1 ~= val2 and val1 == 0x55 and val2 == 0x99)
    
    drawtext(4, y, string.format("Change detected: %s", test5Pass and "YES" or "NO"), test5Pass and 0x28 or 0x16)
    y = y + 8
    if not test5Pass then
        drawtext(4, y, string.format("  snap1[0x0221]=%s snap2[0x0221]=%s", 
              tostring(val1), tostring(val2)), 0x16)
        y = y + 8
    end
    
    -- Summary
    y = y + 4
    local allPass = test1Pass and #changes == 2 and ramCount == 0x0800 and test4Pass and test5Pass
    if allPass then
        drawtext(4, y, "ALL TESTS PASSED!", 0x28)
    else
        drawtext(4, y, "Some tests failed", 0x16)
        y = y + 8
        drawtext(4, y, string.format("Test 1: %s", test1Pass and "PASS" or "FAIL"), test1Pass and 0x28 or 0x16)
        y = y + 8
        drawtext(4, y, string.format("Test 2: %s (%d changes)", #changes == 2 and "PASS" or "FAIL", #changes), #changes == 2 and 0x28 or 0x16)
        y = y + 8
        drawtext(4, y, string.format("Test 3: %s (%d addresses)", ramCount == 0x0800 and "PASS" or "FAIL", ramCount), ramCount == 0x0800 and 0x28 or 0x16)
        y = y + 8
        drawtext(4, y, string.format("Test 4: %s", test4Pass and "PASS" or "FAIL"), test4Pass and 0x28 or 0x16)
        y = y + 8
        drawtext(4, y, string.format("Test 5: %s", test5Pass and "PASS" or "FAIL"), test5Pass and 0x28 or 0x16)
    end
    
    -- Print to console
    print("getmemorysnapshot() Test Results:")
    print(string.format("  Test 1 (small): %d addresses, %s", count1, test1Pass and "PASS" or "FAIL"))
    print(string.format("  Test 2 (comparison): %d changes", #changes))
    print(string.format("  Test 3 (large): %d addresses in %.2f ms", ramCount, elapsed))
    print(string.format("  Test 4 (address keys): %s", test4Pass and "PASS" or "FAIL"))
    print(string.format("  Test 5 (change detection): %s", test5Pass and "PASS" or "FAIL"))
end

