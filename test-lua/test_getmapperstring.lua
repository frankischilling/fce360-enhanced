-- Test script for getmapperstring() function
-- Prints mapper name to console
-- Tests displaying mapper info

local frameCount = 0
local lastMapperString = ""
local lastMapper = -1
local lastRomName = ""

function gui()
    frameCount = frameCount + 1
    
    -- Get mapper info
    local mapperString = getmapperstring()
    local mapper = getmapper()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== getmapperstring() Test ===", 0x3F)
    y = y + 10
    
    if mapperString == "" and romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Mapper: (none)", 0x20)
    else
        -- Display ROM name
        if romName ~= "" then
            drawtext(4, y, "ROM: " .. romName, 0x2E)
            y = y + 10
        end
        
        -- Display mapper number
        drawtext(4, y, string.format("Mapper: %d", mapper), 0x20)
        y = y + 10
        
        -- Display mapper name string
        if mapperString ~= "" then
            drawtext(4, y, "Name: " .. mapperString, 0x29)
        else
            drawtext(4, y, "Name: (empty)", 0x2A)
        end
    end
    
    -- Print to console every 60 frames or when mapper/ROM changes
    if frameCount % 60 == 0 or mapperString ~= lastMapperString or mapper ~= lastMapper or romName ~= lastRomName then
        print("=== getmapperstring() Test ===")
        
        if mapperString == "" and romName == "" then
            print("No ROM loaded")
            print("Mapper String: (empty)")
        else
            print("ROM: " .. (romName ~= "" and romName or "Unknown"))
            print(string.format("Mapper Number: %d", mapper))
            print("Mapper String: " .. (mapperString ~= "" and mapperString or "(empty)"))
            
            -- Compare with getmapper()
            print("--- Comparison ---")
            print(string.format("getmapper() = %d", mapper))
            print(string.format("getmapperstring() = \"%s\"", mapperString))
            
            -- Test different mapper types
            print("--- Mapper Information ---")
            if mapperString == "NROM" then
                print("NROM: Simplest mapper, 16-32KB PRG-ROM")
            elseif mapperString == "MMC1" then
                print("MMC1: Supports bank switching, battery saves")
            elseif mapperString == "MMC3" then
                print("MMC3: Popular mapper, used in many games")
            elseif mapperString:find("Mapper %d") or mapperString:find("^Mapper ") then
                print("Unknown mapper: " .. mapperString)
                print("This mapper is not in the lookup table")
            elseif mapperString ~= "" then
                print("Known mapper: " .. mapperString)
            else
                print("Mapper string is empty")
            end
            
            -- Test string operations
            print("--- String Operations Test ---")
            local strLen = string.len(mapperString)
            print(string.format("String length: %d", strLen))
            
            if mapperString ~= "" then
                local upper = string.upper(mapperString)
                local lower = string.lower(mapperString)
                print(string.format("Uppercase: %s", upper))
                print(string.format("Lowercase: %s", lower))
                
                -- Check if it contains "MMC"
                if string.find(mapperString, "MMC") then
                    print("Contains 'MMC' - Memory Management Controller")
                end
                
                -- Check if it contains "VRC"
                if string.find(mapperString, "VRC") then
                    print("Contains 'VRC' - Video & Sound Chip")
                end
            end
        end
        
        print("========================")
        
        lastMapperString = mapperString
        lastMapper = mapper
        lastRomName = romName
    end
end

