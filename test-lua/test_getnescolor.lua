-- Test script for getnescolor() function
-- Displays packed RGB integer values for palette colors

local initialized = false
-- Test non-black colors that should have visible RGB values
local testIndices = {0x10, 0x20, 0x16, 0x29, 0x37, 0x2A, 0x3B, 0x21}  -- Light gray, white, red, green, yellow, etc.
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Print initial message once
    if not initialized then
        print("=== getnescolor() Test ===")
        print("Gets packed RGB integer (0xRRGGBB) for palette colors (0-63)")
        print("")
        
        -- First, check if getcolorrgb works at all
        print("=== IMPORTANT: Palette Initialization Check ===")
        local testRGB = getcolorrgb(0x20)  -- Bright white - should NOT be 0,0,0
        print(string.format("getcolorrgb(0x20) = {%d, %d, %d}", testRGB[1], testRGB[2], testRGB[3]))
        
        local testPacked = getnescolor(0x20)
        if testPacked < 0 then
            testPacked = testPacked + 0x1000000
        end
        print(string.format("getnescolor(0x20) = 0x%06X (%d)", testPacked, testPacked))
        
        if testRGB[1] == 0 and testRGB[2] == 0 and testRGB[3] == 0 then
            print("")
            print("*** ERROR: Palette is NOT initialized! ***")
            print("Both getcolorrgb() and getnescolor() return all zeros.")
            print("")
            print("SOLUTION:")
            print("1. Make sure a game/ROM is loaded")
            print("2. Wait a few frames for the palette to initialize")
            print("3. Try running the script again")
            print("")
            print("The palette is initialized when the game starts running.")
            print("If you see this message, the emulator may not have a game loaded yet.")
            print("")
            initialized = true  -- Skip further tests
            return  -- Exit early
        else
            print("OK: Palette is initialized - both functions can read colors")
            print("")
        end
        
        -- Test specific known NON-BLACK colors first
        local testColors = {0x10, 0x20, 0x16, 0x29, 0x37, 0x2A, 0x3B, 0x21, 0x2C}
        print("Testing specific NON-BLACK palette indices:")
        for _, idx in ipairs(testColors) do
            local rgb = getcolorrgb(idx)
            local packedRGB = getnescolor(idx)
            
            -- Handle potential negative values (Lua integers are signed)
            if packedRGB < 0 then
                packedRGB = packedRGB + 0x1000000  -- Convert to unsigned
            end
            
            local r = math.floor(packedRGB / 65536) % 256
            local g = math.floor(packedRGB / 256) % 256
            local b = packedRGB % 256
            
            print(string.format("Index %02X:", idx))
            print(string.format("  getcolorrgb: {%d, %d, %d}", rgb[1], rgb[2], rgb[3]))
            print(string.format("  getnescolor: 0x%08X (as number: %d)", packedRGB, packedRGB))
            print(string.format("  Extracted: R=%d G=%d B=%d", r, g, b))
            
            if rgb[1] == 0 and rgb[2] == 0 and rgb[3] == 0 then
                print(string.format("  NOTE: This color is actually black (0,0,0)"))
            end
            
            if r ~= rgb[1] or g ~= rgb[2] or b ~= rgb[3] then
                print(string.format("  ERROR: Mismatch!"))
                print(string.format("    Expected: R=%d G=%d B=%d", rgb[1], rgb[2], rgb[3]))
                print(string.format("    Got:      R=%d G=%d B=%d", r, g, b))
            else
                print(string.format("  OK: Values match"))
            end
            print("")
        end
        
        -- Test all indices and compare with getcolorrgb
        print("Testing all 64 palette indices...")
        local mismatchCount = 0
        local zeroCount = 0
        for i = 0, 63 do
            local rgb = getcolorrgb(i)
            local packedRGB = getnescolor(i)
            
            -- Handle potential negative values (Lua integers are signed)
            if packedRGB < 0 then
                packedRGB = packedRGB + 0x1000000  -- Convert to unsigned (24-bit)
            end
            
            -- Verify the packed value matches the RGB components
            local r = math.floor(packedRGB / 65536) % 256
            local g = math.floor(packedRGB / 256) % 256
            local b = packedRGB % 256
            
            if rgb[1] == 0 and rgb[2] == 0 and rgb[3] == 0 then
                zeroCount = zeroCount + 1
            end
            
            if r ~= rgb[1] or g ~= rgb[2] or b ~= rgb[3] then
                mismatchCount = mismatchCount + 1
                if mismatchCount <= 5 then  -- Only print first 5 mismatches
                    print(string.format("Mismatch at %02X: RGB(%d,%d,%d) vs Extracted(%d,%d,%d) Packed=0x%08X", 
                          i, rgb[1], rgb[2], rgb[3], r, g, b, packedRGB))
                end
            end
        end
        
        print(string.format("Summary: %d mismatches, %d indices with all zeros", mismatchCount, zeroCount))
        if zeroCount == 64 then
            print("WARNING: All palette indices return 0,0,0 - palette may not be initialized!")
            print("Make sure a game is loaded before running this test.")
        end
        print("")
        initialized = true
    end
    
    -- Check if palette is initialized first
    local testRGB = getcolorrgb(0x20)
    if testRGB[1] == 0 and testRGB[2] == 0 and testRGB[3] == 0 then
        -- Palette not initialized - show warning
        drawtext(4, 4, "PALETTE NOT INITIALIZED!", 0x16)
        drawtext(4, 14, "Load a game first!", 0x16)
        drawtext(4, 24, "Both functions return 0,0,0", 0x2E)
        drawtext(4, 34, "when no game is loaded.", 0x2E)
        return
    end
    
    -- Display header
    drawtext(4, 4, "getnescolor() Test", 0x20)
    drawtext(4, 14, "Packed RGB Values:", 0x2E)
    
    -- Display packed RGB values for test indices
    local yPos = 24
    for i, index in ipairs(testIndices) do
        -- Get values from both functions
        local rgb = getcolorrgb(index)
        local packedRGB = getnescolor(index)
        
        -- Handle potential negative values (Lua integers are signed)
        if packedRGB < 0 then
            packedRGB = packedRGB + 0x1000000  -- Convert to unsigned (24-bit)
        end
        
        -- Extract RGB components from packed value
        local r = math.floor(packedRGB / 65536) % 256
        local g = math.floor(packedRGB / 256) % 256
        local b = packedRGB % 256
        
        -- Display palette index
        drawtext(4, yPos, string.format("%02X:", index), 0x20)
        
        -- Display packed RGB (show as hex)
        drawtext(30, yPos, string.format("0x%06X", packedRGB), 0x29)
        
        -- Display RGB components from getcolorrgb
        drawtext(100, yPos, string.format("RGB(%3d,%3d,%3d)", rgb[1], rgb[2], rgb[3]), 0x37)
        
        -- Display extracted components
        drawtext(200, yPos, string.format("E(%3d,%3d,%3d)", r, g, b), 0x2E)
        
        -- Verify components match getcolorrgb
        if r == rgb[1] and g == rgb[2] and b == rgb[3] then
            drawtext(260, yPos, "✓", 0x29)
        else
            drawtext(260, yPos, "✗", 0x16)
            -- Show mismatch details
            print(string.format("Mismatch at %02X: RGB(%d,%d,%d) vs Extracted(%d,%d,%d) Packed=0x%06X", 
                  index, rgb[1], rgb[2], rgb[3], r, g, b, packedRGB))
        end
        
        -- Draw a color swatch using the palette color
        fillrect(270, yPos, 16, 8, index)
        
        yPos = yPos + 10
    end
    
    -- Display comparison with getcolorrgb - use a bright color
    yPos = yPos + 10
    drawtext(4, yPos, "Comparison (Bright White 0x20):", 0x2E)
    yPos = yPos + 10
    
    local testIndex = 0x20  -- Bright white - should NOT be 0,0,0
    local rgb = getcolorrgb(testIndex)
    local packedRGB = getnescolor(testIndex)
    
    -- Handle potential negative values
    if packedRGB < 0 then
        packedRGB = packedRGB + 0x1000000
    end
    
    drawtext(4, yPos, string.format("Index 0x%02X:", testIndex), 0x20)
    yPos = yPos + 10
    
    drawtext(4, yPos, string.format("getcolorrgb(): {%d, %d, %d}", 
          rgb[1], rgb[2], rgb[3]), 0x37)
    yPos = yPos + 10
    
    drawtext(4, yPos, string.format("getnescolor(): 0x%06X", packedRGB), 0x29)
    yPos = yPos + 10
    
    -- Extract and verify components
    local r = math.floor(packedRGB / 65536) % 256
    local g = math.floor(packedRGB / 256) % 256
    local b = packedRGB % 256
    
    drawtext(4, yPos, string.format("Extracted: R=%d G=%d B=%d", r, g, b), 0x2E)
    yPos = yPos + 10
    
    if r == rgb[1] and g == rgb[2] and b == rgb[3] then
        drawtext(4, yPos, "Match: ✓", 0x29)
    else
        drawtext(4, yPos, "Mismatch: ✗", 0x16)
    end
    
    -- Display current NON-BLACK palette index being analyzed (cycles through bright colors)
    local brightColors = {0x10, 0x20, 0x16, 0x29, 0x37, 0x2A, 0x3B, 0x21, 0x2C, 0x39}
    local colorIndex = math.floor(frameCounter / 60) % #brightColors
    local currentIndex = brightColors[colorIndex + 1]
    local currentRGB = getcolorrgb(currentIndex)
    local currentPackedRGB = getnescolor(currentIndex)
    
    -- Handle potential negative values (Lua integers are signed)
    if currentPackedRGB < 0 then
        currentPackedRGB = currentPackedRGB + 0x1000000  -- Convert to unsigned (24-bit)
    end
    
    drawtext(150, 4, string.format("Index: 0x%02X", currentIndex), 0x20)
    drawtext(150, 14, string.format("0x%06X", currentPackedRGB), 0x29)
    drawtext(150, 24, string.format("RGB: %d,%d,%d", 
          currentRGB[1], currentRGB[2], currentRGB[3]), 0x37)
    
    -- Show extracted components
    local cr = math.floor(currentPackedRGB / 65536) % 256
    local cg = math.floor(currentPackedRGB / 256) % 256
    local cb = currentPackedRGB % 256
    drawtext(150, 34, string.format("E: %d,%d,%d", cr, cg, cb), 0x2E)
    
    fillrect(150, 44, 32, 16, currentIndex)
    
    -- Display frame counter
    drawtext(4, 230, string.format("Frame: %d", frameCounter), 0x2E)
    
    -- Test error handling (only once, at start)
    if frameCounter == 1 then
        print("=== Error Handling Tests ===")
        
        -- Test invalid indices (should error)
        local success, err = pcall(function()
            getnescolor(-1)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index -1")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index -1")
        end
        
        success, err = pcall(function()
            getnescolor(64)  -- Should error
        end)
        if not success then
            print("✓ Correctly caught invalid index 64")
            print("  Error: " .. tostring(err))
        else
            print("✗ ERROR: Should have failed for index 64")
        end
        
        -- Test valid boundary indices (should succeed)
        success, err = pcall(function()
            local rgb = getnescolor(0)
            print("✓ Valid index 0: 0x" .. string.format("%06X", rgb))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 0: " .. tostring(err))
        end
        
        success, err = pcall(function()
            local rgb = getnescolor(63)
            print("✓ Valid index 63: 0x" .. string.format("%06X", rgb))
        end)
        if not success then
            print("✗ ERROR: Should have succeeded for index 63: " .. tostring(err))
        end
        
        print("")
    end
end

print("getnescolor() test script loaded")

