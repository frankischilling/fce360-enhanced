# Input Recording Functions

The input recording helpers let Lua scripts capture controller activity frame-by-frame, store it, and play it back later. Recordings always include every player (0-3) and honour the final button state for each frame after any Lua overrides.

## Workflow Overview
1. Call `startinputrecording()` to begin capturing input.
2. (Optional) Use `setrecordingmarker(name)` during recording to bookmark specific positions.
3. (Optional) Use `trimrecording(startFrame, endFrame)` during recording to edit the recording by keeping only a specific frame range.
4. When you are done, call `stopinputrecording()` to retrieve the recording as a Lua table.
5. Use `saveinputrecording()` to save the recording to disk, or pass that table to `playinputrecording()` to replay the captured input exactly as it was recorded.
6. Use `loadinputrecording()` to load a previously saved recording from disk and automatically start playback.
7. (Optional) During playback, use `jumptorecordingmarker(name)` to navigate to bookmarked positions.
8. (Optional) Use `setplaybackspeed(mult)` to control playback speed (slow motion or faster playback).

---

### `startinputrecording()`

**Signature:** `startinputrecording()`

Start a new recording session for all four controller ports.

- **Parameters:** none
- **Returns:** `boolean`
  - `true` if recording has begun
  - `false` when a recording is already in progress (existing recording continues)
- **Behaviour & Notes:**
  - Any previously captured data is cleared when a new recording starts.
  - Button states from every player are captured once per frame.
  - The recorded value is the *final* state of each frame after `setjoypad()`, `pressbutton()`, `playinputrecording()`, etc.
  - Keep a local flag in Lua if you need to know whether you are currently recording.

```lua
local isRecording = false
local lastToggle = false

function beforeframe()
    local hw = gethardwarejoypad(0)
    local selectPressed = (math.floor(hw / getbuttonmask("SELECT")) % 2 == 1)

    if selectPressed and not lastToggle then
        if not isRecording then
            if startinputrecording() then
                print("Recording started")
                isRecording = true
            end
        else
            print("Already recording")
        end
    end

    lastToggle = selectPressed
end
```

---

### `stopinputrecording()`

**Signature:** `stopinputrecording()`

Stop capturing and return the recorded input.

- **Parameters:** none
- **Returns:** table containing four keys (`player0` ... `player3`). Each value is a 1-indexed array of integer button masks (0x00-0xFF) representing each frame.
- **Errors:** Raises a Lua error if you call it while no recording is active.
- **Notes:**
  - Players with no activity still return an empty array for consistency.
  - Save the returned table if you want to replay later or write it to disk after serialising.
  - The table is directly compatible with `playinputrecording()`.

```lua
local isRecording = false
local recording = nil

function beforeframe()
    local hw = gethardwarejoypad(0)
    local selectMask = getbuttonmask("SELECT")
    local selectPressed = (math.floor(hw / selectMask) % 2 == 1)

    if selectPressed and not lastToggle then
        if not isRecording then
            startinputrecording()
            isRecording = true
            print("Recording in progress")
        else
            recording = stopinputrecording()
            isRecording = false
            print(string.format("Captured %d frames", #recording.player0))
        end
    end

    lastToggle = selectPressed
end
```

---

### `playinputrecording(data)`

**Signature:** `playinputrecording(data)`

Replay a recording created by `stopinputrecording()`.

- **Parameters:**
  - `data` - table with keys `player0` ... `player3`; each value is an array of button masks.
- **Returns:** nothing
- **Behaviour:**
  - Playback overrides *all* other input sources (hardware pads, `setjoypad()`, `pressbutton()`, etc.) until the recording finishes.
  - Each player advances independently. If `player2` has fewer frames than `player0`, their playback simply ends earlier.
  - Calling it multiple times replays the same recording again.
- **Tips:**
  - Useful for tool-assisted demonstrations, regression tests, or repeatable gameplay snippets.
  - You may edit the data table before playback to tweak the captured input.

```lua
local recording = nil
local lastSelect = false
local lastA = false

function beforeframe()
    local hw = gethardwarejoypad(0)
    local selectPressed = (math.floor(hw / getbuttonmask("SELECT")) % 2 == 1)
    local aPressed = (math.floor(hw / getbuttonmask("A")) % 2 == 1)

    if selectPressed and not lastSelect then
        if recording == nil then
            startinputrecording()
            print("Recording now...")
        else
            recording = stopinputrecording()
            print("Recording saved")
        end
    end

    if aPressed and not lastA and recording ~= nil then
        playinputrecording(recording)
        print("Playback started")
    end

    lastSelect = selectPressed
    lastA = aPressed
end
```

### Editing a Recording Before Playback
You can modify the table between capture and playback; for example, forcing a jump on frame 10:

```lua
local jumpMask = getbuttonmask("A")
if recording.player0[10] then
    recording.player0[10] = recording.player0[10] | jumpMask
end
playinputrecording(recording)
```

---

### `saveinputrecording(path)`

**Signature:** `saveinputrecording(path)`

Saves the current input recording to a file on disk.

- **Parameters:**
  - `path` - file path string (relative or absolute)
- **Returns:** `boolean`
  - `true` if the file was successfully saved
  - `false` if there's no recording data, the file cannot be opened, or write fails
- **Behaviour & Notes:**
  - Saves the recording data that was captured by `startinputrecording()` and `stopinputrecording()`.
  - If no recording data exists, returns `false`.
  - Files are saved to `hdd1:\fce360-enhanced\lua\recordings\` by default (or `game:\lua\recordings\` as fallback).
  - Absolute paths (containing `:` or starting with `\` or `/`) are used as-is.
  - The parent directory (`recordings`) is created automatically if it doesn't exist.
  - File format: One frame per line, comma-separated button masks for each player (0-3).
  - Use case: Save TAS inputs for later use or sharing.

```lua
local isRecording = false
local recordedData = nil
local lastSelect = false

function beforeframe()
    local hw = gethardwarejoypad(0)
    local selectPressed = isbuttonpressed(0, "SELECT")
    
    -- Toggle recording
    if selectPressed and not lastSelect then
        if not isRecording then
            startinputrecording()
            isRecording = true
            print("Recording started")
        else
            recordedData = stopinputrecording()
            isRecording = false
            print("Recording stopped")
            
            -- Save to file
            local success = saveinputrecording("my_recording.txt")
            if success then
                print("Recording saved to file")
            else
                print("Failed to save recording")
            end
        end
    end
    
    lastSelect = selectPressed
end
```

**Example: Save with custom filename**
```lua
-- Save recording with timestamp
local frameCount = getframecount()
local filename = string.format("recording_%d.txt", frameCount)
local success = saveinputrecording(filename)

if success then
    print(string.format("Saved to: %s", filename))
end
```

---

### `loadinputrecording(path)`

**Signature:** `loadinputrecording(path)`

Loads an input recording from a file and automatically starts playback.

- **Parameters:**
  - `path` - file path string (relative or absolute)
- **Returns:** `boolean`
  - `true` if the file was successfully loaded and playback started
  - `false` if the file cannot be found, opened, or parsed
- **Behaviour & Notes:**
  - Loads a recording file that was previously saved with `saveinputrecording()`.
  - Automatically starts playback immediately after loading.
  - Files are searched in `hdd1:\fce360-enhanced\lua\recordings\` by default (or `game:\lua\recordings\` as fallback).
  - Absolute paths (containing `:` or starting with `\` or `/`) are used as-is.
  - File format: One frame per line, comma-separated button masks for each player (0-3).
  - Playback overrides all other input sources until the recording finishes.
  - Use case: Playback TAS inputs that were previously saved to disk.

```lua
function beforeframe()
    local xPressed = isxboxbuttonpressed(0, "X")
    
    -- Load and play a recording when X is pressed
    if xPressed then
        local success = loadinputrecording("my_recording.txt")
        if success then
            print("Recording loaded and playback started")
        else
            print("Failed to load recording")
        end
    end
end
```

**Example: Load with custom filename**
```lua
-- Load recording with timestamp
local frameCount = getframecount()
local filename = string.format("recording_%d.txt", frameCount)
local success = loadinputrecording(filename)

if success then
    print(string.format("Loaded and playing: %s", filename))
end
```

**Example: Try multiple files**
```lua
-- Try loading different recording files
local recordings = {
    "test_recording.txt",
    "recording_1.txt",
    "my_recording.txt"
}

for i, filename in ipairs(recordings) do
    if loadinputrecording(filename) then
        print("Loaded: " .. filename)
        break
    end
end
```

---

### `setrecordingmarker(name)`

**Signature:** `setrecordingmarker(name)`

Sets a marker at the current frame in the recording. Markers can be used to bookmark specific positions in a recording for later reference or navigation.

- **Parameters:**
  - `name` - marker name string (required)
- **Returns:** nothing
- **Behaviour & Notes:**
  - Sets a marker at the current frame in the active recording.
  - Only works when recording is active (after `startinputrecording()` and before `stopinputrecording()`).
  - If recording is not active, the function does nothing (no error).
  - If the marker name is empty, the function does nothing.
  - Multiple markers can be set during a single recording.
  - Markers are cleared when a new recording starts (when `startinputrecording()` is called).
  - The frame number is stored as the current recording frame (0-indexed from the start of recording).
  - Use case: Bookmark positions in recording for later reference, navigation, or analysis.

```lua
local isRecording = false
local lastSelect = false

function beforeframe()
    local selectPressed = isbuttonpressed(0, "SELECT")
    
    -- Toggle recording
    if selectPressed and not lastSelect then
        if not isRecording then
            startinputrecording()
            isRecording = true
            print("Recording started")
        else
            stopinputrecording()
            isRecording = false
            print("Recording stopped")
        end
    end
    lastSelect = selectPressed
    
    -- Set marker when A button is pressed during recording
    if isRecording and isxboxbuttonpressed(0, "A") then
        setrecordingmarker("jump_point")
        print("Marker 'jump_point' set")
    end
end
```

**Example: Set multiple markers**
```lua
local isRecording = false
local frameCount = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Start recording with SELECT
    if isbuttonpressed(0, "SELECT") and not isRecording then
        startinputrecording()
        isRecording = true
        frameCount = 0  -- Reset frame counter for this recording
    end
    
    -- Set different markers based on button presses
    if isRecording then
        if isxboxbuttonpressed(0, "A") then
            setrecordingmarker("marker_A")
        elseif isxboxbuttonpressed(0, "B") then
            setrecordingmarker("marker_B")
        elseif isxboxbuttonpressed(0, "X") then
            setrecordingmarker("marker_X")
        end
    end
end
```

**Example: Set markers at specific events**
```lua
local isRecording = false
local lastHealth = 0

function beforeframe()
    -- Start/stop recording logic here...
    
    if isRecording then
        -- Set marker when health changes significantly
        local currentHealth = readbyte(0x006A)  -- Example health address
        if math.abs(currentHealth - lastHealth) > 10 then
            setrecordingmarker("health_change")
            print("Health changed - marker set")
        end
        lastHealth = currentHealth
    end
end
```

---

### `jumptorecordingmarker(name)`

**Signature:** `jumptorecordingmarker(name)`

Jumps to a marker position in the current playback. This allows you to navigate to specific bookmarked frames during playback.

- **Parameters:**
  - `name` - marker name string (required)
- **Returns:** `boolean`
  - `true` if the jump was successful
  - `false` if playback is not active, marker not found, or marker frame is out of bounds
- **Behaviour & Notes:**
  - Only works when playback is active (after `playinputrecording()` or `loadinputrecording()`).
  - Looks up the marker by name that was previously set with `setrecordingmarker()`.
  - If the marker is not found, returns `false`.
  - If the marker frame is out of bounds (greater than playback length), it clamps to the last valid frame.
  - The playback frame counter is set to the marker's frame position.
  - Use case: Navigate to specific positions in a recording during playback.

```lua
local isRecording = false
local recordedData = nil
local isPlayback = false

function beforeframe()
    -- Start recording with SELECT
    if isbuttonpressed(0, "SELECT") and not isRecording and not isPlayback then
        startinputrecording()
        isRecording = true
    end
    
    -- Set marker during recording
    if isRecording and isbuttonpressed(0, "A") then
        setrecordingmarker("jump_point")
        print("Marker 'jump_point' set")
    end
    
    -- Stop recording with B
    if isRecording and isbuttonpressed(0, "B") then
        recordedData = stopinputrecording()
        isRecording = false
    end
    
    -- Start playback with Y
    if recordedData ~= nil and not isPlayback and isxboxbuttonpressed(0, "Y") then
        playinputrecording(recordedData)
        isPlayback = true
    end
    
    -- Jump to marker during playback
    if isPlayback and isbuttonpressed(0, "A") then
        local success = jumptorecordingmarker("jump_point")
        if success then
            print("Jumped to marker 'jump_point'")
        else
            print("Failed to jump to marker")
        end
    end
end
```

**Example: Jump to multiple markers**
```lua
local recordedData = nil
local isPlayback = false

function beforeframe()
    -- Start playback logic here...
    
    if isPlayback then
        -- Jump to different markers based on button presses
        if isbuttonpressed(0, "A") then
            jumptorecordingmarker("marker_A")
        elseif isbuttonpressed(0, "START") then
            jumptorecordingmarker("marker_START")
        elseif isbuttonpressed(0, "UP") then
            jumptorecordingmarker("marker_UP")
        elseif isbuttonpressed(0, "DOWN") then
            jumptorecordingmarker("marker_DOWN")
        end
    end
end
```

**Example: Jump with error handling**
```lua
function beforeframe()
    if isPlayback then
        local markerName = "checkpoint_1"
        local success = jumptorecordingmarker(markerName)
        
        if not success then
            print(string.format("Failed to jump to marker '%s'", markerName))
            print("Make sure playback is active and marker exists")
        else
            print(string.format("Jumped to marker '%s'", markerName))
        end
    end
end
```

---

### `setplaybackspeed(mult)`

**Signature:** `setplaybackspeed(mult)`

Sets the playback speed multiplier for input recordings. Controls how fast or slow the playback advances through recorded frames.

- **Parameters:**
  - `mult` (number): Speed multiplier
    - `1.0` = normal speed (default)
    - `0.5` = half speed (slow motion - each frame plays twice)
    - `2.0` = double speed
    - `4.0` = quadruple speed
    - Valid range: `0.1` to `10.0` (automatically clamped)
- **Returns:** Nothing
- **Behaviour & Notes:**
  - Only affects playback when `playinputrecording()` or `loadinputrecording()` is active.
  - At speeds **>= 1.0**: Plays every frame sequentially (one per emulator frame) to ensure no inputs are missed. Playback completes at the same rate regardless of speed multiplier.
  - At speeds **< 1.0**: Holds each frame longer (each frame plays multiple times) for slow motion effects.
  - The speed multiplier persists until changed or playback stops.
  - Use case: Slow down playback to analyze frame-perfect inputs, or speed up playback for faster testing (while preserving all inputs).

```lua
local recordedData = nil
local isPlayback = false
local currentSpeed = 1.0

function beforeframe()
    -- Start recording with SELECT
    if isbuttonpressed(0, "SELECT") and not isRecording and not isPlayback then
        startinputrecording()
        isRecording = true
    end
    
    -- Stop recording with B
    if isRecording and isbuttonpressed(0, "B") then
        recordedData = stopinputrecording()
        isRecording = false
    end
    
    -- Start playback with Y
    if recordedData ~= nil and not isPlayback and isxboxbuttonpressed(0, "Y") then
        playinputrecording(recordedData)
        isPlayback = true
        currentSpeed = 1.0
        setplaybackspeed(1.0)  -- Reset to normal speed
    end
    
    -- Change speed during playback
    if isPlayback then
        if isbuttonpressed(0, "A") then
            currentSpeed = 0.5
            setplaybackspeed(0.5)  -- Half speed (slow motion)
            print("Playback speed: 0.5x")
        elseif isbuttonpressed(0, "B") then
            currentSpeed = 1.0
            setplaybackspeed(1.0)  -- Normal speed
            print("Playback speed: 1.0x")
        elseif isbuttonpressed(0, "X") then
            currentSpeed = 2.0
            setplaybackspeed(2.0)  -- Double speed
            print("Playback speed: 2.0x")
        elseif isbuttonpressed(0, "START") then
            currentSpeed = 0.25
            setplaybackspeed(0.25)  -- Quarter speed (very slow)
            print("Playback speed: 0.25x")
        elseif isbuttonpressed(0, "UP") then
            currentSpeed = 4.0
            setplaybackspeed(4.0)  -- Quadruple speed
            print("Playback speed: 4.0x")
        end
    end
end
```

**Example: Speed control with display**
```lua
local recordedData = nil
local isPlayback = false
local currentSpeed = 1.0

function script()
    if isPlayback then
        -- Display current speed
        local speedText = string.format("Playback: %.2fx", currentSpeed)
        drawtext(4, 4, speedText, 0x39)
    end
end

function beforeframe()
    -- Playback and speed control logic here...
    if isPlayback then
        if isbuttonpressed(0, "A") then
            currentSpeed = 0.5
            setplaybackspeed(0.5)
        elseif isbuttonpressed(0, "B") then
            currentSpeed = 1.0
            setplaybackspeed(1.0)
        elseif isbuttonpressed(0, "X") then
            currentSpeed = 2.0
            setplaybackspeed(2.0)
        end
    end
end
```

**Example: Slow motion analysis**
```lua
local recordedData = nil
local isPlayback = false

function beforeframe()
    -- After loading or starting playback...
    
    if isPlayback then
        -- Slow down to analyze frame-perfect inputs
        setplaybackspeed(0.25)  -- Quarter speed for detailed analysis
    end
end
```

---

### `trimrecording(startFrame, endFrame)`

**Signature:** `trimrecording(startFrame, endFrame)`

Trims the active recording to a specific frame range. Removes all frames outside the specified range and adjusts marker positions accordingly.

- **Parameters:**
  - `startFrame` (number): First frame to keep (0-indexed, inclusive)
  - `endFrame` (number): Last frame to keep (0-indexed, inclusive)
- **Returns:** `boolean`
  - `true` if trimming succeeded
  - `false` if recording is not active, frame range is invalid, or frames are out of bounds
- **Behaviour & Notes:**
  - **Only works when recording is active** (after `startinputrecording()` and before `stopinputrecording()`).
  - Frame numbers are 0-indexed (first frame is 0, second frame is 1, etc.).
  - `startFrame` must be <= `endFrame`, and both must be >= 0.
  - Both `startFrame` and `endFrame` must be within the bounds of the current recording.
  - After trimming, the recording contains only frames from `startFrame` to `endFrame` (inclusive).
  - All four players are trimmed to the same range. If a player has fewer frames, missing frames are padded with 0.
  - **Markers are automatically updated:**
    - Markers within the trimmed range are kept and their positions are adjusted (subtract `startFrame`).
    - Markers outside the trimmed range are removed.
  - Use case: Edit recordings by removing unwanted frames, keeping only the important sections.

```lua
local isRecording = false

function beforeframe()
    -- Start recording with SELECT
    if isbuttonpressed(0, "SELECT") and not isRecording then
        startinputrecording()
        isRecording = true
        print("Recording started")
    end
    
    -- Trim recording while recording (keep frames 10-50)
    if isRecording and isbuttonpressed(0, "X") then
        local success = trimrecording(10, 50)
        if success then
            print("Recording trimmed: kept frames 10-50 (41 frames)")
        else
            print("Trim failed: check frame range")
        end
    end
    
    -- Stop recording with B
    if isRecording and isbuttonpressed(0, "B") then
        local data = stopinputrecording()
        isRecording = false
        print("Recording stopped")
    end
end
```

**Example: Trim based on frame count**
```lua
local isRecording = false
local recordStartFrame = 0

function beforeframe()
    if isbuttonpressed(0, "SELECT") and not isRecording then
        startinputrecording()
        isRecording = true
        recordStartFrame = getframecount()
        print("Recording started")
    end
    
    -- Trim to last 100 frames
    if isRecording and isbuttonpressed(0, "A") then
        -- Get current frame count in recording
        local currentFrame = getframecount() - recordStartFrame - 1
        if currentFrame >= 100 then
            local startFrame = currentFrame - 99  -- Keep last 100 frames
            local endFrame = currentFrame
            local success = trimrecording(startFrame, endFrame)
            if success then
                print(string.format("Trimmed to last 100 frames (%d-%d)", startFrame, endFrame))
            end
        else
            print("Not enough frames to trim")
        end
    end
    
    if isRecording and isbuttonpressed(0, "B") then
        local data = stopinputrecording()
        isRecording = false
    end
end
```

**Example: Trim and preserve markers**
```lua
local isRecording = false

function beforeframe()
    if isbuttonpressed(0, "SELECT") and not isRecording then
        startinputrecording()
        isRecording = true
    end
    
    -- Set markers at important points
    if isRecording then
        if isbuttonpressed(0, "A") then
            setrecordingmarker("jump_start")
        elseif isbuttonpressed(0, "B") then
            setrecordingmarker("jump_end")
        end
    end
    
    -- Trim to keep only the section between markers (example: frames 20-80)
    if isRecording and isbuttonpressed(0, "X") then
        local success = trimrecording(20, 80)
        if success then
            print("Trimmed to frames 20-80")
            print("Markers within range were adjusted, others removed")
        end
    end
    
    if isRecording and isbuttonpressed(0, "Y") then
        local data = stopinputrecording()
        isRecording = false
    end
end
```

**Example: Error handling**
```lua
function beforeframe()
    if isRecording and isbuttonpressed(0, "X") then
        local startFrame = 10
        local endFrame = 50
        
        -- Validate before trimming
        if startFrame < 0 or endFrame < 0 then
            print("ERROR: Frame numbers must be >= 0")
            return
        end
        
        if startFrame > endFrame then
            print("ERROR: Start frame must be <= end frame")
            return
        end
        
        local success = trimrecording(startFrame, endFrame)
        if not success then
            print("ERROR: Trim failed - check recording is active and frame range is valid")
        else
            print(string.format("Successfully trimmed to frames %d-%d", startFrame, endFrame))
        end
    end
end
```

---

## See Also

- **[Input Functions](Input-Functions)** - Inspect or override controller states (`getjoypad()`, `setjoypad()`, `pressbutton()`, etc.).
- **[State Management Functions](State-Management-Functions)** - Save states allow you to combine recordings with quick resets for testing.
- **[File I/O Functions](File-IO-Functions)** - Additional file operations (`readfile()`, `writefile()`) for custom serialization if needed.
- **[Home](Home)** - Return to the main wiki page