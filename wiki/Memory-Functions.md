# Memory Functions

Complete reference for all memory reading and writing functions available in the Lua API. These functions allow you to read from and write to NES memory, enabling HUD overlays, cheats, memory analysis, and game state manipulation.

## Memory Reading Functions

Functions for reading from NES memory. Use these to monitor game state, create HUD overlays, or analyze game data.

### `readbyte`

**Signature:** `readbyte(address)`
Reads a single byte (8-bit value) from the NES memory address space.

**Parameters:**
- `address` (integer): Memory address to read from. Valid range is 0x0000-0xFFFF (NES 16-bit address space).

**Returns:**
- (integer): The byte value at the specified address (0-255).

**Notes:**
- Reads from the full NES address space, including:
  - **RAM** (0x0000-0x1FFF): Work RAM (mirrored)
  - **PPU Registers** (0x2000-0x3FFF): PPU I/O registers (mirrored)
  - **APU and I/O** (0x4000-0x401F): Audio processing unit and I/O registers
  - **Expansion ROM** (0x4020-0x5FFF): Expansion area
  - **Cartridge RAM** (0x6000-0x7FFF): Save RAM
  - **Cartridge ROM** (0x8000-0xFFFF): Program ROM (PRG)
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly for different mappers and regions.
- Address validation: Values outside 0x0000-0xFFFF will return a Lua error.
- **Game-specific addresses:** Memory addresses vary by game, ROM version, and region (US/PAL/JAP). You may need to find the correct addresses for your specific ROM version.
- **Value encoding:** Some games store values in non-obvious formats. For example, Super Mario Bros 1 stores lives at 0x075A as (displayed_lives - 1), so reading 0x02 means 3 lives displayed. Always verify the encoding by comparing memory values to on-screen displays.
- **Common uses:** Reading game state variables like health, score, lives, coins, level, player position, etc.
- Useful for creating HUD overlays that display game information in real-time.
- For reading 16-bit values, use `readword()`. For reading multiple bytes, use `readbytes()`.

**Example:**
```lua
-- Read from RAM (always accessible)
local ramValue = readbyte(0x0000)
drawtext(4, 4, string.format("RAM[0x0000] = %d", ramValue), 0x20)

-- Super Mario Bros 1 example
-- Note: SMB1 stores lives as (displayed_lives - 1) at 0x075A
-- New game = 0x02 (which displays as "Ã—3"), so add 1 to get displayed value
local livesRaw = readbyte(0x075A)
local lives = livesRaw + 1  -- Convert to displayed value
local coins = readbyte(0x075E)
local worldLevel = readbyte(0x075F)

-- Display game info
drawtext(4, 12, string.format("Lives: %d", lives), 0x20)
drawtext(4, 20, string.format("Coins: %d", coins), 0x37)

-- Decode world/level (bits 4-7 = world, bits 0-3 = level)
local world = (worldLevel >> 4) + 1
local level = (worldLevel & 0x0F) + 1
drawtext(4, 28, string.format("World %d-%d", world, level), 0x39)

-- Read score (multi-byte value)
local scoreHigh = readbyte(0x07DE)  -- Tens of thousands
local scoreMid = readbyte(0x07DF)    -- Thousands
local scoreLow = readbyte(0x07E0)    -- Hundreds
local score = scoreHigh * 10000 + scoreMid * 100 + scoreLow
drawtext(4, 36, string.format("Score: %05d", score), 0x29)

-- Health bar example (game-specific address)
local health = readbyte(0x006A)  -- Example address
local maxHealth = 100
local barWidth = 80
local barHeight = 8
local barX = 10
local barY = 100

-- Draw health bar background
fillrect(barX, barY, barWidth, barHeight, 0x16)  -- Red / orange-red background

-- Draw health bar fill
local healthPercent = health / maxHealth
if healthPercent > 0 then
    fillrect(barX, barY, math.floor(barWidth * healthPercent), barHeight, 0x28)  -- Yellow fill
end

drawtext(barX, barY + 10, string.format("HP: %d/%d", health, maxHealth), 0x20)
```

### `readword`

**Signature:** `readword(address)`
Reads a 16-bit value (word) from consecutive memory addresses in little-endian format.

**Parameters:**
- `address` (integer): Starting memory address to read from. Valid range is 0x0000-0xFFFF (NES 16-bit address space).

**Returns:**
- (integer): The 16-bit value read from `address` and `address + 1` (0-65535).

**Notes:**
- Reads two consecutive bytes and combines them in **little-endian format** (standard for NES/6502):
  - Low byte (bits 0-7) is read from `address`
  - High byte (bits 8-15) is read from `address + 1`
  - Combined value = low + (high Ã— 256)
- For example, if address `0x0050` contains `0x34` and `0x0051` contains `0x12`, `readword(0x0050)` returns `0x1234` (0x34 + 0x12 * 256).
- Address validation: Values outside 0x0000-0xFFFF will return a Lua error.
- **Address wrapping:** If `address + 1` exceeds 0xFFFF, only the low byte is read and the high byte is 0.
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly.
- More efficient than calling `readbyte()` twice and manually combining the values.
- Useful for reading 16-bit game values like:
  - Scores stored as 16-bit values
  - Timers stored as 16-bit values
  - Coordinates stored as 16-bit values
  - Any game data that requires two consecutive bytes

**Example:**
```lua
-- Read a 16-bit value from RAM
local value = readword(0x0100)
drawtext(4, 4, string.format("Value at 0x0100: %d (0x%04X)", value, value), 0x20)

-- Compare with manual read
local low = readbyte(0x0100)
local high = readbyte(0x0101)
local manualValue = low + (high * 256)
-- value and manualValue should be the same

-- Read a 16-bit timer
local timer = readword(0x0400)
drawtext(4, 12, string.format("Timer: %d seconds", timer), 0x39)

-- Read player position (if stored as 16-bit)
local playerX = readword(0x0500)
drawtext(4, 20, string.format("Player X: %d", playerX), 0x20)

-- Display in hex format
local hexValue = readword(0x0600)
drawtext(4, 28, string.format("0x0600 = 0x%04X (%d)", hexValue, hexValue), 0x37)
```

### `readbytes`

**Signature:** `readbytes(address, count)`
Reads multiple consecutive bytes from memory and returns them as a Lua table.

**Parameters:**
- `address` (integer): Starting memory address to read from. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `count` (integer): Number of bytes to read. Valid range is 1-256.

**Returns:**
- (table): A Lua table containing the byte values. Table is 1-indexed (Lua standard), so `result[1]` is the first byte, `result[2]` is the second byte, etc.

**Notes:**
- Reads bytes sequentially starting from `address`:
  - `result[1]` = value at `address`
  - `result[2]` = value at `address + 1`
  - `result[3]` = value at `address + 2`
  - And so on...
- Address validation: Starting address must be in range 0x0000-0xFFFF.
- Count validation: Count must be 1-256. Values outside this range will return a Lua error.
- **Address wrapping:** If reading bytes would extend past 0xFFFF, the function will only read up to the address space boundary.
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly.
- More efficient than calling `readbyte()` multiple times in a loop.
- The returned table is standard Lua table, so you can use `#result` to get the count, iterate with `ipairs()`, etc.
- Useful for:
  - Reading multi-byte values (scores, timers, coordinates)
  - Analyzing memory regions
  - Copying memory blocks
  - Reading structured game data that spans multiple bytes

**Example:**
```lua
-- Read 3 bytes starting at address 0x0060
local bytes = readbytes(0x0060, 3)
drawtext(4, 4, string.format("Bytes: %d, %d, %d", bytes[1], bytes[2], bytes[3]), 0x20)

-- Super Mario Bros 1 - Read score (3 bytes)
local scoreBytes = readbytes(0x07DE, 3)
local score = scoreBytes[1] * 10000 + scoreBytes[2] * 100 + scoreBytes[3]
drawtext(4, 12, string.format("Score: %05d", score), 0x29)

-- Read and display multiple bytes
local data = readbytes(0x0100, 8)
for i = 1, #data do
  drawtext(4, 20 + (i * 8), string.format("0x%04X = %d (0x%02X)", 0x0100 + i - 1, data[i], data[i]), 0x20)
end

-- Read timer bytes (3 bytes: hundreds, tens, ones)
local timerBytes = readbytes(0x07F8, 3)
local timer = timerBytes[1] * 100 + timerBytes[2] * 10 + timerBytes[3]
drawtext(4, 84, string.format("Timer: %03d", timer), 0x26)

-- Iterate through read bytes
local memoryBlock = readbytes(0x0200, 16)
for i, value in ipairs(memoryBlock) do
  if value ~= 0 then  -- Only show non-zero values
    drawtext(4, 92 + (i * 8), string.format("[%d] = %d", i, value), 0x39)
  end
end

-- Search for a specific value in memory
local searchArea = readbytes(0x0000, 256)
for i = 1, #searchArea do
  if searchArea[i] == 99 then
    drawtext(4, 100, string.format("Found 99 at address 0x%04X", 0x0000 + i - 1), 0x37)
    break
  end
end
```

### `readram`

**Signature:** `readram(startAddr, count)`
Convenience function to read specifically from RAM (0x0000-0x1FFF). Returns the same format as `readbytes()` but with RAM-specific validation to ensure you're only reading from the RAM region.

**Parameters:**
- `startAddr` (integer): Starting memory address in RAM. Valid range is 0x0000-0x1FFF (NES RAM region).
- `count` (integer): Number of bytes to read. Valid range is 1-256.

**Returns:**
- (table): A Lua table containing the byte values (same format as `readbytes()`). Table is 1-indexed, so `result[1]` is the first byte, `result[2]` is the second byte, etc.

**Notes:**
- Reads bytes sequentially from RAM starting at `startAddr`.
- **RAM-specific validation:** Starting address must be in RAM range (0x0000-0x1FFF). Attempting to read from addresses outside RAM will return an error.
- Count validation: Must be 1-256. Values outside this range will return a Lua error.
- **RAM boundary protection:** If reading would extend past 0x1FFF (end of RAM), the count is automatically adjusted to stop at the RAM boundary.
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly.
- **Same return format as `readbytes()`:** Returns a 1-indexed Lua table, so you can use it exactly like `readbytes()`.
- **Convenience function:** Provides explicit RAM-only access, making it clear in your code that you're reading from RAM without worrying about other memory regions.
- Useful for:
  - Explicitly reading RAM without accidentally accessing other regions
  - Making code intent clearer (RAM-only operations)
  - Validating that addresses are in RAM range
  - Reading game data structures that are guaranteed to be in RAM

**Example:**
```lua
-- Read from RAM (SMB1 score is in RAM at 0x07DE)
local scoreBytes = readram(0x07DE, 3)
local score = scoreBytes[1] * 10000 + scoreBytes[2] * 100 + scoreBytes[3]

-- Read start of RAM
local ramStart = readram(0x0000, 256)  -- First 256 bytes of RAM

-- Read end of RAM
local ramEnd = readram(0x1F00, 256)  -- Last 256 bytes of RAM

-- This will error if address is outside RAM:
-- readram(0x8000, 1)  -- Error: must be in RAM range 0x0000-0x1FFF

-- Super Mario Bros 1 - Read multiple RAM values
function script()
    local ramData = {
        score = readram(0x07DE, 3),
        lives = readram(0x075A, 1),
        coins = readram(0x075E, 1)
    }
    
    -- Display RAM values
    drawtext(4, 4, string.format("Score: %d,%d,%d", ramData.score[1], ramData.score[2], ramData.score[3]), 0x20)
    drawtext(4, 12, string.format("Lives: %d", ramData.lives[1]), 0x20)
    drawtext(4, 20, string.format("Coins: %d", ramData.coins[1]), 0x20)
end

-- Read entire RAM block safely
local fullRam = readram(0x0000, 0x2000)  -- Reads all 8192 bytes of RAM
```

**When to use `readram()` vs `readbytes()`:**
- Use `readram()` when you want to **explicitly read from RAM only** and ensure addresses are validated as RAM addresses (0x0000-0x1FFF)
- Use `readbytes()` when you need to **read from any memory region** (RAM, PPU, APU, ROM, etc.) across the full address space (0x0000-0xFFFF)
- Both functions return the same format (1-indexed Lua table), so they're functionally equivalent for RAM addresses, but `readram()` provides additional validation

### `getmemorytype`

**Signature:** `getmemorytype(address)`
Returns the type of memory at a given address. Useful for validating addresses and understanding the NES memory layout.

**Parameters:**
- `address` (integer): Memory address to check. Valid range is 0x0000-0xFFFF (NES 16-bit address space).

**Returns:**
- (string): Memory type identifier. Returns one of:
  - `"RAM"` - Random Access Memory (0x0000-0x1FFF)
  - `"PPU"` - Picture Processing Unit registers (0x2000-0x3FFF, mirrored)
  - `"APU"` - Audio Processing Unit registers (0x4000-0x401F)
  - `"ROM"` - Program ROM (0x8000-0xFFFF)
  - `"UNKNOWN"` - Expansion ROM, Save RAM, or mapper-specific regions (0x4020-0x7FFF)

**Notes:**
- Determines memory type based on NES memory map address ranges.
- Address validation: Address must be in range 0x0000-0xFFFF.
- **Memory regions:**
  - **RAM (0x0000-0x1FFF):** Main system RAM, game variables, stack
  - **PPU (0x2000-0x3FFF):** PPU registers and mirrors (0x2000-0x2007 repeated)
  - **APU (0x4000-0x401F):** Audio processing unit and I/O registers
  - **UNKNOWN (0x4020-0x7FFF):** Expansion ROM, Save RAM, or mapper-specific areas
  - **ROM (0x8000-0xFFFF):** Program ROM (cartridge code)
- Useful for:
  - Validating addresses before operations
  - Understanding memory layout
  - Debugging memory access issues
  - Conditional logic based on memory type
  - Documentation and memory mapping tools

**Example:**
```lua
-- Check memory type of common addresses
local scoreType = getmemorytype(0x07DE)  -- Returns "RAM"
local romType = getmemorytype(0x8000)    -- Returns "ROM"
local ppuType = getmemorytype(0x2000)    -- Returns "PPU"
local apuType = getmemorytype(0x4000)    -- Returns "APU"
local unknownType = getmemorytype(0x6000) -- Returns "UNKNOWN"

-- Validate address before writing
local addr = 0x07DE
if getmemorytype(addr) == "RAM" then
    writebyte(addr, 99)  -- Safe to write to RAM
    print("Wrote to RAM")
else
    print("Cannot write to " .. getmemorytype(addr))
end

-- Check all memory regions
function script()
    local regions = {
        {0x0000, "Start of RAM"},
        {0x1FFF, "End of RAM"},
        {0x2000, "PPU registers"},
        {0x4000, "APU registers"},
        {0x6000, "Save RAM area"},
        {0x8000, "Program ROM start"},
        {0xFFFF, "End of ROM"}
    }
    
    for i, region in ipairs(regions) do
        local addr = region[1]
        local desc = region[2]
        local memType = getmemorytype(addr)
        print(string.format("0x%04X (%s): %s", addr, desc, memType))
    end
end

-- Conditional logic based on memory type
local addr = 0x07DE
local memType = getmemorytype(addr)
if memType == "RAM" then
    -- Safe to read/write
    local value = readbyte(addr)
    writebyte(addr, value + 1)
elseif memType == "ROM" then
    -- Read-only, or mapper-specific writes
    print("ROM address, read-only")
elseif memType == "PPU" or memType == "APU" then
    -- Special hardware registers
    print("Hardware register, use with caution")
else
    print("Unknown memory region")
end
```

### `ismemorywritable`

**Signature:** `ismemorywritable(address)`
Checks if a memory address is writable. Returns `true` if the address can be written to, `false` otherwise.

**Parameters:**
- `address` (integer): Memory address to check. Valid range is 0x0000-0xFFFF (NES 16-bit address space).

**Returns:**
- (boolean): `true` if the address is writable, `false` if it is read-only or unknown.

**Notes:**
- Determines writability based on NES memory map address ranges.
- Address validation: Address must be in range 0x0000-0xFFFF.
- **Writable regions:**
  - **RAM (0x0000-0x1FFF):** Main system RAM - always writable
  - **PPU (0x2000-0x3FFF):** PPU registers - writable (hardware registers)
  - **APU (0x4000-0x401F):** APU and I/O registers - writable (hardware registers)
- **Read-only regions:**
  - **UNKNOWN (0x4020-0x7FFF):** Expansion ROM, Save RAM, or mapper-specific areas - typically not writable
  - **ROM (0x8000-0xFFFF):** Program ROM - read-only (some mappers support ROM writes via `writeprg()`)
- **Use cases:**
  - Validating addresses before write operations
  - Preventing accidental writes to read-only memory
  - Conditional write logic
  - Memory safety checks
  - Debugging write operations
- **Note:** Even if `ismemorywritable()` returns `true`, writing to PPU/APU registers may have side effects. Use with caution for hardware registers.

**Example:**
```lua
-- Check if addresses are writable
local ramWritable = ismemorywritable(0x07DE)  -- Returns true (RAM)
local romWritable = ismemorywritable(0x8000)   -- Returns false (ROM)
local ppuWritable = ismemorywritable(0x2000)   -- Returns true (PPU register)
local apuWritable = ismemorywritable(0x4000)   -- Returns true (APU register)
local unknownWritable = ismemorywritable(0x6000) -- Returns false (UNKNOWN)

-- Validate before writing
local addr = 0x07DE
if ismemorywritable(addr) then
    local before = readbyte(addr)
    writebyte(addr, 99)
    local after = readbyte(addr)
    print(string.format("Write test: %d->%d", before, after))
else
    print("Address is not writable")
end

-- Check multiple addresses
function script()
    local addresses = {
        {0x0000, "RAM start"},
        {0x07DE, "RAM (score)"},
        {0x2000, "PPU start"},
        {0x4000, "APU start"},
        {0x8000, "ROM start"},
        {0xFFFF, "ROM end"}
    }
    
    for i, entry in ipairs(addresses) do
        local addr = entry[1]
        local desc = entry[2]
        local writable = ismemorywritable(addr)
        local status = writable and "WRITABLE" or "READ-ONLY"
        print(string.format("0x%04X (%s): %s", addr, desc, status))
    end
end

-- Safe write function with validation
function safeWrite(address, value)
    if ismemorywritable(address) then
        writebyte(address, value)
        return true
    else
        print(string.format("Cannot write to 0x%04X (not writable)", address))
        return false
    end
end

-- Conditional write based on writability
local addr = 0x07DE
if ismemorywritable(addr) then
    -- Safe to write
    writebyte(addr, 99)
else
    -- Use alternative method or skip
    print("Address is read-only, skipping write")
end

-- Compare with getmemorytype for validation
local addr = 0x07DE
local memType = getmemorytype(addr)
local writable = ismemorywritable(addr)

if memType == "RAM" and writable then
    -- Definitely safe to write to RAM
    writebyte(addr, 99)
elseif memType == "PPU" and writable then
    -- PPU register - writable but may have side effects
    print("PPU register - use with caution")
else
    -- Not writable or unknown
    print("Cannot write to this address")
end
```

### `scanbyte`

**Signature:** `scanbyte(value, startAddr, endAddr)`
Searches for a specific byte value within an address range and returns all matching addresses.

**Parameters:**
- `value` (integer): Target byte value to search for (0â€“255).
- `startAddr` (integer): Start address (inclusive), 0x0000â€“0xFFFF.
- `endAddr` (integer): End address (inclusive), 0x0000â€“0xFFFF. Order is flexible; if `startAddr > endAddr`, they are swapped.

**Returns:**
- (table): A 1-indexed Lua table of addresses where the byte equals `value`.

**Notes:**
- Searches using the emulator's memory mapping (`ARead`), so it works across RAM/PPU/APU/cartridge spaces depending on the range.
- Value is validated (0â€“255). Addresses are clamped to the NES 16-bit address space.
- Stops at 0xFFFF; does not wrap.

**Example:**
```lua
-- Find all RAM addresses with value 3
local hits = scanbyte(3, 0x0000, 0x07FF)
for i, addr in ipairs(hits) do
  drawtext(4, 4 + i * 8, string.format("0x%04X", addr), 0x20)
end

-- Search whole space for a flag value (may be large, use narrow ranges for speed)
local flags = scanbyte(1, 0x0000, 0xFFFF)
```

### `scanword`

**Signature:** `scanword(value, startAddr, endAddr)`
Searches for a specific 16-bit value (little-endian) within an address range and returns all matching addresses.

**Parameters:**
- `value` (integer): Target 16-bit value to search for (0â€“65535). Compared as little-endian: low byte at `addr`, high byte at `addr+1`.
- `startAddr` (integer): Start address (inclusive), 0x0000â€“0xFFFF.
- `endAddr` (integer): End address (inclusive), 0x0000â€“0xFFFF. Order is flexible; if `startAddr > endAddr`, they are swapped.

**Returns:**
- (table): A 1-indexed Lua table of addresses where the 16-bit word starting at that address equals `value`.

**Notes:**
- Little-endian match: `value & 0xFF` must equal byte at `addr`, and `(value >> 8) & 0xFF` must equal byte at `addr+1`.
- Safe at top of space: when `addr == 0xFFFF`, high byte is treated as 0.
- Uses emulator memory mapping (`ARead`).
- For single-byte flags (e.g., SMB1 power-up at 0x0756), use `scanbyte` instead.

**Examples:**
```lua
-- Find 16-bit value 0x1234 in RAM
local hits = scanword(0x1234, 0x0000, 0x07FF)
for i = 1, math.min(#hits, 10) do
  print(string.format("[%02d] 0x%04X", i, hits[i]))
end

-- Check if any address currently holds 600 (e.g., a timer)
local timerHits = scanword(600, 0x0000, 0xFFFF)
print("timer matches:", #timerHits)
```

### `scanbytes`

**Signature:** `scanbytes(pattern, startAddr, endAddr)`
Searches for a sequence of byte values within an address range and returns all starting addresses where the pattern matches.

**Parameters:**
- `pattern`: Can be either:
  - (table): A Lua table containing byte values `{value1, value2, ...}` (1-indexed)
  - (varargs): Individual byte values as arguments `b1, b2, ..., startAddr, endAddr`
- `startAddr` (integer): Start address (inclusive), 0x0000â€“0xFFFF.
- `endAddr` (integer): End address (inclusive), 0x0000â€“0xFFFF. Order is flexible; if `startAddr > endAddr`, they are swapped.

**Returns:**
- (table): A 1-indexed Lua table of addresses where the pattern starts (i.e., where all pattern bytes match consecutively).

**Notes:**
- Pattern length is limited to 256 bytes maximum.
- All pattern values must be in range 0â€“255.
- When using table form: `scanbytes({0xDE, 0xAD}, 0x0000, 0xFFFF)`
- When using varargs form: `scanbytes(0xDE, 0xAD, 0x0000, 0xFFFF)` (last two args are addresses)
- Uses emulator memory mapping (`ARead`), so it works across RAM/PPU/APU/cartridge spaces.
- Addresses are validated to the NES 16-bit address space.
- Pattern matching stops at address boundaries; no wrapping occurs.

**Examples:**
```lua
-- Search for a 4-byte signature using table pattern
local pattern = {0xDE, 0xAD, 0xBE, 0xEF}
local hits = scanbytes(pattern, 0x0000, 0x07FF)
for i, addr in ipairs(hits) do
  print(string.format("Found pattern at 0x%04X", addr))
end

-- Search using varargs (same result as above)
local hits2 = scanbytes(0xDE, 0xAD, 0xBE, 0xEF, 0x0000, 0x07FF)

-- Find a 2-byte sequence in RAM
local matches = scanbytes({0x03, 0x00}, 0x0200, 0x07FF)
print("Found", #matches, "matches")

-- Search for a longer data structure (e.g., 8 bytes)
local structPattern = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}
local found = scanbytes(structPattern, 0x0000, 0xFFFF)
```

### `findpattern`

**Signature:** `findpattern(pattern, startAddr, endAddr, [mask])`
Searches for a byte pattern with optional wildcard support within an address range and returns all starting addresses where the pattern matches.

**Parameters:**
- `pattern` (table): A Lua table containing byte values `{value1, value2, ...}` (1-indexed). All values must be in range 0â€“255.
- `startAddr` (integer): Start address (inclusive), 0x0000â€“0xFFFF.
- `endAddr` (integer): End address (inclusive), 0x0000â€“0xFFFF. Order is flexible; if `startAddr > endAddr`, they are swapped.
- `mask` (table, optional): A Lua table of the same length as `pattern` where each value indicates whether that position should be matched (non-zero) or treated as a wildcard (0). If `nil` or omitted, all bytes must match exactly (equivalent to `scanbytes`).

**Returns:**
- (table): A 1-indexed Lua table of addresses where the pattern starts (i.e., where pattern bytes match consecutively, with wildcard positions allowing any byte value).

**Notes:**
- Pattern length is limited to 256 bytes maximum.
- All pattern values must be in range 0â€“255.
- Mask table length must match pattern table length if provided.
- Mask values: `0` = wildcard (any byte matches), non-zero = must match pattern byte.
- Uses emulator memory mapping (`ARead`), so it works across RAM/PPU/APU/cartridge spaces.
- Addresses are validated to the NES 16-bit address space.
- Pattern matching stops at address boundaries; no wrapping occurs.
- When no mask is provided, `findpattern` behaves exactly like `scanbytes` with a table pattern.
- Useful for finding code signatures, data structures with variable parts, or patterns where some bytes are unknown.

**Examples:**
```lua
-- Find pattern with wildcard in middle: 0x20, any byte, 0x00
local pattern = {0x20, 0x00, 0x00}  -- The middle byte value in pattern doesn't matter
local mask = {1, 0, 1}  -- 1 = match, 0 = wildcard (ignore this byte)
local results = findpattern(pattern, 0x0000, 0xFFFF, mask)
for i, addr in ipairs(results) do
  print(string.format("Found pattern at 0x%04X", addr))
end

-- Find pattern with multiple wildcards: first and last must match, middle two can be anything
local pattern2 = {0xAA, 0x00, 0x00, 0xCC}
local mask2 = {1, 0, 0, 1}  -- Match first and last, wildcard middle two
local results2 = findpattern(pattern2, 0x0200, 0x07FF, mask2)

-- Find exact pattern (no mask, equivalent to scanbytes)
local exactPattern = {0xDE, 0xAD, 0xBE, 0xEF}
local exactResults = findpattern(exactPattern, 0x0000, 0xFFFF)
-- Same as: scanbytes(exactPattern, 0x0000, 0xFFFF)

-- Find code signature with variable jump address
-- Pattern: 0x20 (JSR opcode), then any two bytes (address), then 0x60 (RTS opcode)
local codePattern = {0x20, 0x00, 0x00, 0x60}
local codeMask = {1, 0, 0, 1}  -- Match opcodes, wildcard address bytes
local codeHits = findpattern(codePattern, 0x8000, 0xFFFF, codeMask)
print("Found", #codeHits, "JSR/RTS patterns")
```

### `scanchanged`

**Signature:** `scanchanged(oldSnapshot, newSnapshot, startAddr)`
Compares two memory snapshots (created with `readbytes()` or `backupbytes()`) and returns a table of addresses where values changed, along with their new values.

**Parameters:**
- `oldSnapshot` (table): A Lua table containing byte values from the first snapshot (1-indexed, same format as `readbytes()`).
- `newSnapshot` (table): A Lua table containing byte values from the second snapshot (1-indexed, same format as `readbytes()`).
- `startAddr` (integer): The starting address where the snapshots begin. Valid range is 0x0000â€“0xFFFF.

**Returns:**
- (table): An address-indexed Lua table where keys are addresses (as integers) and values are the new byte values at those addresses. Only addresses where values changed are included in the result.

**Notes:**
- Both snapshots must be the same length (same number of bytes).
- Snapshots are 1-indexed tables where `snapshot[i]` corresponds to the byte at address `startAddr + (i - 1)`.
- All snapshot values must be in range 0â€“255.
- The result table only contains entries for addresses where values changed between snapshots.
- Use `pairs()` to iterate through the result table (address-indexed, not array-indexed).
- Useful for detecting what changed after a game action, analyzing memory modifications, or monitoring specific memory regions.
- Can be used with snapshots from `readbytes()` or `backupbytes()`.

**Examples:**
```lua
-- Detect what changed after performing an action
local before = readbytes(0x0200, 10)
-- ... perform some game action ...
local after = readbytes(0x0200, 10)
local changes = scanchanged(before, after, 0x0200)

-- Display all changed addresses
for addr, newValue in pairs(changes) do
  print(string.format("Address 0x%04X changed to 0x%02X", addr, newValue))
end

-- Monitor score changes (SMB1 score is 3 bytes at 0x07DE)
local scoreBefore = readbytes(0x07DE, 3)
-- ... wait for score to change ...
local scoreAfter = readbytes(0x07DE, 3)
local scoreChanges = scanchanged(scoreBefore, scoreAfter, 0x07DE)

if next(scoreChanges) then  -- Check if table is not empty
  print("Score changed!")
  for addr, value in pairs(scoreChanges) do
    print(string.format("  Byte at 0x%04X is now 0x%02X", addr, value))
  end
end

-- Compare snapshots with old values
local oldSnapshot = backupbytes(0x0300, 5)
-- ... modify memory ...
local newSnapshot = readbytes(0x0300, 5)
local diff = scanchanged(oldSnapshot, newSnapshot, 0x0300)

-- Get both old and new values for changed addresses
for addr, newValue in pairs(diff) do
  local index = (addr - 0x0300) + 1  -- Convert address to snapshot index
  local oldValue = oldSnapshot[index]
  print(string.format("0x%04X: 0x%02X -> 0x%02X", addr, oldValue, newValue))
end
```

### `watchbyte`

**Signature:** `watchbyte(address)`
Sets up a watchpoint for a memory address. When the watched address changes, the `onwatch()` callback function (if defined) will be called automatically.

**Parameters:**
- `address` (integer): Memory address to watch. Valid range is 0x0000â€“0xFFFF.

**Returns:**
- Nothing

**Notes:**
- The current value at the address is stored as the baseline when `watchbyte()` is called.
- Changes are detected every frame by checking watched addresses.
- If an `onwatch(address, oldValue, newValue)` function is defined in your Lua script, it will be called automatically when a watched address changes.
- Multiple addresses can be watched simultaneously.
- Watchpoints are cleared when Lua stops or is reloaded.
- Uses emulator memory mapping (`ARead`), so it works across RAM/PPU/APU/cartridge spaces.
- Useful for debugging, detecting specific memory changes, monitoring game state variables, or triggering actions when values change.

**Callback Function:**
If you define an `onwatch(address, oldValue, newValue)` function in your script, it will be called automatically when any watched address changes:
- `address`: The address that changed (integer)
- `oldValue`: The previous byte value (0â€“255)
- `newValue`: The new byte value (0â€“255)

**Examples:**
```lua
-- Define callback function to handle watch events
function onwatch(address, oldValue, newValue)
    print(string.format("Address 0x%04X changed: 0x%02X -> 0x%02X (%d -> %d)", 
          address, oldValue, newValue, oldValue, newValue))
end

-- Watch SMB1 lives address
watchbyte(0x075A)

-- Watch multiple addresses
watchbyte(0x075E)  -- Coins
watchbyte(0x07DE)  -- Score byte 1
watchbyte(0x07DF)  -- Score byte 2
watchbyte(0x07E0)  -- Score byte 3

-- Monitor game state changes
function script()
    -- Watch addresses are checked automatically each frame
    -- onwatch() will be called if any watched address changes
end
```

**Advanced Example - SMB1 Watch System:**
```lua
local changeLog = {}
local maxLogEntries = 10

function onwatch(address, oldValue, newValue)
    -- Log the change
    table.insert(changeLog, {
        addr = address,
        old = oldValue,
        new = newValue,
        time = os.clock()
    })
    if #changeLog > maxLogEntries then
        table.remove(changeLog, 1)
    end
    
    -- Handle specific addresses
    if address == 0x075A then
        local oldLives = oldValue + 1  -- SMB1 stores lives as (displayed - 1)
        local newLives = newValue + 1
        print(string.format("Lives changed: %d -> %d", oldLives, newLives))
    elseif address == 0x075E then
        print(string.format("Coins changed: %d -> %d", oldValue, newValue))
    end
end

function script()
    -- Setup watches on first run
    if not watchesSetup then
        watchbyte(0x075A)  -- Lives
        watchbyte(0x075E)  -- Coins
        watchbyte(0x07DE)  -- Score bytes
        watchbyte(0x07DF)
        watchbyte(0x07E0)
        watchesSetup = true
    end
    
    -- Display current values and change log
    -- ... (your display code)
end
```

### `unwatchbyte`

**Signature:** `unwatchbyte(address)`
Removes a watchpoint from a memory address. The address will no longer be monitored for changes.

**Parameters:**
- `address` (integer): Memory address to stop watching. Valid range is 0x0000â€“0xFFFF.

**Returns:**
- Nothing

**Notes:**
- If the address is not currently being watched, this function does nothing (no error).
- Removing a watchpoint does not affect other watched addresses.
- Useful for temporarily disabling monitoring or cleaning up watchpoints when no longer needed.

**Examples:**
```lua
-- Watch an address
watchbyte(0x075A)

-- Later, stop watching it
unwatchbyte(0x075A)

-- Watch multiple addresses, then remove specific ones
watchbyte(0x075A)
watchbyte(0x075E)
watchbyte(0x07DE)

-- Remove only the lives watchpoint
unwatchbyte(0x075A)  -- Coins and score still being watched

-- Conditionally unwatch
function script()
    if someCondition then
        unwatchbyte(0x075A)  -- Stop watching when condition is met
    end
end
```

### `getmemorysnapshot`

**Signature:** `getmemorysnapshot(startAddr, endAddr)`
Creates a complete snapshot of a memory region and returns it as an address-indexed table. Each address in the range is stored as a key with its byte value.

**Parameters:**
- `startAddr` (integer): Start address (inclusive), 0x0000â€“0xFFFF.
- `endAddr` (integer): End address (inclusive), 0x0000â€“0xFFFF. Order is flexible; if `startAddr > endAddr`, they are swapped.

**Returns:**
- (table): An address-indexed Lua table where keys are addresses (as integers) and values are byte values (0â€“255) at those addresses. Use `pairs()` to iterate through the result.

**Notes:**
- The result table is address-indexed (not array-indexed), meaning you access values using `snapshot[address]` rather than `snapshot[index]`.
- Range is limited to 65536 bytes maximum (0x0000-0xFFFF) to prevent excessive memory usage.
- Uses emulator memory mapping (`ARead`), so it works across RAM/PPU/APU/cartridge spaces.
- Addresses are validated to the NES 16-bit address space.
- Useful for comparing memory states over time, debugging, creating memory dumps, or analyzing memory regions.
- Unlike `readbytes()` which returns an array, this returns an address-indexed table for direct address lookups.
- Unlike `backupbytes()` which returns an array, this allows you to access values by their actual addresses.

**Examples:**
```lua
-- Create snapshot of a RAM region
local snapshot = getmemorysnapshot(0x0200, 0x02FF)

-- Access specific addresses
local valueAt200 = snapshot[0x0200]
local valueAt250 = snapshot[0x0250]

-- Iterate through all addresses in snapshot
for addr, value in pairs(snapshot) do
    print(string.format("Address 0x%04X = 0x%02X (%d)", addr, value, value))
end

-- Compare memory states over time
local before = getmemorysnapshot(0x0300, 0x030F)
-- ... perform some action ...
local after = getmemorysnapshot(0x0300, 0x030F)

-- Find what changed
for addr, newValue in pairs(after) do
    local oldValue = before[addr]
    if oldValue ~= newValue then
        print(string.format("0x%04X changed: 0x%02X -> 0x%02X", addr, oldValue, newValue))
    end
end
```

**Advanced Example - Memory State Comparison:**
```lua
-- Take snapshot of entire RAM
local ramSnapshot1 = getmemorysnapshot(0x0000, 0x07FF)

-- ... game runs for a while ...

-- Take another snapshot
local ramSnapshot2 = getmemorysnapshot(0x0000, 0x07FF)

-- Compare and find all changed addresses
local changedAddresses = {}
for addr, newValue in pairs(ramSnapshot2) do
    local oldValue = ramSnapshot1[addr]
    if oldValue ~= newValue then
        table.insert(changedAddresses, {
            addr = addr,
            old = oldValue,
            new = newValue
        })
    end
end

print(string.format("Found %d changed addresses", #changedAddresses))
for i, change in ipairs(changedAddresses) do
    print(string.format("  0x%04X: 0x%02X -> 0x%02X", change.addr, change.old, change.new))
end
```

**Example - Snapshot Specific Game Values:**
```lua
-- SMB1: Snapshot game state
local gameState = getmemorysnapshot(0x075A, 0x07FF)

-- Access specific addresses
local livesRaw = gameState[0x075A]
local lives = livesRaw + 1  -- SMB1 stores lives as (displayed - 1)
local coins = gameState[0x075E]
local scoreHigh = gameState[0x07DE]
local scoreMid = gameState[0x07DF]
local scoreLow = gameState[0x07E0]

print(string.format("Lives: %d, Coins: %d", lives, coins))
print(string.format("Score: %d-%d-%d", scoreHigh, scoreMid, scoreLow))
```

### Memory Reading Function Comparison

| Function | Purpose | Data Size | Returns |
|----------|---------|-----------|---------|
| `readbyte(address)` | Read a single byte | 8-bit (0-255) | Integer |
| `readword(address)` | Read a 16-bit value | 16-bit (0-65535) | Integer (little-endian) |
| `readbytes(address, count)` | Read multiple bytes | 8-bit each (0-255) | Table of integers |
| `readram(startAddr, count)` | Read from RAM only | 8-bit each (0-255) | Table of integers |
| `getmemorytype(address)` | Get memory type | N/A | Returns string ("RAM", "PPU", "APU", "ROM", "UNKNOWN") |
| `ismemorywritable(address)` | Check if writable | N/A | Returns boolean (true if writable) |

**When to use each:**
- **`readbyte()`**: Single byte values (lives, coins, power-up state, flags, single-byte counters)
- **`readword()`**: 16-bit values (scores, timers, coordinates, counters stored as 16-bit)
- **`readbytes()`**: Multi-byte sequences (scores stored across 3+ bytes, arrays, buffers, memory analysis) - works across full address space
- **`readram()`**: Multi-byte sequences specifically from RAM (0x0000-0x1FFF) - explicit RAM-only access with validation
- **`getmemorytype()`**: Identifying memory type at an address (validating addresses, understanding memory layout, debugging)
- **`ismemorywritable()`**: Checking if an address is writable before write operations (validating addresses, preventing write errors, safety checks)

## Memory Writing Functions

Functions for writing to NES memory. Use these to modify game state, create cheats, or manipulate game data.

### Bit Operations

### `setbit`

**Signature:** `setbit(address, bit)`
Sets a specific bit (0â€“7) in the byte at `address`.

**Parameters:**
- `address` (integer): NES address, 0x0000â€“0xFFFF.
- `bit` (integer): Bit index to set, 0â€“7.

**Returns:** Nothing

**Notes:**
- Reads the current byte via the emulator mapping, sets `1 << bit`, and writes back.
- Writing to ROM addresses is typically ignored by the mapper.

**Examples:**
```lua
-- Set a status flag bit
setbit(0x0200, 3)

-- SMB1 (example): ensure a power-up bit is set
setbit(0x0756, 2)
```

### `clearbit`

**Signature:** `clearbit(address, bit)`
Clears a specific bit (0â€“7) in the byte at `address`.

**Parameters:**
- `address` (integer): NES address, 0x0000â€“0xFFFF.
- `bit` (integer): Bit index to clear, 0â€“7.

**Returns:** Nothing

**Notes:**
- Reads the current byte via the emulator mapping, clears `1 << bit`, and writes back.
- Writing to ROM addresses is typically ignored by the mapper.

**Examples:**
```lua
-- Clear a status flag bit
clearbit(0x0200, 3)

-- SMB1 (example): clear a power-up-related bit
clearbit(0x0756, 2)
```

### `togglebit`

**Signature:** `togglebit(address, bit)`
Toggles a specific bit (0â€“7) in the byte at `address`.

**Parameters:**
- `address` (integer): NES address, 0x0000â€“0xFFFF.
- `bit` (integer): Bit index to toggle, 0â€“7.

**Returns:** Nothing

**Notes:**
- Reads the current byte via the emulator mapping, flips `1 << bit` using XOR, and writes back.
- Writing to ROM addresses is typically ignored by the mapper.

**Examples:**
```lua
-- Toggle a status flag bit
togglebit(0x0200, 3)

-- SMB1 (example): toggle a power-up-related bit
togglebit(0x0756, 2)
```

### `testbit`

**Signature:** `testbit(address, bit)`
Tests whether a specific bit (0â€“7) is set in the byte at `address`.

**Parameters:**
- `address` (integer): NES address, 0x0000â€“0xFFFF.
- `bit` (integer): Bit index to test, 0â€“7.

**Returns:**
- (boolean): `true` if the bit is set, `false` if it is clear.

**Notes:**
- Reads the current byte via the emulator mapping and checks `(value & (1 << bit)) != 0`.
- Safe to call on any mapped address; ROM regions are readable but not writable.

**Examples:**
```lua
-- Poll a flag and conditionally act
if testbit(0x0756, 2) then
  -- bit is set
else
  -- bit is clear
end

-- Display a status indicator
local on = testbit(0x0200, 3)
drawtext(4, 4, on and "Flag: ON" or "Flag: OFF", 0x39)
```

### Byte and Word Writing

### `writebyte`

**Signature:** `writebyte(address, value)`
Writes a single byte (8-bit value) to the specified memory address.

**Parameters:**
- `address` (integer): Memory address to write to. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `value` (integer): Byte value to write. Valid range is 0-255.

**Returns:** Nothing

**Notes:**
- Writes to the full NES address space, including RAM, PPU registers, APU registers, and cartridge RAM.
- Uses FCEUX's memory mapping system (`BWrite`), which handles all memory mapping correctly for different mappers and regions.
- Address validation: Values outside 0x0000-0xFFFF will return a Lua error.
- Value validation: Values outside 0-255 will return a Lua error.
- **Writing to ROM:** Writing to cartridge ROM addresses (0x8000-0xFFFF) typically has no effect, as ROM is read-only. Most mappers will ignore these writes.
- **Immediate effect:** The write takes effect immediately. The game will see the new value on its next memory read from that address.
- **Multiple writes:** You can write to the same address multiple times in a single frame - the last write wins.
- Useful for creating cheats, modifying game state, debugging, or creating automated gameplay modifications.
- For writing 16-bit values, use `writeword()`. For writing multiple bytes, use `writebytes()`.

**Example:**
```lua
-- Write to RAM
writebyte(0x0000, 42)  -- Write value 42 to address 0x0000

-- Super Mario Bros 1 - Set lives to 99 (stored as 98)
writebyte(0x075A, 98)  -- Displays as "Ã—99" on screen

-- Set coins to 99
writebyte(0x075E, 99)

-- Set power-up state (0=Small, 1=Super, 2=Fire)
writebyte(0x0756, 2)  -- Always Fire Mario

-- Keep lives at 99 (write every frame)
function script()
  writebyte(0x075A, 98)
end

-- Conditional write (only if value is different)
function script()
  local current = readbyte(0x075A)
  if current < 98 then
    writebyte(0x075A, 98)  -- Restore to 99 lives if it dropped
  end
end
```

### `writeword`

**Signature:** `writeword(address, value)`
Writes a 16-bit value (word) to consecutive memory addresses in little-endian format (low byte first, high byte second).

**Parameters:**
- `address` (integer): Starting memory address to write to. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `value` (integer): 16-bit value to write. Valid range is 0-65535.

**Returns:** Nothing

**Notes:**
- Writes the value in **little-endian format** (standard for NES/6502):
  - Low byte (bits 0-7) is written to `address`
  - High byte (bits 8-15) is written to `address + 1`
- For example, writing `0x1234` to address `0x0050` will:
  - Write `0x34` to address `0x0050`
  - Write `0x12` to address `0x0051`
- Address validation: Values outside 0x0000-0xFFFF will return a Lua error.
- Value validation: Values outside 0-65535 will return a Lua error.
- **Address wrapping:** If `address + 1` exceeds 0xFFFF, only the low byte will be written.
- Uses FCEUX's memory mapping system (`BWrite`), which handles all memory mapping correctly.
- **Immediate effect:** Both bytes are written immediately and take effect on the next memory read.
- Useful for writing 16-bit game values like:
  - Scores stored as 16-bit values
  - Timers stored as 16-bit values
  - Coordinates stored as 16-bit values
  - Any game data that requires two consecutive bytes

**Example:**
```lua
-- Write a 16-bit value to RAM
writeword(0x0100, 0x1234)
-- This writes: 0x34 to 0x0100, 0x12 to 0x0101

-- Verify the write (read back)
local low = readbyte(0x0100)
local high = readbyte(0x0101)
local value = low + (high * 256)  -- Reconstruct: 0x34 + (0x12 * 256) = 0x1234

-- Write a simple 16-bit value
writeword(0x0200, 12345)  -- Writes 12345 as two bytes

-- Write maximum 16-bit value
writeword(0x0300, 65535)  -- Writes 0xFFFF (0xFF, 0xFF)

-- Example: Write to a 16-bit timer
writeword(0x0400, 600)  -- Set timer to 600 (10 minutes * 60 seconds)

-- Example: Write player position (if stored as 16-bit)
writeword(0x0500, 1234)  -- Set X position to 1234
```

### `writebytes`

**Signature:** `writebytes(address, value1, value2, ...)`
Writes multiple consecutive bytes to memory starting at the specified address.

**Parameters:**
- `address` (integer): Starting memory address to write to. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `value1` (integer): First byte value to write (0-255).
- `value2` (integer): Second byte value to write (0-255).
- `...` (integer): Additional byte values to write (0-255 each). Can specify any number of values.

**Returns:** Nothing

**Notes:**
- Writes bytes sequentially starting from `address`:
  - `value1` is written to `address`
  - `value2` is written to `address + 1`
  - `value3` is written to `address + 2`
  - And so on...
- Address validation: Starting address must be in range 0x0000-0xFFFF.
- Value validation: Each value must be in range 0-255. If any value is out of range, a Lua error is returned specifying which value failed.
- **Address wrapping:** If writing bytes would extend past 0xFFFF, the function will stop writing at the address space boundary without error.
- Requires at least 2 arguments (address + at least one value).
- Uses FCEUX's memory mapping system (`BWrite`), which handles all memory mapping correctly.
- **Immediate effect:** All bytes are written immediately in the order specified.
- More efficient than calling `writebyte()` multiple times, as it validates inputs once and writes sequentially.
- Useful for:
  - Writing multi-byte values (scores, timers, coordinates)
  - Initializing arrays or buffers
  - Copying byte sequences
  - Writing structured game data that spans multiple bytes

**Example:**
```lua
-- Write 3 bytes starting at address 0x0060
writebytes(0x0060, 10, 20, 30)
-- This writes: 10 to 0x0060, 20 to 0x0061, 30 to 0x0062

-- Super Mario Bros 1 - Set score (3 bytes: high, mid, low)
-- Score format: (high * 10000) + (mid * 100) + (low)
writebytes(0x07DE, 0, 0, 50)    -- Score: 50
writebytes(0x07DE, 0, 1, 23)    -- Score: 123
writebytes(0x07DE, 5, 0, 0)      -- Score: 50000
writebytes(0x07DE, 9, 9, 99)     -- Score: 99999 (max)

-- Write a 4-byte sequence
writebytes(0x0100, 0xFF, 0xFE, 0xFD, 0xFC)

-- Initialize a buffer with zeros
writebytes(0x0200, 0, 0, 0, 0, 0, 0, 0, 0)  -- Clear 8 bytes

-- Write a string-like byte sequence (ASCII values)
writebytes(0x0300, 0x48, 0x45, 0x4C, 0x4C, 0x4F)  -- "HELLO" in ASCII
```

### `writeprg`

**Signature:** `writeprg(address, value)`
Attempts to write a byte value to program ROM (0x8000-0xFFFF). Note that most ROM is read-only, but some mappers support ROM writes for mapper-specific operations.

**Parameters:**
- `address` (integer): Program ROM address to write to. Valid range is 0x8000-0xFFFF (NES program ROM region).
- `value` (integer): Byte value to write. Valid range is 0-255.

**Returns:** Nothing

**Notes:**
- Attempts to write a byte value to program ROM using FCEUX's memory mapping system (`BWrite`).
- **ROM-specific validation:** Address must be in program ROM range (0x8000-0xFFFF). Attempting to write to addresses outside ROM will return an error.
- Value validation: Must be in range 0-255.
- **Read-only behavior:** Most ROM is read-only, so writes may be ignored by the mapper. The function attempts the write, but the mapper will handle it according to its specific behavior.
- **Mapper-specific support:** Some mappers support ROM writes for mapper-specific operations (bank switching, mapper registers, etc.). Whether the write succeeds depends on the mapper implementation.
- Uses FCEUX's memory mapping system (`BWrite`), which handles mapper-specific behavior correctly.
- **Specialized function:** This is a specialized function for mapper-specific operations. For general memory writing, use `writebyte()` or `writebytes()`.
- Useful for:
  - Mapper-specific operations (bank switching, mapper registers)
  - Special cartridge features that support ROM writes
  - Testing mapper behavior
  - Advanced ROM manipulation (when supported by mapper)

**Example:**
```lua
-- Attempt to write to program ROM (may be ignored if read-only)
writeprg(0x8000, 0xFF)

-- Write to different ROM addresses
writeprg(0xC000, 0xAA)
writeprg(0xFFFF, 0x55)

-- This will error if address is outside ROM:
-- writeprg(0x0000, 0xFF)  -- Error: must be in ROM range 0x8000-0xFFFF

-- Mapper-specific operation example
function script()
    -- Attempt mapper register write (behavior depends on mapper)
    writeprg(0x8000, 0x01)  -- May switch banks or configure mapper
end

-- Verify write (note: may not have effect if ROM is read-only)
local before = readbyte(0x8000)
writeprg(0x8000, 0xFF)
local after = readbyte(0x8000)
if before == after then
    print("ROM write ignored (read-only)")
else
    print("ROM write succeeded (mapper supports it)")
end
```

**When to use `writeprg()` vs `writebyte()`:**
- Use `writeprg()` when you need to **explicitly write to program ROM** (0x8000-0xFFFF) and ensure addresses are validated as ROM addresses
- Use `writebyte()` when you need to **write to any memory region** (RAM, PPU, APU, ROM, etc.) across the full address space (0x0000-0xFFFF)
- Both functions use the same underlying `BWrite` system, but `writeprg()` provides ROM-specific validation

### Memory Region Operations

### `fillbytes`

**Signature:** `fillbytes(address, count, value)`
Fills a memory region with a specific byte value. More efficient than looping `writebyte()` when you need to set multiple bytes to the same value.

**Parameters:**
- `address` (integer): Starting memory address to fill. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `count` (integer): Number of bytes to fill. Valid range is 1-256.
- `value` (integer): Byte value to fill with. Valid range is 0-255.

**Returns:** Nothing

**Notes:**
- Fills `count` consecutive bytes starting at `address`, all with the same `value`.
- Address validation: Starting address must be in range 0x0000-0xFFFF.
- Count validation: Must be at least 1 and cannot exceed 256.
- Value validation: Must be in range 0-255.
- **Address wrapping:** If filling bytes would extend past 0xFFFF, the count is automatically adjusted to stop at the address space boundary.
- Uses FCEUX's memory mapping system (`BWrite`), which handles all memory mapping correctly.
- **More efficient than looping:** Much faster than calling `writebyte()` in a loop, as it validates inputs once and writes sequentially.
- Useful for:
  - Clearing buffers (fill with 0)
  - Resetting arrays to a default value
  - Initializing memory regions
  - Setting flags or state to a known value across a range

**Example:**
```lua
-- Clear a buffer (fill 10 bytes with 0)
fillbytes(0x0200, 10, 0)
-- This writes: 0 to 0x0200, 0 to 0x0201, ..., 0 to 0x0209

-- Fill a region with 0xFF (often used for initialization)
fillbytes(0x0300, 8, 0xFF)
-- This writes: 0xFF to 0x0300 through 0x0307

-- Clear SMB1 score (3 bytes)
fillbytes(0x07DE, 3, 0)
-- This clears the score to 00000

-- Reset a buffer to a specific value
fillbytes(0x0400, 16, 0xAA)  -- Fill 16 bytes with 0xAA

-- Initialize an array with default values
fillbytes(0x0500, 32, 0)  -- Clear 32-byte array
```

### `copybytes`

**Signature:** `copybytes(sourceAddr, destAddr, count)`
Copies memory from one location to another. Handles overlapping regions correctly to prevent data corruption.

**Parameters:**
- `sourceAddr` (integer): Source memory address to copy from. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `destAddr` (integer): Destination memory address to copy to. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `count` (integer): Number of bytes to copy. Valid range is 1-256.

**Returns:** Nothing

**Notes:**
- Copies `count` consecutive bytes from `sourceAddr` to `destAddr`.
- Address validation: Both source and destination addresses must be in range 0x0000-0xFFFF.
- Count validation: Must be at least 1 and cannot exceed 256.
- **Overlapping regions:** If `destAddr > sourceAddr` and the regions overlap, the function automatically copies backwards (from end to beginning) to prevent overwriting source data before it's read. This ensures correct behavior even when copying within the same memory region.
- **Address wrapping:** If copying would extend past 0xFFFF, the count is automatically adjusted to stop at the address space boundary.
- Uses FCEUX's memory mapping system (`ARead`/`BWrite`), which handles all memory mapping correctly.
- **More efficient than manual loops:** Faster than manually reading and writing bytes in a loop, as it validates inputs once and handles overlapping regions automatically.
- Useful for:
  - Creating backups of memory regions
  - Moving data structures
  - Duplicating game state
  - Restoring saved memory snapshots
  - Shifting data within memory regions

**Example:**
```lua
-- Copy score to backup location (non-overlapping)
copybytes(0x07DE, 0x0600, 3)
-- This copies: 0x07DE->0x0600, 0x07DF->0x0601, 0x07E0->0x0602

-- Restore score from backup
copybytes(0x0600, 0x07DE, 3)
-- This restores the score from the backup location

-- Super Mario Bros 1 - Backup and restore score
function script()
  -- Backup score before modification
  copybytes(0x07DE, 0x0600, 3)
  
  -- Modify score
  writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
  
  -- Later, restore from backup
  copybytes(0x0600, 0x07DE, 3)
end

-- Copy overlapping region (automatically handles correctly)
copybytes(0x0100, 0x0101, 10)  -- Copies 10 bytes forward (overlapping)
-- This correctly copies backwards internally to prevent corruption

-- Duplicate a data structure
copybytes(0x0700, 0x0800, 16)  -- Copy 16-byte structure to new location

-- Move data (same as copy, but source can be cleared afterwards)
copybytes(0x0200, 0x0300, 8)  -- Move 8 bytes from 0x0200 to 0x0300
```

### `comparebytes`

**Signature:** `comparebytes(addr1, addr2, count)`
Compares two memory regions byte-by-byte to determine if they are identical. Useful for verifying backups, detecting memory changes, and validating data integrity.

**Parameters:**
- `addr1` (integer): First memory address to compare. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `addr2` (integer): Second memory address to compare. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `count` (integer): Number of bytes to compare. Valid range is 1-256.

**Returns:** Boolean (`true` if identical, `false` if different)

**Notes:**
- Compares `count` consecutive bytes starting at `addr1` and `addr2`.
- Address validation: Both addresses must be in range 0x0000-0xFFFF.
- Count validation: Must be at least 1 and cannot exceed 256.
- **Early exit:** Returns `false` immediately upon finding the first difference (optimized for performance).
- **Address wrapping:** If comparing would extend past 0xFFFF, the count is automatically adjusted to stop at the address space boundary.
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly.
- **Efficient comparison:** Faster than manually reading and comparing bytes in a loop, as it validates inputs once and compares sequentially with early exit.
- Useful for:
  - Verifying backups match originals
  - Detecting memory changes over time
  - Validating data integrity
  - Checking if two regions are identical
  - Testing if copy operations succeeded

**Example:**
```lua
-- Verify backup matches original
local isIdentical = comparebytes(0x07DE, 0x0600, 3)
if isIdentical then
    print("Backup verified!")
else
    print("Backup differs from original!")
end

-- Super Mario Bros 1 - Verify score backup
function script()
    -- Create backup
    copybytes(0x07DE, 0x0600, 3)
    
    -- Verify backup
    if comparebytes(0x07DE, 0x0600, 3) then
        print("Score backup verified")
    else
        print("ERROR: Backup verification failed")
    end
    
    -- Modify original
    writebytes(0x07DE, 9, 9, 99)
    
    -- Check if they differ now
    if not comparebytes(0x07DE, 0x0600, 3) then
        print("Original and backup differ (expected)")
    end
end

-- Compare two different memory regions
local scoreMatches = comparebytes(0x07DE, 0x07E0, 3)
-- This compares score (0x07DE-0x07E0) with the next 3 bytes

-- Verify copy operation succeeded
copybytes(0x0100, 0x0200, 16)
if comparebytes(0x0100, 0x0200, 16) then
    print("Copy operation successful")
else
    print("Copy operation failed!")
end
```

### Backup and Restore

### `backupbytes`

**Signature:** `backupbytes(address, count)`
Creates a backup of a memory region by reading bytes and returning them as a Lua table. The backup table can be stored and later used with `restorebytes()` or manually restored using `writebytes()`.

**Parameters:**
- `address` (integer): Memory address to backup. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `count` (integer): Number of bytes to backup. Valid range is 1-256.

**Returns:** Lua table containing the backed-up bytes (1-indexed, same format as `readbytes()`)

**Notes:**
- Creates a Lua table containing `count` consecutive bytes starting at `address`.
- Address validation: Starting address must be in range 0x0000-0xFFFF.
- Count validation: Must be at least 1 and cannot exceed 256.
- **Address wrapping:** If backing up would extend past 0xFFFF, the count is automatically adjusted to stop at the address space boundary.
- Uses FCEUX's memory mapping system (`ARead`), which handles all memory mapping correctly.
- **Table format:** Returns a 1-indexed Lua table, same as `readbytes()`. Table indices are 1, 2, 3, ... up to count.
- **Semantic purpose:** While functionally equivalent to `readbytes()`, `backupbytes()` is specifically designed for creating backups that will be restored later, making code intent clearer.
- Useful for:
  - Saving state before modifications
  - Creating restore points
  - Temporarily backing up game values
  - Storing memory snapshots for later restoration

**Example:**
```lua
-- Backup SMB1 score (3 bytes)
local scoreBackup = backupbytes(0x07DE, 3)
-- Returns: {highByte, midByte, lowByte} (e.g., {0, 1, 23} for score 123)

-- Backup multiple game values
local gameState = {
    score = backupbytes(0x07DE, 3),
    lives = backupbytes(0x075A, 1),
    coins = backupbytes(0x075E, 1)
}

-- Super Mario Bros 1 - Backup and restore score
function script()
    -- Create backup before modification
    local scoreBackup = backupbytes(0x07DE, 3)
    
    -- Modify score
    writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
    
    -- Later, restore from backup using writebytes
    writebytes(0x07DE, scoreBackup[1], scoreBackup[2], scoreBackup[3])
    
    -- Or use restorebytes() when implemented
    -- restorebytes(0x07DE, scoreBackup)
end

-- Backup a larger memory region
local playerData = backupbytes(0x0700, 16)  -- Backup 16-byte player structure

-- Store backup for later use
local savedScore = backupbytes(0x07DE, 3)
-- ... do other operations ...
-- Restore later
writebytes(0x07DE, savedScore[1], savedScore[2], savedScore[3])
```

**When to use `backupbytes()` vs `readbytes()`:**
- Use `backupbytes()` when you need to **create a backup that will be restored later** (clearer semantic intent)
- Use `readbytes()` when you need to **read memory for analysis or display** (general purpose reading)
- Both functions return the same format (1-indexed Lua table), so they're functionally equivalent but serve different semantic purposes

### `restorebytes`

**Signature:** `restorebytes(address, data)`
Restores a memory region from a backup table created by `backupbytes()`. This is the companion function to `backupbytes()` and provides a convenient way to restore saved memory state.

**Parameters:**
- `address` (integer): Memory address to restore to. Valid range is 0x0000-0xFFFF (NES 16-bit address space).
- `data` (table): Lua table containing the backed-up bytes (from `backupbytes()`). Must be a 1-indexed table with byte values (0-255).

**Returns:** Nothing

**Notes:**
- Restores bytes from the `data` table to memory starting at `address`.
- Address validation: Starting address must be in range 0x0000-0xFFFF.
- Table validation: Second parameter must be a Lua table.
- Value validation: Each byte value in the table must be in range 0-255.
- **Table format:** Expects a 1-indexed Lua table (same format as returned by `backupbytes()`). Table indices are 1, 2, 3, ... up to the number of bytes.
- **Address wrapping:** If restoring would extend past 0xFFFF, the function stops at the address space boundary without error.
- Uses FCEUX's memory mapping system (`BWrite`), which handles all memory mapping correctly.
- **Paired with `backupbytes()`:** Designed to work with tables created by `backupbytes()`, but can also work with any 1-indexed table of byte values.
- **More convenient than manual restore:** Easier than manually extracting values from a backup table and using `writebytes()`.
- Useful for:
  - Restoring state after temporary modifications
  - Reverting changes made to game memory
  - Restoring from saved backup snapshots
  - Implementing undo/redo functionality

**Example:**
```lua
-- Backup and restore SMB1 score
local scoreBackup = backupbytes(0x07DE, 3)
-- Modify score
writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
-- Restore from backup
restorebytes(0x07DE, scoreBackup)

-- Super Mario Bros 1 - Complete backup/restore workflow
function script()
    -- Create backup before modification
    local scoreBackup = backupbytes(0x07DE, 3)
    
    -- Modify score
    writebytes(0x07DE, 9, 9, 99)  -- Set to 99999
    
    -- Later, restore from backup
    restorebytes(0x07DE, scoreBackup)
end

-- Backup and restore multiple game values
local gameBackup = {
    score = backupbytes(0x07DE, 3),
    lives = backupbytes(0x075A, 1),
    coins = backupbytes(0x075E, 1)
}

-- Modify values
writebytes(0x07DE, 5, 0, 0)  -- Set score to 50000
writebyte(0x075A, 98)        -- Set lives to 99
writebyte(0x075E, 99)        -- Set coins to 99

-- Restore all values
restorebytes(0x07DE, gameBackup.score)
restorebytes(0x075A, gameBackup.lives)
restorebytes(0x075E, gameBackup.coins)

-- Store backup for later restoration
local savedState = backupbytes(0x0700, 16)
-- ... do other operations ...
-- Restore later
restorebytes(0x0700, savedState)
```

**When to use `restorebytes()` vs `writebytes()`:**
- Use `restorebytes()` when you have a **backup table from `backupbytes()`** (convenient, handles table extraction automatically)
- Use `writebytes()` when you have **individual known values** to write (more direct for specific values)
- `restorebytes()` is more convenient when working with backups created by `backupbytes()`

## Memory Function Comparison

| Function | Purpose | Data Size | Format |
|----------|---------|-----------|--------|
| `readbyte(address)` | Read a single byte | 8-bit (0-255) | Single byte |
| `readword(address)` | Read a 16-bit value | 16-bit (0-65535) | Integer (little-endian) |
| `readbytes(address, count)` | Read multiple bytes | 8-bit each (0-255) | Table of integers |
| `readram(startAddr, count)` | Read from RAM only | 8-bit each (0-255) | Table of integers |
| `getmemorytype(address)` | Get memory type | N/A | Returns string ("RAM", "PPU", "APU", "ROM", "UNKNOWN") |
| `ismemorywritable(address)` | Check if writable | N/A | Returns boolean (true if writable) |
| `writebyte(address, value)` | Write a single byte | 8-bit (0-255) | Single byte |
| `writeword(address, value)` | Write a 16-bit value | 16-bit (0-65535) | Little-endian (low byte first) |
| `writebytes(address, ...)` | Write multiple bytes | 8-bit each (0-255) | Sequential bytes (different values) |
| `writeprg(address, value)` | Write to program ROM | 8-bit (0-255) | Mapper-specific (may be ignored) |
| `fillbytes(address, count, value)` | Fill memory region | 8-bit each (0-255) | Sequential bytes (same value) |
| `copybytes(sourceAddr, destAddr, count)` | Copy memory region | 8-bit each (0-255) | Copies from source to destination |
| `comparebytes(addr1, addr2, count)` | Compare memory regions | 8-bit each (0-255) | Returns boolean (true if identical) |
| `backupbytes(address, count)` | Backup memory region | 8-bit each (0-255) | Returns table (1-indexed) |
| `restorebytes(address, data)` | Restore memory from backup | 8-bit each (0-255) | Takes table (1-indexed) |

**When to use each:**
- **`readbyte()`**: Single byte values (lives, coins, power-up state, flags, single-byte counters)
- **`readword()`**: 16-bit values (scores, timers, coordinates, counters stored as 16-bit)
- **`readbytes()`**: Multi-byte sequences (scores stored across 3+ bytes, arrays, buffers, memory analysis) - works across full address space
- **`readram()`**: Multi-byte sequences specifically from RAM (0x0000-0x1FFF) - explicit RAM-only access with validation
- **`getmemorytype()`**: Identifying memory type at an address (validating addresses, understanding memory layout, debugging)
- **`ismemorywritable()`**: Checking if an address is writable before write operations (validating addresses, preventing write errors, safety checks)
- **`writebyte()`**: Single byte values (lives, coins, power-up state, flags)
- **`writeword()`**: 16-bit values (scores, timers, coordinates, counters)
- **`writebytes()`**: Multi-byte sequences with different values (scores stored across 3+ bytes, arrays with varied data)
- **`writeprg()`**: Mapper-specific ROM operations (bank switching, mapper registers) - specialized use case
- **`fillbytes()`**: Clearing buffers, resetting arrays, initializing memory regions (same value for all bytes)
- **`copybytes()`**: Copying existing memory (backups, moving data, duplicating structures, restoring snapshots)
- **`comparebytes()`**: Verifying backups, detecting memory changes, validating data integrity
- **`backupbytes()`**: Creating memory backups stored in Lua tables (saving state before modifications)
- **`restorebytes()`**: Restoring memory from backup tables (restoring state after temporary modifications)

## See Also

- **[Examples](Examples)** - Working example scripts
- **[Callbacks](Callbacks)** - Callback functions including `onwatch()` for memory watchpoints
- **[Home](Home)** - Return to the main wiki page
