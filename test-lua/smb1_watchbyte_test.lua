-- SMB1 watchbyte() Test Script
-- Watches Super Mario Bros 1 memory addresses and reports changes

local changeCount = 0
local lastChangeTime = 0
local changeLog = {}
local maxLogEntries = 10
local watchesSetup = false

-- Callback function called when watched addresses change
function onwatch(address, oldValue, newValue)
    changeCount = changeCount + 1
    lastChangeTime = changeCount
    
    -- Add to log (keep only recent entries)
    table.insert(changeLog, {
        addr = address,
        old = oldValue,
        new = newValue,
        frame = changeCount
    })
    if #changeLog > maxLogEntries then
        table.remove(changeLog, 1)
    end
    
    -- Print to console
    local addrName = "Unknown"
    if address == 0x075A then addrName = "Lives"
    elseif address == 0x075E then addrName = "Coins"
    elseif address == 0x07DE then addrName = "Score[0]"
    elseif address == 0x07DF then addrName = "Score[1]"
    elseif address == 0x07E0 then addrName = "Score[2]"
    elseif address == 0x0756 then addrName = "PowerUp"
    elseif address == 0x075F then addrName = "WorldLevel"
    end
    
    -- Format display values for lives (need to add 1)
    local oldDisplay = oldValue
    local newDisplay = newValue
    if address == 0x075A then
        oldDisplay = oldValue + 1
        newDisplay = newValue + 1
    end
    
    print(string.format("WATCH: 0x%04X (%s) changed: 0x%02X -> 0x%02X (%d -> %d)", 
          address, addrName, oldValue, newValue, oldDisplay, newDisplay))
end

function script()
    local y = 4
    
    -- Setup watches on first run
    if not watchesSetup then
        -- Watch SMB1 addresses
        watchbyte(0x075A)  -- Lives
        watchbyte(0x075E)  -- Coins
        watchbyte(0x07DE)  -- Score (tens of thousands)
        watchbyte(0x07DF)  -- Score (thousands)
        watchbyte(0x07E0)  -- Score (hundreds)
        watchbyte(0x0756)  -- Power-up state
        watchbyte(0x075F)  -- World/Level
        
        print("watchbyte() Test: Watching SMB1 addresses")
        print("  Lives: 0x075A")
        print("  Coins: 0x075E")
        print("  Score: 0x07DE-0x07E0")
        print("  PowerUp: 0x0756")
        print("  WorldLevel: 0x075F")
        watchesSetup = true
    end
    
    -- Display header
    drawtext(4, y, "watchbyte() Test - SMB1", 0x2E)
    y = y + 8
    
    -- Display current values
    drawtext(4, y, "Watching:", 0x20)
    y = y + 8
    
    local livesRaw = readbyte(0x075A)
    local lives = livesRaw + 1  -- SMB1 stores lives as (displayed - 1)
    local coins = readbyte(0x075E)  -- Coins are stored directly
    local powerUp = readbyte(0x0756)
    local worldLevel = readbyte(0x075F)
    local scoreHigh = readbyte(0x07DE)
    local scoreMid = readbyte(0x07DF)
    local scoreLow = readbyte(0x07E0)
    
    drawtext(4, y, string.format("Lives: %d (raw: 0x%02X)", lives, livesRaw), 0x20)
    y = y + 8
    drawtext(4, y, string.format("Coins: %d (0x%02X)", coins, coins), 0x20)
    y = y + 8
    drawtext(4, y, string.format("PowerUp: %d (0x%02X)", powerUp, powerUp), 0x20)
    y = y + 8
    drawtext(4, y, string.format("World/Level: 0x%02X", worldLevel), 0x20)
    y = y + 8
    drawtext(4, y, string.format("Score: %d-%d-%d", scoreHigh, scoreMid, scoreLow), 0x20)
    y = y + 8
    
    -- Display change statistics
    y = y + 4
    drawtext(4, y, string.format("Total changes: %d", changeCount), changeCount > 0 and 0x28 or 0x20)
    y = y + 8
    
    -- Display recent changes log
    if #changeLog > 0 then
        drawtext(4, y, "Recent changes:", 0x37)
        y = y + 8
        
        local startIdx = math.max(1, #changeLog - 4)  -- Show last 5 entries
        for i = startIdx, #changeLog do
            local entry = changeLog[i]
            local addrName = "0x" .. string.format("%04X", entry.addr)
            if entry.addr == 0x075A then addrName = "Lives"
            elseif entry.addr == 0x075E then addrName = "Coins"
            elseif entry.addr == 0x07DE then addrName = "Score[0]"
            elseif entry.addr == 0x07DF then addrName = "Score[1]"
            elseif entry.addr == 0x07E0 then addrName = "Score[2]"
            elseif entry.addr == 0x0756 then addrName = "PowerUp"
            elseif entry.addr == 0x075F then addrName = "W/L"
            end
            
            drawtext(4, y, string.format("%s: %d->%d", addrName, entry.old, entry.new), 0x16)
            y = y + 8
            if y > 220 then break end
        end
    end
    
    -- Instructions
    y = 220
    drawtext(4, y, "Watch for changes!", 0x2E)
end

