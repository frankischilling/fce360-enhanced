# Input Recording Functions

The input recording helpers let Lua scripts capture controller activity frame-by-frame, store it, and play it back later. Recordings always include every player (0â€“3) and honour the final button state for each frame after any Lua overrides.

## Workflow Overview
1. Call `startinputrecording()` to begin capturing input.
2. When you are done, call `stopinputrecording()` to retrieve the recording as a Lua table.
3. Use `saveinputrecording()` to save the recording to disk, or pass that table to `playinputrecording()` to replay the captured input exactly as it was recorded.
4. Use `loadinputrecording()` to load a previously saved recording from disk and automatically start playback.

---

## `startinputrecording()`
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

## `stopinputrecording()`
Stop capturing and return the recorded input.

- **Parameters:** none
- **Returns:** table containing four keys (`player0` â€¦ `player3`). Each value is a 1-indexed array of integer button masks (0x00â€“0xFF) representing each frame.
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

## `playinputrecording(data)`
Replay a recording created by `stopinputrecording()`.

- **Parameters:**
  - `data` â€” table with keys `player0` â€¦ `player3`; each value is an array of button masks.
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
            print("Recording nowâ€¦")
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

## `saveinputrecording(path)`
Saves the current input recording to a file on disk.

- **Parameters:**
  - `path` â€" file path string (relative or absolute)
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

## `loadinputrecording(path)`
Loads an input recording from a file and automatically starts playback.

- **Parameters:**
  - `path` â€" file path string (relative or absolute)
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

## See Also
- **[Input Functions](Input-Functions)** â€" Inspect or override controller states (`getjoypad()`, `setjoypad()`, `pressbutton()`, etc.).
- **[State Management Functions](State-Management-Functions)** â€" Save states allow you to combine recordings with quick resets for testing.
- **[File I/O Functions](File-IO-Functions)** â€" Additional file operations (`readfile()`, `writefile()`) for custom serialization if needed.
