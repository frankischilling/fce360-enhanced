-- Test script for getaudiochannelsample() function
-- Tests channel-specific sample extraction for all 5 NES APU channels

local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
local channelColors = {0x27, 0x37, 0x2F, 0x3F, 0x1F}  -- Different colors for each channel
local lastSamples = {0, 0, 0, 0, 0}
local peakSamples = {0, 0, 0, 0, 0}
local testFrameCount = 0
local logInterval = 60  -- Log every 60 frames (~1 second at 60fps)
local waveformData = {}  -- Store waveform data for visualization

-- Initialize waveform buffers for each channel
for i = 0, 4 do
    waveformData[i] = {}
    for j = 1, 64 do
        waveformData[i][j] = 0
    end
end

print("========================================")
print("Channel-Specific Sample Extraction Test")
print("========================================")
print("Testing: getaudiochannelsample(channel)")
print("Channels: 0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC")
print("========================================")
print("This script extracts samples from individual channels")
print("before they are mixed into the final audio output.")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking getaudiochannelsample() availability...")
    
    if type(getaudiochannelsample) == "function" then
        print("  [OK] getaudiochannelsample() is available")
        
        -- Test all channels on load
        for i = 0, 4 do
            local success, result = pcall(function()
                return getaudiochannelsample(i)
            end)
            
            if success then
                print(string.format("  [OK] getaudiochannelsample(%d) works, sample: %d", i, result))
            else
                print(string.format("  [FAIL] getaudiochannelsample(%d) error: %s", i, tostring(result)))
            end
        end
    else
        print("  [FAIL] getaudiochannelsample() is NOT available")
    end
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    if not getaudioenabled() then
        return
    end
    
    -- Get samples from all channels
    for channel = 0, 4 do
        local success, sample = pcall(function()
            return getaudiochannelsample(channel)
        end)
        
        if success and sample then
            lastSamples[channel + 1] = sample
            
            -- Track peak samples
            local absSample = math.abs(sample)
            if absSample > math.abs(peakSamples[channel + 1]) then
                peakSamples[channel + 1] = sample
            end
            
            -- Update waveform buffer (circular buffer)
            table.insert(waveformData[channel], 1, sample)
            if #waveformData[channel] > 64 then
                table.remove(waveformData[channel])
            end
        end
    end
    
    -- Log periodically
    if testFrameCount % logInterval == 0 then
        print(string.format("[TEST] Frame %d - Channel Samples:", testFrameCount))
        
        -- Get mixed sample for comparison
        local mixedSample = getaudiosample()
        
        print(string.format("  Mixed (getaudiosample): %d", mixedSample))
        
        for channel = 0, 4 do
            local sample = lastSamples[channel + 1]
            local peak = peakSamples[channel + 1]
            local channelInfo = getaudiochannel(channel)
            local enabled = channelInfo and channelInfo.enabled or false
            
            print(string.format("  Channel %d (%s): Sample=%d, Peak=%d, Enabled=%s", 
                channel, channelNames[channel + 1], sample, peak, enabled and "YES" or "NO"))
        end
        
        print("")  -- Empty line for readability
    end
end

function gui()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "Channel Sample Extraction Test", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see channel samples", 0x29)
        return
    end
    
    local y = 16
    
    -- Display channel samples
    drawtext(4, y, "Channel Samples (before mixing):", 0x29)
    y = y + 12
    
    for channel = 0, 4 do
        local sample = lastSamples[channel + 1]
        local peak = peakSamples[channel + 1]
        local channelInfo = getaudiochannel(channel)
        local enabled = channelInfo and channelInfo.enabled or false
        local color = enabled and channelColors[channel + 1] or 0x10
        
        -- Channel name and sample value
        local sampleText = string.format("%d: %s = %d", channel, channelNames[channel + 1], sample)
        drawtext(4, y, sampleText, color)
        y = y + 10
        
        -- Peak value
        if peak ~= 0 then
            drawtext(12, y, string.format("Peak: %d", peak), 0x37)
            y = y + 10
        end
        
        -- Waveform visualization (simple bar)
        if enabled and sample ~= 0 then
            local barWidth = math.min(math.abs(sample) / 100, 100)  -- Scale to reasonable width
            local barX = 12
            local barY = y
            local barHeight = 4
            
            -- Draw waveform bar
            if sample > 0 then
                -- Positive: draw to the right
                for i = 0, barWidth do
                    drawpixel(barX + i, barY, color)
                    drawpixel(barX + i, barY + 1, color)
                end
            else
                -- Negative: draw to the left
                for i = 0, barWidth do
                    drawpixel(barX - i, barY, color)
                    drawpixel(barX - i, barY + 1, color)
                end
            end
            y = y + 6
        end
        
        y = y + 2  -- Spacing between channels
    end
    
    -- Comparison with mixed audio
    y = y + 10
    local mixedSample = getaudiosample()
    drawtext(4, y, string.format("Mixed Audio: %d", mixedSample), 0x27)
    y = y + 12
    
    -- Calculate sum of channel samples (approximate)
    local sumSamples = 0
    for i = 1, 5 do
        sumSamples = sumSamples + lastSamples[i]
    end
    drawtext(4, y, string.format("Sum of Channels: %d", sumSamples), 0x37)
    y = y + 12
    
    -- Instructions
    drawtext(4, y, "Testing getaudiochannelsample(0-4)", 0x29)
    drawtext(4, y + 12, "Check console for detailed logs", 0x37)
    
    -- Waveform display (simple oscilloscope view)
    y = y + 30
    drawtext(4, y, "Waveform View (last 64 samples):", 0x29)
    y = y + 12
    
    local waveformY = y
    local waveformHeight = 40
    local waveformWidth = 200
    local centerY = waveformY + waveformHeight / 2
    
    -- Draw center line
    for i = 0, waveformWidth do
        drawpixel(4 + i, centerY, 0x10)
    end
    
    -- Draw waveforms for each enabled channel
    for channel = 0, 4 do
        local channelInfo = getaudiochannel(channel)
        if channelInfo and channelInfo.enabled and #waveformData[channel] > 0 then
            local color = channelColors[channel + 1]
            
            -- Draw waveform line
            for i = 1, math.min(#waveformData[channel] - 1, waveformWidth) do
                local x1 = 4 + (i - 1) * (waveformWidth / 64)
                local x2 = 4 + i * (waveformWidth / 64)
                local y1 = centerY - (waveformData[channel][i] / 200)  -- Scale down
                local y2 = centerY - (waveformData[channel][i + 1] / 200)
                
                -- Simple line drawing (approximate)
                local dx = x2 - x1
                local dy = y2 - y1
                local steps = math.max(math.abs(dx), math.abs(dy))
                if steps > 0 then
                    for j = 0, steps do
                        local px = x1 + (dx * j / steps)
                        local py = y1 + (dy * j / steps)
                        if px >= 4 and px < 4 + waveformWidth and py >= waveformY and py < waveformY + waveformHeight then
                            drawpixel(px, py, color)
                        end
                    end
                end
            end
        end
    end
end

