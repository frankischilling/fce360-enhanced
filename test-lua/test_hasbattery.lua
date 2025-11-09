-- Test script for hasbattery() function
-- Prints battery status to console
-- Tests save state detection

local lastBattery = nil
local lastRomName = ""

function gui()
    -- Get battery status
    local hasBattery = hasbattery()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== hasbattery() Test ===", 0x3F)
    y = y + 10
    
    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Battery: N/A", 0x20)
    else
        -- Display ROM name
        drawtext(4, y, "ROM: " .. romName, 0x2E)
        y = y + 10
        
        -- Display battery status
        if hasBattery then
            drawtext(4, y, "Battery: Yes", 0x29)
            y = y + 10
            drawtext(4, y, "Save RAM: Supported", 0x2E)
        else
            drawtext(4, y, "Battery: No", 0x37)
            y = y + 10
            drawtext(4, y, "Save RAM: Not supported", 0x2A)
        end
    end
    
    -- Print to console only when ROM changes
    if hasBattery ~= lastBattery or romName ~= lastRomName then
        print("=== hasbattery() Test ===")
        
        if romName == "" then
            print("No ROM loaded")
            print("Battery: N/A")
        else
            print("ROM: " .. romName)
            print(string.format("Battery: %s", hasBattery and "Yes" or "No"))
            
            print("--- Battery Information ---")
            if hasBattery then
                print("Battery-backed save RAM: Supported")
                print("This ROM can save game progress to persistent storage")
                print("Save files will be preserved between sessions")
            else
                print("Battery-backed save RAM: Not supported")
                print("This ROM does not support persistent saves")
                print("Game progress cannot be saved permanently")
            end
            
            -- Additional ROM info
            local mapper = getmapper()
            local mapperString = getmapperstring()
            print("--- Related Information ---")
            print(string.format("Mapper: %d (%s)", mapper, mapperString))
            
            -- Common mappers with battery
            if hasBattery then
                print("Note: Games with battery typically include:")
                print("  - Password systems")
                print("  - High score tables")
                print("  - Game progress saves")
                print("  - Configuration settings")
            end
        end
        
        print("========================")
        
        lastBattery = hasBattery
        lastRomName = romName
    end
end

