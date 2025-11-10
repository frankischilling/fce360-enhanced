-- Test script for getaudiofiltered() function
-- Tests real-time frequency filtering with different filter types

local filterTypes = {"lowpass", "highpass", "bandpass", "notch"}
local filterNames = {"Low-Pass", "High-Pass", "Band-Pass", "Notch"}
local filterColors = {0x27, 0x37, 0x2F, 0x3F}  -- Different colors for each filter
local currentFilterIndex = 1  -- Start with lowpass
local cutoffFreq = 1000.0  -- Default cutoff frequency
local qFactor = 0.707  -- Default Q factor
local testFrameCount = 0
local logInterval = 120  -- Log every 120 frames (~2 seconds at 60fps)

-- D-pad state tracking for filter selection
local lastDpadUp = false
local lastDpadDown = false

-- Store waveform data for visualization
local originalWaveform = {}
local filteredWaveform = {}
local waveformLength = 128

-- Initialize waveform buffers
for i = 1, waveformLength do
    originalWaveform[i] = 0
    filteredWaveform[i] = 0
end

print("========================================")
print("Real-Time Frequency Filtering Test")
print("========================================")
print("Testing: getaudiofiltered([filterType], [cutoff], [q], [filterId])")
print("Filter Types: lowpass, highpass, bandpass, notch")
print("========================================")
print("This script applies frequency filters to audio samples")
print("and compares filtered output with original audio.")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking audio filter functions availability...")
    
    -- Check getaudiofiltered
    if type(getaudiofiltered) == "function" then
        print("  [OK] getaudiofiltered() is available")
        
        -- Test with default parameters
        local success, result = pcall(function()
            return getaudiofiltered()
        end)
        
        if success then
            print(string.format("  [OK] getaudiofiltered() works, sample: %d", result))
        else
            print(string.format("  [FAIL] getaudiofiltered() error: %s", tostring(result)))
        end
        
        -- Test all filter types
        for i = 1, #filterTypes do
            local success2, result2 = pcall(function()
                return getaudiofiltered(filterTypes[i], 1000.0, 0.707, 0)
            end)
            
            if success2 then
                print(string.format("  [OK] getaudiofiltered(\"%s\") works, sample: %d", filterTypes[i], result2))
            else
                print(string.format("  [FAIL] getaudiofiltered(\"%s\") error: %s", filterTypes[i], tostring(result2)))
            end
        end
        
        -- Test with different filter IDs
        for i = 0, 2 do
            local success3, result3 = pcall(function()
                return getaudiofiltered("lowpass", 1000.0, 0.707, i)
            end)
            
            if success3 then
                print(string.format("  [OK] getaudiofiltered(..., filterId=%d) works, sample: %d", i, result3))
            end
        end
    else
        print("  [FAIL] getaudiofiltered() is NOT available")
    end
    
    -- Check setaudiofilter and getaudiofilter
    if type(setaudiofilter) == "function" then
        print("  [OK] setaudiofilter() is available")
        
        -- Test enabling filter
        local success, result = pcall(function()
            setaudiofilter(true, "lowpass", 1000.0, 0.707)
            return true
        end)
        
        if success then
            print("  [OK] setaudiofilter() works")
        else
            print(string.format("  [FAIL] setaudiofilter() error: %s", tostring(result)))
        end
    else
        print("  [FAIL] setaudiofilter() is NOT available")
    end
    
    if type(getaudiofilter) == "function" then
        print("  [OK] getaudiofilter() is available")
        
        local success, result = pcall(function()
            return getaudiofilter()
        end)
        
        if success and result then
            print(string.format("  [OK] getaudiofilter() works, enabled: %s", result.enabled and "YES" or "NO"))
        else
            print(string.format("  [FAIL] getaudiofilter() error: %s", tostring(result)))
        end
    else
        print("  [FAIL] getaudiofilter() is NOT available")
    end
    
    -- Enable output filter with default settings on load
    if type(setaudiofilter) == "function" then
        setaudiofilter(true, filterTypes[currentFilterIndex], cutoffFreq, qFactor)
        print(string.format("[FILTER] Output filter enabled: %s", filterNames[currentFilterIndex]))
    end
    
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    local audioEnabled = getaudioenabled()
    
    -- Check D-pad for filter selection
    local dpadUp = isxboxbuttonpressed(0, "DPAD_UP")
    local dpadDown = isxboxbuttonpressed(0, "DPAD_DOWN")
    
    -- Change filter type on D-pad press (edge detection)
    if dpadUp and not lastDpadUp then
        currentFilterIndex = currentFilterIndex - 1
        if currentFilterIndex < 1 then
            currentFilterIndex = #filterTypes
        end
        -- Enable output filter with new type
        setaudiofilter(true, filterTypes[currentFilterIndex], cutoffFreq, qFactor)
        print(string.format("[FILTER] Changed to: %s (OUTPUT ENABLED)", filterNames[currentFilterIndex]))
    end
    if dpadDown and not lastDpadDown then
        currentFilterIndex = currentFilterIndex + 1
        if currentFilterIndex > #filterTypes then
            currentFilterIndex = 1
        end
        -- Enable output filter with new type
        setaudiofilter(true, filterTypes[currentFilterIndex], cutoffFreq, qFactor)
        print(string.format("[FILTER] Changed to: %s (OUTPUT ENABLED)", filterNames[currentFilterIndex]))
    end
    
    -- Update last D-pad state
    lastDpadUp = dpadUp
    lastDpadDown = dpadDown
    
    -- Header
    drawtext(4, 4, "Real-Time Frequency Filtering Test", 0x20)
    drawtext(4, 14, "D-Pad Up/Down: Change Filter Type", 0x37)
    
    -- Show output filter status
    local filterStatus = getaudiofilter()
    if filterStatus and filterStatus.enabled then
        drawtext(4, 24, "OUTPUT FILTER: ENABLED", 0x27)
    else
        drawtext(4, 24, "OUTPUT FILTER: DISABLED", 0x16)
    end
    
    if not audioEnabled then
        drawtext(4, 36, "Audio is DISABLED", 0x16)
        drawtext(4, 48, "Enable audio to hear filtering", 0x29)
        return
    end
    
    -- Get original audio sample
    local originalSample = getaudiosample()
    
    -- Get filtered audio sample (using current filter settings)
    local filterType = filterTypes[currentFilterIndex]
    local filteredSample = getaudiofiltered(filterType, cutoffFreq, qFactor, 0)
    
    -- Update waveform buffers (circular buffer)
    table.insert(originalWaveform, 1, originalSample)
    table.insert(filteredWaveform, 1, filteredSample)
    if #originalWaveform > waveformLength then
        table.remove(originalWaveform)
    end
    if #filteredWaveform > waveformLength then
        table.remove(filteredWaveform)
    end
    
    local y = 36
    
    -- Display current filter settings with highlight
    drawtext(4, y, string.format("Current Filter: %s", filterNames[currentFilterIndex]), filterColors[currentFilterIndex])
    y = y + 10
    drawtext(4, y, string.format("Cutoff: %.0f Hz", cutoffFreq), 0x37)
    y = y + 10
    drawtext(4, y, string.format("Q: %.2f", qFactor), 0x37)
    y = y + 12
    
    -- Display sample values
    drawtext(4, y, string.format("Original: %d", originalSample), 0x27)
    y = y + 10
    drawtext(4, y, string.format("Filtered: %d", filteredSample), filterColors[currentFilterIndex])
    y = y + 10
    
    -- Calculate and display difference
    local difference = math.abs(filteredSample - originalSample)
    drawtext(4, y, string.format("Difference: %d", difference), 0x37)
    y = y + 12
    
    -- Waveform comparison
    drawtext(4, y, "Waveform Comparison:", 0x29)
    y = y + 12
    
    local waveformY = y
    local waveformHeight = 40
    local waveformWidth = 200
    local centerY = waveformY + waveformHeight / 2
    
    -- Draw center line
    for i = 0, waveformWidth do
        drawpixel(4 + i, centerY, 0x10)
    end
    
    -- Draw original waveform (red)
    if #originalWaveform > 1 then
        local maxSamples = math.min(#originalWaveform - 1, waveformLength - 1)
        for i = 1, maxSamples do
            local x1 = 4 + math.floor((i - 1) * (waveformWidth / maxSamples))
            local x2 = 4 + math.floor(i * (waveformWidth / maxSamples))
            local scale = 300  -- Scale factor for waveform display
            local y1 = math.floor(centerY - (originalWaveform[i] / scale))
            local y2 = math.floor(centerY - (originalWaveform[i + 1] / scale))
            
            -- Clamp Y values to waveform area
            y1 = math.max(waveformY, math.min(waveformY + waveformHeight - 1, y1))
            y2 = math.max(waveformY, math.min(waveformY + waveformHeight - 1, y2))
            
            -- Draw line between points
            if x2 > x1 then
                local dx = x2 - x1
                local dy = y2 - y1
                local steps = math.max(math.abs(dx), math.abs(dy))
                if steps > 0 then
                    for j = 0, steps do
                        local px = x1 + math.floor((dx * j) / steps)
                        local py = y1 + math.floor((dy * j) / steps)
                        if px >= 4 and px < 4 + waveformWidth and py >= waveformY and py < waveformY + waveformHeight then
                            drawpixel(px, py, 0x27)  -- Red for original
                        end
                    end
                end
            end
        end
    end
    
    -- Draw filtered waveform (filter color)
    if #filteredWaveform > 1 then
        local maxSamples = math.min(#filteredWaveform - 1, waveformLength - 1)
        for i = 1, maxSamples do
            local x1 = 4 + math.floor((i - 1) * (waveformWidth / maxSamples))
            local x2 = 4 + math.floor(i * (waveformWidth / maxSamples))
            local scale = 300  -- Scale factor for waveform display
            local y1 = math.floor(centerY - (filteredWaveform[i] / scale))
            local y2 = math.floor(centerY - (filteredWaveform[i + 1] / scale))
            
            -- Clamp Y values to waveform area
            y1 = math.max(waveformY, math.min(waveformY + waveformHeight - 1, y1))
            y2 = math.max(waveformY, math.min(waveformY + waveformHeight - 1, y2))
            
            -- Draw line between points
            if x2 > x1 then
                local dx = x2 - x1
                local dy = y2 - y1
                local steps = math.max(math.abs(dx), math.abs(dy))
                if steps > 0 then
                    for j = 0, steps do
                        local px = x1 + math.floor((dx * j) / steps)
                        local py = y1 + math.floor((dy * j) / steps)
                        if px >= 4 and px < 4 + waveformWidth and py >= waveformY and py < waveformY + waveformHeight then
                            drawpixel(px, py, filterColors[currentFilterIndex])  -- Filter color
                        end
                    end
                end
            end
        end
    end
    
    y = waveformY + waveformHeight + 12
    
    -- Legend
    drawtext(4, y, "Red = Original, Colored = Filtered", 0x29)
    y = y + 12
    
    -- Filter type selector (visual indicator)
    drawtext(4, y, "Filter Types (D-Pad Up/Down):", 0x29)
    y = y + 10
    for i = 1, #filterNames do
        local color = (i == currentFilterIndex) and filterColors[i] or 0x10
        local marker = (i == currentFilterIndex) and ">" or " "
        drawtext(4, y, string.format("%s %s", marker, filterNames[i]), color)
        y = y + 10
    end
    
    y = y + 10
    
    -- Instructions
    drawtext(4, y, "Testing getaudiofiltered()", 0x29)
    drawtext(4, y + 12, "Check console for detailed logs", 0x37)
    
    -- Test multiple filters simultaneously (only if there's room)
    if y + 50 < 240 then  -- Check if there's room on screen
        y = y + 30
        drawtext(4, y, "Multiple Filter Test:", 0x29)
        y = y + 12
        
        -- Test different filter IDs
        for i = 0, 3 do
            if y < 240 then  -- Don't draw off screen
                local filterType = filterTypes[i + 1]
                local sample = getaudiofiltered(filterType, 1000.0, 0.707, i)
                drawtext(4, y, string.format("Filter %d (%s): %d", i, filterNames[i + 1], sample), filterColors[i + 1])
                y = y + 10
            end
        end
    end
    
    -- Log periodically
    if testFrameCount % logInterval == 0 then
        print(string.format("[TEST] Frame %d - Filter Analysis:", testFrameCount))
        print(string.format("  Filter Type: %s", filterNames[currentFilterIndex]))
        print(string.format("  Cutoff Frequency: %.1f Hz", cutoffFreq))
        print(string.format("  Q Factor: %.3f", qFactor))
        print(string.format("  Original Sample: %d", originalSample))
        print(string.format("  Filtered Sample: %d", filteredSample))
        
        -- Calculate difference
        local difference = math.abs(filteredSample - originalSample)
        local percentChange = 0
        if originalSample ~= 0 then
            percentChange = (difference / math.abs(originalSample)) * 100
        end
        print(string.format("  Difference: %d (%.1f%%)", difference, percentChange))
        
        -- Get sample rate for context
        local sampleRate = 44100  -- Default, could get from getaudiofft if needed
        print(string.format("  Sample Rate: %d Hz (estimated)", sampleRate))
        print("")  -- Empty line for readability
    end
end
