-- Test script for getromsize() function
-- Displays ROM size on screen and prints to console
-- Tests ROM validation and size checks

local frameCount = 0
local lastRomSize = -1
local lastRomName = ""

function gui()
    frameCount = frameCount + 1
    
    -- Get ROM size
    local romSize = getromsize()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== getromsize() Test ===", 0x3F)
    y = y + 10
    
    if romSize == 0 then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Size: 0 bytes", 0x20)
    else
        -- Format size
        local sizeKB = romSize / 1024
        local sizeMB = sizeKB / 1024
        local sizeFormatted
        
        if sizeMB >= 1.0 then
            sizeFormatted = string.format("%.2f MB", sizeMB)
        else
            sizeFormatted = string.format("%.2f KB", sizeKB)
        end
        
        -- Display ROM name
        if romName ~= "" then
            drawtext(4, y, "ROM: " .. romName, 0x2E)
            y = y + 10
        end
        
        -- Display size in bytes
        drawtext(4, y, string.format("Size: %d bytes", romSize), 0x20)
        y = y + 10
        
        -- Display formatted size
        drawtext(4, y, "Formatted: " .. sizeFormatted, 0x29)
        y = y + 10
        
        -- Display breakdown (PRG + CHR estimate)
        -- Note: We can't get exact PRG/CHR split from getromsize alone
        -- but we can show the total
        drawtext(4, y, string.format("Total ROM: %d bytes", romSize), 0x2A)
    end
    
    -- Print to console every 60 frames or when ROM changes
    if frameCount % 60 == 0 or romSize ~= lastRomSize or romName ~= lastRomName then
        print("=== getromsize() Test ===")
        
        if romSize == 0 then
            print("No ROM loaded")
            print("Size: 0 bytes")
        else
            print("ROM: " .. (romName ~= "" and romName or "Unknown"))
            print(string.format("Size: %d bytes", romSize))
            
            local sizeKB = romSize / 1024
            local sizeMB = sizeKB / 1024
            if sizeMB >= 1.0 then
                print(string.format("Formatted: %.2f MB (%.2f KB)", sizeMB, sizeKB))
            else
                print(string.format("Formatted: %.2f KB", sizeKB))
            end
            
            -- Validation examples
            print("--- Validation Examples ---")
            if romSize < 16384 then
                print("WARNING: ROM size < 16KB (very small)")
            elseif romSize > 4194304 then
                print("WARNING: ROM size > 4MB (very large)")
            else
                print("ROM size is within normal range")
            end
            
            -- Common ROM sizes for reference
            print("--- Common ROM Sizes ---")
            print("16KB PRG (NROM): ~16KB")
            print("32KB PRG (NROM): ~32KB")
            print("128KB PRG (MMC1): ~128KB")
            print("256KB PRG (MMC3): ~256KB")
            print("512KB PRG: ~512KB")
        end
        
        print("========================")
        
        lastRomSize = romSize
        lastRomName = romName
    end
end

