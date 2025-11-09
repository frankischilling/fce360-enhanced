-- Test script for getprgsize() function
-- Prints PRG-ROM size to console
-- Tests ROM analysis

local frameCount = 0
local lastPrgSize = -1
local lastRomName = ""

function gui()
    frameCount = frameCount + 1
    
    -- Get PRG-ROM size
    local prgSize = getprgsize()
    local romSize = getromsize()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== getprgsize() Test ===", 0x3F)
    y = y + 10
    
    if prgSize == 0 then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "PRG-ROM: 0 bytes", 0x20)
    else
        -- Format size
        local prgKB = prgSize / 1024
        local prgMB = prgKB / 1024
        local prgFormatted
        
        if prgMB >= 1.0 then
            prgFormatted = string.format("%.2f MB", prgMB)
        else
            prgFormatted = string.format("%.2f KB", prgKB)
        end
        
        -- Display ROM name
        if romName ~= "" then
            drawtext(4, y, "ROM: " .. romName, 0x2E)
            y = y + 10
        end
        
        -- Display PRG-ROM size
        drawtext(4, y, string.format("PRG-ROM: %d bytes", prgSize), 0x20)
        y = y + 10
        
        -- Display formatted size
        drawtext(4, y, "Formatted: " .. prgFormatted, 0x29)
        y = y + 10
        
        -- Display total ROM size for comparison
        if romSize > 0 then
            local chrSize = romSize - prgSize
            drawtext(4, y, string.format("Total: %d bytes", romSize), 0x2A)
            y = y + 10
            drawtext(4, y, string.format("CHR-ROM: %d bytes", chrSize), 0x2B)
        end
    end
    
    -- Print to console every 60 frames or when ROM changes
    if frameCount % 60 == 0 or prgSize ~= lastPrgSize or romName ~= lastRomName then
        print("=== getprgsize() Test ===")
        
        if prgSize == 0 then
            print("No ROM loaded")
            print("PRG-ROM: 0 bytes")
        else
            print("ROM: " .. (romName ~= "" and romName or "Unknown"))
            print(string.format("PRG-ROM Size: %d bytes", prgSize))
            
            local prgKB = prgSize / 1024
            local prgMB = prgKB / 1024
            if prgMB >= 1.0 then
                print(string.format("Formatted: %.2f MB (%.2f KB)", prgMB, prgKB))
            else
                print(string.format("Formatted: %.2f KB", prgKB))
            end
            
            -- Compare with total ROM size
            if romSize > 0 then
                local chrSize = romSize - prgSize
                print("--- ROM Breakdown ---")
                print(string.format("Total ROM: %d bytes", romSize))
                print(string.format("PRG-ROM: %d bytes (%.1f%%)", prgSize, (prgSize / romSize) * 100))
                print(string.format("CHR-ROM: %d bytes (%.1f%%)", chrSize, (chrSize / romSize) * 100))
            end
            
            -- Common PRG-ROM sizes
            print("--- Common PRG-ROM Sizes ---")
            if prgSize == 16384 then
                print("16KB PRG-ROM: Small game (NROM)")
            elseif prgSize == 32768 then
                print("32KB PRG-ROM: Small game (NROM)")
            elseif prgSize == 65536 then
                print("64KB PRG-ROM: Medium game")
            elseif prgSize == 131072 then
                print("128KB PRG-ROM: Medium-large game (MMC1)")
            elseif prgSize == 262144 then
                print("256KB PRG-ROM: Large game (MMC3)")
            elseif prgSize == 524288 then
                print("512KB PRG-ROM: Very large game")
            elseif prgSize >= 1048576 then
                print("1MB+ PRG-ROM: Extremely large game")
            else
                print(string.format("PRG-ROM size: %d bytes", prgSize))
            end
            
            -- Analysis
            print("--- Analysis ---")
            if prgSize < 16384 then
                print("WARNING: PRG-ROM < 16KB (very small)")
            elseif prgSize > 1048576 then
                print("WARNING: PRG-ROM > 1MB (very large)")
            else
                print("PRG-ROM size is within normal range")
            end
            
            -- Estimate game complexity
            if prgSize <= 32768 then
                print("Estimated complexity: Simple game")
            elseif prgSize <= 131072 then
                print("Estimated complexity: Medium game")
            elseif prgSize <= 262144 then
                print("Estimated complexity: Complex game")
            else
                print("Estimated complexity: Very complex game")
            end
        end
        
        print("========================")
        
        lastPrgSize = prgSize
        lastRomName = romName
    end
end

