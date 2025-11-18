-- Test script for getinesheader() function
-- Tests iNES header information retrieval
-- Displays header data and flags

local frameCount = 0
local lastRomName = ""
local lastHeader = nil

function script()
    frameCount = frameCount + 1
    
    local romName = getromname()
    local y = 4
    
    -- Display header
    drawtext(4, y, "=== getinesheader() Test ===", 0x3F)
    y = y + 10
    
    -- Check if ROM is loaded
    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Load a ROM to test", 0x2A)
        return
    end
    
    -- Get header
    local header = getinesheader()
    if header == nil then
        drawtext(4, y, "Header: nil", 0x37)
        return
    end
    
    -- Display ROM name
    drawtext(4, y, "ROM: " .. romName, 0x2E)
    y = y + 10
    
    -- Display basic info
    if header.mapper then
        drawtext(4, y, "Mapper: " .. header.mapper, 0x20)
        y = y + 10
    end
    
    if header.mirroring_string then
        drawtext(4, y, "Mirroring: " .. header.mirroring_string, 0x20)
        y = y + 10
    end
    
    if header.rom_size then
        drawtext(4, y, "PRG-ROM: " .. header.rom_size .. " x 16KB", 0x29)
        y = y + 10
    end
    
    if header.vrom_size then
        drawtext(4, y, "CHR-ROM: " .. header.vrom_size .. " x 8KB", 0x29)
        y = y + 10
    end
    
    -- Display flags
    y = y + 5
    drawtext(4, y, "Flags:", 0x2A)
    y = y + 10
    
    if header.has_battery then
        drawtext(4, y, "Battery: Yes", 0x2E)
    else
        drawtext(4, y, "Battery: No", 0x2A)
    end
    y = y + 10
    
    if header.has_trainer then
        drawtext(4, y, "Trainer: Yes", 0x2E)
    else
        drawtext(4, y, "Trainer: No", 0x2A)
    end
    y = y + 10
    
    if header.four_screen then
        drawtext(4, y, "Four-screen: Yes", 0x2E)
    else
        drawtext(4, y, "Four-screen: No", 0x2A)
    end
    
    -- Print to console when ROM changes or every 300 frames
    if romName ~= lastRomName or frameCount % 300 == 0 then
        print("=== getinesheader() Test ===")
        print("ROM: " .. romName)
        print("")
        
        if header == nil then
            print("Header: nil (no ROM loaded)")
            return
        end
        
        -- Basic header info
        print("--- Basic Header Information ---")
        if header.id then
            print("ID: " .. header.id)
        end
        if header.mapper then
            print("Mapper: " .. header.mapper)
        end
        if header.mirroring_string then
            print("Mirroring: " .. header.mirroring_string .. " (" .. header.mirroring .. ")")
        end
        if header.rom_size then
            print("PRG-ROM Size: " .. header.rom_size .. " x 16KB = " .. (header.rom_size * 16) .. " KB")
        end
        if header.vrom_size then
            print("CHR-ROM Size: " .. header.vrom_size .. " x 8KB = " .. (header.vrom_size * 8) .. " KB")
        end
        print("")
        
        -- ROM type flags
        print("--- ROM Type Flags ---")
        if header.rom_type then
            print("ROM_type: 0x" .. string.format("%02X", header.rom_type))
        end
        if header.rom_type2 then
            print("ROM_type2: 0x" .. string.format("%02X", header.rom_type2))
        end
        print("")
        
        -- Feature flags
        print("--- Feature Flags ---")
        print("Battery: " .. tostring(header.has_battery))
        print("Trainer: " .. tostring(header.has_trainer))
        print("Four-screen: " .. tostring(header.four_screen))
        print("VS System: " .. tostring(header.vs_system))
        print("PlayChoice-10: " .. tostring(header.playchoice10))
        print("NES 2.0 Format: " .. tostring(header.nes2_format))
        print("")
        
        -- Raw header bytes
        if header.raw_header then
            print("--- Raw Header Bytes (16 bytes) ---")
            local headerStr = ""
            for i = 1, 16 do
                local byte = header.raw_header[i]
                if byte then
                    headerStr = headerStr .. string.format("%02X ", byte)
                    if i % 8 == 0 then
                        print(headerStr)
                        headerStr = ""
                    end
                end
            end
            if headerStr ~= "" then
                print(headerStr)
            end
            print("")
        end
        
        -- Reserve bytes
        if header.reserve then
            print("--- Reserve Bytes (8 bytes) ---")
            local reserveStr = ""
            for i = 1, 8 do
                local byte = header.reserve[i]
                if byte then
                    reserveStr = reserveStr .. string.format("%02X ", byte)
                end
            end
            print(reserveStr)
            print("")
        end
        
        -- Header analysis
        print("--- Header Analysis ---")
        if header.mapper then
            local mapperName = getmapperstring()
            print("Mapper: " .. header.mapper .. " (" .. mapperName .. ")")
        end
        
        if header.mirroring then
            local mirroringDesc = "Unknown"
            if header.mirroring == 0 then
                mirroringDesc = "Horizontal (vertical mirroring)"
            elseif header.mirroring == 1 then
                mirroringDesc = "Vertical (horizontal mirroring)"
            elseif header.mirroring == 2 then
                mirroringDesc = "Four-screen VRAM"
            end
            print("Mirroring: " .. mirroringDesc)
        end
        
        if header.has_battery then
            print("Save RAM: Supported (battery-backed)")
        else
            print("Save RAM: Not supported")
        end
        
        if header.has_trainer then
            print("Trainer: Present (512 bytes at 0x7000-0x71FF)")
        else
            print("Trainer: Not present")
        end
        
        if header.four_screen then
            print("VRAM: Four-screen mode (8KB VRAM)")
        else
            print("VRAM: Standard (2KB VRAM)")
        end
        
        if header.vs_system then
            print("System: VS System (arcade)")
        elseif header.playchoice10 then
            print("System: PlayChoice-10")
        else
            print("System: Standard NES")
        end
        
        if header.nes2_format then
            print("Format: NES 2.0 (extended header)")
        else
            print("Format: iNES 1.0 (standard header)")
        end
        print("")
        
        -- Comparison with other functions
        print("--- Comparison with Other Functions ---")
        local mapperFromHeader = header.mapper
        local mapperFromFunc = getmapper()
        if mapperFromHeader == mapperFromFunc then
            print("Mapper match: PASS (header=" .. mapperFromHeader .. ", getmapper()=" .. mapperFromFunc .. ")")
        else
            print("Mapper match: FAIL (header=" .. mapperFromHeader .. ", getmapper()=" .. mapperFromFunc .. ")")
        end
        
        local batteryFromHeader = header.has_battery
        local batteryFromFunc = hasbattery()
        if batteryFromHeader == batteryFromFunc then
            print("Battery match: PASS")
        else
            print("Battery match: FAIL (header=" .. tostring(batteryFromHeader) .. ", hasbattery()=" .. tostring(batteryFromFunc) .. ")")
        end
        print("")
        
        print("========================")
        
        lastRomName = romName
        lastHeader = header
    end
end

