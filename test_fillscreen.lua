-- Test script for fillscreen() function
-- Demonstrates filling the entire overlay screen with colors

local frameCounter = 0
local testPhase = 0
local phaseDuration = 180  -- 3 seconds at 60 FPS

function gui()
    frameCounter = frameCounter + 1
    
    -- Cycle through different test phases
    testPhase = math.floor(frameCounter / phaseDuration) % 5
    
    if testPhase == 0 then
        -- Phase 0: Basic fillscreen() with different colors
        local colorIndex = math.floor(frameCounter / 30) % 64  -- Change color every 30 frames
        fillscreen(colorIndex)
        
        -- Draw info text
        drawtext(4, 4, "Phase 0: Basic fillscreen()", 0x3F)
        drawtext(4, 12, string.format("Color: 0x%02X", colorIndex), 0x20)
        drawtext(4, 20, "Screen filled with color", 0x20)
        
        -- Draw color swatch
        fillrect(200, 10, 40, 40, colorIndex)
        drawrect(200, 10, 40, 40, 0x3F)
        
    elseif testPhase == 1 then
        -- Phase 1: Fade effect using fillscreen() with alpha blending
        setdrawmode("alpha")
        
        -- Calculate fade amount (0-64)
        local fadeAmount = (frameCounter % 120) / 120 * 64
        local fadeColor = math.floor(fadeAmount) % 64
        
        -- Fill screen with fade color
        fillscreen(fadeColor)
        
        -- Draw some content on top
        drawtext(80, 100, "FADE EFFECT", 0x3F)
        drawtext(60, 110, "Using fillscreen() + alpha", 0x20)
        
        setdrawmode("normal")  -- Reset to normal
        
        drawtext(4, 4, "Phase 1: Fade effect", 0x3F)
        drawtext(4, 12, string.format("Fade: %d/64", fadeColor), 0x2E)
        
    elseif testPhase == 2 then
        -- Phase 2: Color cycling through palette
        local cycle = frameCounter % 240
        local color = math.floor(cycle / 4) % 64  -- Cycle through all 64 colors
        
        fillscreen(color)
        
        -- Draw grid pattern on top
        for y = 0, 239, 30 do
            for x = 0, 255, 30 do
                if ((x / 30) + (y / 30)) % 2 == 0 then
                    fillrect(x, y, 15, 15, 0x00)  -- Dark squares
                end
            end
        end
        
        drawtext(4, 4, "Phase 2: Color cycling", 0x3F)
        drawtext(4, 12, string.format("Color: 0x%02X (%d/64)", color, color), 0x20)
        
    elseif testPhase == 3 then
        -- Phase 3: Compare fillscreen() vs fillrect()
        local useFillScreen = (frameCounter % 120) >= 60
        
        if useFillScreen then
            -- Use fillscreen()
            fillscreen(0x16)  -- Red/orange
            
            -- Draw content on top
            fillrect(50, 50, 156, 80, 0x29)  -- Green rectangle
            drawtext(80, 80, "FILLSCREEN", 0x20)
            
            drawtext(4, 4, "Phase 3: fillscreen(0x16)", 0x3F)
        else
            -- Use fillrect() for comparison
            fillrect(0, 0, 256, 240, 0x16)  -- Same as fillscreen(0x16)
            
            -- Draw content on top
            fillrect(50, 50, 156, 80, 0x29)  -- Green rectangle
            drawtext(80, 80, "FILLRECT", 0x20)
            
            drawtext(4, 4, "Phase 3: fillrect(0,0,256,240,0x16)", 0x3F)
        end
        
        drawtext(4, 12, string.format("Frame: %d", frameCounter), 0x2E)
        drawtext(4, 20, "Compare: fillscreen vs fillrect", 0x39)
        
    else
        -- Phase 4: Screen overlay effects
        -- Create a pulsing overlay
        local pulse = math.sin(frameCounter / 30) * 0.5 + 0.5  -- 0.0 to 1.0
        local overlayColor = math.floor(pulse * 32)  -- 0 to 32
        
        -- Fill screen with semi-transparent overlay
        setdrawmode("alpha")
        fillscreen(overlayColor)
        setdrawmode("normal")
        
        -- Draw game-like content underneath (simulated)
        for i = 0, 4 do
            local x = 20 + i * 50
            local y = 100 + math.sin((frameCounter + i * 20) / 20) * 30
            fillcircle(x, math.floor(y), 15, 0x29 + i * 4)
        end
        
        drawtext(4, 4, "Phase 4: Screen overlay", 0x3F)
        drawtext(4, 12, string.format("Pulse: %.2f", pulse), 0x2E)
        drawtext(4, 20, "Overlay color: 0x" .. string.format("%02X", overlayColor), 0x39)
    end
    
    -- Always show FPS and phase info at bottom
    local fps = getfps()
    drawtext(4, 230, string.format("FPS: %.1f | Phase: %d/%d", fps, testPhase, 4), 0x2E)
end

print("fillscreen() test script loaded")
print("This script demonstrates:")
print("  - Filling entire screen with colors")
print("  - Fade effects using fillscreen()")
print("  - Color cycling through palette")
print("  - Comparison with fillrect()")
print("  - Screen overlay effects")

