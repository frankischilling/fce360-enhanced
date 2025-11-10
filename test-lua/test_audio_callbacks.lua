-- Test script for audio event callbacks
-- Tests onaudiochannelchange() callback function

local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
local channelColors = {0x27, 0x37, 0x2F, 0x3F, 0x1F}
local eventHistory = {}  -- Store event history
local maxHistory = 20
local channelStates = {false, false, false, false, false}  -- Current state of each channel
local eventCount = 0

print("========================================")
print("Audio Event Callbacks Test Script")
print("========================================")
print("Testing: onaudiochannelchange(channel, enabled)")
print("This callback is triggered when APU channels")
print("are enabled or disabled.")
print("========================================")

function onload()
    print("[TEST] Audio event callback system initialized")
    print("[TEST] Define onaudiochannelchange() to receive events")
    print("========================================")
    
    -- Initialize channel states
    if getaudioenabled() then
        for i = 0, 4 do
            local ch = getaudiochannel(i)
            if ch then
                channelStates[i + 1] = ch.enabled or false
            end
        end
    end
end

-- Audio event callback - called automatically when channel state changes
function onaudiochannelchange(channel, enabled)
    eventCount = eventCount + 1
    local name = channelNames[channel + 1] or "Unknown"
    local timestamp = getframecount() or 0
    
    -- Update channel state
    channelStates[channel + 1] = enabled
    
    -- Add to event history
    table.insert(eventHistory, 1, {
        channel = channel,
        name = name,
        enabled = enabled,
        frame = timestamp,
        eventNum = eventCount
    })
    
    if #eventHistory > maxHistory then
        table.remove(eventHistory)
    end
    
    -- Log the event
    print(string.format("[EVENT #%d] Frame %d: Channel %d (%s) %s", 
        eventCount, timestamp, channel, name, enabled and "ENABLED" or "DISABLED"))
    
    -- Get additional channel info
    if getaudioenabled() then
        local ch = getaudiochannel(channel)
        if ch then
            if channel < 2 then
                -- Pulse channels
                print(string.format("  -> Period: %d, Volume: %d, Length: %d", 
                    ch.period or 0, ch.volume or 0, ch.lengthCounter or 0))
            elseif channel == 2 then
                -- Triangle
                print(string.format("  -> Period: %d, Length: %d", 
                    ch.period or 0, ch.lengthCounter or 0))
            elseif channel == 3 then
                -- Noise
                print(string.format("  -> Period: %d, Volume: %d, Length: %d", 
                    ch.period or 0, ch.volume or 0, ch.lengthCounter or 0))
            elseif channel == 4 then
                -- DMC
                print(string.format("  -> Active: %s, Remaining: %d bytes", 
                    ch.active and "YES" or "NO", ch.remainingSize or 0))
            end
        end
    end
end

function script()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "Audio Event Callbacks Test", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see events", 0x29)
        return
    end
    
    local y = 16
    
    -- Display current channel states
    drawtext(4, y, "Current Channel States:", 0x29)
    y = y + 12
    
    for i = 0, 4 do
        local enabled = channelStates[i + 1]
        local color = enabled and channelColors[i + 1] or 0x10
        local name = channelNames[i + 1]
        
        drawtext(4, y, string.format("%d: %s [%s]", i, name, enabled and "ON" or "OFF"), color)
        y = y + 10
    end
    
    y = y + 10
    
    -- Display event count
    drawtext(4, y, string.format("Total Events: %d", eventCount), 0x27)
    y = y + 12
    
    -- Display recent event history
    if #eventHistory > 0 then
        drawtext(4, y, "Recent Events:", 0x29)
        y = y + 12
        
        local displayCount = math.min(8, #eventHistory)
        for i = 1, displayCount do
            local event = eventHistory[i]
            if event then
                local color = event.enabled and channelColors[event.channel + 1] or 0x10
                local status = event.enabled and "ON" or "OFF"
                drawtext(4, y, string.format("#%d: %s %s (Frame %d)", 
                    event.eventNum, event.name, status, event.frame), color)
                y = y + 10
            end
        end
    else
        drawtext(4, y, "No events yet - waiting for channel changes...", 0x37)
        y = y + 12
    end
    
    y = y + 10
    
    -- Instructions
    drawtext(4, y, "Callback: onaudiochannelchange()", 0x29)
    drawtext(4, y + 12, "Check console for detailed event logs", 0x37)
    
    -- Visual indicator when events occur
    if #eventHistory > 0 then
        local latestEvent = eventHistory[1]
        if latestEvent then
            local flashFrame = getframecount() or 0
            local framesSinceEvent = flashFrame - latestEvent.frame
            if framesSinceEvent < 30 then  -- Flash for 30 frames (~0.5 seconds)
                local alpha = math.max(0, 1.0 - (framesSinceEvent / 30.0))
                local color = latestEvent.enabled and channelColors[latestEvent.channel + 1] or 0x10
                
                -- Draw flashing indicator
                local indicatorX = 200
                local indicatorY = 4
                drawtext(indicatorX, indicatorY, "EVENT!", color)
            end
        end
    end
end

