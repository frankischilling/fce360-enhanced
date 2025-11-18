-- Test script for getromhash(algorithm) function
-- Tests CRC32 and MD5 hash algorithms
-- Displays hash values and tests error handling

local frameCount = 0
local lastRomName = ""
local lastCRC32 = ""
local lastMD5 = ""
local testResults = {}

function gui()
    frameCount = frameCount + 1
    
    local romName = getromname()
    local y = 4
    
    -- Display header
    drawtext(4, y, "=== getromhash() Test ===", 0x3F)
    y = y + 10
    
    -- Check if ROM is loaded
    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Load a ROM to test", 0x2A)
        return
    end
    
    -- Display ROM name
    drawtext(4, y, "ROM: " .. romName, 0x2E)
    y = y + 10
    
    -- Test CRC32
    local crc32 = getromhash("crc32")
    if crc32 ~= "" then
        drawtext(4, y, "CRC32: " .. crc32, 0x20)
        y = y + 10
    else
        drawtext(4, y, "CRC32: (empty)", 0x37)
        y = y + 10
    end
    
    -- Test MD5
    local md5 = getromhash("md5")
    if md5 ~= "" then
        -- Display first 16 chars of MD5 on screen (full MD5 is 32 chars)
        drawtext(4, y, "MD5: " .. string.sub(md5, 1, 16) .. "...", 0x29)
        y = y + 10
        drawtext(4, y, "     " .. string.sub(md5, 17, 32), 0x29)
        y = y + 10
    else
        drawtext(4, y, "MD5: (empty)", 0x37)
        y = y + 10
    end
    
    -- Test additional algorithms
    y = y + 5
    drawtext(4, y, "Additional checksums:", 0x2A)
    y = y + 10
    
    -- Test sum (8-bit)
    local sum = getromhash("sum")
    if sum ~= "" then
        drawtext(4, y, "Sum (8-bit): " .. sum, 0x20)
        y = y + 10
    end
    
    -- Test sum16 (16-bit)
    local sum16 = getromhash("sum16")
    if sum16 ~= "" then
        drawtext(4, y, "Sum16: " .. sum16, 0x20)
        y = y + 10
    end
    
    -- Test XOR
    local xor = getromhash("xor")
    if xor ~= "" then
        drawtext(4, y, "XOR: " .. xor, 0x20)
        y = y + 10
    end
    
    -- Test CRC alias
    local crc = getromhash("crc")
    if crc ~= "" and crc == crc32 then
        drawtext(4, y, "CRC (alias): OK", 0x2E)
        y = y + 10
    elseif crc ~= "" then
        drawtext(4, y, "CRC (alias): FAIL", 0x2D)
        y = y + 10
    end
    
    -- Test case-insensitive algorithm names
    y = y + 5
    drawtext(4, y, "Case-insensitive test:", 0x2A)
    y = y + 10
    local crc32Upper = getromhash("CRC32")
    local md5Upper = getromhash("MD5")
    if crc32Upper == crc32 then
        drawtext(4, y, "CRC32 (upper): OK", 0x2E)
    else
        drawtext(4, y, "CRC32 (upper): FAIL", 0x2D)
    end
    y = y + 10
    if md5Upper == md5 then
        drawtext(4, y, "MD5 (upper): OK", 0x2E)
    else
        drawtext(4, y, "MD5 (upper): FAIL", 0x2D)
    end
    
    -- Test error handling (SHA1 and invalid)
    y = y + 10
    drawtext(4, y, "Error tests:", 0x2A)
    y = y + 10
    
    -- Test SHA1 (should error)
    local sha1Success, sha1Result = pcall(function()
        return getromhash("sha1")
    end)
    if not sha1Success then
        drawtext(4, y, "SHA1: Error (expected)", 0x2E)
    else
        drawtext(4, y, "SHA1: No error (unexpected)", 0x2D)
    end
    y = y + 10
    
    -- Test invalid algorithm (should error)
    local invalidSuccess, invalidResult = pcall(function()
        return getromhash("invalid")
    end)
    if not invalidSuccess then
        drawtext(4, y, "Invalid: Error (expected)", 0x2E)
    else
        drawtext(4, y, "Invalid: No error (unexpected)", 0x2D)
    end
    
    -- Print to console when ROM changes or every 300 frames
    if romName ~= lastRomName or crc32 ~= lastCRC32 or md5 ~= lastMD5 or frameCount % 300 == 0 then
        print("=== getromhash() Test ===")
        print("ROM: " .. romName)
        print("")
        
        -- CRC32
        print("--- CRC32 Hash ---")
        if crc32 ~= "" then
            print("Algorithm: crc32")
            print("Hash: " .. crc32)
            print("Length: " .. string.len(crc32) .. " characters")
            print("Format: 8-character hexadecimal string")
        else
            print("CRC32: (empty - no ROM loaded)")
        end
        print("")
        
        -- MD5
        print("--- MD5 Hash ---")
        if md5 ~= "" then
            print("Algorithm: md5")
            print("Hash: " .. md5)
            print("Length: " .. string.len(md5) .. " characters")
            print("Format: 32-character hexadecimal string")
        else
            print("MD5: (empty - no ROM loaded)")
        end
        print("")
        
        -- Additional checksums
        print("--- Additional Checksums ---")
        
        -- Sum (8-bit)
        local sum = getromhash("sum")
        if sum ~= "" then
            print("Algorithm: sum (8-bit checksum)")
            print("Hash: " .. sum)
            print("Length: " .. string.len(sum) .. " characters")
            print("Format: 2-character hexadecimal string")
        end
        print("")
        
        -- Sum16 (16-bit)
        local sum16 = getromhash("sum16")
        if sum16 ~= "" then
            print("Algorithm: sum16 (16-bit checksum)")
            print("Hash: " .. sum16)
            print("Length: " .. string.len(sum16) .. " characters")
            print("Format: 4-character hexadecimal string")
        end
        print("")
        
        -- XOR
        local xor = getromhash("xor")
        if xor ~= "" then
            print("Algorithm: xor (XOR checksum)")
            print("Hash: " .. xor)
            print("Length: " .. string.len(xor) .. " characters")
            print("Format: 2-character hexadecimal string")
        end
        print("")
        
        -- CRC alias test
        print("--- Algorithm Aliases ---")
        local crc = getromhash("crc")
        local checksum = getromhash("checksum")
        if crc == crc32 then
            print("CRC alias: PASS (crc == crc32)")
        else
            print("CRC alias: FAIL (crc != crc32)")
        end
        if checksum == sum then
            print("Checksum alias: PASS (checksum == sum)")
        else
            print("Checksum alias: FAIL (checksum != sum)")
        end
        print("")
        
        -- Case-insensitive test
        print("--- Case-Insensitive Test ---")
        local crc32Upper = getromhash("CRC32")
        local md5Upper = getromhash("MD5")
        local crc32Mixed = getromhash("Crc32")
        local md5Mixed = getromhash("Md5")
        
        if crc32Upper == crc32 and crc32Mixed == crc32 then
            print("CRC32 case-insensitive: PASS")
        else
            print("CRC32 case-insensitive: FAIL")
        end
        
        if md5Upper == md5 and md5Mixed == md5 then
            print("MD5 case-insensitive: PASS")
        else
            print("MD5 case-insensitive: FAIL")
        end
        print("")
        
        -- Error handling tests
        print("--- Error Handling Tests ---")
        
        -- Test SHA1 (should error)
        local sha1Success, sha1Result = pcall(function()
            return getromhash("sha1")
        end)
        if not sha1Success then
            print("SHA1 error handling: PASS (correctly returned error)")
            print("  Error: " .. tostring(sha1Result))
        else
            print("SHA1 error handling: FAIL (should have returned error)")
        end
        
        -- Test invalid algorithm (should error)
        local invalidSuccess, invalidResult = pcall(function()
            return getromhash("invalid")
        end)
        if not invalidSuccess then
            print("Invalid algorithm error handling: PASS (correctly returned error)")
            print("  Error: " .. tostring(invalidResult))
        else
            print("Invalid algorithm error handling: FAIL (should have returned error)")
        end
        
        -- Test empty algorithm (should error)
        local emptySuccess, emptyResult = pcall(function()
            return getromhash("")
        end)
        if not emptySuccess then
            print("Empty algorithm error handling: PASS (correctly returned error)")
            print("  Error: " .. tostring(emptyResult))
        else
            print("Empty algorithm error handling: FAIL (should have returned error)")
        end
        print("")
        
        -- Hash format validation
        print("--- Hash Format Validation ---")
        if crc32 ~= "" then
            if string.len(crc32) == 8 then
                print("CRC32 length: PASS (8 characters)")
            else
                print("CRC32 length: FAIL (expected 8, got " .. string.len(crc32) .. ")")
            end
            
            -- Check if all characters are hex
            local isHex = true
            for i = 1, string.len(crc32) do
                local c = string.sub(crc32, i, i)
                if not ((c >= "0" and c <= "9") or (c >= "a" and c <= "f")) then
                    isHex = false
                    break
                end
            end
            if isHex then
                print("CRC32 format: PASS (valid hexadecimal)")
            else
                print("CRC32 format: FAIL (contains non-hex characters)")
            end
        end
        
        if md5 ~= "" then
            if string.len(md5) == 32 then
                print("MD5 length: PASS (32 characters)")
            else
                print("MD5 length: FAIL (expected 32, got " .. string.len(md5) .. ")")
            end
            
            -- Check if all characters are hex
            local isHex = true
            for i = 1, string.len(md5) do
                local c = string.sub(md5, i, i)
                if not ((c >= "0" and c <= "9") or (c >= "a" and c <= "f")) then
                    isHex = false
                    break
                end
            end
            if isHex then
                print("MD5 format: PASS (valid hexadecimal)")
            else
                print("MD5 format: FAIL (contains non-hex characters)")
            end
        end
        print("")
        
        -- ROM identification example
        print("--- ROM Identification Example ---")
        if crc32 ~= "" and md5 ~= "" then
            print("Use case: ROM identification and verification")
            print("CRC32: " .. crc32 .. " (quick identification)")
            print("MD5: " .. md5 .. " (precise verification)")
            print("")
            print("These hashes can be used to:")
            print("  - Identify ROMs in databases")
            print("  - Verify ROM integrity")
            print("  - Detect ROM changes")
            print("  - Match against known good dumps")
        end
        print("")
        
        print("========================")
        
        lastRomName = romName
        lastCRC32 = crc32
        lastMD5 = md5
    end
end

