-- Test script for getaudiochannel() function
-- Tests all 5 NES audio channels: Pulse1, Pulse2, Triangle, Noise, DMC

local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
local lastChannelStates = {}
local testFrameCount = 0
local logInterval = 60  -- Log every 60 frames (~1 second at 60fps)

-- Initialize last states
for i = 0, 4 do
    lastChannelStates[i] = {enabled = false}
end

print("========================================")
print("Audio Channel API Test Script")
print("========================================")
print("Testing: getaudiochannel(channel)")
print("Channels: 0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC")
print("========================================")
print("Test logs will appear every 60 frames (~1 second)")
print("Check Lua console for detailed channel information")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking getaudiochannel() availability...")
    
    if type(getaudiochannel) == "function" then
        print("  [OK] getaudiochannel() is available")
        
        -- Test all channels on load
        for i = 0, 4 do
            local success, result = pcall(function()
                return getaudiochannel(i)
            end)
            
            if success then
                print(string.format("  [OK] getaudiochannel(%d) works", i))
            else
                print(string.format("  [FAIL] getaudiochannel(%d) error: %s", i, tostring(result)))
            end
        end
    else
        print("  [FAIL] getaudiochannel() is NOT available")
    end
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    if not getaudioenabled() then
        return
    end
    
    -- Test all channels periodically
    if testFrameCount % logInterval == 0 then
        print(string.format("[TEST] Frame %d - Channel Information:", testFrameCount))
        
        for channel = 0, 4 do
            local success, channelInfo = pcall(function()
                return getaudiochannel(channel)
            end)
            
            if success and channelInfo then
                local name = channelInfo.name or "Unknown"
                local enabled = channelInfo.enabled or false
                
                print(string.format("  Channel %d (%s):", channel, name))
                print(string.format("    Enabled: %s", enabled and "YES" or "NO"))
                
                -- Check for state changes
                if lastChannelStates[channel].enabled ~= enabled then
                    print(string.format("    [STATE CHANGE] Enabled: %s -> %s", 
                        lastChannelStates[channel].enabled and "YES" or "NO",
                        enabled and "YES" or "NO"))
                    lastChannelStates[channel].enabled = enabled
                end
                
                -- Channel-specific information
                if channel < 2 then
                    -- Pulse channels
                    if channelInfo.lengthCounter then
                        print(string.format("    Length Counter: %d", channelInfo.lengthCounter))
                    end
                    if channelInfo.period then
                        print(string.format("    Period: %d", channelInfo.period))
                    end
                    if channelInfo.dutyCycle ~= nil then
                        print(string.format("    Duty Cycle: %d", channelInfo.dutyCycle))
                    end
                    if channelInfo.volume ~= nil then
                        print(string.format("    Volume: %d", channelInfo.volume))
                    end
                    if channelInfo.sweepEnabled ~= nil then
                        print(string.format("    Sweep: %s", channelInfo.sweepEnabled and "ON" or "OFF"))
                    end
                elseif channel == 2 then
                    -- Triangle
                    if channelInfo.lengthCounter then
                        print(string.format("    Length Counter: %d", channelInfo.lengthCounter))
                    end
                    if channelInfo.period then
                        print(string.format("    Period: %d", channelInfo.period))
                    end
                    if channelInfo.linearCounterReload ~= nil then
                        print(string.format("    Linear Counter Reload: %d", channelInfo.linearCounterReload))
                    end
                elseif channel == 3 then
                    -- Noise
                    if channelInfo.lengthCounter then
                        print(string.format("    Length Counter: %d", channelInfo.lengthCounter))
                    end
                    if channelInfo.period ~= nil then
                        print(string.format("    Period: %d", channelInfo.period))
                    end
                    if channelInfo.volume ~= nil then
                        print(string.format("    Volume: %d", channelInfo.volume))
                    end
                    if channelInfo.loopNoise ~= nil then
                        print(string.format("    Loop Noise: %s", channelInfo.loopNoise and "YES" or "NO"))
                    end
                elseif channel == 4 then
                    -- DMC
                    if channelInfo.active ~= nil then
                        print(string.format("    Active: %s", channelInfo.active and "YES" or "NO"))
                    end
                    if channelInfo.remainingSize then
                        print(string.format("    Remaining Size: %d", channelInfo.remainingSize))
                    end
                    if channelInfo.sampleAddress then
                        print(string.format("    Sample Address: 0x%04X", channelInfo.sampleAddress))
                    end
                    if channelInfo.sampleLength then
                        print(string.format("    Sample Length: %d", channelInfo.sampleLength))
                    end
                    if channelInfo.loop ~= nil then
                        print(string.format("    Loop: %s", channelInfo.loop and "YES" or "NO"))
                    end
                end
            else
                print(string.format("  Channel %d: ERROR - %s", channel, tostring(channelInfo)))
            end
        end
        
        print("")  -- Empty line for readability
    end
    
    -- Update last states for change detection
    for channel = 0, 4 do
        local success, channelInfo = pcall(function()
            return getaudiochannel(channel)
        end)
        if success and channelInfo then
            lastChannelStates[channel].enabled = channelInfo.enabled or false
        end
    end
end

function gui()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "Audio Channel API Test", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see channel info", 0x29)
        return
    end
    
    local y = 16
    
    -- Display all channels
    for channel = 0, 4 do
        local success, channelInfo = pcall(function()
            return getaudiochannel(channel)
        end)
        
        if success and channelInfo then
            local name = channelInfo.name or "Unknown"
            local enabled = channelInfo.enabled or false
            local color = enabled and 0x27 or 0x10
            
            -- Channel name and status
            local statusText = string.format("%d: %s [%s]", channel, name, enabled and "ON" or "OFF")
            drawtext(4, y, statusText, color)
            y = y + 10
            
            -- Channel-specific info (compact display)
            if channel < 2 then
                -- Pulse channels
                if channelInfo.lengthCounter and channelInfo.lengthCounter > 0 then
                    drawtext(12, y, string.format("Len:%d Per:%d Vol:%d", 
                        channelInfo.lengthCounter or 0,
                        channelInfo.period or 0,
                        channelInfo.volume or 0), 0x37)
                    y = y + 10
                end
            elseif channel == 2 then
                -- Triangle
                if channelInfo.lengthCounter and channelInfo.lengthCounter > 0 then
                    drawtext(12, y, string.format("Len:%d Per:%d", 
                        channelInfo.lengthCounter or 0,
                        channelInfo.period or 0), 0x37)
                    y = y + 10
                end
            elseif channel == 3 then
                -- Noise
                if channelInfo.lengthCounter and channelInfo.lengthCounter > 0 then
                    drawtext(12, y, string.format("Len:%d Per:%d Vol:%d", 
                        channelInfo.lengthCounter or 0,
                        channelInfo.period or 0,
                        channelInfo.volume or 0), 0x37)
                    y = y + 10
                end
            elseif channel == 4 then
                -- DMC
                if channelInfo.active then
                    drawtext(12, y, string.format("Active Size:%d Addr:0x%04X", 
                        channelInfo.remainingSize or 0,
                        channelInfo.sampleAddress or 0), 0x37)
                    y = y + 10
                end
            end
            
            y = y + 2  -- Spacing between channels
        else
            drawtext(4, y, string.format("%d: ERROR", channel), 0x16)
            y = y + 12
        end
    end
    
    -- Instructions
    drawtext(4, y + 10, "Testing getaudiochannel(0-4)", 0x29)
    drawtext(4, y + 22, "Check console for detailed logs", 0x37)
end

