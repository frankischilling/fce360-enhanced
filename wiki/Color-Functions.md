# Color Functions

The Color Functions provide comprehensive color manipulation capabilities for working with the NES 64-color palette. These functions allow you to read and convert palette colors, modify the NES palette RAM, and blend colors for visual effects.

## Color Information Functions

### `getcolorrgb`

**Signature:** `getcolorrgb(paletteIndex)`
Gets RGB values for a palette color. Returns the red, green, and blue components of a palette color as a table. Useful for color conversion, color analysis, and working with palette colors in RGB format.

**Parameters:**
- `paletteIndex` (integer): Palette color index (0-63)
  - Valid range: 0-63 (64 total palette colors)
  - Each index corresponds to one of the NES system's internal palette entries

**Returns:**
- `table` - RGB color values table with the following structure:
  - `[1]` (integer) - Red component (0-255)
  - `[2]` (integer) - Green component (0-255)
  - `[3]` (integer) - Blue component (0-255)
- Can be accessed via array indices: `rgb[1]` (red), `rgb[2]` (green), `rgb[3]` (blue)

**Notes:**
- Returns RGB values in the range 0-255 for each component
- Palette colors vary depending on NTSC tint/hue settings, but their relative ordering is fixed
- Useful for converting palette colors to RGB format for external tools or color analysis
- The palette index corresponds to the NES 64-color palette (0x00-0x3F)
- Throws an error if `paletteIndex` is outside the valid range (0-63)

**Example: Basic Usage:**
```lua
local rgb = getcolorrgb(0x20)  -- Get RGB for bright white
print(string.format("RGB: %d, %d, %d", rgb[1], rgb[2], rgb[3]))
```

**Example: Display RGB Values:**
```lua
function gui()
    local rgb = getcolorrgb(0x16)  -- Red/orange color
    drawtext(4, 4, string.format("RGB: %d, %d, %d", rgb[1], rgb[2], rgb[3]), 0x20)
end
```

**Example: Color Analysis:**
```lua
function gui()
    -- Analyze multiple palette colors
    local colors = {0x00, 0x10, 0x20, 0x16, 0x29, 0x37}
    
    for i, index in ipairs(colors) do
        local rgb = getcolorrgb(index)
        local y = 4 + (i - 1) * 10
        drawtext(4, y, string.format("%02X: RGB(%3d,%3d,%3d)", 
              index, rgb[1], rgb[2], rgb[3]), 0x29)
    end
end
```

**Example: Convert All Palette Colors:**
```lua
-- Convert entire palette to RGB
local paletteRGB = {}
for i = 0, 63 do
    paletteRGB[i] = getcolorrgb(i)
end

function gui()
    -- Use cached RGB values
    local rgb = paletteRGB[0x20]
    drawtext(4, 4, string.format("Color 0x20: RGB(%d,%d,%d)", 
          rgb[1], rgb[2], rgb[3]), 0x20)
end
```

**Example: Color Comparison:**
```lua
function gui()
    local color1 = getcolorrgb(0x20)  -- Bright white
    local color2 = getcolorrgb(0x10)  -- Light gray
    
    -- Calculate brightness (simple average)
    local brightness1 = (color1[1] + color1[2] + color1[3]) / 3
    local brightness2 = (color2[1] + color2[2] + color2[3]) / 3
    
    drawtext(4, 4, string.format("0x20 brightness: %.1f", brightness1), 0x20)
    drawtext(4, 14, string.format("0x10 brightness: %.1f", brightness2), 0x29)
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid index
    local success, rgb = pcall(function()
        return getcolorrgb(0x20)
    end)
    
    if success then
        drawtext(4, 4, string.format("RGB: %d,%d,%d", rgb[1], rgb[2], rgb[3]), 0x29)
    end
    
    -- Test with invalid index
    success, err = pcall(function()
        return getcolorrgb(64)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
end
```

### `getnescolor`

**Signature:** `getnescolor(index)`
Gets NES color value as a packed RGB integer. Returns the red, green, and blue components of a palette color as a single packed integer in 0xRRGGBB format. Useful for color lookup when you need a single integer value instead of a table.

**Parameters:**
- `index` (integer): Palette color index (0-63)
  - Valid range: 0-63 (64 total palette colors)
  - Each index corresponds to one of the NES system's internal palette entries

**Returns:**
- `integer` - Packed RGB value (0x000000-0xFFFFFF)
  - Format: 0xRRGGBB where RR, GG, BB are hex values (0-255 each)
  - Red component is in bits 16-23
  - Green component is in bits 8-15
  - Blue component is in bits 0-7
  - Can be extracted using bit operations or division/modulo

**Notes:**
- Returns RGB values packed into a single integer (0xRRGGBB format)
- More efficient than `getcolorrgb()` when you only need a single integer value
- Useful for color comparisons, color lookups, and when working with external tools that expect packed RGB integers
- Palette colors vary depending on NTSC tint/hue settings, but their relative ordering is fixed
- The palette index corresponds to the NES 64-color palette (0x00-0x3F)
- Throws an error if `index` is outside the valid range (0-63)
- To extract RGB components in Lua: `r = math.floor(rgb / 65536) % 256`, `g = math.floor(rgb / 256) % 256`, `b = rgb % 256`

**Example: Basic Usage:**
```lua
local packedRGB = getnescolor(0x20)  -- Get packed RGB for bright white
print(string.format("Packed RGB: 0x%06X", packedRGB))
```

**Example: Extract RGB Components:**
```lua
local packedRGB = getnescolor(0x20)

-- Extract components (Lua doesn't have bit shifts, use division)
local r = math.floor(packedRGB / 65536) % 256
local g = math.floor(packedRGB / 256) % 256
local b = packedRGB % 256

print(string.format("RGB: %d, %d, %d", r, g, b))
```

**Example: Compare with getcolorrgb():**
```lua
function gui()
    local index = 0x20  -- Bright white
    
    -- Get packed RGB
    local packedRGB = getnescolor(index)
    
    -- Get table RGB
    local rgb = getcolorrgb(index)
    
    -- Extract from packed
    local r = math.floor(packedRGB / 65536) % 256
    local g = math.floor(packedRGB / 256) % 256
    local b = packedRGB % 256
    
    -- Verify they match
    if r == rgb[1] and g == rgb[2] and b == rgb[3] then
        drawtext(4, 4, "Values match!", 0x29)
    end
end
```

**Example: Color Lookup Table:**
```lua
-- Create lookup table of packed RGB values
local colorLookup = {}
for i = 0, 63 do
    colorLookup[i] = getnescolor(i)
end

function gui()
    -- Use packed RGB for quick lookups
    local whiteRGB = colorLookup[0x20]
    drawtext(4, 4, string.format("White: 0x%06X", whiteRGB), 0x20)
end
```

**Example: Color Comparison:**
```lua
function gui()
    local color1 = getnescolor(0x20)  -- Bright white
    local color2 = getnescolor(0x10)  -- Light gray
    
    -- Compare brightness (simple sum)
    local brightness1 = (math.floor(color1 / 65536) % 256) + 
                        (math.floor(color1 / 256) % 256) + 
                        (color1 % 256)
    local brightness2 = (math.floor(color2 / 65536) % 256) + 
                        (math.floor(color2 / 256) % 256) + 
                        (color2 % 256)
    
    drawtext(4, 4, string.format("0x20 brightness: %d", brightness1), 0x20)
    drawtext(4, 14, string.format("0x10 brightness: %d", brightness2), 0x29)
end
```

**Example: Convert to Hex String:**
```lua
function gui()
    local packedRGB = getnescolor(0x16)  -- Red/orange
    
    -- Display as hex string
    local hexStr = string.format("#%06X", packedRGB)
    drawtext(4, 4, string.format("Color: %s", hexStr), 0x20)
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid index
    local success, rgb = pcall(function()
        return getnescolor(0x20)
    end)
    
    if success then
        drawtext(4, 4, string.format("RGB: 0x%06X", rgb), 0x29)
    end
    
    -- Test with invalid index
    success, err = pcall(function()
        return getnescolor(64)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
end
```

## Palette RAM Functions

### `getpalettecolor`

**Signature:** `getpalettecolor(index)`
Gets palette color index for a position in the NES palette RAM (PALRAM). Returns the actual color index (0-63) stored at the specified palette RAM location. Useful for reading the current palette configuration and understanding how games map colors.

**Parameters:**
- `index` (integer): Palette RAM index (0-31)
  - Valid range: 0-31 (32 total palette RAM entries)
  - Each entry corresponds to a position in the NES palette RAM
  - Indices 0x00-0x0F: Background palettes
  - Indices 0x10-0x1F: Sprite palettes

**Returns:**
- `integer` - Color index (0-63)
  - The actual palette color index stored at the specified PALRAM position
  - This value can be used with `getcolorrgb()` to get RGB values
  - Values are masked to 6 bits (0x3F), ensuring range 0-63

**Notes:**
- Reads directly from the NES palette RAM (PALRAM)
- The NES has 32 palette RAM entries, each containing a color index (0-63)
- Palette RAM is organized into background and sprite palettes
- Background palettes: PALRAM[0x00-0x0F] (universal color + 4 palettes of 3 colors each)
- Sprite palettes: PALRAM[0x10-0x1F] (universal color + 4 palettes of 3 colors each)
- The universal background color (PALRAM[0x00]) is mirrored to PALRAM[0x04, 0x08, 0x0C]
- The universal sprite color (PALRAM[0x10]) is mirrored to PALRAM[0x14, 0x18, 0x1C]
- Throws an error if `index` is outside the valid range (0-31)
- Use with `getcolorrgb()` to convert the color index to RGB values

**Example: Basic Usage:**
```lua
local colorIndex = getpalettecolor(0x01)  -- Get color from background palette 0, color 1
print(string.format("Color index: %02X (%d)", colorIndex, colorIndex))
```

**Example: Read Background Palette:**
```lua
function gui()
    -- Read background palette 0
    local universal = getpalettecolor(0x00)
    local color1 = getpalettecolor(0x01)
    local color2 = getpalettecolor(0x02)
    local color3 = getpalettecolor(0x03)
    
    drawtext(4, 4, string.format("BG Pal 0: %02X %02X %02X %02X", 
          universal, color1, color2, color3), 0x20)
end
```

**Example: Read All Palette RAM:**
```lua
function gui()
    local y = 4
    for i = 0, 31 do
        local colorIndex = getpalettecolor(i)
        drawtext(4, y, string.format("PALRAM[%02X] = %02X", i, colorIndex), 0x29)
        y = y + 10
        if y > 230 then break end
    end
end
```

**Example: Convert to RGB:**
```lua
function gui()
    -- Get color index from palette RAM
    local colorIndex = getpalettecolor(0x16)  -- Sprite palette 0, color 2
    
    -- Convert to RGB
    local rgb = getcolorrgb(colorIndex)
    
    drawtext(4, 4, string.format("PALRAM[0x16] -> Color %02X", colorIndex), 0x20)
    drawtext(4, 14, string.format("RGB: %d, %d, %d", rgb[1], rgb[2], rgb[3]), 0x29)
    
    -- Draw color swatch
    fillrect(4, 24, 32, 16, colorIndex)
end
```

**Example: Monitor Palette Changes:**
```lua
local lastPalette = {}
for i = 0, 31 do
    lastPalette[i] = getpalettecolor(i)
end

function gui()
    -- Check for palette changes
    for i = 0, 31 do
        local current = getpalettecolor(i)
        if current ~= lastPalette[i] then
            print(string.format("PALRAM[%02X] changed: %02X -> %02X", 
                  i, lastPalette[i], current))
            lastPalette[i] = current
        end
    end
end
```

**Example: Display Sprite Palettes:**
```lua
function gui()
    drawtext(4, 4, "Sprite Palettes:", 0x20)
    local y = 14
    
    -- Display all 4 sprite palettes
    for pal = 0, 3 do
        local base = 0x10 + (pal * 4)
        local universal = getpalettecolor(base)
        local c1 = getpalettecolor(base + 1)
        local c2 = getpalettecolor(base + 2)
        local c3 = getpalettecolor(base + 3)
        
        drawtext(4, y, string.format("SP Pal %d: %02X %02X %02X %02X", 
              pal, universal, c1, c2, c3), 0x29)
        y = y + 10
    end
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid index
    local success, color = pcall(function()
        return getpalettecolor(0x01)
    end)
    
    if success then
        drawtext(4, 4, string.format("Color: %02X", color), 0x29)
    end
    
    -- Test with invalid index
    success, err = pcall(function()
        return getpalettecolor(32)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
end
```

### `getpalette`

**Signature:** `getpalette()`
Gets current palette as a table. Returns all 32 palette RAM entries (0-31) as a table with 0-indexed keys. More efficient than calling `getpalettecolor()` 32 times. Useful for palette analysis, saving/restoring palettes, and comparing palette states.

**Parameters:**
- None

**Returns:**
- `table` - Table containing all palette RAM entries
  - Keys are 0-indexed integers (0-31) corresponding to PALRAM indices
  - Values are color indices (0-63) from the palette RAM
  - Table structure: `{[0] = color0, [1] = color1, ..., [31] = color31}`
  - The returned table can be used directly with `setpalette()` for save/restore operations

**Notes:**
- Reads directly from the NES palette RAM (PALRAM)
- Returns a snapshot of the current palette state at the time of the call
- The returned table uses 0-indexed keys matching `setpalette()`'s expected format
- More efficient than multiple `getpalettecolor()` calls when reading the entire palette
- Use with `setpalette()` to save and restore palette states
- Use with `getcolorrgb()` to convert color indices to RGB values

**Example: Basic Usage:**
```lua
-- Get current palette
local palette = getpalette()

-- Access individual entries
local color0 = palette[0]   -- PALRAM[0x00]
local color1 = palette[1]   -- PALRAM[0x01]
local color31 = palette[31] -- PALRAM[0x1F]

print(string.format("PALRAM[0x00] = %02X", color0))
```

**Example: Save and Restore Palette:**
```lua
local savedPalette = nil

function gui()
    -- Save palette on first frame
    if not savedPalette then
        savedPalette = getpalette()
        print("Palette saved")
    end
    
    -- ... do palette modifications ...
    
    -- Restore saved palette
    if savedPalette then
        setpalette(savedPalette)
    end
end
```

**Example: Palette Analysis:**
```lua
function gui()
    local palette = getpalette()
    
    -- Count unique colors
    local colorCounts = {}
    for i = 0, 31 do
        local color = palette[i]
        colorCounts[color] = (colorCounts[color] or 0) + 1
    end
    
    local uniqueColors = 0
    for _ in pairs(colorCounts) do
        uniqueColors = uniqueColors + 1
    end
    
    drawtext(4, 4, string.format("Unique colors: %d", uniqueColors), 0x20)
end
```

**Example: Compare Palettes:**
```lua
local originalPalette = nil

function gui()
    -- Capture original palette once
    if not originalPalette then
        originalPalette = getpalette()
    end
    
    -- Get current palette
    local currentPalette = getpalette()
    
    -- Compare and find differences
    local differences = {}
    for i = 0, 31 do
        if currentPalette[i] ~= originalPalette[i] then
            table.insert(differences, {
                index = i,
                original = originalPalette[i],
                current = currentPalette[i]
            })
        end
    end
    
    if #differences > 0 then
        drawtext(4, 4, string.format("Palette changed: %d entries", #differences), 0x16)
        for i, diff in ipairs(differences) do
            if i <= 5 then  -- Show first 5
                drawtext(4, 14 + i * 10, 
                    string.format("[%02X]: %02X -> %02X", 
                    diff.index, diff.original, diff.current), 0x29)
            end
        end
    else
        drawtext(4, 4, "Palette unchanged", 0x29)
    end
end
```

**Example: Round-Trip Test:**
```lua
function gui()
    -- Get current palette
    local palette = getpalette()
    
    -- Modify it
    for i = 0, 31 do
        palette[i] = (palette[i] + 1) % 64  -- Shift all colors by 1
    end
    
    -- Set it back
    setpalette(palette)
    
    -- Verify round-trip
    local verifyPalette = getpalette()
    local matches = true
    for i = 0, 31 do
        if verifyPalette[i] ~= palette[i] then
            matches = false
            break
        end
    end
    
    drawtext(4, 4, string.format("Round-trip: %s", matches and "OK" or "FAILED"), 
        matches and 0x29 or 0x16)
end
```

**Example: Display Full Palette:**
```lua
function gui()
    local palette = getpalette()
    
    drawtext(4, 4, "Palette RAM (0x00-0x1F):", 0x20)
    
    -- Display all 32 entries as color swatches
    for i = 0, 31 do
        local x = 4 + (i % 16) * 12
        local y = 14 + math.floor(i / 16) * 10
        local color = palette[i]
        
        fillrect(x, y, 10, 8, color)
        if i < 8 then  -- Show hex values for first row
            drawtext(x, y + 8, string.format("%02X", color), 0x20)
        end
    end
end
```

**Example: Monitor Palette Changes:**
```lua
local lastPalette = nil

function gui()
    local currentPalette = getpalette()
    
    -- Compare with previous frame
    if lastPalette then
        for i = 0, 31 do
            if currentPalette[i] ~= lastPalette[i] then
                print(string.format("PALRAM[%02X] changed: %02X -> %02X", 
                    i, lastPalette[i], currentPalette[i]))
            end
        end
    end
    
    -- Save current palette for next frame
    lastPalette = currentPalette
end
```

**Example: Convert to RGB:**
```lua
function gui()
    local palette = getpalette()
    
    -- Convert first 4 entries to RGB
    for i = 0, 3 do
        local colorIndex = palette[i]
        local rgb = getcolorrgb(colorIndex)
        
        drawtext(4, 4 + i * 20, 
            string.format("PALRAM[%02X] = %02X -> RGB(%d,%d,%d)", 
            i, colorIndex, rgb[1], rgb[2], rgb[3]), 0x29)
        
        -- Draw color swatch
        fillrect(200, 4 + i * 20, 32, 16, colorIndex)
    end
end
```

### `setpalettecolor`

**Signature:** `setpalettecolor(index, color)`
Sets palette color in the NES palette RAM (PALRAM). Writes a color index (0-63) to the specified palette RAM location. Useful for palette effects, color cycling, and dynamic color changes during gameplay.

**Parameters:**
- `index` (integer): Palette RAM index (0-31)
  - Valid range: 0-31 (32 total palette RAM entries)
  - Each entry corresponds to a position in the NES palette RAM
  - Indices 0x00-0x0F: Background palettes
  - Indices 0x10-0x1F: Sprite palettes
- `color` (integer): Color index (0-63)
  - Valid range: 0-63 (64 total palette colors)
  - The actual palette color index to store at the specified PALRAM position
  - This value can be obtained from `getcolorrgb()` or palette analysis

**Returns:**
- Nothing

**Notes:**
- Writes directly to the NES palette RAM (PALRAM)
- Changes are temporary and persist until the game or emulator modifies PALRAM again
- The universal background color (PALRAM[0x00]) is automatically mirrored to PALRAM[0x04, 0x08, 0x0C] when set
- The universal sprite color (PALRAM[0x10]) is automatically mirrored to PALRAM[0x14, 0x18, 0x1C] when set
- Color values are automatically masked to 6 bits (0x3F), ensuring range 0-63
- Throws an error if `index` is outside the valid range (0-31) or if `color` is outside the valid range (0-63)
- Use with `getpalettecolor()` to read back values and `getcolorrgb()` to convert color indices to RGB
- Changes take effect immediately and affect rendering on the current frame

**Example: Basic Usage:**
```lua
-- Set background palette 0, color 1 to bright white
setpalettecolor(0x01, 0x20)
```

**Example: Color Cycling Effect:**
```lua
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Cycle through colors every 30 frames
    local colorIndex = (frameCounter // 30) % 64
    setpalettecolor(0x11, colorIndex)  -- Change sprite palette color
end
```

**Example: Mario Color Effects:**
```lua
-- Make Mario have crazy colors by cycling sprite palette
local colorSchemes = {
    {0x00, 0x20, 0x37, 0x29},  -- Rainbow
    {0x00, 0x16, 0x2A, 0x3B},  -- Neon
    {0x00, 0x29, 0x2A, 0x39},  -- Green
}

local currentScheme = 1
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Change scheme every 60 frames
    if frameCounter % 60 == 0 then
        currentScheme = ((frameCounter // 60) % #colorSchemes) + 1
    end
    
    local scheme = colorSchemes[currentScheme]
    setpalettecolor(0x10, scheme[1])  -- Universal sprite color
    setpalettecolor(0x11, scheme[2])  -- Main color
    setpalettecolor(0x12, scheme[3])  -- Secondary color
    setpalettecolor(0x13, scheme[4])  -- Accent color
end
```

**Example: Palette Flash Effect:**
```lua
local flashCounter = 0

function gui()
    flashCounter = flashCounter + 1
    
    -- Flash effect: alternate between normal and bright
    if (flashCounter // 10) % 2 == 0 then
        setpalettecolor(0x11, 0x16)  -- Normal red
    else
        setpalettecolor(0x11, 0x20)  -- Bright white
    end
end
```

**Example: Read and Modify:**
```lua
function gui()
    -- Read current palette color
    local currentColor = getpalettecolor(0x11)
    
    -- Modify it (cycle through colors)
    local newColor = (currentColor + 1) % 64
    setpalettecolor(0x11, newColor)
    
    -- Get RGB to display
    local rgb = getcolorrgb(newColor)
    drawtext(4, 4, string.format("Color: %02X RGB(%d,%d,%d)", 
          newColor, rgb[1], rgb[2], rgb[3]), 0x20)
end
```

**Example: Set Multiple Palette Entries:**
```lua
function gui()
    -- Set entire background palette 0
    setpalettecolor(0x00, 0x00)  -- Universal background (black)
    setpalettecolor(0x01, 0x20)  -- Color 1 (bright white)
    setpalettecolor(0x02, 0x16)  -- Color 2 (red)
    setpalettecolor(0x03, 0x29)  -- Color 3 (green)
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid parameters
    local success, err = pcall(function()
        setpalettecolor(0x11, 0x20)
    end)
    
    if not success then
        print("Error: " .. tostring(err))
    end
    
    -- Test with invalid index
    success, err = pcall(function()
        setpalettecolor(32, 0x20)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
    
    -- Test with invalid color
    success, err = pcall(function()
        setpalettecolor(0x11, 64)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
end
```

### `setpalette`

**Signature:** `setpalette(paletteTable)`
Sets palette in bulk operation. Allows you to set multiple palette RAM entries at once using a table of color values. More efficient than calling `setpalettecolor()` multiple times. Useful for palette swapping, color correction, and applying entire color schemes at once.

**Parameters:**
- `paletteTable` (table): Table containing color values for palette indices
  - Supports two formats:
    - **1-indexed array:** `{color1, color2, ..., color32}` where `[1]` maps to `PALRAM[0]`, `[2]` maps to `PALRAM[1]`, etc.
    - **0-indexed key-value pairs:** `{[0] = color0, [1] = color1, ..., [31] = color31}` where keys directly correspond to PALRAM indices
  - Each color value must be in range 0-63
  - You can provide a partial table to update only specific palette entries
  - Keys/index values must be in range 0-31 (for PALRAM indices)

**Returns:**
- Nothing

**Notes:**
- Writes directly to the NES palette RAM (PALRAM)
- Changes are temporary and persist until the game or emulator modifies PALRAM again
- The universal background color (PALRAM[0x00]) is automatically mirrored to PALRAM[0x04, 0x08, 0x0C] when set
- The universal sprite color (PALRAM[0x10]) is automatically mirrored to PALRAM[0x14, 0x18, 0x1C] when set
- Color values are automatically masked to 6 bits (0x3F), ensuring range 0-63
- Throws an error if `paletteTable` is not a table, if any index is outside the valid range (0-31), or if any color value is outside the valid range (0-63)
- More efficient than multiple `setpalettecolor()` calls when updating many palette entries
- Use with `getpalettecolor()` to read back values and verify changes

**Example: Basic Usage (1-indexed array):**
```lua
-- Set entire palette using 1-indexed array
local palette = {
    0x0D, 0x0D, 0x0D, 0x0D,  -- PALRAM[0-3]
    0x0E, 0x0E, 0x0E, 0x0E,  -- PALRAM[4-7]
    0x0F, 0x0F, 0x0F, 0x0F,  -- PALRAM[8-11]
    0x10, 0x10, 0x10, 0x10,  -- PALRAM[12-15]
    0x1D, 0x1D, 0x1D, 0x1D,  -- PALRAM[16-19]
    0x1E, 0x1E, 0x1E, 0x1E,  -- PALRAM[20-23]
    0x1F, 0x1F, 0x1F, 0x1F,  -- PALRAM[24-27]
    0x20, 0x20, 0x20, 0x20,  -- PALRAM[28-31]
}
setpalette(palette)
```

**Example: Key-Value Format (0-indexed):**
```lua
-- Set palette using 0-indexed keys
local palette = {
    [0] = 0x0D, [1] = 0x16, [2] = 0x27, [3] = 0x37,  -- BG Pal 0
    [4] = 0x0D, [5] = 0x16, [6] = 0x27, [7] = 0x37,  -- BG Pal 1
    [16] = 0x1D, [17] = 0x16, [18] = 0x27, [19] = 0x37,  -- SP Pal 0
}
setpalette(palette)
```

**Example: Partial Palette Update:**
```lua
-- Only update sprite palettes (0x10-0x1F)
local spritePalette = {}
for i = 16, 31 do
    spritePalette[i] = ((i - 16) * 2) % 64  -- Gradient pattern
end
setpalette(spritePalette)
```

**Example: Palette Swapping:**
```lua
-- Define multiple color schemes
local colorSchemes = {
    -- Grayscale scheme
    {
        0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E,
        0x0F, 0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10,
        0x1D, 0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E,
        0x1F, 0x1F, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x20,
    },
    -- Warm colors scheme
    {
        [0] = 0x0D, [1] = 0x16, [2] = 0x27, [3] = 0x37,
        [4] = 0x0D, [5] = 0x16, [6] = 0x27, [7] = 0x37,
        -- ... (rest of palette)
    },
}

local currentScheme = 1
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Switch schemes every 60 frames
    if frameCounter % 60 == 0 then
        currentScheme = ((frameCounter // 60) % #colorSchemes) + 1
        setpalette(colorSchemes[currentScheme])
    end
end
```

**Example: Save and Restore Palette:**
```lua
local originalPalette = {}

function gui()
    -- Save original palette on first frame
    if not originalPalette[0] then
        for i = 0, 31 do
            originalPalette[i] = getpalettecolor(i)
        end
    end
    
    -- ... do palette modifications ...
    
    -- Restore original palette
    setpalette(originalPalette)
end
```

**Example: Color Correction:**
```lua
-- Apply color correction by shifting all colors
function applyColorCorrection(shift)
    local correctedPalette = {}
    for i = 0, 31 do
        local originalColor = getpalettecolor(i)
        correctedPalette[i] = (originalColor + shift) % 64
    end
    setpalette(correctedPalette)
end

function gui()
    -- Apply +10 color shift
    applyColorCorrection(10)
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid table
    local success, err = pcall(function()
        setpalette({[0] = 0x20, [1] = 0x16})
    end)
    
    if not success then
        print("Error: " .. tostring(err))
    end
    
    -- Test with invalid table type
    success, err = pcall(function()
        setpalette("not a table")
    end)
    
    if not success then
        print("✓ Correctly caught non-table argument")
    end
    
    -- Test with invalid index
    success, err = pcall(function()
        setpalette({[32] = 0x20})  -- Index 32 is out of range
    end)
    
    if not success then
        print("✓ Correctly caught invalid index")
    end
    
    -- Test with invalid color value
    success, err = pcall(function()
        setpalette({[0] = 64})  -- Color 64 is out of range
    end)
    
    if not success then
        print("✓ Correctly caught invalid color value")
    end
end
```

### `loadpalette`

**Signature:** `loadpalette(path)`
Loads a palette from a `.pal` file and applies it to the emulator. The `.pal` file format contains 64 RGB colors (192 bytes total: 64 colors × 3 bytes per color). Useful for importing custom palettes, applying color correction presets, and loading community-created palette files.

**Parameters:**
- `path` (string): File path to the `.pal` file
  - Can be a relative path (searches in multiple locations) or absolute path
  - Relative paths are searched in the following order:
    1. `game:\<path>`
    2. `game:\lua\<path>`
    3. `game:\Lua\<path>`
    4. `hdd1:\fce360-enhanced\lua\<path>`
    5. `hdd1:\fce360-enhanced\Lua\<path>`
    6. `game:\<path>` (fallback)
  - Path separators (`/` or `\`) are automatically normalized
  - If the path contains a drive letter (`:`) or starts with `\` or `/`, it's treated as an absolute path

**Returns:**
- `boolean`: `true` if the palette was successfully loaded and applied, `false` if the file was not found, could not be read, or was invalid

**Notes:**
- The `.pal` file must be exactly 192 bytes (64 colors × 3 bytes RGB)
- File format: Raw RGB data, 3 bytes per color (Red, Green, Blue), for 64 colors total
- The palette is applied immediately using `FCEUI_SetPaletteArray`
- This replaces the entire NES 64-color palette, not just the 32 PALRAM entries
- Returns `false` if:
  - The path is empty or invalid
  - The file cannot be found in any of the search paths
  - The file exists but is not exactly 192 bytes
  - The file cannot be read
- Path resolution is similar to `readfile()` - supports multiple search locations for convenience
- Use `setpalette()` or `setpalettecolor()` if you need to modify individual PALRAM entries after loading

**Example: Basic Usage:**
```lua
function gui()
    -- Load a palette file from the lua directory
    local success = loadpalette("test.pal")
    
    if success then
        print("Palette loaded successfully!")
    else
        print("Failed to load palette file")
    end
end
```

**Example: Try Multiple Palette Files:**
```lua
local paletteFiles = {
    "test.pal",
    "warm.pal",
    "cool.pal",
    "grayscale.pal"
}

local currentPalette = 1

function gui()
    -- Try loading different palettes
    local success = loadpalette(paletteFiles[currentPalette])
    
    if success then
        print("Loaded: " .. paletteFiles[currentPalette])
    else
        print("Failed: " .. paletteFiles[currentPalette])
    end
end
```

**Example: Load Palette with Path Resolution:**
```lua
function gui()
    -- Try different path formats
    local paths = {
        "test.pal",                    -- Relative (searches in game:\ and game:\lua\)
        "lua\\test.pal",               -- Relative with subdirectory
        "game:\\lua\\test.pal",        -- Absolute path
    }
    
    for i, path in ipairs(paths) do
        local success = loadpalette(path)
        if success then
            print("✓ Loaded: " .. path)
            break
        else
            print("✗ Failed: " .. path)
        end
    end
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid file
    local success = loadpalette("test.pal")
    if success then
        print("✓ Palette loaded")
    else
        print("✗ Failed to load palette")
    end
    
    -- Test with non-existent file
    success = loadpalette("nonexistent.pal")
    if not success then
        print("✓ Correctly returned false for missing file")
    end
    
    -- Test with empty path
    success = loadpalette("")
    if not success then
        print("✓ Correctly returned false for empty path")
    end
end
```

**Example: Load Palette and Save Current One:**
```lua
local originalPalette = {}

function gui()
    -- Save original palette before loading new one
    if not originalPalette[0] then
        originalPalette = getpalette()
    end
    
    -- Load custom palette
    local success = loadpalette("custom.pal")
    
    if success then
        print("Custom palette applied")
    end
    
    -- Later, restore original palette
    -- setpalette(originalPalette)
end
```

## Color Blending Functions

### `blendcolors`

**Signature:** `blendcolors(color1, color2, ratio)`
Blends two palette colors and returns the closest matching palette color index. Performs RGB interpolation between the two input colors based on the specified ratio, then finds the nearest matching color from the NES 64-color palette. Useful for creating color gradients, smooth color transitions, and color mixing effects.

**Parameters:**
- `color1` (integer): First palette color index (0-63)
  - Valid range: 0-63 (64 total palette colors)
  - This color is used when `ratio` is 0.0 (100% color1)
- `color2` (integer): Second palette color index (0-63)
  - Valid range: 0-63 (64 total palette colors)
  - This color is used when `ratio` is 1.0 (100% color2)
- `ratio` (number): Blending ratio (0.0-1.0)
  - Valid range: 0.0 to 1.0 (inclusive)
  - 0.0 = 100% color1, 0% color2
  - 0.5 = 50% color1, 50% color2
  - 1.0 = 0% color1, 100% color2
  - Values are interpolated linearly between the two colors

**Returns:**
- `integer` - Closest matching palette color index (0-63)
  - The function calculates the blended RGB values mathematically
  - Then searches all 64 palette colors to find the closest match
  - Uses Euclidean distance in RGB color space to determine the best match
  - The returned color may not be a perfect intermediate due to the limited NES palette

**Notes:**
- Blends colors by interpolating RGB components: `blendedRGB = color1RGB * (1 - ratio) + color2RGB * ratio`
- Finds the closest matching palette color using Euclidean distance in RGB space
- The NES palette has only 64 colors, so the result may not be a perfect intermediate blend
- Useful for creating gradients, color cycling effects, and smooth color transitions
- More accurate than manual color mixing since it considers the full RGB color space
- Throws an error if any parameter is outside its valid range
- The blending is performed in RGB space, not in the palette index space
- Works best with colors that are relatively close in the color spectrum

**Example: Basic Blending:**
```lua
function gui()
    -- Blend red and white at 50%
    local blended = blendcolors(0x16, 0x20, 0.5)
    drawtext(4, 4, string.format("Blended color: 0x%02X", blended), 0x20)
    fillrect(4, 14, 40, 40, blended)
end
```

**Example: Animated Color Transition:**
```lua
local ratio = 0.0
local direction = 1

function gui()
    -- Animate ratio from 0.0 to 1.0 and back
    ratio = ratio + (0.01 * direction)
    if ratio >= 1.0 then
        ratio = 1.0
        direction = -1
    elseif ratio <= 0.0 then
        ratio = 0.0
        direction = 1
    end
    
    -- Blend red and green
    local blended = blendcolors(0x16, 0x29, ratio)
    fillrect(4, 4, 60, 60, blended)
    drawtext(4, 70, string.format("Ratio: %.2f", ratio), 0x20)
end
```

**Example: Create Gradient:**
```lua
function gui()
    local color1 = 0x16  -- Red
    local color2 = 0x29  -- Green
    
    -- Create a 16-step gradient
    for i = 0, 15 do
        local ratio = i / 15.0
        local gradColor = blendcolors(color1, color2, ratio)
        fillrect(4 + (i * 16), 4, 14, 40, gradColor)
    end
end
```

**Example: Multiple Blend Ratios:**
```lua
function gui()
    local color1 = 0x20  -- White
    local color2 = 0x00  -- Black
    
    local ratios = {0.0, 0.25, 0.5, 0.75, 1.0}
    local yPos = 4
    
    for i, ratio in ipairs(ratios) do
        local blended = blendcolors(color1, color2, ratio)
        drawtext(4, yPos, string.format("Ratio %.2f: 0x%02X", ratio, blended), 0x20)
        fillrect(120, yPos, 20, 12, blended)
        yPos = yPos + 14
    end
end
```

**Example: Color Mixing:**
```lua
function gui()
    -- Mix red and blue to get purple-like colors
    local red = 0x16
    local blue = 0x21
    
    -- Try different mix ratios
    local purple1 = blendcolors(red, blue, 0.3)  -- More red
    local purple2 = blendcolors(red, blue, 0.5)  -- Equal mix
    local purple3 = blendcolors(red, blue, 0.7)  -- More blue
    
    fillrect(4, 4, 40, 40, purple1)
    fillrect(50, 4, 40, 40, purple2)
    fillrect(96, 4, 40, 40, purple3)
end
```

**Example: Smooth Color Cycling:**
```lua
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- Cycle through color spectrum
    local color1 = 0x16  -- Red
    local color2 = 0x29  -- Green
    local color3 = 0x21  -- Blue
    
    -- Blend between colors based on frame
    local cycle = (frameCounter % 120) / 120.0
    
    local blended
    if cycle < 0.5 then
        -- Blend from red to green
        blended = blendcolors(color1, color2, cycle * 2.0)
    else
        -- Blend from green to blue
        blended = blendcolors(color2, color3, (cycle - 0.5) * 2.0)
    end
    
    fillrect(4, 4, 60, 60, blended)
end
```

**Example: Error Handling:**
```lua
function gui()
    -- Test with valid parameters
    local success, result = pcall(function()
        return blendcolors(0x16, 0x20, 0.5)
    end)
    
    if success then
        drawtext(4, 4, string.format("Blended: 0x%02X", result), 0x29)
    end
    
    -- Test with invalid color1
    success, err = pcall(function()
        return blendcolors(-1, 0x20, 0.5)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
    
    -- Test with invalid ratio
    success, err = pcall(function()
        return blendcolors(0x16, 0x20, 1.5)  -- Out of range
    end)
    
    if not success then
        print("Error caught: " .. tostring(err))
    end
end
```

## See Also

- **[Palette Reference](Palette-Reference)** - Complete NES 64-color palette reference
- **[Drawing Functions](Drawing-Functions)** - Functions for drawing with colors
- **[Memory Functions](Memory-Functions)** - Functions for reading and writing memory
- **[Home](Home)** - Return to the main wiki page
