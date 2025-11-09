-- Test script for clearscreen() function
-- Demonstrates clearing the entire overlay screen

local frameCounter = 0
local testPhase = 0
local phaseDuration = 180  -- 3 seconds at 60 FPS

function gui()
    frameCounter = frameCounter + 1
    
    -- Cycle through different test phases
    testPhase = math.floor(frameCounter / phaseDuration) % 4
    
    if testPhase == 0 then
        -- Phase 0: Draw pattern, then clear with clearscreen()
        -- Draw a colorful pattern
        for y = 0, 239, 20 do
            for x = 0, 255, 20 do
                local color = ((x / 20) + (y / 20)) % 64
                fillrect(x, y, 18, 18, color)
            end
        end
        
        -- Draw some text
        drawtext(50, 100, "Pattern Before Clear", 0x20)
        drawtext(50, 110, "Press to see clearscreen()", 0x39)
        
        -- Clear screen every 60 frames (1 second)
        if frameCounter % 60 == 0 then
            clearscreen()
            print("✓ clearscreen() called - entire screen cleared")
        end
        
        -- Draw indicator that we're in phase 0
        drawtext(4, 4, "Phase 0: Pattern + clearscreen()", 0x3F)
        drawtext(4, 12, string.format("Frame: %d", frameCounter), 0x2E)
        
    elseif testPhase == 1 then
        -- Phase 1: Compare clearscreen() vs clearrect()
        local useClearScreen = (frameCounter % 120) >= 60
        
        if useClearScreen then
            -- First clear, then draw
            clearscreen()
            
            -- Draw background pattern
            for i = 0, 15 do
                fillrect(i * 16, 0, 16, 240, (i * 4) % 64)
            end
            
            -- Draw content
            fillrect(10, 50, 100, 80, 0x16)
            drawtext(20, 80, "LEFT SIDE", 0x20)
            fillrect(146, 50, 100, 80, 0x29)
            drawtext(156, 80, "RIGHT SIDE", 0x20)
            
            drawtext(4, 4, "Phase 1: clearscreen() - Full clear", 0x3F)
        else
            -- Draw background pattern
            for i = 0, 15 do
                fillrect(i * 16, 0, 16, 240, (i * 4) % 64)
            end
            
            -- Draw content
            fillrect(10, 50, 100, 80, 0x16)
            drawtext(20, 80, "LEFT SIDE", 0x20)
            fillrect(146, 50, 100, 80, 0x29)
            drawtext(156, 80, "RIGHT SIDE", 0x20)
            
            -- Clear only left half with clearrect()
            clearrect(0, 0, 128, 240)
            
            drawtext(4, 4, "Phase 1: clearrect(0,0,128,240)", 0x3F)
        end
        
        drawtext(4, 12, string.format("Frame: %d", frameCounter), 0x2E)
        drawtext(4, 20, "Compare: clearrect vs clearscreen", 0x39)
        
    elseif testPhase == 2 then
        -- Phase 2: Rapid clear and redraw
        local cycle = frameCounter % 30
        
        if cycle < 20 then
            -- Draw different patterns
            if cycle % 4 == 0 then
                -- Horizontal stripes
                for y = 0, 239, 10 do
                    fillrect(0, y, 256, 5, 0x20)
                end
            elseif cycle % 4 == 1 then
                -- Vertical stripes
                for x = 0, 255, 10 do
                    fillrect(x, 0, 5, 240, 0x16)
                end
            elseif cycle % 4 == 2 then
                -- Checkerboard
                for y = 0, 239, 20 do
                    for x = 0, 255, 20 do
                        if ((x / 20) + (y / 20)) % 2 == 0 then
                            fillrect(x, y, 20, 20, 0x3F)
                        end
                    end
                end
            else
                -- Circles
                for i = 0, 3 do
                    for j = 0, 3 do
                        fillcircle(32 + i * 64, 30 + j * 60, 20, 0x29 + (i + j) * 4)
                    end
                end
            end
        else
            -- Clear screen for 10 frames
            clearscreen()
        end
        
        drawtext(4, 4, "Phase 2: Rapid clear/redraw", 0x3F)
        drawtext(4, 12, string.format("Frame: %d (cycle: %d)", frameCounter, cycle), 0x2E)
        
    else
        -- Phase 3: Text overlay test
        -- Draw background
        fillrect(0, 0, 256, 240, 0x00)
        
        -- Draw text that changes
        local textY = 50 + math.sin(frameCounter / 30) * 30
        drawtext(80, math.floor(textY), "ANIMATED TEXT", 0x3F)
        
        -- Draw multiple text elements
        for i = 0, 5 do
            local x = 20 + i * 40
            local y = 120 + math.sin((frameCounter + i * 10) / 20) * 20
            drawtext(x, math.floor(y), string.format("T%d", i), 0x20 + i * 4)
        end
        
        -- Clear screen periodically
        if frameCounter % 90 == 0 then
            clearscreen()
            print("✓ clearscreen() called - text cleared")
        end
        
        drawtext(4, 4, "Phase 3: Text overlay test", 0x3F)
        drawtext(4, 12, string.format("Frame: %d", frameCounter), 0x2E)
        drawtext(4, 20, "Screen clears every 90 frames", 0x39)
    end
    
    -- Always show FPS and phase info at bottom
    local fps = getfps()
    drawtext(4, 230, string.format("FPS: %.1f | Phase: %d/%d", fps, testPhase, 3), 0x2E)
end

print("clearscreen() test script loaded")
print("This script demonstrates:")
print("  - Clearing entire overlay screen")
print("  - Comparison with clearrect()")
print("  - Rapid clear/redraw cycles")
print("  - Text overlay clearing")

