-- Test script for getromname() function
-- Prints the current ROM filename to console

local frameCount = 0
local lastRomName = ""

function gui()
    frameCount = frameCount + 1
    
    -- Check every 60 frames to avoid spam
    if frameCount % 60 == 0 then
        local romName = getromname()
        
        -- Only print if ROM name changed
        if romName ~= lastRomName then
            print("=== getromname() Test ===")
            
            if romName == "" then
                print("No ROM loaded")
            else
                print("ROM filename: " .. romName)
                
                -- Check if it's NES or FDS
                local lowerName = string.lower(romName)
                if string.find(lowerName, "%.nes$") then
                    print("Type: NES")
                elseif string.find(lowerName, "%.fds$") then
                    print("Type: FDS")
                else
                    print("Type: Unknown")
                end
                
                -- Show filename length
                print(string.format("Length: %d chars", string.len(romName)))
            end
            
            print("========================")
            lastRomName = romName
        end
    end
end

