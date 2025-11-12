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

- [Palette Reference](Palette-Reference) - Complete NES 64-color palette reference
- [Drawing Functions](Drawing-Functions) - Functions for drawing with colors
- [Memory Functions](Memory-Functions) - Functions for reading and writing memory
- [Home](Home) - Return to the main wiki page