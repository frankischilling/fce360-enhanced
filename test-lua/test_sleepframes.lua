-- Test script for sleepframes() function
-- Demonstrates frame-accurate delays by sleeping for specified frames

local initialized = false
local sleepStartFrame = nil
local sleepDuration = 60  -- Sleep for 60 frames (~1 second)
local sleepCount = 0
local lastActionFrame = 0
local actionInterval = 180  -- Perform action every 180 frames (~3 seconds)

function gui()
    local currentFrame = getframecount()
    local elapsedTime = getelapsedtime()
    
    -- Print initial message once
    if not initialized then
        print("=== sleepframes() Test ===")
        print("Script will sleep for 60 frames every 3 seconds")
        print("Watch the frame counter pause during sleep")
        print("")
        initialized = true
    end
    
    -- Display current frame and time
    drawtext(4, 4, "Frame:", 0x20)
    drawtext(4, 14, string.format("%d", currentFrame), 0x29)
    
    drawtext(4, 24, "Time:", 0x2E)
    drawtext(4, 34, string.format("%.2f sec", elapsedTime), 0x37)
    
    -- Check if we should trigger a sleep
    if currentFrame - lastActionFrame >= actionInterval then
        sleepStartFrame = currentFrame
        sleepCount = sleepCount + 1
        print(string.format("Frame %d: Starting sleep for %d frames (sleep #%d)", 
              currentFrame, sleepDuration, sleepCount))
        sleepframes(sleepDuration)
        lastActionFrame = currentFrame
    end
    
    -- Display sleep status
    if sleepStartFrame then
        local framesSinceSleep = currentFrame - sleepStartFrame
        if framesSinceSleep < sleepDuration then
            -- Still sleeping
            local remaining = sleepDuration - framesSinceSleep
            drawtext(4, 44, string.format("SLEEPING: %d frames left", remaining), 0x2D)
        else
            -- Sleep complete
            drawtext(4, 44, string.format("AWAKE (slept %d frames)", sleepDuration), 0x2E)
            if framesSinceSleep == sleepDuration then
                -- Just woke up - print message
                print(string.format("Frame %d: Sleep complete, resuming execution", currentFrame))
            end
        end
    else
        drawtext(4, 44, "AWAKE", 0x2E)
    end
    
    -- Display sleep count
    drawtext(4, 54, string.format("Sleeps: %d", sleepCount), 0x37)
    
    -- Show if paused
    if not isframeadvancing() then
        drawtext(4, 64, "PAUSED", 0x2D)
    end
end

print("sleepframes() test script loaded")

