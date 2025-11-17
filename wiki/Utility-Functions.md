# Utility Functions

Utility functions for debugging, logging, and console output.

---

### `print(...)`

**Signature:** `print(...)`

Outputs text to the Lua console.

- **Parameters:** Variable number of arguments (strings, numbers, etc.)
- **Returns:** nothing
- **Behaviour & Notes:**
  - Outputs all arguments to the Lua console, separated by tabs
  - Multiple arguments are automatically concatenated with tab characters
  - Empty calls (`print()`) output a blank line
  - Only string arguments are processed (non-string arguments are skipped)
  - Output appears in the Lua console (press the console toggle key to view)
  - Useful for debugging scripts, logging events, and displaying information

**Example: Basic Usage**
```lua
print("Hello, world!")
print("Frame:", getframecount())
print("FPS:", getfps())
```

**Example: Multiple Arguments**
```lua
local x = 10
local y = 20
print("Position:", x, y)  -- Outputs: "Position:	10	20"
```

**Example: Debugging**
```lua
function beforeframe()
    local buttons = getjoypad(0)
    if buttons ~= 0 then
        print("Buttons pressed:", getbuttonname(buttons))
    end
end
```

**Example: Status Logging**
```lua
function script()
    local frame = getframecount()
    if frame % 60 == 0 then  -- Log every second
        print(string.format("Frame %d: FPS=%.1f", frame, getfps()))
    end
end
```

---

### `log(...)`

**Signature:** `log(...)`

Alias for `print()`. Outputs text to the Lua console.

- **Parameters:** Variable number of arguments (strings, numbers, etc.)
- **Returns:** nothing
- **Behaviour & Notes:**
  - Identical to `print()` - both functions do the same thing
  - Provided for convenience and code clarity
  - Use `log()` when you want to emphasize logging/debugging output
  - Use `print()` for general output

**Example: Using log()**
```lua
log("Script initialized")
log("Current frame:", getframecount())
log("Game loaded:", getromname())
```

**Example: Conditional Logging**
```lua
local DEBUG = true

function debugLog(...)
    if DEBUG then
        log(...)
    end
end

-- Usage
debugLog("This only prints if DEBUG is true")
```

---

### `setconsolespacing(pixels)`

**Signature:** `setconsolespacing(pixels)`

Sets the line spacing (gap) between lines in the Lua console.

- **Parameters:**
  - `pixels` (integer) - Line spacing in pixels (0-8)
- **Returns:** nothing
- **Behaviour & Notes:**
  - Adjusts the vertical spacing between console lines
  - Default spacing is 2 pixels
  - Value is clamped to 0-8 pixels
  - Affects all console output (from `print()` and `log()`)
  - Useful for adjusting console readability or fitting more/less text on screen

**Example: Adjust console spacing**
```lua
-- Set tight spacing (0 pixels)
setconsolespacing(0)

-- Set default spacing (2 pixels)
setconsolespacing(2)

-- Set loose spacing (8 pixels)
setconsolespacing(8)
```

**Example: Dynamic spacing based on content**
```lua
function script()
    -- Use tighter spacing when showing lots of debug info
    if DEBUG_MODE then
        setconsolespacing(1)
    else
        setconsolespacing(2)
    end
end
```

---

## Console Access

The Lua console can be toggled on/off using the console toggle key (check your emulator settings). The console displays:
- All output from `print()` and `log()` calls
- Script error messages
- Other Lua-related information

The console supports scrolling to view older messages.
- Line spacing can be adjusted with `setconsolespacing()`

---

## See Also

- **[Technical Details](Technical-Details)** - Script timing, console behavior, and implementation details
- **[Troubleshooting](Troubleshooting)** - Debugging tips and common issues
- **[Monitoring Functions](Monitoring-Functions)** - Functions to get frame count, FPS, and timing information
- **[Home](Home)** - Return to the main wiki page