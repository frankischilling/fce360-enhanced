-- Test script for getchrsize() function
-- Prints CHR-ROM size to console
-- Tests ROM analysis

local frameCount = 0
local lastChrSize = -1
local lastRomName = ""

function gui()
    frameCount = frameCount + 1
    
    -- Get CHR-ROM size
    local chrSize = getchrsize()
    local prgSize = getprgsize()
    local romSize = getromsize()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== getchrsize() Test ===", 0x3F)
    y = y + 10
    
    if chrSize == 0 and romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "CHR-ROM: 0 bytes", 0x20)
    else
        -- Format size
        local chrKB = chrSize / 1024
        local chrMB = chrKB / 1024
        local chrFormatted
        
        if chrMB >= 1.0 then
            chrFormatted = string.format("%.2f MB", chrMB)
        else
            chrFormatted = string.format("%.2f KB", chrKB)
        end
        
        -- Display ROM name
        if romName ~= "" then
            drawtext(4, y, "ROM: " .. romName, 0x2E)
            y = y + 10
        end
        
        -- Display CHR-ROM size
        drawtext(4, y, string.format("CHR-ROM: %d bytes", chrSize), 0x20)
        y = y + 10
        
        -- Display formatted size
        drawtext(4, y, "Formatted: " .. chrFormatted, 0x29)
        y = y + 10
        
        -- Display total ROM size for comparison
        if romSize > 0 then
            drawtext(4, y, string.format("Total: %d bytes", romSize), 0x2A)
            y = y + 10
            if prgSize > 0 then
                drawtext(4, y, string.format("PRG-ROM: %d bytes", prgSize), 0x2B)
            end
        end
    end
    
    -- Print to console only when ROM changes
    if chrSize ~= lastChrSize or romName ~= lastRomName then
        print("=== getchrsize() Test ===")
        
        if chrSize == 0 and romName == "" then
            print("No ROM loaded")
            print("CHR-ROM: 0 bytes")
        else
            print("ROM: " .. (romName ~= "" and romName or "Unknown"))
            print(string.format("CHR-ROM Size: %d bytes", chrSize))
            
            local chrKB = chrSize / 1024
            local chrMB = chrKB / 1024
            if chrMB >= 1.0 then
                print(string.format("Formatted: %.2f MB (%.2f KB)", chrMB, chrKB))
            else
                print(string.format("Formatted: %.2f KB", chrKB))
            end
            
            -- Compare with total ROM size
            if romSize > 0 then
                print("--- ROM Breakdown ---")
                print(string.format("Total ROM: %d bytes", romSize))
                if prgSize > 0 then
                    print(string.format("PRG-ROM: %d bytes (%.1f%%)", prgSize, (prgSize / romSize) * 100))
                end
                print(string.format("CHR-ROM: %d bytes (%.1f%%)", chrSize, (chrSize / romSize) * 100))
            end
            
            -- Common CHR-ROM sizes
            print("--- Common CHR-ROM Sizes ---")
            if chrSize == 0 then
                print("0KB CHR-ROM: Uses CHR-RAM (mapper provides RAM instead of ROM)")
            elseif chrSize == 8192 then
                print("8KB CHR-ROM: Small graphics set")
            elseif chrSize == 16384 then
                print("16KB CHR-ROM: Medium graphics set")
            elseif chrSize == 32768 then
                print("32KB CHR-ROM: Large graphics set")
            elseif chrSize == 65536 then
                print("64KB CHR-ROM: Very large graphics set")
            elseif chrSize == 131072 then
                print("128KB CHR-ROM: Extremely large graphics set")
            elseif chrSize >= 262144 then
                print("256KB+ CHR-ROM: Massive graphics set")
            else
                print(string.format("CHR-ROM size: %d bytes", chrSize))
            end
            
            -- Analysis
            print("--- Analysis ---")
            if chrSize == 0 then
                print("Note: CHR-RAM mode (no CHR-ROM, uses RAM instead)")
                print("This is common in mappers that support CHR-RAM")
            elseif chrSize < 8192 then
                print("WARNING: CHR-ROM < 8KB (very small)")
            elseif chrSize > 131072 then
                print("WARNING: CHR-ROM > 128KB (very large)")
            else
                print("CHR-ROM size is within normal range")
            end
            
            -- Estimate graphics complexity
            if chrSize == 0 then
                print("Graphics: CHR-RAM (dynamic graphics)")
            elseif chrSize <= 16384 then
                print("Estimated graphics: Simple graphics set")
            elseif chrSize <= 32768 then
                print("Estimated graphics: Medium graphics set")
            elseif chrSize <= 65536 then
                print("Estimated graphics: Complex graphics set")
            else
                print("Estimated graphics: Very complex graphics set")
            end
            
            -- Check if PRG + CHR = Total
            if prgSize > 0 and romSize > 0 then
                local calculatedTotal = prgSize + chrSize
                print("--- Verification ---")
                print(string.format("PRG-ROM + CHR-ROM = %d bytes", calculatedTotal))
                print(string.format("Total ROM = %d bytes", romSize))
                if calculatedTotal == romSize then
                    print("✓ Sizes match correctly!")
                else
                    print(string.format("⚠ Difference: %d bytes", math.abs(calculatedTotal - romSize)))
                end
            end
        end
        
        print("========================")
        
        lastChrSize = chrSize
        lastRomName = romName
    end
end

