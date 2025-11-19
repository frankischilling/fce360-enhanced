# Monitoring Functions

The Monitoring Functions provide access to performance metrics, timing information, screen dimensions, and audio monitoring capabilities. These functions are essential for creating performance overlays, timing systems, audio visualizations, and debugging tools.

## Performance and Timing Functions

### `getfps`

**Signature:** `getfps()`
Returns the current frame rate as a floating-point number. The FPS is recalculated every second.

**Parameters:** None

**Returns:** 
- `number` - Current FPS value (typically 60.0 for normal speed, 120.0 for 2ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â fast-forward, etc.)

**Notes:**
- The FPS value is updated once per second, so rapid calls within the same second will return the same value.
- During fast-forward (RT trigger), FPS will reflect the increased emulation speed.
- During pause, FPS should remain stable (may show the last calculated value before pause).

**Example:**
```lua
local fps = getfps()
drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
```

**Advanced Example:**
```lua
local lastFPS = 0
local fpsHistory = {}

function gui()
    local fps = getfps()
    
    -- Track FPS history for averaging
    table.insert(fpsHistory, fps)
    if #fpsHistory > 60 then
        table.remove(fpsHistory, 1)
    end
    
    -- Calculate average
    local sum = 0
    for i = 1, #fpsHistory do
        sum = sum + fpsHistory[i]
    end
    local avgFPS = sum / #fpsHistory
    
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    drawtext(4, 12, string.format("Avg: %.1f", avgFPS), 0x30)
end
```

### `getframecount`

**Signature:** `getframecount()`
Gets the total frame count since the game started. Returns the number of frames that have been emulated since the ROM was loaded. Useful for timing, frame-accurate scripts, and tracking game progress.

**Parameters:** None

**Returns:**
- `integer` - Total number of frames emulated since game start
- Starts at 0 when a ROM is loaded
- Increments every frame during emulation
- Resets to 0 when the game is closed or a new ROM is loaded

**Notes:**
- The frame counter increments every frame during normal emulation
- The counter continues to increment during fast-forward and rewind
- The counter resets to 0 when you close the game or load a new ROM
- Useful for frame-accurate timing in TAS (Tool-Assisted Speedrun) scripts
- Can be used to calculate elapsed time: `seconds = frameCount / 60.0`
- The counter is independent of pause state (continues counting even when paused, if frames are being processed)

**Example: Display Frame Count:**
```lua
function gui()
    local frames = getframecount()
    drawtext(4, 4, string.format("Frames: %d", frames), 0x20)
end
```

**Example: Calculate Elapsed Time:**
```lua
function gui()
    local frames = getframecount()
    local seconds = math.floor(frames / 60)
    local minutes = math.floor(seconds / 60)
    local hours = math.floor(minutes / 60)
    
    drawtext(4, 4, string.format("Frames: %d", frames), 0x20)
    
    if hours > 0 then
        drawtext(4, 14, string.format("Time: %d:%02d:%02d", hours, minutes % 60, seconds % 60), 0x29)
    elseif minutes > 0 then
        drawtext(4, 14, string.format("Time: %d:%02d", minutes, seconds % 60), 0x29)
    else
        drawtext(4, 14, string.format("Time: %d sec", seconds), 0x29)
    end
end
```

**Example: Frame-Accurate Timing:**
```lua
function gui()
    local frames = getframecount()
    
    -- Perform action at specific frame
    if frames == 60 then
        print("One second has passed!")
    end
    
    -- Perform action every N frames
    if frames % 180 == 0 then  -- Every 3 seconds
        print(string.format("Frame %d: 3 seconds passed", frames))
    end
    
    drawtext(4, 4, string.format("Frame: %d", frames), 0x20)
end
```

### `getelapsedframes`

**Signature:** `getelapsedframes()`
Gets elapsed frames since game start. Returns the total number of frames that have elapsed since the ROM was loaded. This function is functionally identical to `getframecount()` but provides a name that pairs with `getelapsedtime()` for consistency.

**Parameters:** None

**Returns:**
- `integer` - Total number of frames that have elapsed since game start
- Starts at 0 when a ROM is loaded
- Increments every frame during emulation
- Resets to 0 when the game is closed or a new ROM is loaded

**Notes:**
- This function returns the same value as `getframecount()` - it's an alternative name for consistency with `getelapsedtime()`
- The frame counter increments every frame during normal emulation
- The counter continues to increment during fast-forward and rewind
- The counter resets to 0 when you close the game or load a new ROM
- Useful for frame-accurate timing in TAS (Tool-Assisted Speedrun) scripts
- Can be used to calculate elapsed time: `seconds = elapsedFrames / 60.0`
- The counter is independent of pause state (continues counting even when paused, if frames are being processed)
- Use this function when you want to pair it with `getelapsedtime()` for consistent naming

**Example: Display Elapsed Frames:**
```lua
function gui()
    local frames = getelapsedframes()
    drawtext(4, 4, string.format("Elapsed Frames: %d", frames), 0x20)
end
```

**Example: Pair with getelapsedtime():**
```lua
function gui()
    local frames = getelapsedframes()
    local time = getelapsedtime()
    
    drawtext(4, 4, string.format("Frames: %d", frames), 0x20)
    drawtext(4, 14, string.format("Time: %.2f sec", time), 0x29)
end
```

**Example: Calculate Elapsed Time from Frames:**
```lua
function gui()
    local frames = getelapsedframes()
    local seconds = math.floor(frames / 60)
    local minutes = math.floor(seconds / 60)
    local hours = math.floor(minutes / 60)
    
    drawtext(4, 4, string.format("Elapsed Frames: %d", frames), 0x20)
    
    if hours > 0 then
        drawtext(4, 14, string.format("Time: %d:%02d:%02d", hours, minutes % 60, seconds % 60), 0x29)
    elseif minutes > 0 then
        drawtext(4, 14, string.format("Time: %d:%02d", minutes, seconds % 60), 0x29)
    else
        drawtext(4, 14, string.format("Time: %d sec", seconds), 0x29)
    end
end
```

### `getframecycles`

**Signature:** `getframecycles()`
Gets the number of CPU cycles executed in the current frame. Returns the cycle count for the most recently completed frame. Useful for cycle-accurate timing, performance analysis, and understanding CPU load per frame.

**Parameters:** None

**Returns:**
- `integer` - Number of CPU cycles executed in the current frame
- Typical value: ~29,782 cycles per NTSC frame (varies slightly with DMC/DMA activity)
- The value represents cycles accumulated during frame emulation
- Returns the latched value from the last completed frame when called from `gui()` callback

**Notes:**
- The NES CPU runs at approximately 1.79 MHz (NTSC)
- At 60.0988 Hz frame rate, each frame contains approximately 29,782 CPU cycles
- The cycle count is latched at the end of each frame before the counter resets
- When called from `gui()` (which runs after frame completion), returns the latched value
- When called during frame emulation (e.g., from `beforeframe()`), returns the live accumulating value
- Cycle count can vary slightly between frames due to DMC (Delta Modulation Channel) DMA and other timing-sensitive operations
- Useful for cycle-accurate scripts, performance profiling, and understanding CPU utilization

**Example: Display Frame Cycles:**
```lua
function gui()
    local cycles = getframecycles()
    drawtext(4, 4, string.format("Cycles: %d", cycles), 0x20)
end
```

**Example: Performance Monitoring:**
```lua
function gui()
    local frames = getframecount()
    local cycles = getframecycles()
    local fps = getfps()
    
    -- Display performance metrics
    drawtext(4, 4, string.format("Frame: %d", frames), 0x20)
    drawtext(4, 14, string.format("Cycles: %d", cycles), 0x29)
    drawtext(4, 24, string.format("FPS: %.1f", fps), 0x37)
    
    -- Calculate CPU utilization (approximate)
    local expectedCycles = 29782  -- Typical NTSC frame
    local utilization = (cycles / expectedCycles) * 100
    drawtext(4, 34, string.format("CPU: %.1f%%", utilization), 0x2E)
end
```

### `getppucycles`

**Signature:** `getppucycles()`
Gets the number of PPU (Picture Processing Unit) cycles executed in the current frame. Provides a 3x finer-grained counter than `getframecycles()` and is ideal for overlays that need to align with PPU timing.

**Parameters:** None

**Returns:**
- `integer` - Number of PPU cycles executed in the current frame
- Typical value: ~89,346 cycles for NTSC (3 PPU cycles per 29,782 CPU cycles)
- Returns the latched value from the last completed frame when called from `gui()`

**Notes:**
- Derived directly from the CPU cycle timer (multiplied by 3) so it stays in sync with emulation timing
- Updates continuously during emulation and is latched at frame boundaries, just like `getframecycles()`
- Works in all callbacks (`beforeframe()`, `gui()`, etc.) and during pause/frame advance
- Use it to derive dot positions or verify CPU vs PPU timing in diagnostics (see `lua/test_ppucycles.lua`)

**Example: Display CPU vs PPU Cycles:**
```lua
function gui()
    local cpuCycles = getframecycles()
    local ppuCycles = getppucycles()
    local ratio = cpuCycles > 0 and (ppuCycles / cpuCycles) or 0

    drawtext(4, 4,  string.format("CPU cycles: %d", cpuCycles), 0x20)
    drawtext(4, 14, string.format("PPU cycles: %d", ppuCycles), 0x29)
    drawtext(4, 24, string.format("PPU/CPU ratio: %.2f", ratio), 0x2E)
end
```

### `getapucycles`

**Signature:** `getapucycles()`
Gets the APU (Audio Processing Unit) cycle count for the current frame. Because the APU runs at the CPU clock, this value mirrors `getframecycles()` but stays separate so overlays can label CPU vs APU timing explicitly.

**Parameters:** None

**Returns:**
- `integer` - Number of APU cycles executed in the current frame
- Matches CPU cycle counts (~29,782 per NTSC frame)
- Returns the latched value when sampled after a frame completes

**Notes:**
- Useful for logging or overlays that compare CPU, PPU, and APU workload
- Shares the same live/latched behavior as `getframecycles()` and `getppucycles()`
- See `lua/test_ppucycles.lua` for a quick console demo of all three counters

**Example: Display APU Cycles:**
```lua
function gui()
    local apuCycles = getapucycles()
    drawtext(4, 44, string.format("APU cycles: %d", apuCycles), 0x23)
end
```

### `getelapsedtime`

**Signature:** `getelapsedtime()`
Gets the elapsed time since the game started in seconds. Returns a floating-point number representing the total time that has passed since the ROM was loaded. Useful for timers, elapsed time displays, and time-based script logic.

**Parameters:** None

**Returns:**
- `number` (float) - Elapsed time in seconds since game start
- Starts at 0.0 when a ROM is loaded
- Increments continuously during emulation
- Calculated from frame count divided by NTSC frame rate (60.0988118623484 Hz)
- Resets to 0.0 when the game is closed or a new ROM is loaded

**Notes:**
- The time is calculated from the total frame count divided by the NTSC frame rate
- Provides sub-second precision (milliseconds)
- Time continues to advance during fast-forward and rewind
- Useful for creating timers, stopwatches, and time-based game logic
- Can be formatted into hours:minutes:seconds for display
- More convenient than manually calculating from `getframecount() / 60.0988`

**Example: Display Elapsed Time:**
```lua
function gui()
    local time = getelapsedtime()
    drawtext(4, 4, string.format("Time: %.2f sec", time), 0x20)
end
```

**Example: Format as Hours:Minutes:Seconds:**
```lua
function gui()
    local time = getelapsedtime()
    local totalSeconds = math.floor(time)
    local hours = math.floor(totalSeconds / 3600)
    local minutes = math.floor((totalSeconds % 3600) / 60)
    local seconds = totalSeconds % 60
    
    if hours > 0 then
        drawtext(4, 4, string.format("Time: %d:%02d:%02d", hours, minutes, seconds), 0x20)
    elseif minutes > 0 then
        drawtext(4, 4, string.format("Time: %d:%02d", minutes, seconds), 0x20)
    else
        drawtext(4, 4, string.format("Time: %d sec", seconds), 0x20)
    end
end
```

### `sleepframes`

**Signature:** `sleepframes(frames)`
Delays script execution for N frames by pausing emulation. The game will freeze during the sleep period, and script callbacks (`beforeframe()`, `gui()`, `script()`) will be skipped until the sleep duration completes. Useful for frame-accurate delays and timing control.

**Parameters:**
- `frames` (integer) - Number of frames to sleep
  - Must be >= 0
  - Sleep duration is calculated based on NTSC frame rate (60.0988118623484 Hz)
  - Each frame is approximately 16.639 milliseconds

**Returns:** Nothing (nil)

**Notes:**
- **Pauses emulation during sleep** - The game will freeze while sleeping
- Sleep duration is tracked by time (not frame count) since frames don't advance while paused
- The original pause state is preserved - if the game was already paused, it will remain paused after sleep completes
- Script callbacks are skipped during sleep - `beforeframe()`, `gui()`, and `script()` will not execute
- Sleep completes automatically when the specified duration elapses
- Useful for creating frame-accurate delays, cutscenes, or timed sequences
- Can be used to synchronize script actions with specific frame timings

**Example: Basic Sleep:**
```lua
function gui()
    local frame = getframecount()
    
    -- Sleep for 60 frames (~1 second) every 180 frames
    if frame % 180 == 0 then
        print(string.format("Frame %d: Sleeping for 60 frames", frame))
        sleepframes(60)
        print("Sleep complete, resuming")
    end
    
    drawtext(4, 4, string.format("Frame: %d", frame), 0x20)
end
```

### `gettime`

**Signature:** `gettime()`
Gets the current system time in milliseconds since system boot. Returns an integer representing the number of milliseconds that have elapsed since the system started. Useful for time-based logic, timestamps, and relative time measurements.

**Parameters:** None

**Returns:**
- `integer` - Current system time in milliseconds since system boot
- Value increases continuously while the system is running
- Useful for calculating time differences and implementing time-based logic
- Note: This returns milliseconds since system boot, not since Unix epoch

**Notes:**
- Returns milliseconds since system boot (not since Unix epoch)
- Useful for relative time measurements within a session
- Can be used to calculate time differences between events
- Value wraps around after approximately 49.7 days of continuous operation
- More precise than `getelapsedtime()` for short intervals (millisecond precision)
- System time continues to advance even when emulation is paused
- Useful for implementing debouncing, throttling, and periodic actions

**Example: Basic Time Display:**
```lua
function gui()
    local time = gettime()
    drawtext(4, 4, string.format("Time: %d ms", time), 0x20)
end
```

**Example: Periodic Action (Every 3 Seconds):**
```lua
local lastActionTime = nil

function gui()
    local currentTime = gettime()
    
    -- Initialize on first frame
    if lastActionTime == nil then
        lastActionTime = currentTime
    end
    
    -- Perform action every 3000 milliseconds (3 seconds)
    if currentTime - lastActionTime >= 3000 then
        print(string.format("Action at %d ms", currentTime))
        lastActionTime = currentTime
    end
    
    drawtext(4, 4, string.format("Time: %d ms", currentTime), 0x20)
end
```

### `gettimedelta`

**Signature:** `gettimedelta()`
Gets the time since the last frame in seconds. Returns a floating-point number representing the elapsed time between the current frame and the previous frame. Essential for delta time calculations, physics simulations, and frame-independent movement.

**Parameters:** None

**Returns:**
- `number` (float) - Time since last frame in seconds
- Returns `0.0` on the first call (no previous frame to compare)
- Subsequent calls return the actual time difference
- Typical values: ~0.0167 seconds (60 FPS) or ~0.033 seconds (30 Hz callback rate)
- Value increases when paused (time advances even when frames don't)

**Notes:**
- Returns time difference between consecutive calls to the function
- First call always returns `0.0` (no previous frame)
- Useful for frame-independent calculations (movement, physics, animations)
- Delta time allows consistent behavior regardless of frame rate
- Larger values indicate longer time between frames (frame drops, pauses)
- Smaller values indicate faster frame rate or more frequent callbacks
- Should be called every frame for accurate delta time calculations
- Essential for smooth, frame-rate independent movement and physics

**Example: Basic Delta Time Display:**
```lua
function gui()
    local delta = gettimedelta()
    drawtext(4, 4, string.format("Delta: %.4f sec", delta), 0x20)
    drawtext(4, 14, string.format("%.2f ms", delta * 1000), 0x29)
end
```

**Example: Frame-Independent Movement:**
```lua
local position = 0.0
local velocity = 100.0  -- pixels per second

function gui()
    local delta = gettimedelta()
    
    -- Move at constant velocity regardless of frame rate
    position = position + (velocity * delta)
    
    -- Wrap around screen
    if position > 240 then
        position = 0
    end
    
    drawtext(math.floor(position), 4, "X", 0x2D)
    drawtext(4, 14, string.format("Pos: %.1f (%.1f px/s)", position, velocity), 0x20)
end
```

## Profiling Functions

### `beginprofile(tag)`

**Signature:** `beginprofile(tag)`
Begins a profiling section identified by `tag`. Stores the current timestamp so elapsed time can be measured later.

**Parameters:**
- `tag` (string) - Non-empty identifier shared with `endprofile()`

**Returns:**
- `nil`

**Notes:**
- Each tag tracks its own start time; calling `beginprofile` again for the same tag overwrites the previous start.
- Pair with [`endprofile(tag)`](#endprofiletag) to emit a `[PROFILE]` line in the Lua console/log.
- Profiling output uses milliseconds (via `GetTickCount()`), making it ideal for script-side performance checks.

**Example:**
```lua
function gui()
    beginprofile("overlay")
    -- heavy drawing work
    endprofile("overlay")
end
```

### `endprofile(tag)`

**Signature:** `endprofile(tag)`
Ends a profiling section and logs the elapsed time for `tag`.

**Parameters:**
- `tag` (string) - Identifier previously passed to `beginprofile()`

**Returns:**
- `nil`

**Notes:**
- Prints `[PROFILE] <tag>: <ms> ms` to the Lua console/log.
- If `endprofile()` is called without a matching `beginprofile()`, a warning is logged instead of raising an error.
- Removing a tag from the internal map keeps memory usage low when profiling many sections.

**Example:**
```lua
beginprofile("loader")
-- do some work
endprofile("loader")  -- logs elapsed milliseconds
```

---

## Screen Dimension Functions

### `getframetime_ms()`

**Signature:** `getframetime_ms()`
Returns the elapsed time between the current frame and previous frame in milliseconds. Essentially a millisecond version of [`gettimedelta()`](#gettimedelta).

**Parameters:** None

**Returns:**
- `number` - Milliseconds since the last call to `getframetime_ms()` or `gettimedelta()` (0 on the first call).

**Notes:**
- Uses `GetTickCount()` so it reflects fast-forward, rewind, and pauses.
- Calling either function updates the shared timestamp; call just one per frame for consistent measurements.

**Example:**
```lua
function gui()
    local ms = getframetime_ms()
    drawtext(4, 28, string.format("Frame %.2f ms", ms), 0x28)
end
```

### `getjitter_ms()`

**Signature:** `getjitter_ms()`
Returns the absolute deviation from the ideal 60 Hz frame duration (16.64 ms). Handy for spotting pacing spikes even when average frame time looks fine.

**Parameters:** None

**Returns:**
- `number` - Absolute milliseconds difference between the current frame time and the ideal 16.64 ms (0 on the first call).

**Notes:**
- Shares the same underlying timestamp source as `getframetime_ms()` but does not disturb its state, so you can call both each frame.
- Fast-forward, pauses, and dropped frames will increase jitter; steady 60 Hz should report near zero.

**Example:**
```lua
local jitter = getjitter_ms()
drawtext(4, 40, string.format("Jitter: %.3f ms", jitter), 0x27)
```

### `getluamem()`

**Signature:** `getluamem()`
Returns a table describing the current Lua allocator usage. Helpful for watching memory growth in long-running scripts or after allocating large tables/images.

**Parameters:** None

**Returns:**
- `table` with the fields:
  - `kilobytes` (number) â€“ Approximate KB reported by Lua (same as `collectgarbage("count")`).
  - `bytes` (number) â€“ Floating-point byte count (`kilobytes * 1024`).
  - `rounded_bytes` (integer) â€“ Byte count rounded to an integer.

**Notes:**
- Uses `lua_gc(L, LUA_GCCOUNT/B)` internally, so it does not trigger garbage collection.
- Useful for detecting runaway allocations or verifying that cleanup logic works (watch the values drop after `collectgarbage()`).

**Example:**
```lua
local mem = getluamem()
drawtext(4, 60, string.format("Lua: %.2f KB", mem.kilobytes or 0), 0x20)
```

### `collectgarbage_now()`

**Signature:** `collectgarbage_now()`
Forces an immediate full garbage collection cycle (equivalent to `collectgarbage("collect")`). Useful after freeing large tables/images to reclaim memory before continuing.

**Parameters:** None

**Returns:**
- `nil`

**Notes:**
- Uses Lua's `lua_gc(..., LUA_GCCOLLECT)` under the hood, so it blocks until the GC finishes.
- Pair it with [`getluamem()`](#getluamem) to confirm memory drops after cleanup.

**Example:**
```lua
junk = nil
collectgarbage_now()
local mem = getluamem()
print(string.format("Lua mem: %.2f KB", mem.kilobytes))
```

### `getscreenwidth`

**Signature:** `getscreenwidth()`
Gets the screen width in pixels. Returns an integer representing the width of the NES screen. Useful for dynamic positioning, centering calculations, and screen-relative layouts.

**Parameters:** None

**Returns:**
- `integer` - Screen width in pixels
- Always returns `256` for NES (standard NES screen width)
- Constant value regardless of game or ROM

**Notes:**
- Returns the standard NES screen width (256 pixels)
- Useful for calculating center positions (`width / 2`)
- Essential for right-aligned text and elements
- Can be used with `getscreenheight()` for full screen dimensions
- Screen coordinates range from `0` to `width - 1` (0-255)

**Example: Center Text Horizontally:**
```lua
function gui()
    local width = getscreenwidth()
    local text = "CENTERED"
    local textWidth = gettextwidth(text)
    local x = (width - textWidth) / 2  -- Center horizontally
    drawtext(x, 100, text, 0x29)
end
```

### `getscreenheight`

**Signature:** `getscreenheight()`
Gets the screen height in pixels. Returns an integer representing the height of the NES screen. Useful for dynamic positioning, vertical centering, and screen-relative layouts.

**Parameters:** None

**Returns:**
- `integer` - Screen height in pixels
- Always returns `240` for NES (standard NES screen height)
- Constant value regardless of game or ROM

**Notes:**
- Returns the standard NES screen height (240 pixels)
- Useful for calculating center positions (`height / 2`)
- Essential for bottom-aligned text and elements
- Can be used with `getscreenwidth()` for full screen dimensions
- Screen coordinates range from `0` to `height - 1` (0-239)

**Example: Full Screen Centering:**
```lua
function gui()
    local width = getscreenwidth()
    local height = getscreenheight()
    local text = "CENTERED"
    
    local textWidth = gettextwidth(text)
    local textHeight = gettextheight(text)
    
    local x = (width - textWidth) / 2   -- Center horizontally
    local y = (height - textHeight) / 2  -- Center vertically
    
    drawtext(x, y, text, 0x29)
end
```

### `getscreensize`

**Signature:** `getscreensize()`
Gets screen dimensions as a table. Returns a Lua table containing both width and height values. Useful for getting both screen dimensions in a single call and for screen size queries.

**Parameters:** None

**Returns:**
- `table` - Screen dimensions table with the following structure:
  - `width` (integer) - Screen width in pixels (256 for NES)
  - `height` (integer) - Screen height in pixels (240 for NES)
  - `[1]` (integer) - Screen width in pixels (same as `width`)
  - `[2]` (integer) - Screen height in pixels (same as `height`)
- Can be accessed via named keys (`size.width`, `size.height`) or array indices (`size[1]`, `size[2]`)

**Notes:**
- Returns a table with both width and height values
- Provides convenient access to both dimensions in a single call
- Supports both named key access (`size.width`, `size.height`) and array index access (`size[1]`, `size[2]`)
- More efficient than calling `getscreenwidth()` and `getscreenheight()` separately
- Useful for functions that need both dimensions at once
- Table values are constant (256 x 240 for NES)

**Example: Basic Usage:**
```lua
local size = getscreensize()
print(string.format("Screen: %d x %d", size.width, size.height))
```

**Example: Calculate Center:**
```lua
function gui()
    local size = getscreensize()
    local centerX = size.width / 2
    local centerY = size.height / 2
    
    drawtext(centerX - 20, centerY, "CENTER", 0x29)
end
```

## Audio Monitoring Functions

Audio sampling, per-channel analysis, FFTs, and filtering are now documented on the dedicated [Audio Functions](Audio-Functions) page. This section of the monitoring guide focuses on timing and performance metrics, so please refer to the audio page for `getaudioenabled()`, `getaudiosample()`, channel helpers, and related examples.

## Audio Conversion Functions

Sample conversion helpers (`audiosampletofloat()`, `normalizeaudiosample()`, and others) have also moved to the [Audio Functions](Audio-Functions) page to keep the monitoring documentation concise.

## See Also

- **[Drawing Functions](Drawing-Functions)** - For displaying monitoring data
- **[Memory Functions](Memory-Functions)** - For memory monitoring
- **[State Management Functions](State-Management-Functions)** - For save state operations
- **[Home](Home)** - Return to the main wiki page
