-- Test script for Audio API functions (v0.7.8)
-- Tests: getaudioenabled(), getaudiosample(), getaudiobuffer(), getaudiosampleleft(), getaudiosampleright()

local lastAudioState = nil
local peakSample = 0
local sampleHistory = {}
local historySize = 100
local testFrameCount = 0
local logInterval = 60  -- Log every 60 frames (~1 second at 60fps)

-- Initialize sample history
for i = 1, historySize do
    sampleHistory[i] = 0
end

print("========================================")
print("Audio API Test Script (v0.7.8)")
print("========================================")
print("Testing functions:")
print("  - getaudioenabled()")
print("  - getaudiosample() / getaudiosample(index)")
print("  - getaudiobuffer(count)")
print("  - getaudiosampleleft() / getaudiosampleright()")
print("========================================")
print("Test logs will appear every 60 frames (~1 second)")
print("Check Lua console for detailed test output")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking function availability...")
    
    local functions = {
        "getaudioenabled",
        "getaudiosample",
        "getaudiobuffer",
        "getaudiosampleleft",
        "getaudiosampleright"
    }
    
    local allAvailable = true
    for _, funcName in ipairs(functions) do
        if type(_G[funcName]) == "function" then
            print(string.format("  [OK] %s() is available", funcName))
        else
            print(string.format("  [FAIL] %s() is NOT available", funcName))
            allAvailable = false
        end
    end
    
    if allAvailable then
        print("[TEST] All audio API functions are available!")
    else
        print("[TEST] WARNING: Some functions are missing!")
    end
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    -- Test getaudioenabled()
    local audioEnabled = getaudioenabled()
    
    -- Log state changes
    if lastAudioState == nil then
        lastAudioState = audioEnabled
        print(string.format("[TEST] Audio state: %s", audioEnabled and "ENABLED" or "DISABLED"))
        if audioEnabled then
            print("[TEST] Starting audio API tests...")
        else
            print("[TEST] Audio disabled - tests will run when audio is enabled")
        end
    elseif audioEnabled ~= lastAudioState then
        print(string.format("[TEST] Audio state changed: %s -> %s", 
            lastAudioState and "ENABLED" or "DISABLED",
            audioEnabled and "ENABLED" or "DISABLED"))
        lastAudioState = audioEnabled
        if audioEnabled then
            print("[TEST] Resuming audio API tests...")
        else
            print("[TEST] Audio disabled - pausing tests")
        end
    end
    
    if not audioEnabled then
        -- Reset peak when audio disabled
        if peakSample > 0 then
            print("[TEST] Audio disabled - resetting peak sample")
            peakSample = 0
        end
        return
    end
    
    -- Test getaudiosample() - backward compatible (no parameter = last sample)
    local lastSample = getaudiosample()
    
    -- Test getaudiosample(index) - with index parameter
    local firstSample = getaudiosample(0)  -- Oldest sample
    local middleSample = getaudiosample(-10)  -- Middle sample (negative index)
    
    -- Test getaudiosampleleft() and getaudiosampleright()
    local leftSample = getaudiosampleleft()
    local rightSample = getaudiosampleright()
    
    -- Test getaudiobuffer() - log periodically
    if testFrameCount % logInterval == 0 then
        local buffer = getaudiobuffer(16)
        local bufferCount = buffer and #buffer or 0
        
        print(string.format("[TEST] Frame %d - Sample values:", testFrameCount))
        print(string.format("  getaudiosample() = %d (last sample)", lastSample))
        print(string.format("  getaudiosample(0) = %d (first sample)", firstSample))
        print(string.format("  getaudiosample(-10) = %d (middle sample)", middleSample))
        print(string.format("  getaudiosampleleft() = %d", leftSample))
        print(string.format("  getaudiosampleright() = %d", rightSample))
        print(string.format("  getaudiobuffer(16) = %d samples", bufferCount))
        
        if bufferCount > 0 then
            local minVal, maxVal = buffer[1], buffer[1]
            local sum = 0
            for i = 1, bufferCount do
                local val = buffer[i]
                sum = sum + math.abs(val)
                if val < minVal then minVal = val end
                if val > maxVal then maxVal = val end
            end
            local avg = sum / bufferCount
            print(string.format("  Buffer stats: min=%d, max=%d, avg=%.1f", minVal, maxVal, avg))
        end
    end
    
    -- Track peak sample
    local magnitude = math.abs(lastSample)
    if magnitude > peakSample then
        local oldPeak = peakSample
        peakSample = magnitude
        if oldPeak == 0 or magnitude > oldPeak * 1.1 then  -- Log significant increases
            print(string.format("[TEST] New peak sample: %d (was %d)", magnitude, oldPeak))
        end
    end
    
    -- Update sample history for waveform display
    table.insert(sampleHistory, 1, lastSample)
    if #sampleHistory > historySize then
        table.remove(sampleHistory)
    end
end

function gui()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "Audio API Test (v0.7.8)", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see test results", 0x29)
        return
    end
    
    -- Test 1: getaudiosample() - last sample (backward compatible)
    local lastSample = getaudiosample()
    drawtext(4, 16, string.format("Last Sample: %d", lastSample), 0x29)
    
    -- Test 2: getaudiosample(index) - with index
    local firstSample = getaudiosample(0)
    local middleSample = getaudiosample(-10)
    drawtext(4, 28, string.format("First (0): %d", firstSample), 0x37)
    drawtext(4, 40, string.format("Middle (-10): %d", middleSample), 0x37)
    
    -- Test 3: getaudiosampleleft() and getaudiosampleright()
    local leftSample = getaudiosampleleft()
    local rightSample = getaudiosampleright()
    drawtext(4, 52, string.format("Left: %d  Right: %d", leftSample, rightSample), 0x2E)
    
    -- Test 4: Peak detection
    drawtext(4, 64, string.format("Peak: %d", peakSample), 0x27)
    
    -- Test 5: getaudiobuffer() - get multiple samples
    local buffer = getaudiobuffer(32)  -- Get 32 samples
    if buffer and #buffer > 0 then
        local bufferAvg = 0
        for i = 1, #buffer do
            bufferAvg = bufferAvg + math.abs(buffer[i])
        end
        bufferAvg = bufferAvg / #buffer
        drawtext(4, 76, string.format("Buffer (32): Avg %d", math.floor(bufferAvg)), 0x2D)
    end
    
    -- Visual waveform display using sample history
    local waveformY = 100
    local waveformHeight = 60
    local waveformWidth = 200
    
    -- Draw waveform background
    fillrect(4, waveformY, waveformWidth, waveformHeight, 0x10)
    drawrect(4, waveformY, waveformWidth, waveformHeight, 0x2D)
    
    -- Draw center line
    local centerY = waveformY + waveformHeight / 2
    drawline(4, centerY, 4 + waveformWidth, centerY, 0x20)
    
    -- Draw waveform
    if #sampleHistory > 1 then
        local stepX = waveformWidth / (historySize - 1)
        local scale = waveformHeight / 65536  -- Scale to fit ±32768 range
        
        for i = 1, #sampleHistory - 1 do
            local x1 = 4 + (i - 1) * stepX
            local x2 = 4 + i * stepX
            local y1 = centerY - (sampleHistory[i] * scale)
            local y2 = centerY - (sampleHistory[i + 1] * scale)
            
            -- Clamp to waveform bounds
            if y1 < waveformY then y1 = waveformY end
            if y1 > waveformY + waveformHeight then y1 = waveformY + waveformHeight end
            if y2 < waveformY then y2 = waveformY end
            if y2 > waveformY + waveformHeight then y2 = waveformY + waveformHeight end
            
            drawline(x1, y1, x2, y2, 0x27)
        end
    end
    
    -- Draw VU meter using getaudiobuffer()
    local vuY = waveformY + waveformHeight + 10
    local vuHeight = 20
    local vuWidth = 200
    
    local buffer = getaudiobuffer(16)  -- Get 16 samples for VU meter
    if buffer and #buffer > 0 then
        local maxLevel = 0
        for i = 1, #buffer do
            local level = math.abs(buffer[i])
            if level > maxLevel then
                maxLevel = level
            end
        end
        
        local normalizedLevel = math.min(maxLevel / 32768, 1.0)
        local barWidth = math.floor(normalizedLevel * vuWidth)
        
        -- VU meter background
        fillrect(4, vuY, vuWidth, vuHeight, 0x10)
        drawrect(4, vuY, vuWidth, vuHeight, 0x2D)
        
        -- VU meter bar
        if barWidth > 0 then
            fillrect(4, vuY, barWidth, vuHeight, 0x27)
        end
        
        -- VU meter label
        drawtext(4, vuY + vuHeight + 2, string.format("VU Meter: %.1f%%", normalizedLevel * 100), 0x20)
    end
    
    -- Instructions
    drawtext(4, vuY + vuHeight + 20, "Testing all audio API functions:", 0x29)
    drawtext(4, vuY + vuHeight + 32, "- getaudioenabled()", 0x37)
    drawtext(4, vuY + vuHeight + 44, "- getaudiosample() / getaudiosample(index)", 0x37)
    drawtext(4, vuY + vuHeight + 56, "- getaudiobuffer(count)", 0x37)
    drawtext(4, vuY + vuHeight + 68, "- getaudiosampleleft() / getaudiosampleright()", 0x37)
end

