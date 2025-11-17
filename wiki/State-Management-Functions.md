# State Management Functions

The State Management Functions allow you to save and load game states programmatically. Save states capture the complete game state including RAM, registers, and PPU state, allowing you to create checkpoint systems, automated save/load functionality, and script-controlled state management.

## Functions

### `savestate`

**Signature:** `savestate(slot)`
Saves the current game state to a specified slot.

**Parameters:**
- `slot` (integer, optional): Save state slot number (0-9). Defaults to 0 if not provided or nil.

**Returns:**
- `boolean` - `true` if save was successful, `false` if save failed

**Notes:**
- Save states are saved to `game:\states\` directory
- Slot numbers range from 0 to 9 (10 total slots)
- If slot is out of range (< 0 or > 9), the function will return an error
- A game must be loaded for save states to work (returns error if no game is loaded)
- The function verifies that the save file was created and has content before returning success
- Save states capture the complete game state including RAM, registers, and PPU state
- Useful for automated save states, checkpoint systems, and script-controlled state management
- Save state files are game-specific and will be overwritten if saving to the same slot

**Example: Basic Save:**
```lua
-- Save to slot 0 (default)
local success = savestate()
if success then
    print("State saved to slot 0")
else
    print("Save failed")
end

-- Save to slot 1
local success = savestate(1)
if success then
    print("State saved to slot 1")
else
    print("Save failed")
end
```

**Example: Save to Multiple Slots:**
```lua
-- Save to different slots
for slot = 0, 9 do
    local success = savestate(slot)
    if success then
        print(string.format("Saved to slot %d", slot))
    end
end
```

**Example: Checkpoint System:**
```lua
local checkpointSlot = 0

function gui()
    -- Check for checkpoint save (Y button)
    if isxboxbuttonpressed(0, "Y") then
        local success = savestate(checkpointSlot)
        if success then
            print("Checkpoint saved!")
        end
    end
end
```

**Example: Error Handling:**
```lua
-- Test with invalid slot
local success, err = pcall(function()
    return savestate(10)  -- Invalid slot (out of range)
end)

if not success then
    print("Error: " .. tostring(err))
end

-- Test with no game loaded
local success = savestate(0)
if not success then
    print("Save failed - check if game is loaded")
end
```

### `loadstate`

**Signature:** `loadstate(slot)`
Loads a game state from a specified slot.

**Parameters:**
- `slot` (integer, optional): Save state slot number (0-9). Defaults to 0 if not provided or nil.

**Returns:**
- `boolean` - `true` if load was successful, `false` if load failed (file doesn't exist or error occurred)

**Notes:**
- Save states are loaded from `game:\states\` directory
- Slot numbers range from 0 to 9 (10 total slots)
- If slot is out of range (< 0 or > 9), the function will return an error
- A game must be loaded for load states to work (returns error if no game is loaded)
- Returns `false` if the save state file doesn't exist (no error thrown, just returns false)
- Returns `true` if the state was successfully loaded
- Loading a state restores the complete game state including RAM, registers, and PPU state
- Useful for automated load states, checkpoint restore, and script-controlled state management
- The game will immediately jump to the saved state when loaded

**Example: Basic Load:**
```lua
-- Load from slot 0 (default)
local success = loadstate()
if success then
    print("State loaded from slot 0")
else
    print("Load failed - state may not exist")
end

-- Load from slot 1
local success = loadstate(1)
if success then
    print("State loaded from slot 1")
else
    print("Load failed - slot 1 may not exist")
end
```

**Example: Load from Multiple Slots:**
```lua
-- Try loading from different slots
for slot = 0, 9 do
    local success = loadstate(slot)
    if success then
        print(string.format("Loaded from slot %d", slot))
        break  -- Stop after first successful load
    end
end
```

**Example: Checkpoint Restore:**
```lua
local checkpointSlot = 0

function gui()
    -- Check for checkpoint load (B button)
    if isxboxbuttonpressed(0, "B") then
        local success = loadstate(checkpointSlot)
        if success then
            print("Checkpoint restored!")
        else
            print("No checkpoint found")
        end
    end
end
```

**Example: Slot Selector:**
```lua
local selectedSlot = 0

function gui()
    -- Navigate slots with D-pad
    if isxboxbuttonpressed(0, "DPAD_UP") then
        selectedSlot = (selectedSlot - 1) % 10
    elseif isxboxbuttonpressed(0, "DPAD_DOWN") then
        selectedSlot = (selectedSlot + 1) % 10
    end
    
    -- Save with Y button
    if isxboxbuttonpressed(0, "Y") then
        local success = savestate(selectedSlot)
        if success then
            print(string.format("Saved to slot %d", selectedSlot))
        end
    end
    
    -- Load with B button
    if isxboxbuttonpressed(0, "B") then
        local success = loadstate(selectedSlot)
        if success then
            print(string.format("Loaded from slot %d", selectedSlot))
        else
            print(string.format("Slot %d is empty", selectedSlot))
        end
    end
    
    -- Display current slot
    drawtext(10, 10, string.format("Slot: %d", selectedSlot), 0x20)
end
```

**Example: Error Handling:**
```lua
-- Test with invalid slot
local success, err = pcall(function()
    return loadstate(10)  -- Invalid slot (out of range)
end)

if not success then
    print("Error: " .. tostring(err))
end

-- Test loading non-existent state
local success = loadstate(9)  -- Slot 9 probably doesn't exist
if not success then
    print("Slot 9 is empty")
end
```

### `hasstate`

**Signature:** `hasstate(slot)`
Checks if a save state exists in the specified slot.

**Parameters:**
- `slot` (integer, optional): Save state slot number (0-9). Defaults to 0 if not provided or nil.

**Returns:**
- `boolean` - `true` if save state exists in the slot, `false` if it doesn't exist

**Notes:**
- Save states are checked in `game:\states\` directory
- Slot numbers range from 0 to 9 (10 total slots)
- If slot is out of range (< 0 or > 9), the function will return an error
- This function only checks for file existence; it does not require a game to be loaded
- Useful for checking which slots have saves before attempting to load
- Can be used to display save slot status in UI or for conditional logic
- Returns `false` if the file doesn't exist (no error thrown)

**Example: Basic Check:**
```lua
-- Check if slot 0 has a save
if hasstate(0) then
    print("Slot 0 has a save")
else
    print("Slot 0 is empty")
end

-- Check if slot 1 has a save
local exists = hasstate(1)
if exists then
    print("Slot 1 has a save")
end
```

**Example: Check All Slots:**
```lua
-- Check which slots have saves
for slot = 0, 9 do
    if hasstate(slot) then
        print(string.format("Slot %d has a save", slot))
    end
end
```

**Example: Display Save Status:**
```lua
function gui()
    -- Display which slots have saves
    local y = 10
    for slot = 0, 9 do
        local exists = hasstate(slot)
        local text = string.format("Slot %d: %s", slot, exists and "SAVED" or "EMPTY")
        local color = exists and 0x29 or 0x37  -- Green if saved, yellow if empty
        drawtext(10, y, text, color)
        y = y + 12
    end
end
```

**Example: Conditional Load:**
```lua
local selectedSlot = 0

function gui()
    -- Only load if slot has a save
    if isxboxbuttonpressed(0, "B") then
        if hasstate(selectedSlot) then
            local success = loadstate(selectedSlot)
            if success then
                print("Loaded successfully")
            end
        else
            print("No save in this slot")
        end
    end
end
```

### `savestatefile`

**Signature:** `savestatefile(filename)`
Saves the current game state to a custom filename.

**Parameters:**
- `filename` (string, required): Custom filename for the save state. Extension is optional (`.fc0` will be added if not provided).

**Returns:**
- `boolean` - `true` if save was successful, `false` if save failed

**Notes:**
- Save states are saved to `game:\states\` directory
- Filename is required (cannot be nil or empty)
- If filename doesn't have an extension, `.fc0` will be automatically added
- A game must be loaded for save states to work (returns error if no game is loaded)
- The function verifies that the save file was created and has content before returning success
- Save states capture the complete game state including RAM, registers, and PPU state
- Useful for named save states, custom filenames, and script-controlled state management
- Save state files are game-specific and will be overwritten if saving to the same filename

**Example: Basic Save:**
```lua
-- Save with custom filename
local success = savestatefile("checkpoint")
if success then
    print("State saved to 'checkpoint'")
else
    print("Save failed")
end

-- Save with extension
local success = savestatefile("my_save.fc0")
if success then
    print("State saved")
end
```

**Example: Named Checkpoints:**
```lua
function gui()
    -- Save checkpoint with name "cupid"
    if isxboxbuttonpressed(0, "Y") then
        local success = savestatefile("cupid")
        if success then
            print("Checkpoint 'cupid' saved!")
        end
    end
end
```

**Example: Multiple Named Saves:**
```lua
local saveNames = {"start", "midpoint", "final"}

function gui()
    -- Save to different named slots
    if isxboxbuttonpressed(0, "Y") then
        local success = savestatefile(saveNames[1])
        if success then
            print("Saved to 'start'")
        end
    end
end
```

**Example: Error Handling:**
```lua
-- Test with empty filename
local success, err = pcall(function()
    return savestatefile("")  -- Empty filename
end)

if not success then
    print("Error: " .. tostring(err))
end

-- Test with no game loaded
local success = savestatefile("test")
if not success then
    print("Save failed - check if game is loaded")
end
```

### `loadstatefile`

**Signature:** `loadstatefile(filename)`
Loads a game state from a custom filename.

**Parameters:**
- `filename` (string, required): Custom filename for the save state. Extension is optional (`.fc0` will be added if not provided).

**Returns:**
- `boolean` - `true` if load was successful, `false` if load failed (file doesn't exist or error occurred)

**Notes:**
- Save states are loaded from `game:\states\` directory
- Filename is required (cannot be nil or empty)
- If filename doesn't have an extension, `.fc0` will be automatically added
- A game must be loaded for load states to work (returns error if no game is loaded)
- Returns `false` if the save state file doesn't exist (no error thrown, just returns false)
- Returns `true` if the state was successfully loaded
- Loading a state restores the complete game state including RAM, registers, and PPU state
- Useful for loading named states, custom filenames, and script-controlled state management
- The game will immediately jump to the saved state when loaded

**Example: Basic Load:**
```lua
-- Load from custom filename
local success = loadstatefile("checkpoint")
if success then
    print("State loaded from 'checkpoint'")
else
    print("Load failed - file may not exist")
end

-- Load with extension
local success = loadstatefile("my_save.fc0")
if success then
    print("State loaded")
end
```

**Example: Named Checkpoint Load:**
```lua
function gui()
    -- Load checkpoint with name "cupid"
    if isxboxbuttonpressed(0, "B") then
        local success = loadstatefile("cupid")
        if success then
            print("Checkpoint 'cupid' loaded!")
        else
            print("Checkpoint 'cupid' not found")
        end
    end
end
```

**Example: Try Multiple Named Saves:**
```lua
local saveNames = {"checkpoint1", "checkpoint2", "backup"}

function gui()
    -- Try loading from different named saves
    if isxboxbuttonpressed(0, "B") then
        for i, name in ipairs(saveNames) do
            if loadstatefile(name) then
                print(string.format("Loaded '%s'", name))
                break
            end
        end
    end
end
```

**Example: Error Handling:**
```lua
-- Test with empty filename
local success, err = pcall(function()
    return loadstatefile("")  -- Empty filename
end)

if not success then
    print("Error: " .. tostring(err))
end

-- Test loading non-existent file
local success = loadstatefile("nonexistent")
if not success then
    print("File doesn't exist")
end
```

## Usage Tips

### Slot-Based vs. File-Based States

- **Slot-based functions** (`savestate()`, `loadstate()`, `hasstate()`): Use numbered slots (0-9) for quick access. Best for temporary checkpoints, quick saves, or when you need a fixed number of save slots.

- **File-based functions** (`savestatefile()`, `loadstatefile()`): Use custom filenames for named checkpoints or when you need more than 10 save states. Best for permanent checkpoints, milestone saves, or when you want descriptive names.

### Save State Directory

All save states are stored in the `game:\states\` directory. This directory is game-specific, so save states from one game won't appear for another game.

### Error Handling

- Always check the return value of save/load functions
- Use `hasstate()` before loading to check if a save exists
- Wrap calls in `pcall()` if you want to catch errors for invalid slots
- Remember that a game must be loaded before save/load operations will work

### Checkpoint Systems

State management functions are perfect for creating checkpoint systems:

```lua
local checkpointSlot = 0

function gui()
    -- Save checkpoint
    if isxboxbuttonpressed(0, "Y") then
        if savestate(checkpointSlot) then
            print("Checkpoint saved!")
        end
    end
    
    -- Load checkpoint
    if isxboxbuttonpressed(0, "B") then
        if hasstate(checkpointSlot) then
            if loadstate(checkpointSlot) then
                print("Checkpoint restored!")
            end
        else
            print("No checkpoint available")
        end
    end
end
```

## See Also

- **[Drawing Functions](Drawing-Functions)** - For displaying save state UI
- **[Input Functions](Input-Functions)** - For button-based save/load controls
- **[Memory Functions](Memory-Functions)** - For memory operations
- **[Home](Home)** - Return to the main wiki page