-- Test script for getmapper() function
-- Prints mapper number and information to console
-- Tests mapper-specific scripts and compatibility checks

local frameCount = 0
local lastMapper = -1
local lastRomName = ""

-- Common mapper names for reference
local mapperNames = {
    [0] = "NROM",
    [1] = "MMC1 (SxROM)",
    [2] = "UNROM",
    [3] = "CNROM",
    [4] = "MMC3 (TxROM)",
    [5] = "MMC5",
    [7] = "AOROM",
    [9] = "MMC2 (PxROM)",
    [10] = "MMC4 (PxROM)",
    [11] = "Color Dreams",
    [16] = "Bandai",
    [19] = "Namco 163",
    [21] = "VRC4",
    [22] = "VRC2",
    [23] = "VRC2",
    [24] = "VRC6",
    [25] = "VRC4",
    [26] = "VRC6",
    [34] = "BNROM / NINA-001",
    [66] = "GNROM",
    [69] = "FME-7 / Sunsoft",
    [71] = "Camerica",
    [78] = "Irem",
    [85] = "VRC7",
    [94] = "UN1ROM",
    [118] = "TLSROM",
    [119] = "TQROM",
    [159] = "Bandai",
    [232] = "Camerica"
}

function gui()
    frameCount = frameCount + 1
    
    -- Get mapper number
    local mapper = getmapper()
    local romName = getromname()
    
    -- Display on screen
    local y = 4
    drawtext(4, y, "=== getmapper() Test ===", 0x3F)
    y = y + 10
    
    if mapper == 0 and romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Mapper: 0", 0x20)
    else
        -- Display ROM name
        if romName ~= "" then
            drawtext(4, y, "ROM: " .. romName, 0x2E)
            y = y + 10
        end
        
        -- Display mapper number
        drawtext(4, y, string.format("Mapper: %d", mapper), 0x20)
        y = y + 10
        
        -- Display mapper name if known
        if mapperNames[mapper] then
            drawtext(4, y, "Name: " .. mapperNames[mapper], 0x29)
            y = y + 10
        else
            drawtext(4, y, "Name: Unknown", 0x2A)
        end
    end
    
    -- Print to console every 60 frames or when mapper/ROM changes
    if frameCount % 60 == 0 or mapper ~= lastMapper or romName ~= lastRomName then
        print("=== getmapper() Test ===")
        
        if mapper == 0 and romName == "" then
            print("No ROM loaded")
            print("Mapper: 0")
        else
            print("ROM: " .. (romName ~= "" and romName or "Unknown"))
            print(string.format("Mapper Number: %d", mapper))
            
            -- Display mapper name
            if mapperNames[mapper] then
                print("Mapper Name: " .. mapperNames[mapper])
            else
                print("Mapper Name: Unknown")
            end
            
            -- Mapper-specific information
            print("--- Mapper Information ---")
            if mapper == 0 then
                print("NROM: Simplest mapper, 16-32KB PRG-ROM")
            elseif mapper == 1 then
                print("MMC1: Supports bank switching, battery saves")
            elseif mapper == 4 then
                print("MMC3: Popular mapper, used in many games")
            elseif mapper >= 0 and mapper <= 255 then
                print(string.format("Mapper %d: Valid mapper number", mapper))
            else
                print("Invalid mapper number")
            end
            
            -- Compatibility check examples
            print("--- Compatibility Examples ---")
            if mapper == 0 then
                print("Compatible: Simple games, basic functionality")
            elseif mapper == 1 or mapper == 4 then
                print("Compatible: Common mapper, well-supported")
            elseif mapperNames[mapper] then
                print("Compatible: Known mapper, may have special features")
            else
                print("Compatible: Unknown mapper, may have limited support")
            end
        end
        
        print("========================")
        
        lastMapper = mapper
        lastRomName = romName
    end
end

