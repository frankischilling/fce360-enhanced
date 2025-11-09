-- Test script for gettime() function
-- Displays current system time and demonstrates time-based logic

local initialized = false
local startTime = nil
local lastPrintTime = nil
local printInterval = 5000  -- Print to console every 5 seconds
local lastActionTime = nil
local actionInterval = 3000  -- Perform action every 3 seconds

function gui()
    local currentTime = gettime()
    local frame = getframecount()
    local elapsedTime = getelapsedtime()
    
    -- Print initial message once
    if not initialized then
        print("=== gettime() Test ===")
        print("Displays system time (milliseconds since boot)")
        print("Shows time differences and periodic actions")
        print("")
        startTime = currentTime
        lastPrintTime = currentTime
        lastActionTime = currentTime
        initialized = true
    end
    
    -- Calculate time since script start
    local timeSinceStart = currentTime - startTime
    
    -- Display current time
    drawtext(4, 4, "System Time:", 0x20)
    drawtext(4, 14, string.format("%d ms", currentTime), 0x29)
    
    -- Display time since script start
    drawtext(4, 24, "Since Start:", 0x2E)
    local secondsSinceStart = timeSinceStart / 1000.0
    if secondsSinceStart >= 60 then
        local minutes = math.floor(secondsSinceStart / 60)
        local seconds = math.floor(secondsSinceStart % 60)
        drawtext(4, 34, string.format("%d:%02d sec", minutes, seconds), 0x37)
    else
        drawtext(4, 34, string.format("%.1f sec", secondsSinceStart), 0x37)
    end
    
    -- Display frame count for reference
    drawtext(4, 44, string.format("Frame: %d", frame), 0x2E)
    
    -- Display elapsed time (game time) for comparison
    drawtext(4, 54, string.format("Game Time: %.2f", elapsedTime), 0x37)
    
    -- Print to console periodically
    if currentTime - lastPrintTime >= printInterval then
        print(string.format("System Time: %d ms (Frame: %d, Game Time: %.2f sec)", 
              currentTime, frame, elapsedTime))
        print(string.format("  Time since script start: %.2f seconds", secondsSinceStart))
        lastPrintTime = currentTime
    end
    
    -- Perform periodic action
    if currentTime - lastActionTime >= actionInterval then
        print(string.format("Periodic action at %d ms (every 3 seconds)", currentTime))
        lastActionTime = currentTime
    end
    
    -- Show if paused
    if not isframeadvancing() then
        drawtext(4, 64, "PAUSED", 0x2D)
    end
end

print("gettime() test script loaded")

