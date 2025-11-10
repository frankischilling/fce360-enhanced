-- Test script for getaudiochannelfft() function
-- Tests channel-specific FFT analysis for all 5 NES APU channels

local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
local channelColors = {0x27, 0x37, 0x2F, 0x3F, 0x1F}  -- Different colors for each channel
local fftSize = 256  -- Default FFT size
local lastFFTData = {}  -- Store FFT data for each channel
local peakFrequencies = {}  -- Store peak frequencies for each channel
local testFrameCount = 0
local logInterval = 120  -- Log every 120 frames (~2 seconds at 60fps)
local spectrumHistory = {}  -- Store spectrum history for each channel

-- Initialize data structures for each channel
for i = 0, 4 do
    lastFFTData[i] = nil
    peakFrequencies[i] = {}
    spectrumHistory[i] = {}
end

print("========================================")
print("Channel-Specific FFT Analysis Test")
print("========================================")
print("Testing: getaudiochannelfft(channel, [size])")
print("Channels: 0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC")
print("Performs FFT on individual channel samples")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking getaudiochannelfft() availability...")
    
    if type(getaudiochannelfft) == "function" then
        print("  [OK] getaudiochannelfft() is available")
        
        -- Test all channels on load
        for i = 0, 4 do
            local success, result = pcall(function()
                return getaudiochannelfft(i)
            end)
            
            if success and result then
                print(string.format("  [OK] getaudiochannelfft(%d) works, size: %d", i, result.size or 0))
                if result.sampleRate then
                    print(string.format("    Sample rate: %d Hz", result.sampleRate))
                end
                if result.frequencyResolution then
                    print(string.format("    Frequency resolution: %.2f Hz/bin", result.frequencyResolution))
                end
            else
                print(string.format("  [FAIL] getaudiochannelfft(%d) error: %s", i, tostring(result)))
            end
        end
        
        -- Test with custom size
        local success2, result2 = pcall(function()
            return getaudiochannelfft(0, 128)
        end)
        
        if success2 and result2 then
            print(string.format("  [OK] getaudiochannelfft(0, 128) works, size: %d", result2.size or 0))
        end
    else
        print("  [FAIL] getaudiochannelfft() is NOT available")
    end
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    if not getaudioenabled() then
        return
    end
    
    -- Perform FFT on all channels
    for channel = 0, 4 do
        local success, fftData = pcall(function()
            return getaudiochannelfft(channel, fftSize)
        end)
        
        if success and fftData and fftData.magnitude then
            lastFFTData[channel] = fftData
            
            -- Find peak frequencies
            local maxMagnitude = 0
            local peakBin = 1
            for i = 1, #fftData.magnitude do
                local mag = fftData.magnitude[i]
                if mag > maxMagnitude then
                    maxMagnitude = mag
                    peakBin = i
                end
            end
            
            -- Calculate frequency of peak bin
            if fftData.frequencyResolution and peakBin > 1 then
                local peakFreq = (peakBin - 1) * fftData.frequencyResolution
                table.insert(peakFrequencies[channel], {freq = peakFreq, magnitude = maxMagnitude})
                if #peakFrequencies[channel] > 10 then
                    table.remove(peakFrequencies[channel], 1)
                end
            end
            
            -- Store spectrum history for visualization
            table.insert(spectrumHistory[channel], 1, fftData.magnitude)
            if #spectrumHistory[channel] > 16 then
                table.remove(spectrumHistory[channel])
            end
        end
    end
    
    -- Log periodically
    if testFrameCount % logInterval == 0 then
        print(string.format("[TEST] Frame %d - Channel FFT Analysis:", testFrameCount))
        
        -- Compare with mixed audio FFT
        local mixedFFT = getaudiofft(fftSize)
        if mixedFFT and mixedFFT.magnitude then
            print("  Mixed Audio FFT:")
            print(string.format("    Size: %d, Sample Rate: %d Hz", mixedFFT.size or 0, mixedFFT.sampleRate or 0))
            
            -- Find peak in mixed audio
            local maxMag = 0
            local peakBin = 1
            for i = 1, #mixedFFT.magnitude do
                if mixedFFT.magnitude[i] > maxMag then
                    maxMag = mixedFFT.magnitude[i]
                    peakBin = i
                end
            end
            if mixedFFT.frequencyResolution and peakBin > 1 then
                local peakFreq = (peakBin - 1) * mixedFFT.frequencyResolution
                print(string.format("    Peak: %.1f Hz (magnitude: %.4f)", peakFreq, maxMag))
            end
        end
        
        print("  Channel-Specific FFT:")
        for channel = 0, 4 do
            local fftData = lastFFTData[channel]
            if fftData and fftData.magnitude then
                local channelInfo = getaudiochannel(channel)
                local enabled = channelInfo and channelInfo.enabled or false
                
                print(string.format("    Channel %d (%s) - Enabled: %s", 
                    channel, channelNames[channel + 1], enabled and "YES" or "NO"))
                print(string.format("      Size: %d, Sample Rate: %d Hz, Resolution: %.2f Hz/bin", 
                    fftData.size or 0, fftData.sampleRate or 0, fftData.frequencyResolution or 0))
                
                -- Find max magnitude for debugging
                local maxMag = 0
                local maxMagBin = 1
                for i = 1, #fftData.magnitude do
                    local mag = fftData.magnitude[i]
                    if mag > maxMag then
                        maxMag = mag
                        maxMagBin = i
                    end
                end
                print(string.format("      Max magnitude: %.6f (bin %d)", maxMag, maxMagBin))
                
                -- Check channel sample to see if it's active
                local channelSample = getaudiochannelsample(channel)
                print(string.format("      Current sample: %d", channelSample))
                
                -- Always show top frequencies (no threshold for channel-specific FFT)
                -- Channel samples are at frame rate (60 Hz), so magnitudes are typically much smaller
                local allFreqs = {}
                for i = 1, #fftData.magnitude do
                    local mag = fftData.magnitude[i]
                    local freq = (i - 1) * fftData.frequencyResolution
                    table.insert(allFreqs, {freq = freq, mag = mag})
                end
                
                -- Sort by magnitude
                table.sort(allFreqs, function(a, b) return a.mag > b.mag end)
                
                -- Show top frequencies (always show top 5, even if magnitudes are small)
                print("      Top Frequencies:")
                for i = 1, math.min(5, #allFreqs) do
                    print(string.format("        %d. %.2f Hz (magnitude: %.6f)", 
                        i, allFreqs[i].freq, allFreqs[i].mag))
                end
                
                -- Also show DC component (bin 1) separately
                if #fftData.magnitude >= 1 then
                    print(string.format("      DC Component (bin 1): %.6f", fftData.magnitude[1] or 0))
                end
            end
        end
        
        print("")  -- Empty line for readability
    end
end

function gui()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "Channel-Specific FFT Test", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see FFT analysis", 0x29)
        return
    end
    
    local y = 16
    
    -- Display FFT info for each channel
    for channel = 0, 4 do
        local fftData = lastFFTData[channel]
        local channelInfo = getaudiochannel(channel)
        local enabled = channelInfo and channelInfo.enabled or false
        local color = enabled and channelColors[channel + 1] or 0x10
        
        -- Channel header
        local headerText = string.format("Ch%d: %s", channel, channelNames[channel + 1])
        if enabled then
            headerText = headerText .. " [ON]"
        else
            headerText = headerText .. " [OFF]"
        end
        drawtext(4, y, headerText, color)
        y = y + 10
        
        if fftData and fftData.magnitude and enabled then
            -- Find peak frequency
            local maxMagnitude = 0
            local peakBin = 1
            for i = 1, #fftData.magnitude do
                local mag = fftData.magnitude[i]
                if mag > maxMagnitude then
                    maxMagnitude = mag
                    peakBin = i
                end
            end
            
            if fftData.frequencyResolution and peakBin > 1 then
                local peakFreq = (peakBin - 1) * fftData.frequencyResolution
                drawtext(12, y, string.format("Peak: %.2f Hz (%.3f)", peakFreq, maxMagnitude), 0x37)
                y = y + 10
            end
            
            -- Draw mini spectrum (first 32 bins)
            local spectrumY = y
            local spectrumHeight = 20
            local spectrumWidth = 100
            local maxFreqDisplay = math.min(32, #fftData.magnitude)
            
            -- Find max magnitude for scaling
            local maxMag = 0
            for i = 1, maxFreqDisplay do
                if fftData.magnitude[i] > maxMag then
                    maxMag = fftData.magnitude[i]
                end
            end
            
            if maxMag > 0 then
                local barWidth = spectrumWidth / maxFreqDisplay
                for i = 1, maxFreqDisplay do
                    local mag = fftData.magnitude[i]
                    local barHeight = (mag / maxMag) * spectrumHeight
                    local x = 12 + (i - 1) * barWidth
                    
                    -- Draw bar
                    for j = 0, barHeight do
                        for k = 0, barWidth - 1 do
                            if x + k < 12 + spectrumWidth then
                                drawpixel(x + k, spectrumY + spectrumHeight - j, color)
                            end
                        end
                    end
                end
            end
            
            y = spectrumY + spectrumHeight + 8
        else
            y = y + 8
        end
        
        y = y + 2  -- Spacing between channels
    end
    
    -- Comparison section
    y = y + 10
    drawtext(4, y, "Mixed Audio FFT (for comparison):", 0x29)
    y = y + 12
    
    local mixedFFT = getaudiofft(fftSize)
    if mixedFFT and mixedFFT.magnitude then
        drawtext(4, y, string.format("Size: %d, Rate: %d Hz", mixedFFT.size or 0, mixedFFT.sampleRate or 0), 0x37)
        y = y + 10
        
        -- Find peak in mixed audio
        local maxMag = 0
        local peakBin = 1
        for i = 1, #mixedFFT.magnitude do
            if mixedFFT.magnitude[i] > maxMag then
                maxMag = mixedFFT.magnitude[i]
                peakBin = i
            end
        end
        
        if mixedFFT.frequencyResolution and peakBin > 1 then
            local peakFreq = (peakBin - 1) * mixedFFT.frequencyResolution
            drawtext(4, y, string.format("Peak: %.1f Hz (%.3f)", peakFreq, maxMag), 0x27)
            y = y + 12
        end
        
        -- Draw mixed audio spectrum
        local spectrumY = y
        local spectrumHeight = 40
        local spectrumWidth = 200
        local maxFreqDisplay = math.min(64, #mixedFFT.magnitude)
        
        -- Find max magnitude for scaling
        local maxMag = 0
        for i = 1, maxFreqDisplay do
            if mixedFFT.magnitude[i] > maxMag then
                maxMag = mixedFFT.magnitude[i]
            end
        end
        
        if maxMag > 0 then
            local barWidth = spectrumWidth / maxFreqDisplay
            for i = 1, maxFreqDisplay do
                local mag = mixedFFT.magnitude[i]
                local barHeight = (mag / maxMag) * spectrumHeight
                local x = 4 + (i - 1) * barWidth
                local color = 0x27
                
                -- Color code by frequency range
                if mixedFFT.frequencyResolution then
                    local freq = (i - 1) * mixedFFT.frequencyResolution
                    if freq < 200 then
                        color = 0x27  -- Low frequencies (red)
                    elseif freq < 1000 then
                        color = 0x37  -- Mid frequencies (yellow)
                    else
                        color = 0x2F  -- High frequencies (green)
                    end
                end
                
                -- Draw bar
                for j = 0, barHeight do
                    for k = 0, barWidth - 1 do
                        if x + k < 4 + spectrumWidth then
                            drawpixel(x + k, spectrumY + spectrumHeight - j, color)
                        end
                    end
                end
            end
        end
        
        y = spectrumY + spectrumHeight + 12
    else
        drawtext(4, y, "Waiting for mixed audio FFT data...", 0x29)
        y = y + 12
    end
    
    -- Instructions
    drawtext(4, y, "Testing getaudiochannelfft(channel, [size])", 0x29)
    drawtext(4, y + 12, "Check console for detailed logs", 0x37)
end

