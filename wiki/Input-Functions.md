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

**Button Bitmask:**onbuttonpress
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

### `getbuttonheldms`

**Signature:** `getbuttonheldms(btn)`
Returns how long a specified Xbox 360 button, trigger, stick direction, or NES button has been held down (in milliseconds).

**Parameters:**
- `btn` (string): Button name. Supports the same names and aliases as `isxboxbuttonpressed()` plus analog directions:
  - Triggers: `"LT"`, `"LEFT_TRIGGER"`, `"RT"`, `"RIGHT_TRIGGER"`
  - Stick directions: `"LS_UP"`, `"LS_DOWN"`, `"LS_LEFT"`, `"LS_RIGHT"`, `"RS_UP"`, `"RS_DOWN"`, `"RS_LEFT"`, `"RS_RIGHT"`
  - NES buttons (hardware D-pad mapped to Virtual Console): `"NES_A"`, `"NES_B"`, `"NES_SELECT"`, `"NES_START"`, `"NES_UP"`, `"NES_DOWN"`, `"NES_LEFT"`, `"NES_RIGHT"`

**Returns:**
- `number` - Hold duration in milliseconds (longest active hold across connected controllers). Returns `0` if not currently held.

**Notes:**
- Duration resets to `0` as soon as the button/stick returns to neutral.
- Uses hardware state before Lua overrides, so it reflects real controller input.
- `LT` (Left Trigger) doubles as the rewind button in this build. If you need uninterrupted LT timing, you’ll have to modify the source to change the rewind mapping (or open a pull request if it’s a broader issue).
- Works even when Lua is injecting input via `setjoypad()` or `pressbutton()`—only actual hardware holds are reported.
- **NES buttons** (`NES_A`, `NES_B`, `NES_SELECT`, `NES_START`, `NES_UP`, `NES_DOWN`, `NES_LEFT`, `NES_RIGHT`) track the actual NES controller state, not the Xbox button mappings. This allows you to measure hold times for the NES buttons specifically, independent of which Xbox buttons are mapped to them.

**Example: Quick Hold Display**
```lua
function gui()
    drawtext(4, 4, string.format("A held: %d ms", getbuttonheldms("A")), 0x20)
    drawtext(4, 14, string.format("Left stick up: %d ms", getbuttonheldms("LS_UP")), 0x20)
    drawtext(4, 24, string.format("Left trigger: %d ms", getbuttonheldms("LT")), 0x20)
end
```

**Example: NES Button Hold Detection**
```lua
function gui()
    -- Display hold times for all NES buttons
    local y = 4
    local nesButtons = {"NES_A", "NES_B", "NES_SELECT", "NES_START", 
                        "NES_UP", "NES_DOWN", "NES_LEFT", "NES_RIGHT"}
    
    for i, btn in ipairs(nesButtons) do
        local ms = getbuttonheldms(btn)
        if ms > 0 then
            drawtext(4, y, string.format("%s: %d ms", btn, ms), 0x20)
            y = y + 10
        end
    end
    
    -- Example: Long-press detection for NES_A
    if getbuttonheldms("NES_A") > 1000 then
        drawtext(4, 200, "NES_A long-pressed!", 0x28)
    end
end
```

### `onbuttonpress`

**Signature:** `onbuttonpress(btn, cb)`
Registers or removes a callback that fires when a specific Xbox 360 controller button is pressed (transition from released to pressed).

**Parameters:**
- `btn` (string): Button name (case-insensitive). Supports the same names and aliases as `isxboxbuttonpressed()`:
  - `"A"`, `"B"`, `"X"`, `"Y"`, `"START"`, `"BACK"`
  - `"LEFT_SHOULDER"` / `"LB"`, `"RIGHT_SHOULDER"` / `"RB"`
  - `"LEFT_THUMB"` / `"LS"`, `"RIGHT_THUMB"` / `"RS"`
  - `"DPAD_UP"` / `"UP"`, `"DPAD_DOWN"` / `"DOWN"`, `"DPAD_LEFT"` / `"LEFT"`, `"DPAD_RIGHT"` / `"RIGHT"`
- `cb` (function or `nil`): Callback to run when the button is pressed. Pass `nil` to unregister the existing callback.

**Returns:**
- Nothing

**Callback Signature:**
The callback receives two arguments:
1. `player` (integer): Player index (0-3). `0` = Player 1, `1` = Player 2, etc.
2. `button` (string): Canonical button name (e.g., `"A"`, `"DPAD_UP"`)

**Notes:**
- Fires once per press (rising edge). Holding the button does not retrigger until released.
- The callback runs for whichever player pressed the button.
- Registering a new callback for the same button replaces the previous one.
- Pass `nil` as the callback to remove the handler.
- Uses hardware Xbox button state (not NES-mapped buttons).
- Callbacks run during input processing, so keep work inside them lightweight.

**Example: Simple Button Trigger:**
```lua
local function onAPressed(player, button)
    print(string.format("Player %d pressed %s", player + 1, button))
end

function beforeframe()
    -- Register once
    onbuttonpress("A", onAPressed)
end
```

**Example: Toggle UI with D-pad:**
```lua
local menuVisible = false

local function toggleMenu(player, button)
    menuVisible = not menuVisible
    print(string.format("Player %d toggled menu (%s). Visible=%s",
        player + 1, button, tostring(menuVisible)))
end

onbuttonpress("DPAD_UP", toggleMenu)

function gui()
    if menuVisible then
        drawtext(10, 10, "Menu is OPEN", 0x39)
    else
        drawtext(10, 10, "Menu is CLOSED", 0x39)
    end
end

-- To unregister:
-- onbuttonpress("DPAD_UP", nil)
```

**Example: Per-Player Callbacks:**
```lua
local function handleStart(player, button)
    print(string.format("Player %d paused the game!", player + 1))
end

local function handleBack(player, button)
    print(string.format("Player %d opened the menu!", player + 1))
end

onbuttonpress("START", handleStart)
onbuttonpress("BACK", handleBack)
```

### `onbuttonrelease`

**Signature:** `onbuttonrelease(btn, cb)`
Registers or removes a callback that fires when a specific Xbox 360 controller button is released (transition from pressed to released).

**Parameters:**
- `btn` (string): Button name (case-insensitive). Supports the same names and aliases as `onbuttonpress()` / `isxboxbuttonpressed()`.
- `cb` (function or `nil`): Callback to run when the button is released. Pass `nil` to unregister the existing callback.

**Returns:**
- Nothing

**Callback Signature:**
The callback receives two arguments:
1. `player` (integer): Player index (0-3). `0` = Player 1, etc.
2. `button` (string): Canonical button name (e.g., `"A"`, `"DPAD_DOWN"`)

**Notes:**
- Fires once per release (falling edge). Holding the button down does not trigger the callback.
- Works per-player, even when multiple controllers are connected.
- Registering another callback for the same button replaces the previous one.
- Pass `nil` to remove the callback.
- Runs using the raw Xbox controller state; independent of NES button mapping.
- Ideal for handling “button-up” events (e.g., confirming actions when the button is released).

**Example: Confirm Action on Release:**
```lua
local function confirmAction(player, button)
    print(string.format("Player %d confirmed action on %s release", player + 1, button))
end

onbuttonrelease("A", confirmAction)
```

**Example: Press-to-Hold, Release-to-Commit:**
```lua
local charging = {}

local function startCharge(player, button)
    charging[player] = getframecount()
    print(string.format("Player %d started charging...", player + 1))
end

local function releaseCharge(player, button)
    local startFrame = charging[player]
    charging[player] = nil
    if startFrame then
        local chargeFrames = getframecount() - startFrame
        print(string.format("Player %d released after %d frames", player + 1, chargeFrames))
    end
end

onbuttonpress("X", startCharge)
onbuttonrelease("X", releaseCharge)
```

**Example: Toggle State on Release Only:**
```lua
local menuVisible = false

local function toggleMenuOnRelease(player, button)
    menuVisible = not menuVisible
    print(string.format("Menu visibility for player %d: %s", player + 1, tostring(menuVisible)))
end

onbuttonrelease("BACK", toggleMenuOnRelease)
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

### `setrumble`

**Signature:** `setrumble(ms, intensity)`
Sets controller haptic feedback (rumble/vibration) for player 0 (first controller). The rumble will automatically stop after the specified duration.

**Parameters:**
- `ms` (integer): Duration in milliseconds (must be >= 0)
- `intensity` (number): Rumble intensity from 0.0 to 1.0
  - `0.0` = No rumble (off)
  - `0.5` = Medium rumble
  - `1.0` = Maximum rumble

**Returns:**
- Nothing

**Notes:**
- Rumble is applied to player 0 (first controller) by default
- Intensity values outside the 0.0-1.0 range are automatically clamped
- The rumble automatically stops after the specified duration
- If the controller is not connected, the function will silently fail (no error)
- Multiple calls to `setrumble()` will override the previous rumble state
- Useful for providing haptic feedback in response to game events

**Example: Basic Rumble on Button Press:**
```lua
local lastA = false

function gui()
    local buttons = getjoypad(0)
    local aPressed = (math.floor(buttons / 0x01) % 2 == 1)
    
    -- Trigger rumble when A button is pressed
    if aPressed and not lastA then
        setrumble(200, 0.5)  -- 200ms rumble at 50% intensity
    end
    
    lastA = aPressed
end
```

**Example: Different Rumble Intensities:**
```lua
local lastB = false
local lastStart = false

function gui()
    local buttons = getjoypad(0)
    local bPressed = (math.floor(buttons / 0x02) % 2 == 1)
    local startPressed = (math.floor(buttons / 0x08) % 2 == 1)
    
    -- Light rumble for B button
    if bPressed and not lastB then
        setrumble(100, 0.2)  -- Short, light rumble
    end
    
    -- Strong rumble for START button
    if startPressed and not lastStart then
        setrumble(500, 1.0)  -- Longer, maximum intensity
    end
    
    lastB = bPressed
    lastStart = startPressed
end
```

**Example: Game Event Feedback:**
```lua
local lastHealth = 0

function gui()
    -- Read health from memory (example address)
    local health = readbyte(0x0756)  -- Example: Super Mario Bros health address
    
    -- Rumble when health decreases (damage taken)
    if health < lastHealth then
        local damage = lastHealth - health
        if damage > 0 then
            -- Stronger rumble for more damage
            local intensity = math.min(1.0, damage * 0.3)
            setrumble(300, intensity)
        end
    end
    
    lastHealth = health
    
    -- Display health
    drawtext(4, 4, string.format("Health: %d", health), 0x20)
end
```

**Example: Rumble Patterns:**
```lua
local frameCount = 0
local rumblePattern = {
    {ms = 100, intensity = 0.5},
    {ms = 50, intensity = 0.0},  -- Pause
    {ms = 100, intensity = 0.5},
    {ms = 50, intensity = 0.0},  -- Pause
    {ms = 200, intensity = 1.0}  -- Strong finish
}
local patternIndex = 0
local patternStartTime = 0

function gui()
    frameCount = frameCount + 1
    local currentTime = gettime()
    
    -- Trigger pattern on A button press
    local buttons = getjoypad(0)
    local aPressed = (math.floor(buttons / 0x01) % 2 == 1)
    
    if aPressed and patternIndex == 0 then
        patternIndex = 1
        patternStartTime = currentTime
        setrumble(rumblePattern[1].ms, rumblePattern[1].intensity)
    end
    
    -- Continue pattern
    if patternIndex > 0 then
        local elapsed = 0
        for i = 1, patternIndex do
            elapsed = elapsed + rumblePattern[i].ms
        end
        
        if currentTime - patternStartTime >= elapsed then
            patternIndex = patternIndex + 1
            if patternIndex <= #rumblePattern then
                setrumble(rumblePattern[patternIndex].ms, rumblePattern[patternIndex].intensity)
            else
                patternIndex = 0  -- Pattern complete
            end
        end
    end
end
```

### `mapinput`

**Signature:** `mapinput(virtualBtn, physicalSpec)`
Per-script input remapping - maps virtual button names to physical input specifications. This allows scripts to create custom control schemes by using meaningful button names (like "JUMP", "ATTACK") instead of physical button names.

**Parameters:**
- `virtualBtn` (string): Virtual button name to create (case-insensitive)
  - Examples: `"JUMP"`, `"ATTACK"`, `"FIRE"`, `"PAUSE"`, `"MENU"`
- `physicalSpec` (string): Physical input specification (case-insensitive)
  - **NES buttons:** `"A"`, `"B"`, `"SELECT"`, `"START"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`
  - **Xbox buttons:** `"A"`, `"B"`, `"X"`, `"Y"`, `"START"`, `"BACK"`, `"LEFT_SHOULDER"`, `"RIGHT_SHOULDER"`, `"LEFT_THUMB"`, `"RIGHT_THUMB"`, `"DPAD_UP"`, `"DPAD_DOWN"`, `"DPAD_LEFT"`, `"DPAD_RIGHT"`

**Returns:**
- Nothing

**Notes:**
- Mappings are per-script - each script can have its own virtual button mappings
- Virtual button names are case-insensitive (e.g., `"JUMP"`, `"jump"`, `"Jump"` all work)
- Physical button specs are case-insensitive
- Mappings can be overwritten by calling `mapinput()` again with the same virtual button name
- Virtual buttons can be used with `isbuttonpressed()` - the function will automatically resolve virtual names to physical specs
- Mappings are automatically cleaned up when the script is unloaded
- Useful for creating custom control schemes, game-specific button names, or remapping controls per script

**Example: Basic Custom Control Scheme:**
```lua
-- Set up custom mappings at script start
mapinput("JUMP", "A")
mapinput("ATTACK", "B")
mapinput("PAUSE", "START")
mapinput("MENU", "SELECT")

function gui()
    -- Use virtual button names instead of physical ones
    if isbuttonpressed(0, "JUMP") then
        drawtext(4, 4, "JUMP pressed!", 0x29)
    end
    
    if isbuttonpressed(0, "ATTACK") then
        drawtext(4, 12, "ATTACK pressed!", 0x29)
    end
end
```

**Example: Game-Specific Button Names:**
```lua
-- Map game-specific actions to buttons
mapinput("FIRE", "A")
mapinput("JUMP", "B")
mapinput("CROUCH", "DOWN")
mapinput("RUN", "RIGHT")

function gui()
    local fire = isbuttonpressed(0, "FIRE")
    local jump = isbuttonpressed(0, "JUMP")
    local crouch = isbuttonpressed(0, "CROUCH")
    local run = isbuttonpressed(0, "RUN")
    
    -- Use meaningful names in your script logic
    if fire and jump then
        drawtext(4, 4, "Fire + Jump combo!", 0x37)
    end
end
```

**Example: Xbox Button Mappings:**
```lua
-- Map virtual buttons to Xbox controller buttons
mapinput("PRIMARY", "A")
mapinput("SECONDARY", "B")
mapinput("TERTIARY", "X")
mapinput("QUATERNARY", "Y")
mapinput("SHOULDER_L", "LEFT_SHOULDER")
mapinput("SHOULDER_R", "RIGHT_SHOULDER")

function gui()
    -- Virtual buttons work with both NES and Xbox physical buttons
    if isbuttonpressed(0, "PRIMARY") then
        drawtext(4, 4, "Primary action", 0x29)
    end
    
    if isbuttonpressed(0, "SHOULDER_L") then
        drawtext(4, 12, "Left shoulder", 0x29)
    end
end
```

**Example: Remapping Controls:**
```lua
-- Remap controls for left-handed players or custom schemes
mapinput("ACTION", "B")  -- Swap A and B
mapinput("CANCEL", "A")
mapinput("MOVE_UP", "UP")
mapinput("MOVE_DOWN", "DOWN")
mapinput("MOVE_LEFT", "LEFT")
mapinput("MOVE_RIGHT", "RIGHT")

function gui()
    -- Script uses virtual names, physical mapping can be changed easily
    local action = isbuttonpressed(0, "ACTION")
    local cancel = isbuttonpressed(0, "CANCEL")
    
    if action then
        -- Perform action
    end
    
    if cancel then
        -- Perform cancel
    end
end
```

**Example: Dynamic Remapping:**
```lua
local useAlternateControls = false

function gui()
    -- Toggle control scheme (example: press START+SELECT to switch)
    local buttons = getjoypad(0)
    local startPressed = (math.floor(buttons / 0x08) % 2 == 1)
    local selectPressed = (math.floor(buttons / 0x04) % 2 == 1)
    
    if startPressed and selectPressed then
        if not useAlternateControls then
            -- Switch to alternate controls
            mapinput("JUMP", "B")
            mapinput("ATTACK", "A")
            useAlternateControls = true
            print("Switched to alternate controls")
        end
    else
        if useAlternateControls then
            -- Switch back to default controls
            mapinput("JUMP", "A")
            mapinput("ATTACK", "B")
            useAlternateControls = false
            print("Switched to default controls")
        end
    end
    
    -- Use virtual names - works regardless of current mapping
    if isbuttonpressed(0, "JUMP") then
        drawtext(4, 4, "JUMP!", 0x29)
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