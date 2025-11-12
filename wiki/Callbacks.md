# Callbacks

Callbacks are functions that your script defines, which the emulator will call automatically at specific times.

## Required Callback

### `script`

**Signature:** `script()`

**Required callback** - Called every ~33ms (~30Hz) to execute your script logic. This is your main function and must be defined in your script.

**Parameters:** None

**Returns:** Nothing (return values are ignored)

**When Called:**
- Automatically called by the emulator during each frame rendering cycle
- Runs at approximately 30Hz (every 33 milliseconds) for performance
- The overlay is double-buffered and composited at 60Hz to prevent flicker
- Called even when emulation is paused (if a game is loaded)

**Important Notes:**
- Your script **must** define this function, or nothing will run
- Keep this function lightweight - heavy computations can impact emulation performance
- All drawing functions (`drawtext`, etc.) must be called from within `script()` to appear on screen
- The function can be empty if you only use `joypad()` for input modification
- **Backward Compatibility:** The legacy `gui()` function name is still supported for existing scripts

**Basic Example:**
```lua
function script()
    -- Draw FPS counter
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    
    -- Draw a status message
    drawtext(4, 12, "Lua Active", 0x20)
end
```

**Advanced Example with State:**
```lua
local startTime = os.clock()
local frameCounter = 0

function script()
    frameCounter = frameCounter + 1
    
    -- FPS display
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    
    -- Frame counter
    drawtext(4, 12, string.format("Frames: %d", frameCounter), 0x30)
    
    -- Elapsed time (approximate)
    local elapsed = os.clock() - startTime
    local minutes = math.floor(elapsed / 60)
    local seconds = math.floor(elapsed % 60)
    drawtext(4, 20, string.format("Time: %02d:%02d", minutes, seconds), 0x20)
end
```

## Optional Callbacks

### `joypad`

**Signature:** `joypad(player, buttons)` *(Optional)*

Optional callback - Called when the emulator reads controller input. Allows you to intercept and modify button presses before they reach the game.

**Parameters:**
- `player` (integer): Player number (0-based)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller, if connected)
- `buttons` (integer): Current raw button state as a bitmask

**Returns:** 
- `integer` - Modified button state that will be used by the game

**Button Bitmask Reference:**
The `buttons` parameter is a bitmask where each bit represents a button:
- Bit 0 (0x01): A
- Bit 1 (0x02): B
- Bit 2 (0x04): Select
- Bit 3 (0x08): Start
- Bit 4 (0x10): Up
- Bit 5 (0x20): Down
- Bit 6 (0x40): Left
- Bit 7 (0x80): Right

**Notes:**
- This function is called every frame when reading controller input
- If you don't define this function, input passes through unchanged
- You can use bitwise operations to modify buttons:
  - `buttons | mask` - Press a button (set bit)
  - `buttons & ~mask` - Release a button (clear bit)
  - `buttons ^ mask` - Toggle a button (flip bit)
- Modifications apply immediately to the game
- Can be used for auto-fire, button combinations, macros, etc.

**Example: Auto-Fire A Button:**
```lua
local autoFireFrame = 0

function joypad(player, buttons)
    -- Auto-fire for player 1 only
    if player == 0 then
        autoFireFrame = autoFireFrame + 1
        -- Press A button every other frame (30 Hz auto-fire)
        if (autoFireFrame % 2) == 0 then
            return buttons | 0x01  -- Set A button (bit 0)
        end
    end
    return buttons  -- Pass through unmodified
end
```

**Example: Turbo Mode (All Buttons Auto-Fire):**
```lua
local turboFrame = 0

function joypad(player, buttons)
    turboFrame = turboFrame + 1
    if player == 0 then
        -- Fast turbo (every 2 frames)
        if (turboFrame % 2) == 0 then
            -- Toggle A and B buttons for turbo effect
            return buttons ^ (0x01 | 0x02)
        end
    end
    return buttons
end
```

### `onaudiochannelchange`

**Signature:** `onaudiochannelchange(channel, enabled)` *(Optional)*

Optional callback - Called automatically when an APU channel is enabled or disabled. This callback is triggered in real-time when the audio channel state changes, allowing scripts to react to audio events such as sound effects starting/stopping, music changes, or channel muting.

**Parameters:**
- `channel` (integer): Channel number (0-4)
  - `0` = Pulse 1 (Square 1) - Used for melodies, sound effects, bass lines
  - `1` = Pulse 2 (Square 2) - Used for harmonies, sound effects, additional melodies
  - `2` = Triangle - Used for bass lines, melodies, and smooth tones
  - `3` = Noise - Used for percussion, sound effects, white noise
  - `4` = DMC (Delta Modulation Channel) - Used for sample playback, drums, speech
- `enabled` (boolean): Whether the channel is now enabled
  - `true` = Channel was enabled (turned on) - Sound is now playing
  - `false` = Channel was disabled (turned off) - Sound has stopped

**Returns:** Nothing (return values are ignored)

**When Called:**
- Automatically called by the emulator when a channel's enabled state changes
- Called from `FCEU_LuaFrameBoundary()` which runs every frame
- Triggered by comparing the current `EnabledChannels` register value with the previous frame's value
- Only fires when the state actually changes (enabled → disabled or disabled → enabled)
- Called even when audio is disabled (though channels will typically be disabled in that case)
- Runs synchronously during the frame boundary, before `script()` / `gui()` callbacks

**Important Notes:**
- **No registration required** - Simply define the function in your script to receive events
- **Automatic detection** - The emulator automatically detects channel state changes every frame
- **State change only** - Only triggers when the channel state actually changes, not on every frame
- **Frame-accurate** - Events are detected with frame-level precision
- **Error handling** - Errors in the callback are logged to console but don't crash the emulator
- **Performance** - The callback check is lightweight and runs every frame with minimal overhead

**Example: Basic Channel Change Detection:**
```lua
function onaudiochannelchange(channel, enabled)
    local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}
    local name = channelNames[channel + 1] or "Unknown"
    print(string.format("Channel %d (%s) changed: %s", 
        channel, name, enabled and "ENABLED" or "DISABLED"))
end
```

### `beforeframe`

**Signature:** `beforeframe()` *(Optional)*

Optional callback - Called **before** input polling each frame. This is the ideal place to call `setjoypad()` and `clearjoypad()` for frame-accurate input control in TAS scripts.

**Parameters:** None

**Returns:** Nothing (return values are ignored)

**When Called:**
- Automatically called by the emulator **before** `FCEU_UpdateInput()` is called
- Runs once per frame, before the game polls controller input
- Called before `script()` / `gui()` callback
- The overlay is not ready for drawing in this callback - use `script()` / `gui()` for drawing

**Important Notes:**
- **Must use `beforeframe()` for `setjoypad()` and `clearjoypad()`** - calling these in `script()` / `gui()` will cause input to be applied one frame late
- Perfect for TAS (Tool-Assisted Speedrun) scripts that need frame-accurate input control
- Use `gethardwarejoypad()` to detect real controller input even when `setjoypad()` is active
- Do **not** call drawing functions (`drawtext`, etc.) in `beforeframe()` - the overlay is not ready
- Keep this function lightweight for best performance

**Example: Simple TAS Script:**
```lua
local frameCount = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Run right and jump every 60 frames
    local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
    if frameCount % 60 == 0 then
        buttons = buttons + getbuttonmask("A")
    end
    
    setjoypad(0, buttons)
end

function gui()
    -- Draw status (drawing must be in gui(), not beforeframe())
    drawtext(4, 4, string.format("Frame: %d", frameCount), 0x20)
end
```

**Example: TAS with Toggle:**
```lua
local frameCount = 0
local autoMode = false
local lastSelectState = false

function beforeframe()
    -- Detect real hardware input for toggle (even when setjoypad is active)
    local hw = gethardwarejoypad(0)
    local selectMask = getbuttonmask("SELECT")
    local selectPressed = (math.floor(hw / selectMask) % 2 == 1)
    
    -- Edge detection: toggle only on rising edge
    if selectPressed and not lastSelectState then
        autoMode = not autoMode
    end
    lastSelectState = selectPressed
    
    if autoMode then
        frameCount = frameCount + 1
        local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
        setjoypad(0, buttons)
    else
        clearjoypad(0)  -- Allow hardware input
    end
end

function gui()
    drawtext(4, 4, autoMode and "AUTO MODE: ON" or "AUTO MODE: OFF", 0x29)
    if autoMode then
        drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    end
end
```

## See Also

- [Home](Home) - Overview of all API functions
- [Input Functions](Input-Functions) - Input manipulation functions
- [Examples](Examples) - Complete working examples