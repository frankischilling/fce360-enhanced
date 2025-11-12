# Input Functions

The Input Functions provide comprehensive controller input management capabilities. These functions allow you to read controller state, check button presses, override hardware input for TAS (Tool-Assisted Speedrun) scripts, and work with both NES button mappings and Xbox 360 controller buttons directly.

## Input Reading Functions

### `getjoypad`

**Signature:** `getjoypad(player)`
Gets the current controller state for a specified player as a raw button bitmask.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)

**Returns:**
- `integer` - Button state as a bitmask (0x00-0xFF)

**Button Bitmask:**
The returned value is a bitmask where each bit represents a button:
- Bit 0 (0x01): A
- Bit 1 (0x02): B
- Bit 2 (0x04): Select
- Bit 3 (0x08): Start
- Bit 4 (0x10): Up
- Bit 5 (0x20): Down
- Bit 6 (0x40): Left
- Bit 7 (0x80): Right

**Notes:**
- Returns the current button state at the time of the call
- Useful for reading controller input in `gui()` function
- Can be used for input logging, input-based cheats, or visual feedback
- To check if a button is pressed, use: `math.floor(buttons / mask) % 2 == 1` (Lua 5.1 doesn't have bitwise &)
- The bitmask format matches the `joypad()` callback parameter

**Example: Display Pressed Buttons:**
```lua
function gui()
    local buttons = getjoypad(0)  -- Get Player 1 state
    
    -- Check which buttons are pressed (Lua 5.1 bit checking)
    local buttonsPressed = {}
    if math.floor(buttons / 0x01) % 2 == 1 then table.insert(buttonsPressed, "A") end
    if math.floor(buttons / 0x02) % 2 == 1 then table.insert(buttonsPressed, "B") end
    if math.floor(buttons / 0x04) % 2 == 1 then table.insert(buttonsPressed, "Select") end
    if math.floor(buttons / 0x08) % 2 == 1 then table.insert(buttonsPressed, "Start") end
    if math.floor(buttons / 0x10) % 2 == 1 then table.insert(buttonsPressed, "Up") end
    if math.floor(buttons / 0x20) % 2 == 1 then table.insert(buttonsPressed, "Down") end
    if math.floor(buttons / 0x40) % 2 == 1 then table.insert(buttonsPressed, "Left") end
    if math.floor(buttons / 0x80) % 2 == 1 then table.insert(buttonsPressed, "Right") end
    
    -- Display button state
    local status = #buttonsPressed > 0 and table.concat(buttonsPressed, ", ") or "None"
    drawtext(4, 4, "P1 Buttons: " .. status, 0x20)
    drawtext(4, 12, string.format("Raw: 0x%02X", buttons), 0x2D)
end
```

**Example: Input-Based Cheat:**
```lua
local lastHealth = 0

function gui()
    -- Check if Start + Select are pressed (health restore cheat)
    local buttons = getjoypad(0)
    local startPressed = math.floor(buttons / 0x08) % 2 == 1
    local selectPressed = math.floor(buttons / 0x04) % 2 == 1
    
    if startPressed and selectPressed then
        -- Trigger health restore (implementation depends on game)
        drawtext(4, 4, "CHEAT ACTIVATED", 0x29)
    end
end
```

**Example: Input Logging:**
```lua
local inputLog = {}

function gui()
    local buttons = getjoypad(0)
    
    -- Log input changes
    if buttons ~= 0 then
        table.insert(inputLog, string.format("Frame %d: 0x%02X", getfps(), buttons))
    end
    
    -- Display last few inputs
    local y = 4
    for i = math.max(1, #inputLog - 5), #inputLog do
        drawtext(4, y, inputLog[i], 0x2D)
        y = y + 8
    end
end
```

### `isbuttonpressed`

**Signature:** `isbuttonpressed(player, button)`
Checks if a specific button is pressed for a specified player.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)
- `button` (string): Button name (case-insensitive)
  - Valid buttons: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`

**Returns:**
- `boolean` - `true` if the button is pressed, `false` otherwise

**Notes:**
- Button names are case-insensitive (e.g., `"A"`, `"a"`, `"Up"`, `"UP"` all work)
- Returns the current button state at the time of the call
- More convenient than manually checking bits from `getjoypad()`
- Useful for conditional logic based on button state

**Example: Simple Button Check:**
```lua
function gui()
    if isbuttonpressed(0, "A") then
        drawtext(4, 4, "A Button Pressed!", 0x29)
    end
    
    if isbuttonpressed(0, "B") then
        drawtext(4, 12, "B Button Pressed!", 0x29)
    end
end
```

**Example: Button Combination:**
```lua
function gui()
    -- Check for Start + Select combo
    if isbuttonpressed(0, "START") and isbuttonpressed(0, "SELECT") then
        drawtext(4, 4, "Start+Select Combo!", 0x37)
    end
end
```

**Example: Multiple Buttons:**
```lua
function gui()
    local buttons = {}
    if isbuttonpressed(0, "UP") then table.insert(buttons, "UP") end
    if isbuttonpressed(0, "DOWN") then table.insert(buttons, "DOWN") end
    if isbuttonpressed(0, "LEFT") then table.insert(buttons, "LEFT") end
    if isbuttonpressed(0, "RIGHT") then table.insert(buttons, "RIGHT") end
    
    if #buttons > 0 then
        drawtext(4, 4, "D-Pad: " .. table.concat(buttons, ", "), 0x20)
    end
end
```

### `isxboxbuttonpressed`

**Signature:** `isxboxbuttonpressed(player, button)`
Checks if a specific Xbox 360 controller button is currently pressed.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)
- `button` (string): Button name (case-insensitive)

**Returns:**
- `boolean` - `true` if the button is currently pressed, `false` otherwise

**Button Names:**
The following button names are supported (case-insensitive):
- `"A"` - A button
- `"B"` - B button
- `"X"` - X button
- `"Y"` - Y button
- `"START"` - Start button
- `"BACK"` - Back button
- `"LEFT_SHOULDER"` or `"LB"` - Left shoulder button
- `"RIGHT_SHOULDER"` or `"RB"` - Right shoulder button
- `"LEFT_THUMB"` or `"LS"` - Left thumbstick click
- `"RIGHT_THUMB"` or `"RS"` - Right thumbstick click
- `"DPAD_UP"` or `"UP"` - D-pad up
- `"DPAD_DOWN"` or `"DOWN"` - D-pad down
- `"DPAD_LEFT"` or `"LEFT"` - D-pad left
- `"DPAD_RIGHT"` or `"RIGHT"` - D-pad right

**Notes:**
- Returns the current button state at the time of the call
- Useful for reading Xbox 360 controller input directly (not mapped to NES buttons)
- Unlike `isbuttonpressed()`, this checks Xbox 360 controller buttons directly
- Button names are case-insensitive
- Returns `true` only while the button is held down
- Use edge detection (checking previous state) to detect button presses rather than holds
- This function reads hardware controller state, not NES button mappings

**Example: Check Xbox 360 Buttons:**
```lua
function gui()
    -- Check if Y button is pressed
    if isxboxbuttonpressed(0, "Y") then
        print("Y button is pressed")
    end
    
    -- Check if B button is pressed
    if isxboxbuttonpressed(0, "B") then
        print("B button is pressed")
    end
end
```

**Example: Edge Detection (Button Press):**
```lua
local lastYButton = false

function gui()
    local yPressed = isxboxbuttonpressed(0, "Y")
    
    -- Detect button press (not hold)
    if yPressed and not lastYButton then
        print("Y button was just pressed!")
        -- Do something once per press
    end
    
    lastYButton = yPressed
end
```

**Example: Multiple Buttons:**
```lua
function gui()
    local yPressed = isxboxbuttonpressed(0, "Y")
    local bPressed = isxboxbuttonpressed(0, "B")
    local xPressed = isxboxbuttonpressed(0, "X")
    local aPressed = isxboxbuttonpressed(0, "A")
    
    if yPressed then
        drawtext(10, 10, "Y pressed", 0x2E)
    end
    if bPressed then
        drawtext(10, 22, "B pressed", 0x2E)
    end
    if xPressed then
        drawtext(10, 34, "X pressed", 0x2E)
    end
    if aPressed then
        drawtext(10, 46, "A pressed", 0x2E)
    end
end
```

**Example: D-pad Navigation:**
```lua
local selectedSlot = 0
local lastDpadUp = false
local lastDpadDown = false

function gui()
    local dpadUp = isxboxbuttonpressed(0, "DPAD_UP")
    local dpadDown = isxboxbuttonpressed(0, "DPAD_DOWN")
    
    -- Navigate with D-pad
    if dpadUp and not lastDpadUp then
        selectedSlot = selectedSlot - 1
        if selectedSlot < 0 then selectedSlot = 9 end
    end
    if dpadDown and not lastDpadDown then
        selectedSlot = selectedSlot + 1
        if selectedSlot > 9 then selectedSlot = 0 end
    end
    
    lastDpadUp = dpadUp
    lastDpadDown = dpadDown
    
    drawtext(10, 10, string.format("Slot: %d", selectedSlot), 0x20)
end
```

**Example: Shoulder Buttons:**
```lua
function gui()
    local lbPressed = isxboxbuttonpressed(0, "LEFT_SHOULDER")
    local rbPressed = isxboxbuttonpressed(0, "RIGHT_SHOULDER")
    
    if lbPressed then
        drawtext(10, 10, "LB pressed", 0x2E)
    end
    if rbPressed then
        drawtext(10, 22, "RB pressed", 0x2E)
    end
end
```

**Example: Thumbstick Clicks:**
```lua
function gui()
    local lsPressed = isxboxbuttonpressed(0, "LEFT_THUMB")
    local rsPressed = isxboxbuttonpressed(0, "RIGHT_THUMB")
    
    if lsPressed then
        drawtext(10, 10, "Left stick clicked", 0x2E)
    end
    if rsPressed then
        drawtext(10, 22, "Right stick clicked", 0x2E)
    end
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with invalid button name
    local success, result = pcall(function()
        return isxboxbuttonpressed(0, "INVALID")
    end)
    
    if not success then
        print("Error: " .. tostring(result))
    end
    
    -- Test with invalid player
    local success, result = pcall(function()
        return isxboxbuttonpressed(10, "Y")  -- Invalid player
    end)
    
    if not success then
        print("Error: " .. tostring(result))
    end
end
```

## Button Conversion Functions

### `getbuttonname`

**Signature:** `getbuttonname(buttonMask)`
Converts a button bitmask to a comma-separated string of button names.

**Parameters:**
- `buttonMask` (integer): Button state bitmask (0x00-0xFF)

**Returns:**
- `string` - Comma-separated list of pressed button names, or empty string if no buttons are pressed

**Button Bitmask:**
The bitmask uses the same format as `getjoypad()`:
- Bit 0 (0x01): A
- Bit 1 (0x02): B
- Bit 2 (0x04): Select
- Bit 3 (0x08): Start
- Bit 4 (0x10): Up
- Bit 5 (0x20): Down
- Bit 6 (0x40): Left
- Bit 7 (0x80): Right

**Notes:**
- Returns button names in uppercase (e.g., "A, B", "START, SELECT")
- Button names are in the order: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT
- Returns empty string (`""`) if no buttons are pressed (bitmask = 0x00)
- Useful for displaying pressed buttons in a human-readable format
- Perfect for use with `getjoypad()` to convert raw bitmask to readable text

**Example: Display Button Names:**
```lua
function gui()
    local buttons = getjoypad(0)  -- Get Player 1 state
    local buttonNames = getbuttonname(buttons)  -- Convert to string
    
    if buttonNames == "" then
        drawtext(4, 4, "No buttons pressed", 0x2D)
    else
        drawtext(4, 4, "Buttons: " .. buttonNames, 0x29)
    end
end
```

**Example: Input Logging with Names:**
```lua
local inputLog = {}

function gui()
    local buttons = getjoypad(0)
    
    if buttons ~= 0 then
        local names = getbuttonname(buttons)
        table.insert(inputLog, string.format("0x%02X: %s", buttons, names))
    end
    
    -- Display last few inputs
    local y = 4
    for i = math.max(1, #inputLog - 5), #inputLog do
        drawtext(4, y, inputLog[i], 0x2D)
        y = y + 8
    end
end
```

**Example: Direct Bitmask Conversion:**
```lua
-- Convert specific bitmasks to names
print(getbuttonname(0x01))  -- "A"
print(getbuttonname(0x03))  -- "A, B"
print(getbuttonname(0x0C))  -- "SELECT, START"
print(getbuttonname(0xF0))  -- "UP, DOWN, LEFT, RIGHT"
print(getbuttonname(0x00))  -- "" (empty string)
```

### `getbuttonmask`

**Signature:** `getbuttonmask(buttonName)`
Converts a button name to its corresponding bitmask value.

**Parameters:**
- `buttonName` (string): Button name (case-insensitive)
  - Valid buttons: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`

**Returns:**
- `integer` - Button bitmask value (0x01-0x80)

**Button Bitmask Values:**
- `"A"` → 0x01
- `"B"` → 0x02
- `"SELECT"` → 0x04
- `"START"` → 0x08
- `"UP"` → 0x10
- `"DOWN"` → 0x20
- `"LEFT"` → 0x40
- `"RIGHT"` → 0x80

**Notes:**
- Button names are case-insensitive (e.g., `"A"`, `"a"`, `"Up"`, `"UP"` all work)
- This is the inverse function of `getbuttonname()` - converts names to bitmasks
- Useful for building bitmasks from button names or combining multiple buttons
- Can be used with `getjoypad()` to check button states manually

**Example: Get Button Bitmask:**
```lua
local aMask = getbuttonmask("A")      -- Returns 0x01
local bMask = getbuttonmask("B")      -- Returns 0x02
local startMask = getbuttonmask("START")  -- Returns 0x08
```

**Example: Combine Multiple Buttons:**
```lua
function gui()
    local buttons = getjoypad(0)
    
    -- Check if A and B are both pressed using bitmasks
    local aMask = getbuttonmask("A")
    local bMask = getbuttonmask("B")
    local bothPressed = (math.floor(buttons / aMask) % 2 == 1) and 
                        (math.floor(buttons / bMask) % 2 == 1)
    
    if bothPressed then
        drawtext(4, 4, "A and B pressed!", 0x29)
    end
end
```

**Example: Round-Trip Conversion:**
```lua
-- Convert name to mask and back to name
local buttonName = "A"
local mask = getbuttonmask(buttonName)      -- 0x01
local name = getbuttonname(mask)            -- "A"
print(string.format('"%s" -> 0x%02X -> "%s"', buttonName, mask, name))
```

**Example: Building Custom Bitmasks:**
```lua
-- Create a bitmask for Start + Select combo
local startMask = getbuttonmask("START")
local selectMask = getbuttonmask("SELECT")
local comboMask = startMask + selectMask  -- For Lua 5.1: use addition (bits don't overlap)

-- Check if combo is pressed
local buttons = getjoypad(0)
local startPressed = math.floor(buttons / startMask) % 2 == 1
local selectPressed = math.floor(buttons / selectMask) % 2 == 1
if startPressed and selectPressed then
    drawtext(4, 4, "Start+Select combo!", 0x37)
end
```

## Hardware Input Functions

### `gethardwarejoypad`

**Signature:** `gethardwarejoypad(player)`
Gets the raw hardware controller state for a specified player **before** any Lua override is applied. This is useful for detecting real controller input even when `setjoypad()` is active.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)

**Returns:**
- `integer` - Hardware button state as a bitmask (0x00-0xFF)

**Button Bitmask:**
The returned value uses the same bitmask format as `getjoypad()`:
- Bit 0 (0x01): A
- Bit 1 (0x02): B
- Bit 2 (0x04): Select
- Bit 3 (0x08): Start
- Bit 4 (0x10): Up
- Bit 5 (0x20): Down
- Bit 6 (0x40): Left
- Bit 7 (0x80): Right

**Notes:**
- Returns the **hardware** button state, not the emulated state
- Useful for detecting real controller input when `setjoypad()` is overriding input
- Perfect for toggle mechanisms in TAS scripts - you can detect when the user presses a button on the real controller even while the script is controlling Mario
- The hardware state is captured before `setjoypad()` overrides are applied
- Use `getjoypad()` to get the final emulated state (after Lua overrides)

**Example: TAS Toggle Detection:**
```lua
local autoMode = false
local lastSelectState = false

function beforeframe()
    -- Use gethardwarejoypad to detect real controller input
    -- This works even when setjoypad() is controlling Mario
    local hw = gethardwarejoypad(0)
    local selectMask = getbuttonmask("SELECT")
    local selectPressed = (math.floor(hw / selectMask) % 2 == 1)
    
    -- Edge detection: toggle only on rising edge
    if selectPressed and not lastSelectState then
        autoMode = not autoMode
    end
    lastSelectState = selectPressed
    
    -- If auto mode is on, control Mario with setjoypad()
    if autoMode then
        local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
        setjoypad(0, buttons)
    else
        clearjoypad(0)  -- Allow hardware input
    end
end
```

**Example: Compare Hardware vs Emulated:**
```lua
function gui()
    local hw = gethardwarejoypad(0)      -- Real controller
    local emu = getjoypad(0)              -- After Lua override
    
    drawtext(4, 4, string.format("Hardware: 0x%02X", hw), 0x20)
    drawtext(4, 12, string.format("Emulated: 0x%02X", emu), 0x29)
    
    if hw ~= emu then
        drawtext(4, 20, "Lua override active!", 0x37)
    end
end
```

## Input Override Functions

### `setjoypad`

**Signature:** `setjoypad(player, buttons)`
Sets the controller state for a specified player, overriding hardware input. This allows Lua scripts to control the game programmatically, enabling TAS (Tool-Assisted Speedrun) tools and automated input.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)
- `buttons` (integer): Button state bitmask (0x00-0xFF)
  - Use `getbuttonmask()` to build bitmasks from button names
  - Combine multiple buttons by adding their bitmasks: `RIGHT + B + A`

**Returns:**
- Nothing

**Button Bitmask:**
The bitmask uses the same format as `getjoypad()`:
- Bit 0 (0x01): A
- Bit 1 (0x02): B
- Bit 2 (0x04): Select
- Bit 3 (0x08): Start
- Bit 4 (0x10): Up
- Bit 5 (0x20): Down
- Bit 6 (0x40): Left
- Bit 7 (0x80): Right

**Notes:**
- **Override persists** until `clearjoypad()` is called or the script ends
- Must be called in `beforeframe()` callback for proper timing (before input polling)
- The override takes effect immediately and persists across frames
- Hardware input is completely overridden while the override is active
- Use `gethardwarejoypad()` to read real controller input even when override is active
- **Important:** Do not call `setjoypad()` in `gui()` - use `beforeframe()` instead for frame-accurate input

**Example: Simple Automated Input:**
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
```

**Example: TAS Script (SMB1):**
```lua
local frameCount = 0
local tasStarted = false

-- Jump windows: {startFrame, endFrame}
local JUMPS = {
    {30, 60},   -- First Goomba
    {100, 140}, -- First pipe
    {170, 210}, -- Second pipe
}

function beforeframe()
    -- Toggle with SELECT (use gethardwarejoypad to detect real input)
    local hw = gethardwarejoypad(0)
    local selectMask = getbuttonmask("SELECT")
    local selectPressed = (math.floor(hw / selectMask) % 2 == 1)
    -- ... toggle logic ...
    
    if tasStarted then
        frameCount = frameCount + 1
        
        -- Always run right at full speed
        local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
        
        -- Check if we're in a jump window
        for i = 1, #JUMPS do
            if frameCount >= JUMPS[i][1] and frameCount <= JUMPS[i][2] then
                buttons = buttons + getbuttonmask("A")
                break
            end
        end
        
        setjoypad(0, buttons)
    else
        clearjoypad(0)  -- Allow hardware input
    end
end
```

**Example: Combining Buttons:**
```lua
function beforeframe()
    -- Run right + hold B (run fast) + press A (jump)
    local right = getbuttonmask("RIGHT")
    local b = getbuttonmask("B")
    local a = getbuttonmask("A")
    
    local buttons = right + b + a  -- All three buttons pressed
    setjoypad(0, buttons)
end
```

### `clearjoypad`

**Signature:** `clearjoypad(player)`
Clears the Lua override for a specified player, allowing hardware input to take control again.

**Parameters:**
- `player` (integer): Player number (-1, 0-3)
  - `-1` = Clear override for all players (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)

**Returns:**
- Nothing

**Notes:**
- Clears the override set by `setjoypad()` for the specified player(s)
- After calling `clearjoypad()`, hardware input will control the player again
- Use `-1` to clear overrides for all players at once
- Should be called in `beforeframe()` callback for proper timing
- If `setjoypad()` is not active, this function has no effect (safe to call)

**Example: Toggle Auto Mode:**
```lua
local autoMode = false

function beforeframe()
    -- Toggle with SELECT (detect real hardware input)
    local hw = gethardwarejoypad(0)
    local selectPressed = (math.floor(hw / getbuttonmask("SELECT")) % 2 == 1)
    
    if selectPressed and not lastSelectState then
        autoMode = not autoMode
    end
    lastSelectState = selectPressed
    
    if autoMode then
        -- Control Mario with script
        local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
        setjoypad(0, buttons)
    else
        -- Allow hardware input
        clearjoypad(0)
    end
end
```

**Example: Clear All Players:**
```lua
function beforeframe()
    -- Stop all automated input
    clearjoypad(-1)  -- Clear all players
end
```

**Example: Conditional Override:**
```lua
function beforeframe()
    local hw = gethardwarejoypad(0)
    
    -- Only override if user is not pressing anything
    if hw == 0 then
        -- User not pressing anything, use automated input
        setjoypad(0, getbuttonmask("RIGHT"))
    else
        -- User is pressing buttons, let hardware control
        clearjoypad(0)
    end
end
```

### `pressbutton`

**Signature:** `pressbutton(player, button)`
Simulates pressing a button for **one frame only**. The button is automatically released on the next frame. This is useful for single-frame inputs like menu navigation or precise timing requirements.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)
- `button` (string): Button name (case-insensitive)
  - Valid buttons: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`

**Returns:**
- Nothing

**Notes:**
- The button is pressed for **one frame only** and automatically released on the next frame
- Multiple calls to `pressbutton()` in the same frame will combine (OR'd together)
- Works alongside `setjoypad()` - one-frame presses are applied on top of any persistent override
- Must be called in `beforeframe()` callback for proper timing (before input polling)
- Unlike `setjoypad()`, you don't need to call `clearjoypad()` - the button is automatically released
- Perfect for menu navigation, single-frame actions, or precise timing requirements

**Example: Single-Frame Menu Navigation:**
```lua
local frameCount = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Press A button every 60 frames (one-frame press)
    if frameCount % 60 == 0 then
        pressbutton(0, "A")
    end
    
    -- Press RIGHT button every 30 frames (one-frame press)
    if frameCount % 30 == 0 then
        pressbutton(0, "RIGHT")
    end
end

function gui()
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    drawtext(4, 4, "Buttons: " .. (buttonNames == "" and "(none)" or buttonNames), 0x29)
end
```

**Example: Multiple Buttons in One Frame:**
```lua
function beforeframe()
    -- Press multiple buttons for one frame
    pressbutton(0, "A")
    pressbutton(0, "B")
    pressbutton(0, "RIGHT")
    -- All three buttons will be pressed for one frame, then released
end
```

**Example: Comparison with setjoypad():**
```lua
local frameCount = 0
local usePressButton = true

function beforeframe()
    frameCount = frameCount + 1
    
    if usePressButton then
        -- pressbutton(): Press A every 60 frames (one frame only)
        if frameCount % 60 == 0 then
            pressbutton(0, "A")
        end
    else
        -- setjoypad(): Hold A continuously (until cleared)
        setjoypad(0, getbuttonmask("A"))
    end
end

function gui()
    local mode = usePressButton and "pressbutton()" or "setjoypad()"
    drawtext(4, 4, "Mode: " .. mode, 0x20)
    
    local buttons = getjoypad(0)
    if buttons ~= 0 then
        local names = getbuttonname(buttons)
        drawtext(4, 12, "Buttons: " .. names, 0x29)
    end
end
```

**Example: Precise Timing (Menu Selection):**
```lua
local frameCount = 0
local menuIndex = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Navigate menu: press UP/DOWN every 30 frames (one-frame press)
    if frameCount % 30 == 0 then
        if menuIndex < 3 then
            pressbutton(0, "DOWN")  -- Move down in menu
            menuIndex = menuIndex + 1
        else
            pressbutton(0, "A")  -- Select item
        end
    end
end
```

### `releasebutton`

**Signature:** `releasebutton(player, button)`
Simulates releasing a button for **one frame only**. The button is automatically pressed again on the next frame (if it was being held). This is useful for creating brief button releases while maintaining a held state, or for precise timing requirements.

**Parameters:**
- `player` (integer): Player number (0-3)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller)
  - `2` = Player 3 (third controller, if connected)
  - `3` = Player 4 (fourth controller, if connected)
- `button` (string): Button name (case-insensitive)
  - Valid buttons: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`

**Returns:**
- Nothing

**Notes:**
- The button is released for **one frame only** and automatically returns to its previous state on the next frame
- If the button was being held (via `setjoypad()` or hardware input), it will be held again after the one-frame release
- Multiple calls to `releasebutton()` in the same frame will combine (OR'd together)
- Works alongside `setjoypad()` and `pressbutton()` - one-frame releases are applied after presses
- Must be called in `beforeframe()` callback for proper timing (before input polling)
- Unlike `clearjoypad()`, this only releases the button for one frame, not permanently
- Perfect for creating brief button releases while maintaining a held state, or for precise timing requirements

**Example: Brief Release While Holding:**
```lua
local frameCount = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Hold A and RIGHT continuously
    local buttons = getbuttonmask("A") + getbuttonmask("RIGHT")
    setjoypad(0, buttons)
    
    -- Release A button every 60 frames (one-frame release)
    if frameCount % 60 == 0 then
        releasebutton(0, "A")
    end
    
    -- Release RIGHT button every 30 frames (one-frame release)
    if frameCount % 30 == 0 then
        releasebutton(0, "RIGHT")
    end
end

function gui()
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    drawtext(4, 4, "Buttons: " .. (buttonNames == "" and "(none)" or buttonNames), 0x29)
end
```

**Example: Multiple Buttons Released in One Frame:**
```lua
function beforeframe()
    -- Hold A, B, and RIGHT continuously
    local buttons = getbuttonmask("A") + getbuttonmask("B") + getbuttonmask("RIGHT")
    setjoypad(0, buttons)
    
    -- Release multiple buttons for one frame
    releasebutton(0, "A")
    releasebutton(0, "B")
    -- Both A and B will be released for one frame, then held again
end
```

**Example: Comparison with pressbutton():**
```lua
local frameCount = 0
local useReleaseButton = true

function beforeframe()
    frameCount = frameCount + 1
    
    if useReleaseButton then
        -- releasebutton(): Hold A continuously, release every 60 frames (one frame only)
        setjoypad(0, getbuttonmask("A"))
        if frameCount % 60 == 0 then
            releasebutton(0, "A")
        end
    else
        -- pressbutton(): Press A every 60 frames (one frame only)
        if frameCount % 60 == 0 then
            pressbutton(0, "A")
        end
    end
end

function gui()
    local mode = useReleaseButton and "releasebutton()" or "pressbutton()"
    drawtext(4, 4, "Mode: " .. mode, 0x20)
    
    local buttons = getjoypad(0)
    if buttons ~= 0 then
        local names = getbuttonname(buttons)
        drawtext(4, 12, "Buttons: " .. names, 0x29)
    end
end
```

**Example: Precise Timing (Menu Navigation):**
```lua
local frameCount = 0
local holdingRight = true

function beforeframe()
    frameCount = frameCount + 1
    
    if holdingRight then
        -- Hold RIGHT continuously
        setjoypad(0, getbuttonmask("RIGHT"))
        
        -- Release RIGHT every 30 frames to prevent menu scrolling too fast
        if frameCount % 30 == 0 then
            releasebutton(0, "RIGHT")
        end
    end
end
```

## Input Recording Helpers

The actual capture/playback API is documented separately for clarity.

- **[Input Recording Functions](Input-Recording-Functions)** — `startinputrecording()`, `stopinputrecording()`, and `playinputrecording()` let you capture controller activity frame-by-frame and replay it later. Combine them with the helpers on this page (e.g., `gethardwarejoypad()` or `setjoypad()`) to build TAS tooling or reproducible test scenarios.

## See Also

- [Callbacks](Callbacks) - The `joypad()` callback function for input handling
- [State Management Functions](State-Management-Functions) - Functions for save/load states (useful for TAS)
- [Monitoring Functions](Monitoring-Functions) - Functions for frame counting and timing
- [Home](Home) - Return to the main wiki page