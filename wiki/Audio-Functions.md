# Audio Functions

The FCE360 Enhanced audio API lets Lua scripts observe the mixed output that players hear, inspect individual APU channels, run spectral analysis, and even apply real-time filters to the audible signal. This page reorganises the functions by task and explains how they fit together so you can pick the right tool for metering, visualisation, or audio-driven gameplay logic.

---

## Mixed Output Functions

### `getaudioenabled`

**Signature:** `getaudioenabled()`
Checks whether the emulator's audio output is enabled. Returns `true` when sound generation is active and `false` when audio has been globally disabled. Ideal for scripts that respond to audio availability or need to pause timing when the user mutes sound.

**Parameters:** None

**Returns:**
- `boolean` - `true` if audio output is enabled, `false` if audio is disabled.

**Notes:**
- Mirrors the emulator's master sound setting (`FSettings.SndRate != 0`).
- Value remains consistent for the entire frame; safe to read from any Lua callback.
- Useful for gating rhythm-based overlays, metronomes, or audio-driven automation when sound is muted.
- Combine with `gettimedelta()` or `sleepframes()` for precise timing that only runs when audio is active.
- Registered in both `InitLua()` and `EnsureLuaInit()` for consistent availability.

**Example: Log audio state changes**
```lua
local lastState

function script()
    local enabled = getaudioenabled()
    if enabled ~= lastState then
        print(string.format("Audio is now %s", enabled and "enabled" or "disabled"))
        lastState = enabled
    end
end
```

### `getaudiosample`

**Signature:** `getaudiosample([index])`
Retrieves an audio sample from the emulator's final mix buffer. Can access any sample in the buffer by index, or returns the most recent sample by default. Returns the raw integer value that can be used for audio visualization, peak detection, or real-time audio analysis in Lua scripts.

**Parameters:**
- `index` (integer, optional): Buffer index to retrieve
  - Default: `-1` (last/newest sample in buffer) - backward compatible with original behavior
  - `0` = oldest sample in buffer
  - `count-1` = newest sample in buffer
  - `-1` = last sample (default, same as `count-1`)
  - Negative indices count from end: `-1` = last, `-2` = second-to-last, etc.
  - Index is automatically clamped to valid buffer range

**Returns:**
- `integer` - Audio sample value (signed 32-bit, typically within ±32767 range)
  - Returns `0` when audio is disabled (`getaudioenabled()` is `false`)
  - Returns `0` when audio buffer is empty
  - Sample values can exceed ±32767 if filters or expansion audio boost the mix

**Notes:**
- Samples are provided in the same format that is sent to the audio output pipeline.
- When audio is disabled (`getaudioenabled()` is `false`), this function returns `0`.
- Values can exceed ±32767 if filters or expansion audio boost the mix; clamp or normalize in Lua as needed.
- Read once per frame for VU meters, or multiple times inside `script()` when running at 60 Hz to build short buffers.
- **Backward compatible**: Calling without parameters returns the last sample (same as original behavior).
- **Buffer access**: Use index parameter to access any sample in the buffer for waveform analysis.
- **Negative indices**: Support for Python-style negative indexing (counts from end of buffer).
- Registered in both `InitLua()` and `EnsureLuaInit()` for consistent availability.
- Useful for:
  - Audio visualization (oscilloscopes, waveforms, VU meters)
  - Peak level detection
  - Audio-reactive visual effects
  - Real-time audio analysis in Lua scripts
  - Waveform analysis with buffer access

**Example: Simple level bar**
```lua
function gui()
    if not getaudioenabled() then
        drawtext(8, 8, "Audio disabled", 0x16)
        return
    end
    
    local sample = getaudiosample()
    local level = math.min(math.abs(sample) / 32768, 1.0)
    local barWidth = math.floor(level * 200)
    
    fillrect(16, 40, 200, 10, 0x21)        -- background
    fillrect(16, 40, barWidth, 10, 0x27)   -- level bar
    drawtext(16, 52, string.format("Level: %.2f", level), 0x20)
end
```

### `getaudiobuffer`

**Signature:** `getaudiobuffer([count])`
Retrieves multiple audio samples from the buffer as a Lua table. Useful for waveform visualization, oscilloscope displays, and batch audio analysis operations.

**Parameters:**
- `count` (integer, optional): Number of samples to retrieve
  - Default: All available samples in buffer
  - Maximum: 256 samples (automatically clamped)
  - Returns samples starting from oldest (index 0) to newest

**Returns:**
- `table` - 1-indexed Lua table containing sample values
  - Returns empty table `{}` when audio is disabled
  - Returns empty table `{}` when buffer is empty
  - Table indices are 1, 2, 3, ... up to `count` (Lua-style 1-indexing)
  - Each table entry contains a signed 32-bit integer sample value

**Notes:**
- Samples are provided in the same format that is sent to the audio output pipeline.
- When audio is disabled (`getaudioenabled()` is `false`), this function returns an empty table.
- **Efficient batch access**: More efficient than calling `getaudiosample(index)` multiple times in a loop.
- **Table format**: Returns 1-indexed table (Lua standard) for easy iteration with `ipairs()`.
- **Sample order**: Samples are returned in chronological order (oldest first, newest last).
- **Maximum limit**: Limited to 256 samples per call to prevent excessive memory usage.
- Registered in both `InitLua()` and `EnsureLuaInit()` for consistent availability.
- Useful for:
  - Waveform visualization (drawing complete waveforms)
  - Oscilloscope displays
  - Batch audio analysis (calculating averages, peaks, etc.)
  - VU meters with multiple sample averaging
  - Audio spectrum analysis preparation

**Example: Waveform visualization**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    -- Get 64 samples for waveform display
    local buffer = getaudiobuffer(64)
    
    if #buffer > 1 then
        local startX = 4
        local startY = 100
        local width = 200
        local height = 60
        local centerY = startY + height / 2
        
        -- Draw waveform
        for i = 1, #buffer - 1 do
            local x1 = startX + (i - 1) * (width / (#buffer - 1))
            local x2 = startX + i * (width / (#buffer - 1))
            local y1 = centerY - (buffer[i] / 32768 * height / 2)
            local y2 = centerY - (buffer[i + 1] / 32768 * height / 2)
            
            drawline(x1, y1, x2, y2, 0x27)
        end
    end
end
```

### `getaudiosampleleft`

**Signature:** `getaudiosampleleft()`
Retrieves the left channel audio sample from the emulator's final mix buffer. For NES (mono audio), this returns the same value as `getaudiosample()` since NES audio is mono and duplicated to both channels.

**Parameters:** None

**Returns:**
- `integer` - Left channel audio sample value (signed 32-bit, typically within ±32767 range)
  - Returns `0` when audio is disabled (`getaudioenabled()` is `false`)
  - Returns `0` when audio buffer is empty
  - For NES: Same value as `getaudiosample()` (mono audio)

**Notes:**
- **NES is mono**: NES audio is fundamentally mono, so left and right channels contain identical samples.
- **Future compatibility**: Function provided for stereo API compatibility and potential future stereo support.
- When audio is disabled (`getaudioenabled()` is `false`), this function returns `0`.
- Sample values can exceed ±32767 if filters or expansion audio boost the mix.
- Registered in both `InitLua()` and `EnsureLuaInit()` for consistent availability.
- Useful for:
  - Stereo API compatibility in scripts
  - Future-proofing for potential stereo audio support
  - Scripts that need explicit channel separation (even if currently identical)

**Example: Stereo-compatible audio display**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local left = getaudiosampleleft()
    local right = getaudiosampleright()
    
    -- Display both channels (will be identical for NES)
    drawtext(4, 4, string.format("Left: %d", left), 0x29)
    drawtext(4, 16, string.format("Right: %d", right), 0x37)
    
    -- Note: For NES, left == right (mono audio)
end
```

### `getaudiosampleright`

**Signature:** `getaudiosampleright()`
Retrieves the right channel audio sample from the emulator's final mix buffer. For NES (mono audio), this returns the same value as `getaudiosample()` since NES audio is mono and duplicated to both channels.

**Parameters:** None

**Returns:**
- `integer` - Right channel audio sample value (signed 32-bit, typically within ±32767 range)
  - Returns `0` when audio is disabled (`getaudioenabled()` is `false`)
  - Returns `0` when audio buffer is empty
  - For NES: Same value as `getaudiosample()` (mono audio)

**Notes:**
- **NES is mono**: NES audio is fundamentally mono, so left and right channels contain identical samples.
- **Future compatibility**: Function provided for stereo API compatibility and potential future stereo support.
- When audio is disabled (`getaudioenabled()` is `false`), this function returns `0`.
- Sample values can exceed ±32767 if filters or expansion audio boost the mix.
- Registered in both `InitLua()` and `EnsureLuaInit()` for consistent availability.
- Useful for:
  - Stereo API compatibility in scripts
  - Future-proofing for potential stereo audio support
  - Scripts that need explicit channel separation (even if currently identical)

**Example: Stereo-compatible audio display**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local left = getaudiosampleleft()
    local right = getaudiosampleright()
    
    -- Display both channels (will be identical for NES)
    drawtext(4, 4, string.format("Left: %d", left), 0x29)
    drawtext(4, 16, string.format("Right: %d", right), 0x37)
    
    -- Note: For NES, left == right (mono audio)
end
```

---

## Per-Channel Analysis

### `getaudiochannel`

**Signature:** `getaudiochannel(channel)`
Gets detailed state information for a specific NES APU (Audio Processing Unit) channel. Returns a table containing channel-specific properties such as enabled status, register values, length counters, and other channel parameters. Useful for audio analysis, channel monitoring, debugging audio issues, and creating detailed audio visualizations.

**Parameters:**
- `channel` (integer, required): Channel number (0-4)
  - `0` = Pulse 1 (Square 1)
  - `1` = Pulse 2 (Square 2)
  - `2` = Triangle
  - `3` = Noise
  - `4` = DMC (Delta Modulation Channel)

**Returns:**
- `table` - Channel information table with the following structure:
  - **All channels:**
    - `name` (string) - Channel name ("Pulse1", "Pulse2", "Triangle", "Noise", "DMC")
    - `enabled` (boolean) - Whether the channel is currently enabled
    - `channel` (integer) - Channel number (0-4)
    - `lengthCounter` (integer) - Length counter value (channels 0-3 only)
  - **Pulse channels (0, 1) only:**
    - `dutyCycle` (integer) - Duty cycle (0-3)
    - `volume` (integer) - Volume level (0-15)
    - `constantVolume` (boolean) - Whether constant volume mode is enabled
    - `sweepEnabled` (boolean) - Whether frequency sweep is enabled
    - `sweepPeriod` (integer) - Sweep period (0-7)
    - `sweepNegate` (boolean) - Whether sweep negates frequency
    - `sweepShift` (integer) - Sweep shift amount (0-7)
    - `period` (integer) - Frequency period (combined from periodLow and periodHigh)
    - `periodLow` (integer) - Low byte of period
    - `periodHigh` (integer) - High 3 bits of period
    - `lengthCounterHalt` (boolean) - Whether length counter is halted
  - **Triangle channel (2) only:**
    - `linearCounterReload` (integer) - Linear counter reload value (0-127)
    - `linearCounterControl` (boolean) - Linear counter control flag
    - `period` (integer) - Frequency period (combined from periodLow and periodHigh)
    - `periodLow` (integer) - Low byte of period
    - `periodHigh` (integer) - High 3 bits of period
    - `lengthCounterHalt` (boolean) - Whether length counter is halted
  - **Noise channel (3) only:**
    - `volume` (integer) - Volume level (0-15)
    - `constantVolume` (boolean) - Whether constant volume mode is enabled
    - `period` (integer) - Noise period (0-15)
    - `loopNoise` (boolean) - Whether noise loop mode is enabled
    - `lengthCounterHalt` (boolean) - Whether length counter is halted
  - **DMC channel (4) only:**
    - `irqEnabled` (boolean) - Whether IRQ generation is enabled
    - `loop` (boolean) - Whether sample looping is enabled
    - `period` (integer) - DMC period (0-15)
    - `directLoad` (integer) - Direct load value (0-127)
    - `sampleAddress` (integer) - Sample start address (0xC000-0xFFC0)
    - `sampleLength` (integer) - Sample length in bytes
    - `remainingSize` (integer) - Remaining bytes to play
    - `active` (boolean) - Whether DMC is currently playing

**Notes:**
- Returns a table with channel-specific information based on the channel type.
- When audio is disabled (`getaudioenabled()` is `false`), returns basic channel info with `enabled = false`.
- Channel register values are read from the APU's internal PSG register array.
- Length counter values are real-time and update as the channel plays.
- Period values determine the frequency/pitch of the channel.
- DMC channel information includes sample playback state.
- Throws an error if `channel` is outside the valid range (0-4).
- Useful for debugging audio issues, monitoring channel activity, and creating detailed audio analysis tools.

**Example: Basic Usage:**
```lua
-- Get information about Pulse 1 channel
local pulse1 = getaudiochannel(0)
print(string.format("Pulse 1: %s, Enabled: %s", pulse1.name, pulse1.enabled))
```

**Example: Monitor All Channels:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local y = 4
    local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
    
    for i = 0, 4 do
        local ch = getaudiochannel(i)
        local color = ch.enabled and 0x27 or 0x10
        drawtext(4, y, string.format("%s: %s", ch.name, ch.enabled and "ON" or "OFF"), color)
        y = y + 10
    end
end
```

**Example: Display Pulse Channel Details:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local pulse1 = getaudiochannel(0)
    
    if pulse1.enabled then
        drawtext(4, 4, string.format("Pulse 1: Period=%d", pulse1.period), 0x27)
        drawtext(4, 14, string.format("Volume=%d Duty=%d", pulse1.volume, pulse1.dutyCycle), 0x37)
        drawtext(4, 24, string.format("Length=%d", pulse1.lengthCounter), 0x37)
    end
end
```

### `getaudiochannelsample`

**Signature:** `getaudiochannelsample(channel)`
Gets the last sample from a specific NES APU channel before mixing. This function extracts individual channel samples before they are combined into the final mixed audio output. Useful for channel-specific audio visualization, isolating individual channel waveforms, and analyzing each channel's contribution to the final mix.

**Parameters:**
- `channel` (integer, required): Channel number (0-4)
  - `0` = Pulse 1 (Square 1)
  - `1` = Pulse 2 (Square 2)
  - `2` = Triangle
  - `3` = Noise
  - `4` = DMC (Delta Modulation Channel)

**Returns:**
- `integer` - Sample value (32-bit signed, typically within 16-bit range)
  - Returns the last representative sample from the specified channel
  - Sample values are computed from the channel's current state (duty cycle, volume, frequency, etc.)
  - Values are scaled to be comparable to mixed audio samples
  - Returns `0` when audio is disabled or channel is not producing output

**Notes:**
- Returns samples from individual channels **before** they are mixed together.
- Samples are computed in real-time from the channel's current rendering state.
- Each channel's sample represents its contribution before mixing with other channels.
- Sample values are representative and may not exactly match the raw buffer values due to scaling.
- When audio is disabled (`getaudioenabled()` is `false`), this function returns `0`.
- Throws an error if `channel` is outside the valid range (0-4).
- Useful for:
  - Channel-specific audio visualization (oscilloscopes, waveforms)
  - Isolating individual channel outputs
  - Comparing individual channels vs. mixed output
  - Audio analysis and debugging
  - Creating channel-specific visual effects

**Example: Basic Usage:**
```lua
-- Get sample from Pulse 1 channel
local pulse1Sample = getaudiochannelsample(0)
print(string.format("Pulse 1 sample: %d", pulse1Sample))
```

**Example: Display All Channel Samples:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local y = 4
    local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
    
    for i = 0, 4 do
        local sample = getaudiochannelsample(i)
        drawtext(4, y, string.format("%s: %d", channelNames[i + 1], sample), 0x27)
        y = y + 10
    end
end
```

**Example: Channel-Specific Waveform Visualization:**
```lua
local waveformData = {}

function script()
    if not getaudioenabled() then
        return
    end
    
    -- Store samples for waveform display
    for i = 0, 4 do
        if not waveformData[i] then
            waveformData[i] = {}
        end
        local sample = getaudiochannelsample(i)
        table.insert(waveformData[i], 1, sample)
        if #waveformData[i] > 64 then
            table.remove(waveformData[i])
        end
    end
end

function gui()
    if not getaudioenabled() then
        return
    end
    
    -- Draw waveform for Pulse 1 channel
    local y = 100
    local centerY = y + 20
    local width = 200
    
    drawtext(4, y - 12, "Pulse 1 Waveform:", 0x27)
    
    -- Draw center line
    for i = 0, width do
        drawpixel(4 + i, centerY, 0x10)
    end
    
    -- Draw waveform
    if waveformData[0] and #waveformData[0] > 1 then
        for i = 1, math.min(#waveformData[0] - 1, width) do
            local x1 = 4 + (i - 1) * (width / #waveformData[0])
            local x2 = 4 + i * (width / #waveformData[0])
            local y1 = centerY - (waveformData[0][i] / 200)
            local y2 = centerY - (waveformData[0][i + 1] / 200)
            drawline(x1, y1, x2, y2, 0x27)
        end
    end
end
```

**Example: Compare Individual Channels vs. Mixed Audio:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    -- Get individual channel samples
    local pulse1 = getaudiochannelsample(0)
    local pulse2 = getaudiochannelsample(1)
    local triangle = getaudiochannelsample(2)
    local noise = getaudiochannelsample(3)
    local dmc = getaudiochannelsample(4)
    
    -- Get mixed audio sample
    local mixed = getaudiosample()
    
    -- Calculate sum of channels (approximate)
    local sum = pulse1 + pulse2 + triangle + noise + dmc
    
    drawtext(4, 4, string.format("Pulse1: %d", pulse1), 0x27)
    drawtext(4, 14, string.format("Pulse2: %d", pulse2), 0x37)
    drawtext(4, 24, string.format("Triangle: %d", triangle), 0x2F)
    drawtext(4, 34, string.format("Noise: %d", noise), 0x3F)
    drawtext(4, 44, string.format("DMC: %d", dmc), 0x1F)
    drawtext(4, 54, string.format("Sum: %d", sum), 0x37)
    drawtext(4, 64, string.format("Mixed: %d", mixed), 0x29)
end
```

### `getaudiochannelfft`

**Signature:** `getaudiochannelfft(channel, [size])`
Performs Fast Fourier Transform (FFT) on samples from a specific NES APU channel and returns frequency domain data. This function performs channel-specific frequency analysis by analyzing individual channel samples before they are mixed into the final audio output. Uses a radix-2 FFT algorithm with Hanning window function to reduce spectral leakage. Channel samples are stored at frame rate (60 Hz) in a circular buffer, allowing for real-time frequency analysis of individual channels.

**Parameters:**
- `channel` (integer, required): Channel number (0-4)
  - `0` = Pulse 1 (Square 1)
  - `1` = Pulse 2 (Square 2)
  - `2` = Triangle
  - `3` = Noise
  - `4` = DMC (Delta Modulation Channel)
- `size` (integer, optional, default: 256): FFT size (must be power of 2)
  - Valid range: 32 to 512 (automatically rounded to nearest power of 2)
  - Limited to available buffer size (512 samples maximum)
  - Larger sizes provide better frequency resolution but require more computation
  - Common sizes: 128, 256, 512
  - If non-power-of-2 is provided, it's automatically rounded down to nearest power of 2

**Returns:**
- `table` - Frequency domain data table with the following structure:
  - `magnitude` (table, 1-indexed array) - Magnitude of each frequency bin
  - `phase` (table, 1-indexed array) - Phase of each frequency bin
  - `size` (integer) - FFT size actually used (power of 2)
  - `sampleRate` (integer) - Effective sample rate in Hz (60 Hz, frame rate)
  - `frequencyResolution` (number) - Frequency resolution in Hz per bin
  - `channel` (integer) - Channel number that was analyzed (0-4)

**Notes:**
- Performs FFT on channel-specific samples from a circular buffer (512 samples per channel).
- Channel samples are stored once per frame (60 Hz), not at audio sample rate.
- This analyzes channel activity patterns/envelopes rather than raw audio waveforms.
- Uses most recent samples from the circular buffer (newest to oldest).
- Returns only the first half of frequency bins (size/2 + 1) since FFT is symmetric for real input.
- Frequency bin 1 (index 1) represents DC component (0 Hz).
- Frequency bin i (index i) represents frequency: `(i - 1) * frequencyResolution` Hz.
- Maximum frequency represented: `sampleRate / 2` (30 Hz for 60 Hz frame rate).
- When audio is disabled (`getaudioenabled()` is `false`), returns table with `size = 0`.
- FFT computation uses double-precision floating point for accuracy.
- Sample values are normalized to -1.0 to 1.0 range before FFT.
- Magnitude values are typically much smaller than mixed audio FFT (e.g., 0.0001-0.01 range is normal).
- Useful for channel-specific frequency analysis, isolating individual channel frequencies, detecting rhythms, and debugging.

**Example: Basic Usage:**
```lua
-- Perform FFT on Pulse 1 channel with default size (256)
local pulse1FFT = getaudiochannelfft(0)
print(string.format("Pulse 1 FFT Size: %d", pulse1FFT.size))
print(string.format("Sample Rate: %d Hz", pulse1FFT.sampleRate))
print(string.format("Frequency Resolution: %.2f Hz/bin", pulse1FFT.frequencyResolution))
```

**Example: Channel-Specific Spectrum Visualization:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    -- Get FFT for Pulse 1 channel
    local fft = getaudiochannelfft(0, 256)
    if not fft.magnitude then
        return
    end
    
    -- Draw frequency spectrum bars
    local y = 100
    local height = 60
    local width = 200
    local maxBins = math.min(64, #fft.magnitude)
    
    -- Find maximum magnitude for scaling
    local maxMag = 0
    for i = 1, maxBins do
        if fft.magnitude[i] > maxMag then
            maxMag = fft.magnitude[i]
        end
    end
    
    -- Draw spectrum bars
    if maxMag > 0 then
        local barWidth = width / maxBins
        for i = 1, maxBins do
            local mag = fft.magnitude[i]
            local barHeight = (mag / maxMag) * height
            local x = 4 + (i - 1) * barWidth
            
            -- Draw bar
            for j = 0, barHeight do
                for k = 0, barWidth - 1 do
                    drawpixel(x + k, y + height - j, 0x27)
                end
            end
        end
    end
end
```

**Example: Compare All Channels:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
    local channelColors = {0x27, 0x37, 0x2F, 0x3F, 0x1F}
    local y = 4
    
    for channel = 0, 4 do
        local fft = getaudiochannelfft(channel, 128)
        if fft.magnitude then
            -- Find peak frequency
            local maxMag = 0
            local peakBin = 1
            for i = 1, #fft.magnitude do
                if fft.magnitude[i] > maxMag then
                    maxMag = fft.magnitude[i]
                    peakBin = i
                end
            end
            
            if fft.frequencyResolution and peakBin > 1 then
                local peakFreq = (peakBin - 1) * fft.frequencyResolution
                drawtext(4, y, string.format("%s: Peak %.2f Hz (%.4f)", 
                    channelNames[channel + 1], peakFreq, maxMag), channelColors[channel + 1])
            else
                drawtext(4, y, string.format("%s: No signal", channelNames[channel + 1]), 0x10)
            end
        end
        y = y + 10
    end
end
```

**Example: Channel vs. Mixed Audio Comparison:**
```lua
function script()
    if not getaudioenabled() then
        return
    end
    
    -- Get channel-specific FFT
    local pulse1FFT = getaudiochannelfft(0, 256)
    
    -- Get mixed audio FFT for comparison
    local mixedFFT = getaudiofft(256)
    
    if pulse1FFT.magnitude and mixedFFT.magnitude then
        -- Find peak in Pulse 1
        local maxMag1 = 0
        local peakBin1 = 1
        for i = 1, #pulse1FFT.magnitude do
            if pulse1FFT.magnitude[i] > maxMag1 then
                maxMag1 = pulse1FFT.magnitude[i]
                peakBin1 = i
            end
        end
        
        -- Find peak in mixed audio
        local maxMag2 = 0
        local peakBin2 = 1
        for i = 1, #mixedFFT.magnitude do
            if mixedFFT.magnitude[i] > maxMag2 then
                maxMag2 = mixedFFT.magnitude[i]
                peakBin2 = i
            end
        end
        
        if pulse1FFT.frequencyResolution and mixedFFT.frequencyResolution then
            local freq1 = (peakBin1 - 1) * pulse1FFT.frequencyResolution
            local freq2 = (peakBin2 - 1) * mixedFFT.frequencyResolution
            print(string.format("Pulse1 peak: %.2f Hz (%.4f), Mixed peak: %.1f Hz (%.4f)", 
                freq1, maxMag1, freq2, maxMag2))
        end
    end
end
```

---

## Spectral Analysis of Mixed Audio

### `getaudiofft`

**Signature:** `getaudiofft([size])`
Performs Fast Fourier Transform (FFT) on audio samples and returns frequency domain data. This function converts time-domain audio samples into frequency-domain representation, allowing for spectrum analysis, frequency visualization, and audio frequency analysis. Uses a radix-2 FFT algorithm with Hanning window function to reduce spectral leakage.

**Parameters:**
- `size` (integer, optional, default: 256): FFT size (must be power of 2)
  - Valid range: 32 to 512 (automatically rounded to nearest power of 2)
  - Larger sizes provide better frequency resolution but require more computation
  - Common sizes: 128, 256, 512
  - If non-power-of-2 is provided, it's automatically rounded down to nearest power of 2

**Returns:**
- `table` - Frequency domain data table with the following structure:
  - `magnitude` (table, 1-indexed array) - Magnitude of each frequency bin
  - `phase` (table, 1-indexed array) - Phase of each frequency bin
  - `size` (integer) - FFT size actually used (power of 2)
  - `sampleRate` (integer) - Audio sample rate in Hz
  - `frequencyResolution` (number) - Frequency resolution in Hz per bin

**Notes:**
- Performs real-time FFT on the most recent audio samples from the buffer.
- Uses Hanning window function to reduce spectral leakage and improve frequency accuracy.
- Returns only the first half of frequency bins (size/2 + 1) since FFT is symmetric for real input.
- Frequency bin 1 (index 1) represents DC component (0 Hz).
- Frequency bin i (index i) represents frequency: `(i - 1) * frequencyResolution` Hz.
- Maximum frequency represented: `sampleRate / 2` (Nyquist frequency).
- When audio is disabled (`getaudioenabled()` is `false`), returns table with `size = 0`.
- FFT computation uses double-precision floating point for accuracy.
- Sample values are normalized to -1.0 to 1.0 range before FFT.
- Useful for spectrum analyzers, visualizers, and debugging.

**Example: Basic Usage:**
```lua
-- Perform FFT with default size (256)
local fft = getaudiofft()
print(string.format("FFT Size: %d", fft.size))
print(string.format("Sample Rate: %d Hz", fft.sampleRate))
print(string.format("Frequency Resolution: %.2f Hz/bin", fft.frequencyResolution))
```

**Example: Frequency Spectrum Visualization:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local fft = getaudiofft(256)
    if not fft.magnitude then
        return
    end
    
    -- Draw frequency spectrum bars
    local y = 100
    local height = 60
    local width = 200
    local maxBins = math.min(64, #fft.magnitude)
    
    -- Find maximum magnitude for scaling
    local maxMag = 0
    for i = 1, maxBins do
        if fft.magnitude[i] > maxMag then
            maxMag = fft.magnitude[i]
        end
    end
    
    -- Draw spectrum bars
    if maxMag > 0 then
        local barWidth = width / maxBins
        for i = 1, maxBins do
            local mag = fft.magnitude[i]
            local barHeight = (mag / maxMag) * height
            local x = 4 + (i - 1) * barWidth
            
            -- Draw bar
            for j = 0, barHeight do
                for k = 0, barWidth - 1 do
                    drawpixel(x + k, y + height - j, 0x27)
                end
            end
        end
    end
end
```

**Example: Find Peak Frequency:**
```lua
function script()
    if not getaudioenabled() then
        return
    end
    
    local fft = getaudiofft(256)
    if not fft.magnitude or not fft.frequencyResolution then
        return
    end
    
    -- Find frequency with highest magnitude
    local maxMagnitude = 0
    local peakBin = 1
    for i = 1, #fft.magnitude do
        if fft.magnitude[i] > maxMagnitude then
            maxMagnitude = fft.magnitude[i]
            peakBin = i
        end
    end
    
    -- Calculate peak frequency
    local peakFreq = (peakBin - 1) * fft.frequencyResolution
    print(string.format("Peak frequency: %.1f Hz (magnitude: %.4f)", peakFreq, maxMagnitude))
end
```

**Example: Display Top Frequencies:**
```lua
function gui()
    if not getaudioenabled() then
        return
    end
    
    local fft = getaudiofft(256)
    if not fft.magnitude or not fft.frequencyResolution then
        return
    end
    
    -- Collect significant frequencies
    local freqs = {}
    for i = 2, #fft.magnitude do  -- Skip DC (bin 1)
        local mag = fft.magnitude[i]
        if mag > 0.01 then  -- Threshold
            local freq = (i - 1) * fft.frequencyResolution
            table.insert(freqs, {freq = freq, mag = mag})
        end
    end
    
    -- Sort by magnitude
    table.sort(freqs, function(a, b) return a.mag > b.mag end)
    
    -- Display top 5 frequencies
    local y = 4
    drawtext(4, y, "Top Frequencies:", 0x29)
    y = y + 12
    
    for i = 1, math.min(5, #freqs) do
        drawtext(4, y, string.format("%d. %.1f Hz (%.3f)", 
            i, freqs[i].freq, freqs[i].mag), 0x27)
        y = y + 10
    end
end
```

**Example: Waterfall Visualization:**
```lua
local spectrumHistory = {}

function script()
    if not getaudioenabled() then
        return
    end
    
    local fft = getaudiofft(256)
    if fft.magnitude then
        -- Store spectrum history
        table.insert(spectrumHistory, 1, fft.magnitude)
        if #spectrumHistory > 64 then
            table.remove(spectrumHistory)
        end
    end
end

function gui()
    if not getaudioenabled() or #spectrumHistory == 0 then
        return
    end
    
    -- Draw waterfall (time vs frequency)
    local x = 4
    local y = 100
    local width = 200
    local height = 64
    
    for timeIdx = 1, math.min(#spectrumHistory, width) do
        local spectrum = spectrumHistory[timeIdx]
        if spectrum then
            local maxMag = 0
            for i = 1, math.min(height, #spectrum) do
                if spectrum[i] > maxMag then
                    maxMag = spectrum[i]
                end
            end
            
            if maxMag > 0 then
                for freqIdx = 1, math.min(height, #spectrum) do
                    local mag = spectrum[freqIdx]
                    local intensity = (mag / maxMag) * 255
                    local color = 0x10
                    
                    -- Color based on intensity
                    if intensity > 200 then color = 0x27
                    elseif intensity > 150 then color = 0x37
                    elseif intensity > 100 then color = 0x2F
                    elseif intensity > 50 then color = 0x1F
                    end
                    
                    drawpixel(x + timeIdx - 1, y + height - freqIdx, color)
                end
            end
        end
    end
end
```

---

## Audio Filtering

### `getaudiofiltered`

**Signature:** `getaudiofiltered([filterType], [cutoff], [q], [filterId])`
Applies real-time frequency filtering to audio samples and returns the filtered sample value. This function performs frequency filtering for analysis and visualization purposes. Uses a biquad (second-order IIR) filter for efficient real-time processing. **Note:** This function returns filtered samples for analysis only - it does not affect the actual audio output. To filter the audio that plays through speakers, use `setaudiofilter()` instead.

**Parameters:**
- `filterType` (string, optional, default: "lowpass"): Filter type
  - `"lowpass"` or `"lp"` - Low-pass filter (removes high frequencies)
  - `"highpass"` or `"hp"` - High-pass filter (removes low frequencies)
  - `"bandpass"` or `"bp"` - Band-pass filter (keeps only a frequency range)
  - `"notch"` or `"bandstop"` or `"bs"` - Notch filter (removes a frequency range)
- `cutoff` (number, optional, default: 1000.0): Cutoff frequency in Hz
- `q` (number, optional, default: 0.707): Q factor (quality factor)
- `filterId` (integer, optional, default: 0): Filter instance ID (0-9)

**Returns:**
- `integer` - Filtered sample value (32-bit signed, typically within 16-bit range)

**Notes:**
- Analysis only—this function does not change what you hear.
- Each `filterId` keeps independent state so you can run multiple filters in parallel.
- Based on RBJ Audio EQ Cookbook formulas.
- Useful for comparing original vs. filtered samples or drawing filtered waveforms.

### `setaudiofilter`

**Signature:** `setaudiofilter(enabled, [filterType], [cutoff], [q])`
Enables/disables and configures the audio output filter that affects actual audio playback. This function applies frequency filtering to the audio buffer before it's sent to the speakers, allowing you to hear the filtered audio in real-time. Uses a biquad (second-order IIR) filter for efficient processing.

**Parameters:**
- `enabled` (boolean, required): Whether to enable the output filter
- `filterType` (string, optional, default: "lowpass")
- `cutoff` (number, optional, default: 1000.0)
- `q` (number, optional, default: 0.707)

**Returns:** Nothing

**Notes:**
- Affects the actual audio output.
- Changing parameters resets filter state to avoid artifacts.
- Useful for real-time audio effects or smoothing.

```lua
setaudiofilter(true, "lowpass", 1200.0, 0.707)
-- ... later ...
setaudiofilter(false)  -- return to unfiltered audio
```

### `getaudiofilter`

**Signature:** `getaudiofilter()`
Gets the current audio output filter settings. Returns a table containing the enabled state, filter type, cutoff frequency, and Q factor of the output filter.

**Parameters:** None

**Returns:**
- `table` - Filter settings table with keys `enabled`, `filterType`, `cutoff`, `q`

**Example:**
```lua
local filter = getaudiofilter()
if filter.enabled then
    drawtext(4, 4, string.format("Filter: %s", filter.filterType), 0x27)
    drawtext(4, 14, string.format("Cutoff: %.0f Hz", filter.cutoff), 0x37)
    drawtext(4, 24, string.format("Q: %.2f", filter.q), 0x37)
else
    drawtext(4, 4, "Output Filter: DISABLED", 0x10)
end
```

---

## Sample Conversion Helpers

### `audiosampletofloat`

**Signature:** `audiosampletofloat(sample)`
Converts an audio sample (integer) to a normalized float value in the range -1.0 to 1.0. This is useful for floating-point audio processing, mathematical operations, and when working with normalized audio values.

**Parameters:**
- `sample` (integer, required): Audio sample value to convert
  - Typically ranges from -32768 to 32767 (16-bit signed integer)
  - Can accept values outside this range, but will be clamped to -1.0 to 1.0

**Returns:**
- `number` (float): Normalized float value between -1.0 and 1.0
  - Value of 0.0 represents silence
  - Positive values represent positive audio samples
  - Negative values represent negative audio samples
  - Values are clamped to prevent overflow

**Notes:**
- Normalizes based on 16-bit signed integer range (32768)
- Formula: `floatValue = sample / 32768.0`
- Output is clamped to -1.0 to 1.0 range
- Useful for:
  - Floating-point audio processing
  - Mathematical operations on audio
  - Normalized audio visualization
  - Audio analysis and filtering
- To convert back to integer, use `floattosample()`

**Example: Basic Usage:**
```lua
local sample = getaudiosample()
local floatVal = audiosampletofloat(sample)
print(string.format("Sample: %d -> Float: %.6f", sample, floatVal))
```

### `floattosample`

**Signature:** `floattosample(floatValue)`
Converts a normalized float value (-1.0 to 1.0) back to an audio sample (integer). This is the inverse operation of `audiosampletofloat()`.

**Parameters:**
- `floatValue` (number, required): Normalized float value to convert
  - Should be in range -1.0 to 1.0 (will be clamped if outside)
  - 0.0 represents silence
  - Positive values become positive samples
  - Negative values become negative samples

**Returns:**
- `integer`: Audio sample value (typically -32768 to 32767)
  - Clamped to prevent overflow
  - Formula: `sample = floatValue * 32768.0` (rounded to integer)

**Notes:**
- Converts from normalized float range to 16-bit signed integer range
- Input values are clamped to -1.0 to 1.0 before conversion
- Output is clamped to -32768 to 32767 to prevent overflow
- Useful for:
  - Converting processed float audio back to integer samples
  - Audio synthesis and generation
  - Applying floating-point effects to audio
- Round-trip conversion (sample -> float -> sample) may have small rounding errors

**Example: Basic Usage:**
```lua
local floatVal = 0.5  -- Half amplitude
local sample = floattosample(floatVal)
print(string.format("Float: %.2f -> Sample: %d", floatVal, sample))
-- Output: Float: 0.50 -> Sample: 16384
```

### `audiosampletouint8`

**Signature:** `audiosampletouint8(sample)`
Converts an audio sample (signed integer) to an 8-bit unsigned value (0-255). Useful for compatibility with 8-bit audio systems, visualization, or when working with unsigned audio formats.

**Parameters:**
- `sample` (integer, required): Audio sample value to convert
  - Typically ranges from -32768 to 32767 (16-bit signed)
  - Zero (silence) maps to 128 (middle of 8-bit range)

**Returns:**
- `integer`: 8-bit unsigned value (0-255)
  - Value 128 represents silence (zero crossing)
  - Values 0-127 represent negative samples
  - Values 129-255 represent positive samples

**Notes:**
- Conversion formula: `uint8 = (sample >> 8) + 128`
- Zero (silence) maps to 128 (middle of unsigned range)
- Maximum positive sample (32767) maps to 255
- Minimum negative sample (-32768) maps to 0
- Useful for:
  - 8-bit audio processing
  - Compatibility with legacy systems
  - Audio visualization (8-bit color mapping)
  - Compact audio storage
- To convert back to signed sample, use `uint8tosample()`

**Example: Basic Usage:**
```lua
local sample = getaudiosample()
local uint8Val = audiosampletouint8(sample)
print(string.format("Sample: %d -> Uint8: %d", sample, uint8Val))
```

### `uint8tosample`

**Signature:** `uint8tosample(uint8Value)`
Converts an 8-bit unsigned value (0-255) to an audio sample (signed integer). This is the inverse operation of `audiosampletouint8()`.

**Parameters:**
- `uint8Value` (integer, required): 8-bit unsigned value to convert
  - Should be in range 0-255 (will be clamped if outside)
  - Value 128 represents silence (zero crossing)
  - Values 0-127 represent negative samples
  - Values 129-255 represent positive samples

**Returns:**
- `integer`: Audio sample value (typically -32768 to 32767)
  - Clamped to prevent overflow
  - Formula: `sample = (uint8Value - 128) << 8`

**Notes:**
- Converts from 8-bit unsigned range to 16-bit signed integer range
- Input values are clamped to 0-255 before conversion
- Value 128 (middle of range) maps to 0 (silence)
- Maximum value (255) maps to 32512 (close to max positive)
- Minimum value (0) maps to -32768 (max negative)
- Useful for:
  - Converting 8-bit audio to 16-bit samples
  - Processing legacy audio formats
  - Audio synthesis from 8-bit data
- Round-trip conversion may have precision differences due to bit depth reduction

**Example: Basic Usage:**
```lua
local uint8Val = 200  -- Positive sample
local sample = uint8tosample(uint8Val)
print(string.format("Uint8: %d -> Sample: %d", uint8Val, sample))
```

### `normalizeaudiosample`

**Signature:** `normalizeaudiosample(sample, [maxValue])`
Normalizes an audio sample to a specific maximum value range. This is useful for scaling audio samples to different bit depths or volume levels.

**Parameters:**
- `sample` (integer, required): Audio sample value to normalize
  - Typically ranges from -32768 to 32767
  - Can accept values outside this range
- `maxValue` (number, optional, default: 32767): Maximum value for the normalization range
  - Must be positive
  - Determines the output range: -maxValue to +maxValue
  - Common values:
    - `127` for 8-bit signed range
    - `32767` for 16-bit signed range
    - `16383` for 14-bit range
    - Custom values for volume scaling

**Returns:**
- `integer`: Normalized sample value
  - Range: -maxValue to +maxValue
  - Preserves the relative amplitude of the original sample
  - Clamped to prevent overflow

**Notes:**
- Normalizes by first converting to float (-1.0 to 1.0), then scaling to the target range
- Preserves the original sample's relative amplitude and sign
- Useful for:
  - Scaling audio to different bit depths
  - Volume adjustment
  - Audio format conversion
  - Normalizing audio levels
- Formula: `normalized = (sample / 32768.0) * maxValue`

**Example: Basic Usage:**
```lua
local sample = 16384  -- Half amplitude
local normalized8 = normalizeaudiosample(sample, 127)
local normalized16 = normalizeaudiosample(sample, 32767)

print(string.format("Original: %d", sample))
print(string.format("Normalized to 8-bit: %d", normalized8))
print(string.format("Normalized to 16-bit: %d", normalized16))
```

### `monotostereo`

**Signature:** `monotostereo(monoSample)`
Converts a mono audio sample to stereo format by duplicating the sample to both left and right channels. Useful for converting mono audio sources to stereo output.

**Parameters:**
- `monoSample` (integer, required): Mono audio sample value
  - Single sample value to be duplicated to both channels

**Returns:**
- `table`: Stereo sample table with the following structure:
  - `left` (integer) - Left channel sample (same as input)
  - `right` (integer) - Right channel sample (same as input)
  - Both channels contain identical sample values

**Notes:**
- Simply duplicates the mono sample to both stereo channels
- No panning or spatial processing is applied
- Both channels receive identical values
- Useful for:
  - Converting mono audio to stereo format
  - Ensuring stereo compatibility
  - Audio format conversion
  - Mono source playback through stereo system
- To convert stereo back to mono, use `stereotomono()`

**Example: Basic Usage:**
```lua
local monoSample = 1000
local stereo = monotostereo(monoSample)

print(string.format("Mono: %d -> Stereo: L=%d, R=%d", 
    monoSample, stereo.left, stereo.right))
-- Output: Mono: 1000 -> Stereo: L=1000, R=1000
```

### `stereotomono`

**Signature:** `stereotomono(leftSample, rightSample)`
Converts stereo audio samples (left and right channels) to mono by averaging the two channels. Useful for downmixing stereo audio to mono or extracting a single channel representation.

**Parameters:**
- `leftSample` (integer, required): Left channel audio sample
  - Typically ranges from -32768 to 32767
- `rightSample` (integer, required): Right channel audio sample
  - Typically ranges from -32768 to 32767

**Returns:**
- `integer`: Mono audio sample (average of left and right)
  - Calculated as: `(leftSample + rightSample) / 2`
  - Preserves overall amplitude while combining channels

**Notes:**
- Averages the left and right channels to create a mono representation
- Simple arithmetic mean: `mono = (left + right) / 2`
- Preserves the overall audio level while combining channels
- Useful for:
  - Downmixing stereo to mono
  - Mono output compatibility
  - Audio analysis (single channel representation)
  - Reducing audio data size
- To convert mono to stereo, use `monotostereo()`

**Example: Basic Usage:**
```lua
local leftSample = 1000
local rightSample = 2000
local mono = stereotomono(leftSample, rightSample)

print(string.format("Stereo: L=%d, R=%d -> Mono: %d", 
    leftSample, rightSample, mono))
-- Output: Stereo: L=1000, R=2000 -> Mono: 1500
```

---

## See Also

- **[Monitoring Functions](Monitoring-Functions)** - Frame timing helpers including `getfps()` and `gettimedelta()`
- **[Input Functions](Input-Functions)** - Inspect or override controller states
- **[ROM Information Functions](ROM-Info-Functions)** - Mapper, battery, and Game Genie helpers for deeper metadata
- **[Home](Home)** - Return to the main wiki page