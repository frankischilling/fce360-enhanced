# ROM Info Functions

The ROM Info Functions provide comprehensive information about the loaded game ROM, including file details, ROM structure, mapper information, emulation state, and Game Genie code generation/decoding. These functions are essential for ROM analysis, game detection, and compatibility checking.

## ROM File Information Functions

### `getromname`

**Signature:** `getromname()`
Gets the current ROM filename (without path) as a string. Works for both NES and FDS games.

**Parameters:** None

**Returns:**
- `string` - Current ROM filename with extension (e.g., `"Super Mario Bros.nes"` or `"game.fds"`)
- Returns empty string (`""`) if no game is loaded

**Notes:**
- Returns just the filename, not the full path
- Works for both NES (`.nes`) and FDS (`.fds`) games
- Handles zip archive format: extracts filename from `"path.zip|internal.nes"` format
- Useful for game detection, ROM-specific scripts, or displaying current game name
- The filename includes the extension (`.nes`, `.fds`, etc.)

**Example: Display Current ROM:**
```lua
function gui()
    local romName = getromname()
    if romName ~= "" then
        drawtext(4, 4, "ROM: " .. romName, 0x20)
    else
        drawtext(4, 4, "No ROM loaded", 0x2D)
    end
end
```

**Example: Game-Specific Script:**
```lua
local lastRomName = ""

function gui()
    local romName = getromname()
    
    -- Detect ROM change
    if romName ~= lastRomName then
        print("ROM changed: " .. romName)
        lastRomName = romName
        
        -- Initialize game-specific variables
        if romName:find("Mario") then
            print("Mario game detected!")
        elseif romName:find("%.fds$") then
            print("FDS game detected!")
        end
    end
    
    -- Display ROM name
    drawtext(4, 4, romName, 0x20)
end
```

**Example: Log ROM Changes:**
```lua
local lastRomName = ""

function gui()
    local romName = getromname()
    
    -- Log when ROM changes
    if romName ~= "" and romName ~= lastRomName then
        print("Loaded: " .. romName)
        lastRomName = romName
    end
end
```

## ROM Size Functions

### `getromsize`

**Signature:** `getromsize()`
Gets the total ROM size in bytes (PRG-ROM + CHR-ROM combined). Useful for ROM validation, size checks, and ROM analysis.

**Parameters:** None

**Returns:**
- `integer` - Total ROM size in bytes (PRG-ROM + CHR-ROM)
- Returns `0` if no game is loaded

**Notes:**
- Returns the combined size of both PRG-ROM (program ROM) and CHR-ROM (character/graphics ROM)
- PRG-ROM size is in 16KB units, CHR-ROM size is in 8KB units
- Total size = (PRG-ROM × 16KB) + (CHR-ROM × 8KB)
- Useful for validating ROM integrity, checking for expected ROM sizes, or displaying ROM information
- Common ROM sizes:
  - Small ROMs: 16KB-32KB (simple games like NROM)
  - Medium ROMs: 128KB-256KB (MMC1, MMC3 games)
  - Large ROMs: 512KB+ (complex games with large CHR-ROM)

**Example: Display ROM Size:**
```lua
function gui()
    local romSize = getromsize()
    
    if romSize == 0 then
        drawtext(4, 4, "No ROM loaded", 0x2D)
    else
        local sizeKB = romSize / 1024
        local sizeMB = sizeKB / 1024
        
        if sizeMB >= 1.0 then
            drawtext(4, 4, string.format("ROM Size: %.2f MB", sizeMB), 0x20)
        else
            drawtext(4, 4, string.format("ROM Size: %.2f KB", sizeKB), 0x20)
        end
        
        drawtext(4, 14, string.format("Bytes: %d", romSize), 0x29)
    end
end
```

**Example: ROM Validation:**
```lua
function gui()
    local romSize = getromsize()
    
    if romSize > 0 then
        -- Validate ROM size is within expected range
        if romSize < 16384 then
            drawtext(4, 4, "WARNING: ROM < 16KB", 0x37)
        elseif romSize > 4194304 then
            drawtext(4, 4, "WARNING: ROM > 4MB", 0x37)
        else
            drawtext(4, 4, "ROM size OK", 0x2E)
        end
        
        drawtext(4, 14, string.format("Size: %d bytes", romSize), 0x20)
    end
end
```

**Example: ROM Analysis:**
```lua
local lastRomSize = 0

function gui()
    local romSize = getromsize()
    local romName = getromname()
    
    -- Detect ROM change
    if romSize ~= lastRomSize and romSize > 0 then
        print("=== ROM Analysis ===")
        print("ROM: " .. romName)
        print(string.format("Size: %d bytes", romSize))
        
        local sizeKB = romSize / 1024
        print(string.format("Size: %.2f KB", sizeKB))
        
        -- Estimate ROM type based on size
        if romSize <= 32768 then
            print("Type: Small ROM (likely NROM)")
        elseif romSize <= 131072 then
            print("Type: Medium ROM (likely MMC1)")
        elseif romSize <= 262144 then
            print("Type: Large ROM (likely MMC3)")
        else
            print("Type: Very Large ROM")
        end
        
        print("===================")
        lastRomSize = romSize
    end
end
```

### `getprgsize`

**Signature:** `getprgsize()`
Gets the PRG-ROM (Program ROM) size in bytes. Useful for ROM analysis, determining game complexity, and analyzing the program code size separately from graphics data.

**Parameters:** None

**Returns:**
- `integer` - PRG-ROM size in bytes
- Returns `0` if no game is loaded

**Notes:**
- Returns only the PRG-ROM size, not the total ROM size (use `getromsize()` for total)
- PRG-ROM contains the game's program code and data
- PRG-ROM size is in 16KB units internally, converted to bytes (ROM_size × 16KB)
- Common PRG-ROM sizes:
  - `16KB` = Small game (NROM, simple games)
  - `32KB` = Small game (NROM)
  - `64KB` = Medium game
  - `128KB` = Medium-large game (MMC1)
  - `256KB` = Large game (MMC3)
  - `512KB+` = Very large game
- CHR-ROM size can be calculated: `getromsize() - getprgsize()`
- Useful for analyzing game complexity, as larger PRG-ROM typically indicates more game code

**Example: Display PRG-ROM Size:**
```lua
function gui()
    local prgSize = getprgsize()
    
    if prgSize == 0 then
        drawtext(4, 4, "No ROM loaded", 0x2D)
    else
        local prgKB = prgSize / 1024
        local prgMB = prgKB / 1024
        
        if prgMB >= 1.0 then
            drawtext(4, 4, string.format("PRG-ROM: %.2f MB", prgMB), 0x20)
        else
            drawtext(4, 4, string.format("PRG-ROM: %.2f KB", prgKB), 0x20)
        end
        
        drawtext(4, 14, string.format("Bytes: %d", prgSize), 0x29)
    end
end
```

**Example: ROM Breakdown:**
```lua
function gui()
    local prgSize = getprgsize()
    local romSize = getromsize()
    
    if prgSize > 0 and romSize > 0 then
        local chrSize = romSize - prgSize
        local prgPercent = (prgSize / romSize) * 100
        local chrPercent = (chrSize / romSize) * 100
        
        drawtext(4, 4, string.format("PRG-ROM: %d bytes (%.1f%%)", prgSize, prgPercent), 0x20)
        drawtext(4, 14, string.format("CHR-ROM: %d bytes (%.1f%%)", chrSize, chrPercent), 0x29)
        drawtext(4, 24, string.format("Total: %d bytes", romSize), 0x2A)
    end
end
```

**Example: ROM Analysis:**
```lua
local lastPrgSize = 0

function gui()
    local prgSize = getprgsize()
    local romName = getromname()
    
    -- Detect ROM change
    if prgSize ~= lastPrgSize and prgSize > 0 then
        print("=== PRG-ROM Analysis ===")
        print("ROM: " .. romName)
        print(string.format("PRG-ROM: %d bytes", prgSize))
        
        local prgKB = prgSize / 1024
        print(string.format("PRG-ROM: %.2f KB", prgKB))
        
        -- Estimate game complexity
        if prgSize <= 32768 then
            print("Complexity: Simple game")
        elseif prgSize <= 131072 then
            print("Complexity: Medium game")
        elseif prgSize <= 262144 then
            print("Complexity: Complex game")
        else
            print("Complexity: Very complex game")
        end
        
        -- Common sizes
        if prgSize == 16384 then
            print("Type: 16KB PRG-ROM (NROM)")
        elseif prgSize == 32768 then
            print("Type: 32KB PRG-ROM (NROM)")
        elseif prgSize == 131072 then
            print("Type: 128KB PRG-ROM (MMC1)")
        elseif prgSize == 262144 then
            print("Type: 256KB PRG-ROM (MMC3)")
        end
        
        print("========================")
        lastPrgSize = prgSize
    end
end
```

### `getchrsize`

**Signature:** `getchrsize()`
Gets the CHR-ROM (Character/Graphics ROM) size in bytes. Useful for ROM analysis, determining graphics complexity, and analyzing the graphics data size separately from program code.

**Parameters:** None

**Returns:**
- `integer` - CHR-ROM size in bytes
- Returns `0` if no game is loaded or if the ROM uses CHR-RAM instead of CHR-ROM

**Notes:**
- Returns only the CHR-ROM size, not the total ROM size (use `getromsize()` for total)
- CHR-ROM contains the game's graphics tiles, sprites, and character data
- CHR-ROM size is in 8KB units internally, converted to bytes (VROM_size × 8KB)
- Common CHR-ROM sizes:
  - `0KB` = CHR-RAM mode (uses RAM instead of ROM, common in some mappers)
  - `8KB` = Small graphics set
  - `16KB` = Medium graphics set
  - `32KB` = Large graphics set
  - `64KB` = Very large graphics set
  - `128KB+` = Extremely large graphics set
- PRG-ROM size can be calculated: `getromsize() - getchrsize()`
- Useful for analyzing graphics complexity, as larger CHR-ROM typically indicates more graphics tiles and sprites
- Some mappers use CHR-RAM (0 bytes) instead of CHR-ROM, allowing dynamic graphics

**Example: Display CHR-ROM Size:**
```lua
function gui()
    local chrSize = getchrsize()
    
    if chrSize == 0 then
        drawtext(4, 4, "CHR-RAM mode", 0x2D)
    else
        local chrKB = chrSize / 1024
        local chrMB = chrKB / 1024
        
        if chrMB >= 1.0 then
            drawtext(4, 4, string.format("CHR-ROM: %.2f MB", chrMB), 0x20)
        else
            drawtext(4, 4, string.format("CHR-ROM: %.2f KB", chrKB), 0x20)
        end
        
        drawtext(4, 14, string.format("Bytes: %d", chrSize), 0x29)
    end
end
```

**Example: ROM Breakdown:**
```lua
function gui()
    local chrSize = getchrsize()
    local prgSize = getprgsize()
    local romSize = getromsize()
    
    if chrSize > 0 and romSize > 0 then
        local prgPercent = (prgSize / romSize) * 100
        local chrPercent = (chrSize / romSize) * 100
        
        drawtext(4, 4, string.format("PRG-ROM: %d bytes (%.1f%%)", prgSize, prgPercent), 0x20)
        drawtext(4, 14, string.format("CHR-ROM: %d bytes (%.1f%%)", chrSize, chrPercent), 0x29)
        drawtext(4, 24, string.format("Total: %d bytes", romSize), 0x2A)
    elseif chrSize == 0 and romSize > 0 then
        drawtext(4, 4, "CHR-RAM mode (no CHR-ROM)", 0x2D)
        drawtext(4, 14, string.format("PRG-ROM: %d bytes", prgSize), 0x20)
    end
end
```

**Example: ROM Analysis:**
```lua
local lastChrSize = 0

function gui()
    local chrSize = getchrsize()
    local romName = getromname()
    
    -- Detect ROM change
    if chrSize ~= lastChrSize and romName ~= "" then
        print("=== CHR-ROM Analysis ===")
        print("ROM: " .. romName)
        print(string.format("CHR-ROM: %d bytes", chrSize))
        
        if chrSize == 0 then
            print("Mode: CHR-RAM (dynamic graphics)")
        else
            local chrKB = chrSize / 1024
            print(string.format("CHR-ROM: %.2f KB", chrKB))
            
            -- Estimate graphics complexity
            if chrSize <= 16384 then
                print("Complexity: Simple graphics")
            elseif chrSize <= 32768 then
                print("Complexity: Medium graphics")
            elseif chrSize <= 65536 then
                print("Complexity: Complex graphics")
            else
                print("Complexity: Very complex graphics")
            end
        end
        
        print("========================")
        lastChrSize = chrSize
    end
end
```

## ROM Feature Functions

### `hasbattery`

**Signature:** `hasbattery()`
Checks if the ROM has battery-backed save RAM. Useful for save state detection and determining if a game supports persistent saves.

**Parameters:** None

**Returns:**
- `boolean` - `true` if the ROM has battery-backed save RAM, `false` otherwise
- Returns `false` if no game is loaded

**Notes:**
- Battery-backed save RAM allows games to save progress permanently (even when the console is turned off)
- Games with battery typically support:
  - Password systems
  - High score tables
  - Game progress saves
  - Configuration settings
- Common games with battery: The Legend of Zelda, Metroid, Final Fantasy, etc.
- Games without battery cannot save progress permanently
- Useful for detecting which games support save files and persistent data

**Example: Display Battery Status:**
```lua
function gui()
    local hasBattery = hasbattery()
    local romName = getromname()
    
    if romName == "" then
        drawtext(4, 4, "No ROM loaded", 0x2D)
    else
        drawtext(4, 4, "ROM: " .. romName, 0x2E)
        
        if hasBattery then
            drawtext(4, 14, "Battery: Yes", 0x29)
            drawtext(4, 24, "Save RAM: Supported", 0x2E)
        else
            drawtext(4, 14, "Battery: No", 0x37)
            drawtext(4, 24, "Save RAM: Not supported", 0x2A)
        end
    end
end
```

**Example: Save State Detection:**
```lua
function gui()
    local hasBattery = hasbattery()
    local romName = getromname()
    
    if hasBattery then
        drawtext(4, 4, "This game supports", 0x20)
        drawtext(4, 14, "persistent saves", 0x20)
        drawtext(4, 24, "Save files will be", 0x20)
        drawtext(4, 34, "preserved", 0x20)
    else
        drawtext(4, 4, "This game does not", 0x37)
        drawtext(4, 14, "support persistent", 0x37)
        drawtext(4, 24, "saves", 0x37)
    end
end
```

**Example: ROM Analysis:**
```lua
local lastBattery = nil

function gui()
    local hasBattery = hasbattery()
    local romName = getromname()
    
    -- Detect ROM change
    if hasBattery ~= lastBattery and romName ~= "" then
        print("=== Battery Information ===")
        print("ROM: " .. romName)
        print(string.format("Battery: %s", hasBattery and "Yes" or "No"))
        
        if hasBattery then
            print("Save RAM: Supported")
            print("This game can save progress permanently")
        else
            print("Save RAM: Not supported")
            print("This game cannot save progress permanently")
        end
        
        -- Additional info
        local mapper = getmapper()
        print(string.format("Mapper: %d", mapper))
        
        print("========================")
        lastBattery = hasBattery
    end
end
```

## Emulation State Functions

### `isframeadvancing`

**Signature:** `isframeadvancing()`
Checks if emulation is advancing frames. Returns `false` if paused. Useful for detecting pause state and adjusting script behavior accordingly.

**Parameters:** None

**Returns:**
- `boolean` - `true` if frames are advancing (emulation running), `false` if paused
- Always returns a boolean value (never nil)

**Notes:**
- Returns `true` when emulation is running and frames are being processed
- Returns `false` when emulation is paused (frames are not advancing)
- Useful for scripts that need to detect pause state and skip updates when paused
- Can be used to disable expensive operations while paused
- Frame advancement state can change at any time (user can pause/unpause)

**Example: Display Pause Status:**
```lua
function gui()
    local isAdvancing = isframeadvancing()
    
    if isAdvancing then
        drawtext(4, 4, "Emulation: Running", 0x2E)
        drawtext(4, 14, "Frames: Advancing", 0x29)
    else
        drawtext(4, 4, "Emulation: Paused", 0x37)
        drawtext(4, 14, "Frames: Not advancing", 0x37)
    end
end
```

**Example: Skip Updates When Paused:**
```lua
function gui()
    local isAdvancing = isframeadvancing()
    
    -- Only update expensive calculations when frames are advancing
    if isAdvancing then
        -- Perform expensive operations
        local complexCalculation = performExpensiveOperation()
        drawtext(4, 4, string.format("Result: %d", complexCalculation), 0x20)
    else
        -- Show cached or static info when paused
        drawtext(4, 4, "Paused - updates disabled", 0x37)
    end
end
```

**Example: Pause State Detection:**
```lua
local lastAdvancing = nil

function gui()
    local isAdvancing = isframeadvancing()
    
    -- Detect pause state change
    if isAdvancing ~= lastAdvancing then
        if isAdvancing then
            print("Emulation resumed - frames advancing")
        else
            print("Emulation paused - frames stopped")
        end
        lastAdvancing = isAdvancing
    end
    
    -- Display status
    if isAdvancing then
        drawtext(4, 4, "Running", 0x2E)
    else
        drawtext(4, 4, "Paused", 0x37)
    end
end
```

### `isrewinding`

**Signature:** `isrewinding()`
Checks if the emulator is currently rewinding. Returns `true` if rewinding is active, `false` otherwise. Useful for disabling scripts during rewind and adjusting script behavior when game state is being restored from saved states.

**Parameters:** None

**Returns:**
- `boolean` - `true` if currently rewinding, `false` otherwise
- Always returns a boolean value (never nil)

**Notes:**
- Returns `true` when the rewind button (LT) is held and rewind is active
- Returns `false` during normal emulation or when rewind is not active
- Useful for scripts that need to disable expensive operations during rewind
- Can be used to skip script updates while game state is being restored
- Rewind state can change at any time (user can start/stop rewind)
- During rewind, game state is being restored from saved states, so scripts should avoid modifying game state

**Example: Display Rewind Status:**
```lua
function gui()
    local isRewinding = isrewinding()
    
    if isRewinding then
        drawtext(4, 4, "Rewind: ACTIVE", 0x37)
        drawtext(4, 14, "Status: Rewinding", 0x37)
    else
        drawtext(4, 4, "Rewind: Inactive", 0x29)
        drawtext(4, 14, "Status: Normal", 0x2E)
    end
end
```

**Example: Disable Scripts During Rewind:**
```lua
function gui()
    local isRewinding = isrewinding()
    
    -- Only run expensive operations when not rewinding
    if not isRewinding then
        -- Perform expensive calculations
        local complexCalculation = performExpensiveOperation()
        drawtext(4, 4, string.format("Result: %d", complexCalculation), 0x20)
    else
        -- Show cached or static info when rewinding
        drawtext(4, 4, "Rewinding - updates disabled", 0x37)
    end
end
```

**Example: Rewind State Detection:**
```lua
local lastRewinding = nil

function gui()
    local isRewinding = isrewinding()
    
    -- Detect rewind state change
    if isRewinding ~= lastRewinding then
        if isRewinding then
            print("Rewind started - disabling script updates")
        else
            print("Rewind stopped - resuming script updates")
        end
        lastRewinding = isRewinding
    end
    
    -- Display status
    if isRewinding then
        drawtext(4, 4, "Rewinding", 0x37)
    else
        drawtext(4, 4, "Normal", 0x2E)
    end
end
```

**Example: Skip Updates During Rewind:**
```lua
local lastFrameCount = 0

function gui()
    local isRewinding = isrewinding()
    local frameCount = getframecount()
    
    -- Only update frame counter when not rewinding
    if not isRewinding and frameCount ~= lastFrameCount then
        print(string.format("Frame: %d", frameCount))
        lastFrameCount = frameCount
    end
    
    -- Display current state
    if isRewinding then
        drawtext(4, 4, "Rewinding...", 0x37)
    else
        drawtext(4, 4, string.format("Frame: %d", frameCount), 0x2E)
    end
end
```

### `isfastforwarding`

**Signature:** `isfastforwarding()`
Checks if the emulator is currently fast-forwarding. Returns `true` if fast-forward is active, `false` otherwise. Useful for adjusting script behavior during fast-forward, such as disabling expensive operations or skipping updates.

**Parameters:** None

**Returns:**
- `boolean` - `true` if currently fast-forwarding, `false` otherwise
- Always returns a boolean value (never nil)

**Notes:**
- Returns `true` when the fast-forward button (RT) is held and fast-forward is active
- Returns `false` during normal emulation or when fast-forward is not active
- Useful for scripts that need to disable expensive operations during fast-forward
- Can be used to skip script updates while fast-forwarding to improve performance
- Fast-forward state can change at any time (user can start/stop fast-forward)
- During fast-forward, emulation runs at 2× speed, so scripts may want to reduce update frequency

**Example: Display Fast-Forward Status:**
```lua
function gui()
    local isFastForwarding = isfastforwarding()
    
    if isFastForwarding then
        drawtext(4, 4, "Fast-Forward: ACTIVE", 0x37)
        drawtext(4, 14, "Status: 2× Speed", 0x37)
    else
        drawtext(4, 4, "Fast-Forward: Inactive", 0x29)
        drawtext(4, 14, "Status: Normal Speed", 0x2E)
    end
end
```

**Example: Disable Scripts During Fast-Forward:**
```lua
function gui()
    local isFastForwarding = isfastforwarding()
    
    -- Only run expensive operations when not fast-forwarding
    if not isFastForwarding then
        -- Perform expensive calculations
        local complexCalculation = performExpensiveOperation()
        drawtext(4, 4, string.format("Result: %d", complexCalculation), 0x20)
    else
        -- Show cached or static info when fast-forwarding
        drawtext(4, 4, "Fast-Forward - updates disabled", 0x37)
    end
end
```

**Example: Fast-Forward State Detection:**
```lua
local lastFastForwarding = nil

function gui()
    local isFastForwarding = isfastforwarding()
    
    -- Detect fast-forward state change
    if isFastForwarding ~= lastFastForwarding then
        if isFastForwarding then
            print("Fast-forward started - disabling script updates")
        else
            print("Fast-forward stopped - resuming script updates")
        end
        lastFastForwarding = isFastForwarding
    end
    
    -- Display status
    if isFastForwarding then
        drawtext(4, 4, "Fast-Forward", 0x37)
    else
        drawtext(4, 4, "Normal", 0x2E)
    end
end
```

**Example: Skip Updates During Fast-Forward:**
```lua
local lastFrameCount = 0

function gui()
    local isFastForwarding = isfastforwarding()
    local frameCount = getframecount()
    
    -- Only update frame counter when not fast-forwarding (or update less frequently)
    if not isFastForwarding and frameCount ~= lastFrameCount then
        print(string.format("Frame: %d", frameCount))
        lastFrameCount = frameCount
    end
    
    -- Display current state
    if isFastForwarding then
        drawtext(4, 4, "Fast-Forwarding...", 0x37)
    else
        drawtext(4, 4, string.format("Frame: %d", frameCount), 0x2E)
    end
end
```

## Mapper Information Functions

### `getmapper`

**Signature:** `getmapper()`
Gets the NES mapper number (0-255). Useful for mapper-specific scripts, compatibility checks, and determining which mapper chip a ROM uses.

**Parameters:** None

**Returns:**
- `integer` - Mapper number (0-255)
- Returns `0` if no game is loaded

**Notes:**
- The mapper number identifies which mapper chip (MMC) the ROM uses
- Different mappers have different capabilities (bank switching, battery saves, etc.)
- Common mapper numbers:
  - `0` = NROM (simplest mapper, 16-32KB PRG-ROM)
  - `1` = MMC1 (SxROM) - Supports bank switching, battery saves
  - `2` = UNROM - Simple bank switching
  - `3` = CNROM - Character ROM switching
  - `4` = MMC3 (TxROM) - Popular mapper, used in many games
  - `5` = MMC5 - Advanced mapper with extra features
  - `7` = AOROM - Simple bank switching
  - `9` = MMC2 (PxROM) - Used in Punch-Out!!
  - `10` = MMC4 (PxROM) - Similar to MMC2
- Mapper numbers above 255 are not valid for standard iNES format
- Useful for enabling mapper-specific features or compatibility checks in scripts

**Example: Display Mapper Number:**
```lua
function gui()
    local mapper = getmapper()
    
    if mapper == 0 then
        drawtext(4, 4, "No ROM loaded", 0x2D)
    else
        drawtext(4, 4, string.format("Mapper: %d", mapper), 0x20)
        
        -- Display mapper name
        if mapper == 0 then
            drawtext(4, 14, "NROM", 0x29)
        elseif mapper == 1 then
            drawtext(4, 14, "MMC1", 0x29)
        elseif mapper == 4 then
            drawtext(4, 14, "MMC3", 0x29)
        else
            drawtext(4, 14, "Unknown", 0x2A)
        end
    end
end
```

**Example: Mapper-Specific Script:**
```lua
function gui()
    local mapper = getmapper()
    
    -- Enable mapper-specific features
    if mapper == 1 then
        -- MMC1 specific code
        drawtext(4, 4, "MMC1 detected", 0x2E)
        -- MMC1 supports battery saves, bank switching, etc.
    elseif mapper == 4 then
        -- MMC3 specific code
        drawtext(4, 4, "MMC3 detected", 0x2E)
        -- MMC3 has IRQ support, different bank switching
    elseif mapper == 0 then
        -- NROM specific code
        drawtext(4, 4, "NROM detected", 0x2E)
        -- Simple mapper, no special features
    end
end
```

**Example: Compatibility Check:**
```lua
local lastMapper = -1

function gui()
    local mapper = getmapper()
    local romName = getromname()
    
    -- Detect mapper change
    if mapper ~= lastMapper and mapper > 0 then
        print("=== Mapper Information ===")
        print("ROM: " .. romName)
        print(string.format("Mapper: %d", mapper))
        
        -- Check compatibility
        if mapper == 0 then
            print("Compatible: NROM - Simple mapper, basic functionality")
        elseif mapper == 1 or mapper == 4 then
            print("Compatible: Common mapper, well-supported")
        elseif mapper >= 0 and mapper <= 255 then
            print(string.format("Compatible: Valid mapper %d", mapper))
        else
            print("Warning: Invalid mapper number")
        end
        
        print("========================")
        lastMapper = mapper
    end
end
```

### `getmapperstring`

**Signature:** `getmapperstring()`
Gets the mapper name as a string (e.g., "NROM", "MMC1", "MMC3"). Useful for displaying mapper information in a human-readable format.

**Parameters:** None

**Returns:**
- `string` - Mapper name (e.g., `"NROM"`, `"MMC1"`, `"MMC3"`)
- Returns `"Mapper X"` format for unknown mappers (where X is the mapper number)
- Returns empty string (`""`) if no game is loaded

**Notes:**
- Returns a human-readable mapper name instead of just the number
- Known mapper names include: "NROM", "MMC1", "MMC3", "UNROM", "CNROM", "VRC4", "VRC6", "Bandai", etc.
- For unknown mappers (0-255), returns formatted string like `"Mapper 42"`
- For invalid mappers, returns `"Unknown"`
- Complements `getmapper()` by providing the name instead of just the number
- Useful for displaying mapper information in user interfaces or logs

**Example: Display Mapper Name:**
```lua
function gui()
    local mapperString = getmapperstring()
    local mapper = getmapper()
    
    if mapperString == "" then
        drawtext(4, 4, "No ROM loaded", 0x2D)
    else
        drawtext(4, 4, string.format("Mapper: %d", mapper), 0x20)
        drawtext(4, 14, "Name: " .. mapperString, 0x29)
    end
end
```

**Example: Mapper Name Comparison:**
```lua
function gui()
    local mapperString = getmapperstring()
    local mapper = getmapper()
    
    -- Check for specific mapper types
    if mapperString == "MMC1" or mapperString == "MMC3" then
        drawtext(4, 4, "MMC mapper detected", 0x2E)
    elseif string.find(mapperString, "VRC") then
        drawtext(4, 4, "VRC mapper detected", 0x2E)
    elseif mapperString == "NROM" then
        drawtext(4, 4, "Simple NROM mapper", 0x2E)
    end
    
    drawtext(4, 14, string.format("%d = %s", mapper, mapperString), 0x20)
end
```

**Example: Display Mapper Info:**
```lua
local lastMapperString = ""

function gui()
    local mapperString = getmapperstring()
    local romName = getromname()
    
    -- Detect mapper change
    if mapperString ~= lastMapperString and mapperString ~= "" then
        print("=== Mapper Information ===")
        print("ROM: " .. romName)
        print("Mapper: " .. mapperString)
        
        -- Check if it's a known mapper
        if string.find(mapperString, "Mapper %d") then
            print("Note: Unknown mapper (not in lookup table)")
        else
            print("Note: Known mapper name")
        end
        
        -- String operations
        local upper = string.upper(mapperString)
        print("Uppercase: " .. upper)
        
        print("========================")
        lastMapperString = mapperString
    end
end
```

## Game Genie Code Functions

### `getgamegeniecode`

**Signature:** `getgamegeniecode(address, value, compare)`
Generates a Game Genie code string from an address, value, and optional compare value. Game Genie codes are 6-character (or 8-character with compare) codes used to modify ROM reads at specific addresses, similar to cheat codes.

**Parameters:**
- `address` (integer): ROM address (0x8000-0xFFFF)
  - Must be in the valid NES ROM address range
  - Addresses below 0x8000 are not valid for Game Genie codes
- `value` (integer): Value to write (0-255)
  - The byte value that will replace the original value at the address
- `compare` (integer, optional): Compare value (0-255)
  - If provided, the code only applies when the current value matches this compare value
  - If omitted or `nil`, the code applies unconditionally
  - Used for conditional cheats that only activate when a specific value is present

**Returns:**
- `string` - Game Genie code (6 characters without compare, 8 characters with compare)
- Returns a string using only Game Genie characters: A, P, Z, L, G, I, T, Y, E, O, X, U, K, S, V, N
- Throws an error if parameters are invalid (address out of range, value out of range, etc.)

**Notes:**
- Game Genie codes modify ROM reads, not RAM writes
- The generated code is in the standard NES Game Genie format
- Codes can be entered into FCEUX's cheat system or Game Genie interface
- To apply the code, you can either:
  1. Use the generated Game Genie code string directly (if FCEUX supports Game Genie code entry)
  2. Add a cheat manually using the address and value parameters
- The address is masked to 15 bits (0x7FFF) internally as per Game Genie encoding
- Compare values are useful for conditional cheats (e.g., "only activate if health is below a certain value")
- Game Genie codes work by intercepting ROM reads and replacing the value at the specified address

**Example: Generate Simple Game Genie Code:**
```lua
function gui()
    -- Generate a code for address 0x8123 with value 0x63
    local code = getgamegeniecode(0x8123, 0x63)
    drawtext(4, 4, "Game Genie Code: " .. code, 0x20)
    print("Code for 0x8123 -> 0x63: " .. code)
end
```

**Example: Generate Code with Compare Value:**
```lua
function gui()
    -- Generate a code that only applies when current value is 0x02
    local code = getgamegeniecode(0xB456, 0x63, 0x02)
    drawtext(4, 4, "Conditional Code: " .. code, 0x20)
    print("Code for 0xB456 -> 0x63 (if = 0x02): " .. code)
end
```

**Example: Generate Multiple Codes:**
```lua
function gui()
    local romName = getromname()
    
    if romName ~= "" then
        -- Generate several example codes
        local code1 = getgamegeniecode(0x8123, 0x63)
        local code2 = getgamegeniecode(0x8456, 0xFF)
        local code3 = getgamegeniecode(0x8ABC, 0x02)
        
        drawtext(4, 4, "Code 1: " .. code1, 0x20)
        drawtext(4, 14, "Code 2: " .. code2, 0x20)
        drawtext(4, 24, "Code 3: " .. code3, 0x20)
        
        print("=== Generated Game Genie Codes ===")
        print("0x8123 -> 0x63: " .. code1)
        print("0x8456 -> 0xFF: " .. code2)
        print("0x8ABC -> 0x02: " .. code3)
    end
end
```

**Example: Validate and Display Code:**
```lua
function gui()
    local address = 0x8123
    local value = 0x63
    
    -- Validate address range
    if address >= 0x8000 and address <= 0xFFFF and value >= 0 and value <= 255 then
        local code = getgamegeniecode(address, value)
        drawtext(4, 4, string.format("Address: 0x%04X", address), 0x20)
        drawtext(4, 14, string.format("Value: 0x%02X", value), 0x20)
        drawtext(4, 24, "Code: " .. code, 0x29)
        drawtext(4, 34, "Length: " .. string.len(code), 0x2E)
    else
        drawtext(4, 4, "Invalid address or value", 0x2D)
    end
end
```

**Example: Generate Codes for Cheat List:**
```lua
local codesGenerated = false

function gui()
    local romName = getromname()
    
    if romName ~= "" and not codesGenerated then
        codesGenerated = true
        
        print("=== Game Genie Codes for " .. romName .. " ===")
        
        -- Generate codes with different parameters
        local codes = {
            {addr = 0x8123, val = 0x63, desc = "Infinite Lives"},
            {addr = 0x8456, val = 0xFF, desc = "Max Health"},
            {addr = 0x8ABC, val = 0x02, desc = "Power-Up"},
            {addr = 0x9DEF, val = 0x99, desc = "Max Time", compare = 0x00},
        }
        
        for i = 1, #codes do
            local code
            if codes[i].compare then
                code = getgamegeniecode(codes[i].addr, codes[i].val, codes[i].compare)
            else
                code = getgamegeniecode(codes[i].addr, codes[i].val)
            end
            
            print(string.format("%s: %s (0x%04X -> 0x%02X)", 
                  codes[i].desc, code, codes[i].addr, codes[i].val))
        end
        
        print("========================================")
    end
end
```

### `decodegamegenie`

**Signature:** `decodegamegenie(code)`
Decodes a Game Genie code string back into its address, value, and optional compare value. This is the inverse operation of `getgamegeniecode()`, allowing you to parse Game Genie codes and extract their components.

**Parameters:**
- `code` (string): Game Genie code string
  - Must be exactly 6 characters (no compare) or 8 characters (with compare)
  - Must contain only valid Game Genie characters: A, P, Z, L, G, I, T, Y, E, O, X, U, K, S, V, N
  - Case-sensitive (must be uppercase)

**Returns:**
- `table` - A Lua table with the following keys:
  - `address` (integer): ROM address (0x8000-0xFFFF)
  - `value` (integer): Value to write (0-255)
  - `compare` (integer, optional): Compare value (0-255) - only present if code is 8 characters
- Throws an error if the code is invalid (wrong length, invalid characters, etc.)

**Notes:**
- This function reverses the encoding performed by `getgamegeniecode()`
- 6-character codes have no compare value (unconditional cheat)
- 8-character codes include a compare value (conditional cheat that only applies when current value matches)
- The `compare` field will be `nil` in the returned table if the code is 6 characters
- Useful for parsing Game Genie codes from external sources or validating codes
- Can be used to convert Game Genie codes into cheat format (address + value + compare)

**Example: Decode a Game Genie Code:**
```lua
function gui()
    local code = "LTLZPA"
    local result = decodegamegenie(code)
    
    drawtext(4, 4, "Code: " .. code, 0x20)
    drawtext(4, 14, string.format("Address: 0x%04X", result.address), 0x29)
    drawtext(4, 24, string.format("Value: 0x%02X", result.value), 0x29)
    
    if result.compare then
        drawtext(4, 34, string.format("Compare: 0x%02X", result.compare), 0x29)
    end
end
```

**Example: Round-Trip Test (Encode then Decode):**
```lua
function gui()
    local address = 0x8123
    local value = 0x63
    
    -- Encode
    local code = getgamegeniecode(address, value)
    
    -- Decode
    local decoded = decodegamegenie(code)
    
    -- Verify round-trip
    if decoded.address == address and decoded.value == value then
        drawtext(4, 4, "Round-trip: PASS", 0x2E)
    else
        drawtext(4, 4, "Round-trip: FAIL", 0x2D)
    end
    
    drawtext(4, 14, "Code: " .. code, 0x20)
    drawtext(4, 24, string.format("Decoded: 0x%04X -> 0x%02X", decoded.address, decoded.value), 0x29)
end
```

**Example: Decode Code with Compare Value:**
```lua
function gui()
    local code = "APZLGITY"  -- 8-character code with compare
    local result = decodegamegenie(code)
    
    print("=== Decoded Game Genie Code ===")
    print("Code: " .. code)
    print("Address: 0x" .. string.format("%04X", result.address))
    print("Value: 0x" .. string.format("%02X", result.value))
    
    if result.compare then
        print("Compare: 0x" .. string.format("%02X", result.compare))
        print("Type: Conditional (only applies when current value = compare)")
    else
        print("Compare: nil")
        print("Type: Unconditional")
    end
end
```

**Example: Parse Multiple Codes:**
```lua
function gui()
    local codes = {"LTLZPA", "NNTIGA", "APZLGITY"}
    
    for i = 1, #codes do
        local result = decodegamegenie(codes[i])
        print(string.format("Code %d: %s", i, codes[i]))
        print(string.format("  Address: 0x%04X", result.address))
        print(string.format("  Value: 0x%02X", result.value))
        if result.compare then
            print(string.format("  Compare: 0x%02X", result.compare))
        end
        print("")
    end
end
```

**Example: Validate and Display Code Information:**
```lua
function gui()
    local code = "LTLZPA"
    
    -- Check code length
    if string.len(code) == 6 or string.len(code) == 8 then
        local result = decodegamegenie(code)
        
        drawtext(4, 4, "Code: " .. code, 0x20)
        drawtext(4, 14, string.format("Address: 0x%04X", result.address), 0x29)
        drawtext(4, 24, string.format("Value: 0x%02X", result.value), 0x29)
        
        if result.compare then
            drawtext(4, 34, string.format("Compare: 0x%02X", result.compare), 0x29)
            drawtext(4, 44, "Type: Conditional", 0x37)
        else
            drawtext(4, 34, "Type: Unconditional", 0x2E)
        end
    else
        drawtext(4, 4, "Invalid code length", 0x2D)
    end
end
```

**Example: Convert Game Genie Code to Cheat Format:**
```lua
function gui()
    local code = "LTLZPA"
    local decoded = decodegamegenie(code)
    
    -- Convert to cheat format
    local cheatFormat
    if decoded.compare then
        cheatFormat = string.format("0x%04X:0x%02X:0x%02X", 
                                    decoded.address, decoded.value, decoded.compare)
    else
        cheatFormat = string.format("0x%04X:0x%02X", 
                                    decoded.address, decoded.value)
    end
    
    print("Game Genie Code: " .. code)
    print("Cheat Format: " .. cheatFormat)
    
    drawtext(4, 4, "Code: " .. code, 0x20)
    drawtext(4, 14, "Cheat: " .. cheatFormat, 0x29)
end
```

## See Also

- [Memory Functions](Memory-Functions) - Functions for reading and writing ROM/RAM memory
- [State Management Functions](State-Management-Functions) - Functions for save/load states
- [Monitoring Functions](Monitoring-Functions) - Functions for frame counting and timing
- [Home](Home) - Return to the main wiki page