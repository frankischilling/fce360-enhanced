-- Test script for getaudiofft() function
-- Tests real-time FFT for frequency domain visualization

local fftSize = 256  -- Default FFT size
local lastFFTData = nil
local peakFrequencies = {}
local testFrameCount = 0
local logInterval = 120  -- Log every 120 frames (~2 seconds at 60fps)
local spectrumHistory = {}  -- Store spectrum history for visualization

print("========================================")
print("FFT (Frequency Domain) Test Script")
print("========================================")
print("Testing: getaudiofft([size])")
print("Performs Fast Fourier Transform on audio samples")
print("Returns frequency domain data (magnitude and phase)")
print("========================================")

-- Test function availability on load
function onload()
    print("[TEST] Checking getaudiofft() availability...")
    
    if type(getaudiofft) == "function" then
        print("  [OK] getaudiofft() is available")
        
        -- Test FFT with default size
        local success, result = pcall(function()
            return getaudiofft()
        end)
        
        if success and result then
            print(string.format("  [OK] getaudiofft() works, size: %d", result.size or 0))
            if result.sampleRate then
                print(string.format("  [OK] Sample rate: %d Hz", result.sampleRate))
            end
            if result.frequencyResolution then
                print(string.format("  [OK] Frequency resolution: %.2f Hz/bin", result.frequencyResolution))
            end
        else
            print(string.format("  [FAIL] getaudiofft() error: %s", tostring(result)))
        end
        
        -- Test FFT with custom size
        local success2, result2 = pcall(function()
            return getaudiofft(128)
        end)
        
        if success2 and result2 then
            print(string.format("  [OK] getaudiofft(128) works, size: %d", result2.size or 0))
        end
    else
        print("  [FAIL] getaudiofft() is NOT available")
    end
    print("========================================")
end

function script()
    testFrameCount = testFrameCount + 1
    
    if not getaudioenabled() then
        return
    end
    
    -- Perform FFT every frame
    local success, fftData = pcall(function()
        return getaudiofft(fftSize)
    end)
    
    if success and fftData and fftData.magnitude then
        lastFFTData = fftData
        
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
            table.insert(peakFrequencies, {freq = peakFreq, magnitude = maxMagnitude})
            if #peakFrequencies > 10 then
                table.remove(peakFrequencies, 1)
            end
        end
        
        -- Store spectrum history for visualization
        table.insert(spectrumHistory, 1, fftData.magnitude)
        if #spectrumHistory > 32 then
            table.remove(spectrumHistory)
        end
    end
    
    -- Log periodically
    if testFrameCount % logInterval == 0 and lastFFTData then
        print(string.format("[TEST] Frame %d - FFT Analysis:", testFrameCount))
        print(string.format("  FFT Size: %d", lastFFTData.size or 0))
        print(string.format("  Sample Rate: %d Hz", lastFFTData.sampleRate or 0))
        print(string.format("  Frequency Resolution: %.2f Hz/bin", lastFFTData.frequencyResolution or 0))
        print(string.format("  Magnitude Bins: %d", lastFFTData.magnitude and #lastFFTData.magnitude or 0))
        
        -- Find and display top frequencies
        if lastFFTData.magnitude and lastFFTData.frequencyResolution then
            local topFreqs = {}
            for i = 1, #lastFFTData.magnitude do
                local mag = lastFFTData.magnitude[i]
                if mag > 0.01 then  -- Threshold for significant frequencies
                    local freq = (i - 1) * lastFFTData.frequencyResolution
                    table.insert(topFreqs, {freq = freq, mag = mag})
                end
            end
            
            -- Sort by magnitude
            table.sort(topFreqs, function(a, b) return a.mag > b.mag end)
            
            print("  Top Frequencies:")
            for i = 1, math.min(5, #topFreqs) do
                print(string.format("    %d. %.1f Hz (magnitude: %.4f)", 
                    i, topFreqs[i].freq, topFreqs[i].mag))
            end
        end
        
        print("")  -- Empty line for readability
    end
end

function gui()
    local audioEnabled = getaudioenabled()
    
    -- Header
    drawtext(4, 4, "FFT Frequency Domain Test", 0x20)
    
    if not audioEnabled then
        drawtext(4, 16, "Audio is DISABLED", 0x16)
        drawtext(4, 28, "Enable audio to see FFT analysis", 0x29)
        return
    end
    
    if not lastFFTData or not lastFFTData.magnitude then
        drawtext(4, 16, "Waiting for FFT data...", 0x29)
        return
    end
    
    local y = 16
    
    -- Display FFT info
    drawtext(4, y, string.format("FFT Size: %d", lastFFTData.size or 0), 0x27)
    y = y + 10
    drawtext(4, y, string.format("Sample Rate: %d Hz", lastFFTData.sampleRate or 0), 0x37)
    y = y + 10
    drawtext(4, y, string.format("Resolution: %.1f Hz/bin", lastFFTData.frequencyResolution or 0), 0x37)
    y = y + 12
    
    -- Find peak frequency
    local maxMagnitude = 0
    local peakBin = 1
    for i = 1, #lastFFTData.magnitude do
        local mag = lastFFTData.magnitude[i]
        if mag > maxMagnitude then
            maxMagnitude = mag
            peakBin = i
        end
    end
    
    if lastFFTData.frequencyResolution and peakBin > 1 then
        local peakFreq = (peakBin - 1) * lastFFTData.frequencyResolution
        drawtext(4, y, string.format("Peak: %.1f Hz (mag: %.3f)", peakFreq, maxMagnitude), 0x27)
        y = y + 12
    end
    
    -- Draw frequency spectrum (bar chart)
    local spectrumY = y
    local spectrumHeight = 60
    local spectrumWidth = 200
    local maxFreqDisplay = math.min(64, #lastFFTData.magnitude)  -- Show first 64 bins
    
    drawtext(4, spectrumY - 12, "Frequency Spectrum:", 0x29)
    
    -- Draw axes
    for i = 0, spectrumWidth do
        drawpixel(4 + i, spectrumY + spectrumHeight, 0x10)
    end
    
    -- Draw spectrum bars
    local barWidth = spectrumWidth / maxFreqDisplay
    local maxMag = 0
    for i = 1, maxFreqDisplay do
        if lastFFTData.magnitude[i] > maxMag then
            maxMag = lastFFTData.magnitude[i]
        end
    end
    
    if maxMag > 0 then
        for i = 1, maxFreqDisplay do
            local mag = lastFFTData.magnitude[i]
            local barHeight = (mag / maxMag) * spectrumHeight
            local x = 4 + (i - 1) * barWidth
            local color = 0x27
            
            -- Color code by frequency range
            if lastFFTData.frequencyResolution then
                local freq = (i - 1) * lastFFTData.frequencyResolution
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
    
    -- Display frequency labels
    if lastFFTData.frequencyResolution then
        drawtext(4, y, string.format("0 Hz", 0), 0x37)
        local midFreq = (maxFreqDisplay / 2) * lastFFTData.frequencyResolution
        drawtext(4 + spectrumWidth / 2 - 20, y, string.format("%.0f Hz", midFreq), 0x37)
        local maxFreq = maxFreqDisplay * lastFFTData.frequencyResolution
        drawtext(4 + spectrumWidth - 40, y, string.format("%.0f Hz", maxFreq), 0x37)
        y = y + 12
    end
    
    -- Waterfall visualization (spectrum history)
    if #spectrumHistory > 0 then
        y = y + 10
        drawtext(4, y, "Waterfall (Time -> Frequency):", 0x29)
        y = y + 12
        
        local waterfallY = y
        local waterfallHeight = 32
        local waterfallWidth = 200
        
        -- Draw waterfall
        for timeIdx = 1, math.min(#spectrumHistory, waterfallWidth) do
            local spectrum = spectrumHistory[timeIdx]
            if spectrum then
                local maxMag = 0
                for i = 1, math.min(32, #spectrum) do
                    if spectrum[i] > maxMag then
                        maxMag = spectrum[i]
                    end
                end
                
                if maxMag > 0 then
                    for freqIdx = 1, math.min(32, #spectrum) do
                        local mag = spectrum[freqIdx]
                        local intensity = (mag / maxMag) * 255
                        local color = 0x10
                        
                        -- Color based on intensity
                        if intensity > 200 then
                            color = 0x27  -- Bright red
                        elseif intensity > 150 then
                            color = 0x37  -- Yellow
                        elseif intensity > 100 then
                            color = 0x2F  -- Green
                        elseif intensity > 50 then
                            color = 0x1F  -- Blue
                        end
                        
                        local x = 4 + (timeIdx - 1)
                        local yPos = waterfallY + waterfallHeight - freqIdx
                        if x < 4 + waterfallWidth and yPos >= waterfallY then
                            drawpixel(x, yPos, color)
                        end
                    end
                end
            end
        end
        
        y = waterfallY + waterfallHeight + 12
    end
    
    -- Instructions
    drawtext(4, y, "Testing getaudiofft([size])", 0x29)
    drawtext(4, y + 12, "Check console for detailed logs", 0x37)
end

