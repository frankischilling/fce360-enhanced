-- Test script for gettimedelta() function
-- Demonstrates delta time calculations and frame-independent movement

local initialized = false
local position = 0.0  -- Example position for movement
local velocity = 50.0  -- Pixels per second
local lastPrintTime = nil
local printInterval = 2000  -- Print every 2 seconds
local frameCount = 0
local totalDelta = 0.0
local minDelta = math.huge
local maxDelta = 0.0

function gui()
    local delta = gettimedelta()
    local frame = getframecount()
    local elapsed = getelapsedtime()
    
    -- Print initial message once
    if not initialized then
        print("=== gettimedelta() Test ===")
        print("Shows time since last frame in seconds")
        print("Demonstrates frame-independent movement")
        print("")
        lastPrintTime = gettime()
        initialized = true
    end
    
    -- Update statistics
    frameCount = frameCount + 1
    totalDelta = totalDelta + delta
    if delta < minDelta then minDelta = delta end
    if delta > maxDelta then maxDelta = delta end
    
    -- Example: Frame-independent movement using delta time
    -- Move at constant velocity regardless of frame rate
    position = position + (velocity * delta)
    
    -- Wrap position for display
    if position > 240 then
        position = position - 240
    end
    
    -- Display delta time
    drawtext(4, 4, "Delta Time:", 0x20)
    drawtext(4, 14, string.format("%.4f sec", delta), 0x29)
    drawtext(4, 24, string.format("%.2f ms", delta * 1000), 0x2E)
    
    -- Display statistics
    local avgDelta = totalDelta / frameCount
    drawtext(4, 34, string.format("Avg: %.4f sec", avgDelta), 0x37)
    drawtext(4, 44, string.format("Min: %.4f sec", minDelta), 0x37)
    drawtext(4, 54, string.format("Max: %.4f sec", maxDelta), 0x37)
    
    -- Display frame count
    drawtext(4, 64, string.format("Frame: %d", frame), 0x2E)
    
    -- Display example movement (frame-independent)
    local displayPos = math.floor(position)
    drawtext(4, 74, string.format("Pos: %d (%.1f px/s)", displayPos, velocity), 0x29)
    
    -- Draw a simple indicator at the position
    if displayPos >= 0 and displayPos < 240 then
        drawtext(displayPos, 84, "|", 0x2D)
    end
    
    -- Print to console periodically
    local currentTime = gettime()
    if lastPrintTime == nil or currentTime - lastPrintTime >= printInterval then
        print(string.format("Delta: %.4f sec (%.2f ms) | Frame: %d | Avg: %.4f sec | Pos: %.1f", 
              delta, delta * 1000, frame, avgDelta, position))
        lastPrintTime = currentTime
    end
    
    -- Show if paused (delta should be larger when paused)
    if not isframeadvancing() then
        drawtext(4, 94, "PAUSED", 0x2D)
    end
end

print("gettimedelta() test script loaded")

