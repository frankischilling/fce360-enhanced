# Drawing Functions

Complete reference for all drawing functions available in the Lua API. These functions allow you to draw text, shapes, images, and graphics primitives on the screen overlay.

## Text Functions

### `drawtext`

**Signature:** `drawtext(x, y, text [, color])`
Draws **borderless** text on the screen overlay using FCEUX's built-in font renderer. This function draws only the glyph pixels (characters) with no background, outline, or shadow.

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `text` (string): Text string to display. Multi-line text is not directly supported - use `drawtextwh()` for multi-line support.
- `color` (integer, optional): Palette color index (default: `0x20`). Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Text is drawn using an 8Ã—8 pixel font. Each character occupies 8 pixels horizontally.
- **Borderless rendering:** This function draws only the glyph pixels with no background, outline, or shadow effects. For text with borders/outlines, use `drawtextwh()`.
- Coordinates (0, 0) represent the top-left corner of the screen.
- Text drawn outside the visible area (0-255, 0-239) will be clipped automatically.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- The entire 8-pixel height row is cleared before drawing to prevent ghosting from previous frames.

**Example:**
```lua
drawtext(4, 4, "Hello World!", 0x20)        -- Bright white text at top-left (no border)
drawtext(100, 120, "Score: 1000", 0x39)     -- Yellow-green text centered (no border)
drawtext(4, 232, "Bottom text", 0x20)       -- Near bottom of screen (no border)
```

### `textstyle`

**Signature:** `textstyle(options)`

Sets advanced text rendering options that affect how `drawtext()` and related text functions render text. This function allows you to configure text appearance including size, alignment, word wrapping, shadow effects, and character spacing.

**Parameters:**
- `options` (table): A table containing one or more of the following optional keys:
  - `font` (integer, optional): Font index. Currently reserved for future use (default: 0).
  - `size` (number, optional): Text size/scale factor. Valid range is 0.5 to 4.0. Default is 1.0 (normal size). Values less than 1.0 make text smaller, values greater than 1.0 make text larger.
  - `wrap` (boolean, optional): Enable word wrapping. When `true`, text will wrap to the next line if it exceeds the available width. Default is `false`.
  - `align` (string, optional): Text alignment. Valid values are `"left"`, `"center"`, or `"right"`. Default is `"left"`.
  - `shadow` (boolean or integer, optional): Enable text shadow effect. When `true` or `1`, text will be drawn with a shadow offset by 1 pixel down and right. Default is `false`.
  - `spacing` (integer, optional): Character spacing in pixels. Valid range is -2 to 10. Positive values increase spacing between characters, negative values decrease spacing. Default is 0.

**Returns:** Nothing

**Notes:**
- Text style settings persist until changed by another call to `textstyle()`.
- All options are optional - you can set only the options you want to change.
- Invalid values are automatically clamped to valid ranges.
- The `size` option affects text scaling - larger values make text bigger but may reduce clarity.
- The `shadow` option draws a darker shadow behind the text for better visibility on complex backgrounds.
- The `align` option affects how text is positioned relative to the x coordinate when drawing.
- The `wrap` option enables automatic word wrapping for long text strings.
- The `spacing` option adjusts the horizontal spacing between characters.

**Example:**
```lua
-- Set text size to 1.5x normal
textstyle({size = 1.5})
drawtext(10, 10, "Large Text", 0x20)

-- Enable shadow for better visibility
textstyle({shadow = true})
drawtext(10, 30, "Text with shadow", 0x20)

-- Set multiple options at once
textstyle({
    size = 2.0,
    align = "center",
    shadow = true,
    spacing = 1
})
drawtext(128, 100, "Styled Text", 0x39)

-- Reset to defaults
textstyle({size = 1.0, shadow = false, align = "left", spacing = 0})
```

### `drawtextwh`

**Signature:** `drawtextwh(x, y, text, color, max_w, max_h, border)`
Draws text on the screen overlay with width/height clipping, multi-line support, and optional borders/outlines. This is the advanced text rendering function that supports bordered text for better visibility.

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `text` (string): Text string to display. Supports newline characters (`\n`) for multi-line text.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).
- `max_w` (integer): Maximum width in pixels for text rendering. Text will wrap to the next line if it exceeds this width. Maximum 256 pixels.
- `max_h` (integer): Maximum height in pixels for text rendering. Text beyond this height will be clipped. Maximum 64 pixels.
- `border` (integer): Border style (0, 1, or 2). Values are clamped to this range.
  - **`0`** - **Borderless:** Draws only glyph pixels with no background, outline, or shadow (same as `drawtext()`). Best for simple overlays.
  - **`1`** - **Thin outline:** Draws text with a 1-pixel outline/shadow for better contrast. Adds a dimmed background around text.
  - **`2`** - **Thick outline:** Draws text with a 2-pixel outline/shadow and enhanced background for maximum visibility. Best for text over busy backgrounds.

**Returns:** Nothing

**Notes:**
- Text is drawn using an 8Ã—8 pixel font. Each character occupies 8 pixels horizontally.
- **Multi-line support:** Use newline characters (`\n`) in the text string to create multi-line displays. Text automatically wraps within `max_w` pixels.
- **Border rendering:** When `border > 0`, the function draws a dimmed background (using palette indices 0xC1, 0xD1, 0xCF) around text for improved contrast and readability. Border style 2 provides the thickest outline for maximum visibility.
- **Borderless mode (`border = 0`):** When border is 0, the function proactively clears the specified `max_w Ã— max_h` area before drawing to prevent ghosting from previous frames, then draws only the glyph pixels.
- Coordinates (0, 0) represent the top-left corner of the screen.
- Text drawn outside the visible area (0-255, 0-239) will be clipped automatically.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- For simple single-line text without borders, `drawtext()` is more efficient and automatically handles ghosting prevention.

**Example:**
```lua
-- Borderless text (same as drawtext but with size limits)
drawtextwh(4, 4, "FPS: 60.0", 0x39, 200, 16, 0)

-- Multi-line text with thin border for better visibility
drawtextwh(10, 50, "Line 1\nLine 2\nLine 3", 0x20, 150, 32, 1)

-- Text with thick border for maximum contrast
drawtextwh(10, 100, "IMPORTANT!", 0x20, 200, 16, 2)

-- Wrapped text with border in a panel
fillrect(5, 115, 120, 60, 0x10)           -- Light gray background panel
drawrect(5, 115, 120, 60, 0x20)           -- Bright white border
drawtextwh(10, 120, "Status:\nHealth: 100\nScore: 5000", 0x39, 110, 50, 1)
```

**Border Style Comparison:**
- **`border = 0`:** Clean glyph-only rendering, no background interference. Fastest, least visual impact.
- **`border = 1`:** Thin outline provides good contrast on most backgrounds. Slightly dimmed background.
- **`border = 2`:** Thick outline with enhanced background for maximum readability. Best for text over complex or moving backgrounds.

### `drawtextscaled`

**Signature:** `drawtextscaled(x, y, text, color, scaleX, scaleY)`
Draws text on the screen overlay with custom X and Y scaling. This function allows you to render text at different sizes, from smaller (0.5x) to much larger (4.0x), with independent horizontal and vertical scaling for special effects.

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `text` (string): Text string to display. Supports newline characters (`\n`) for multi-line text.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).
- `scaleX` (float): Horizontal scaling factor. Valid range is 0.5-4.0. Values outside this range are automatically clamped.
- `scaleY` (float): Vertical scaling factor. Valid range is 0.5-4.0. Values outside this range are automatically clamped.

**Returns:** Nothing

**Notes:**
- Text is drawn using an 8Ã—8 pixel font. Each character is scaled independently based on `scaleX` and `scaleY`.
- **Scaling method:** Uses nearest-neighbor scaling, where each source pixel is expanded to a block of pixels. This provides crisp, pixelated scaling suitable for retro-style graphics.
- **Scale range:** Both `scaleX` and `scaleY` are clamped to the range 0.5-4.0. Values below 0.5 are set to 0.5, values above 4.0 are set to 4.0.
- **Anisotropic scaling:** You can use different values for `scaleX` and `scaleY` to create wide or tall text effects (e.g., `scaleX=2.0, scaleY=1.0` for wide text, or `scaleX=1.0, scaleY=2.0` for tall text).
- **Multi-line support:** Use newline characters (`\n`) in the text string to create multi-line displays. Line spacing scales with `scaleY`.
- **Borderless rendering:** This function draws only the glyph pixels with no background, outline, or shadow effects (similar to `drawtext()`).
- Coordinates (0, 0) represent the top-left corner of the screen.
- Text drawn outside the visible area (0-255, 0-239) will be clipped automatically.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- For simple text without scaling, `drawtext()` is more efficient. For text with borders, use `drawtextwh()`.

**Example:**
```lua
-- Normal size text (1.0x scale)
drawtextscaled(4, 4, "Normal Text", 0x20, 1.0, 1.0)

-- Small text (0.5x scale - minimum)
drawtextscaled(4, 20, "Small Text", 0x30, 0.5, 0.5)

-- Large text (2.0x scale)
drawtextscaled(4, 36, "Large Text", 0x20, 2.0, 2.0)

-- Very large text (4.0x scale - maximum)
drawtextscaled(4, 60, "HUGE!", 0x20, 4.0, 4.0)

-- Wide text (stretched horizontally)
drawtextscaled(4, 100, "Wide Text", 0x39, 2.0, 1.0)

-- Tall text (stretched vertically)
drawtextscaled(4, 120, "Tall", 0x37, 1.0, 2.0)

-- Anisotropic scaling (different X and Y scales)
drawtextscaled(4, 150, "Aniso", 0x26, 1.5, 2.5)

-- Multi-line scaled text
drawtextscaled(4, 180, "Line 1\nLine 2\nLine 3", 0x20, 1.5, 1.5)
```

### `drawtextrotated`

**Signature:** `drawtextrotated(x, y, text, color, angle)`
Draws text on the screen overlay rotated by a specified angle in degrees. The text rotates around its top-left origin point (x, y).

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels. This is the rotation origin point.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels. This is the rotation origin point.
- `text` (string): Text string to display. Supports newline characters (`\n`) for multi-line text.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).
- `angle` (integer): Rotation angle in degrees. Valid range is 0-360. Values outside this range are automatically normalized (e.g., 370Â° becomes 10Â°, -10Â° becomes 350Â°).

**Returns:** Nothing

**Notes:**
- **Rotation origin:** Text rotates around the (x, y) point, which is the top-left corner of the unrotated text.
- **Angle convention:** 
  - `0Â°` = Text points right (normal, unrotated)
  - `90Â°` = Text points down (rotated 90Â° clockwise)
  - `180Â°` = Text points left (upside down)
  - `270Â°` = Text points up (rotated 270Â° clockwise)
- **Angle wrapping:** Angles are automatically normalized to 0-360 range. Negative angles and angles > 360 are wrapped (e.g., -10Â° â†’ 350Â°, 370Â° â†’ 10Â°).
- **Multi-line support:** Use newline characters (`\n`) in the text string to create multi-line displays. Each line advances 8 pixels vertically in the unrotated coordinate space.
- **Borderless rendering:** This function draws only the glyph pixels with no background, outline, or shadow effects (similar to `drawtext()`).
- **Performance:** For unrotated text (angle = 0Â°), the function uses a fast path that calls `drawtext()` directly for better performance.
- **Clipping:** Rotated text pixels are individually clipped to the screen bounds (0-255, 0-239). Text that rotates outside the visible area will be clipped.
- **Blending:** Rotated text respects the current drawing mode set by `setdrawmode()`.
- Coordinates (0, 0) represent the top-left corner of the screen.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- For simple unrotated text, `drawtext()` is more efficient. For text with borders, use `drawtextwh()`. For scaled text, use `drawtextscaled()`.

**Example:**
```lua
-- Normal text (0 degrees - unrotated)
drawtextrotated(128, 120, "Hello", 0x20, 0)

-- Text rotated 90 degrees (points down)
drawtextrotated(128, 120, "Down", 0x39, 90)

-- Text rotated 180 degrees (upside down)
drawtextrotated(128, 120, "Upside", 0x20, 180)

-- Text rotated 270 degrees (points up)
drawtextrotated(128, 120, "Up", 0x3C, 270)

-- Animated rotating text
local frameCount = 0
function script()
    frameCount = frameCount + 1
    local angle = frameCount % 360
    drawtextrotated(128, 120, "SPIN", 0x20, angle)
end
```

### `gettextwidth`

**Signature:** `gettextwidth(text)`
Returns the pixel width of the longest line in a text string, using the same variable-width font metrics as the text drawing functions. This is useful for centering text, calculating layout, and determining if text will fit in a given space.

**Parameters:**
- `text` (string): Text string to measure. Supports newline characters (`\n`) for multi-line text, tabs (`\t`) which are treated as 4 spaces, and carriage returns (`\r`) which are ignored.

**Returns:** Integer - The pixel width of the longest line in the text string. Returns 0 for empty strings.

**Notes:**
- **Variable-width font:** Uses the same font metrics (`Font6x7` + `JoedCharWidth`) as `drawtext()`, `drawtextwh()`, `drawtextscaled()`, and `drawtextrotated()`, so measurements match exactly how text is rendered.
- **Multi-line handling:** For text with newlines, returns the width of the longest line, not the total width of all lines.
- **Tab handling:** Tab characters (`\t`) are treated as 4 spaces. The width of a tab is calculated as 4 Ã— the width of a space character.
- **Empty strings:** Returns 0 for empty strings or strings containing only whitespace/newlines.
- **Carriage returns:** Carriage return characters (`\r`) are ignored (handles Windows-style line endings `\r\n`).
- **Non-printable characters:** Characters that don't map to valid font glyphs contribute 0 pixels to the width.
- This function is useful for:
  - Centering text: `centerX = 128 - math.floor(gettextwidth(text) / 2)`
  - Right-aligning text: `rightX = maxX - gettextwidth(text)`
  - Checking if text fits: `if gettextwidth(text) <= availableWidth then ...`
  - Calculating layout and spacing

**Example:**
```lua
-- Measure single-line text
local width = gettextwidth("Hello")
print(string.format("'Hello' is %d pixels wide", width))

-- Center text on screen
local text = "CENTERED"
local textWidth = gettextwidth(text)
local centerX = 128 - math.floor(textWidth / 2)  -- Screen center is 128
drawtext(centerX, 120, text, 0x20)
```

### `gettextheight`

**Signature:** `gettextheight(text)`
Returns the pixel height of a text string, calculated as the number of lines multiplied by 8 pixels (the glyph height). This is useful for calculating vertical layout, determining if text will fit in a given height, and positioning multi-line text.

**Parameters:**
- `text` (string): Text string to measure. Supports newline characters (`\n`) which separate lines. Trailing newlines count as an extra empty line.

**Returns:** Integer - The pixel height of the text string. Returns 0 for empty strings. Each line contributes 8 pixels to the total height.

**Notes:**
- **Line counting:** The function counts lines separated by `\n` characters. A single-line string (no newlines) has height 8 pixels.
- **Trailing newlines:** A trailing `\n` adds an extra empty line. For example, `"Hello\n"` has 2 lines = 16 pixels.
- **Empty strings:** Returns 0 for empty strings or null input.
- **Glyph height:** Each line is 8 pixels tall (`GLYPH_H = 8`), matching the font used by all text drawing functions.
- **Tabs and other characters:** Tab characters and other non-newline characters don't affect the height calculation - only `\n` characters create new lines.
- This function is useful for:
  - Calculating vertical layout: `totalHeight = gettextheight(multiLineText)`
  - Checking if text fits: `if gettextheight(text) <= availableHeight then ...`
  - Centering text vertically: `centerY = 120 - math.floor(gettextheight(text) / 2)`
  - Calculating spacing between text blocks

**Example:**
```lua
-- Measure single-line text
local height = gettextheight("Hello")
print(string.format("'Hello' is %d pixels tall", height))  -- 8 pixels

-- Measure multi-line text
local multiHeight = gettextheight("Line 1\nLine 2\nLine 3")
print(string.format("3 lines = %d pixels tall", multiHeight))  -- 24 pixels

-- Center text vertically
local text = "Centered\nText"
local textHeight = gettextheight(text)
local centerY = 120 - math.floor(textHeight / 2)  -- Screen center is 120
drawtext(4, centerY, text, 0x20)
```

### `measuretextblock`

**Signature:** `measuretextblock(text, width)`

Calculates the dimensions of text when wrapped to a specified width. Returns a table with width, height, and line count. This function is useful for text layout calculations, determining how much space wrapped text will occupy, and planning UI layouts before drawing.

**Parameters:**
- `text` (string): Text string to measure. Supports newline characters (`\n`) for explicit line breaks and word wrapping when text exceeds the specified width.
- `width` (integer): Wrap width in pixels. Text will wrap to new lines when it exceeds this width. Must be positive.

**Returns:**
- `table`: A table containing:
  - `width` (integer): Maximum line width in pixels (the widest line after wrapping)
  - `height` (integer): Total height in pixels (lineCount × lineHeight, accounting for text style size)
  - `lineCount` (integer): Number of lines (including wrapped lines and explicit newlines)

**Notes:**
- **Word wrapping:** Text wraps intelligently at word boundaries (spaces) when possible. If a word is too long to fit on a line, it will be broken mid-word.
- **Text style support:** Respects `textstyle()` settings:
  - `size` - Text size scaling affects both character widths and line height
  - `spacing` - Character spacing is included in width calculations
- **Explicit newlines:** Newline characters (`\n`) in the text create explicit line breaks, which are counted in `lineCount`.
- **Empty strings:** Returns `{width=0, height=0, lineCount=0}` for empty strings or invalid width.
- **Invalid width:** Returns zeros for width ≤ 0.
- **Variable-width font:** Uses the same font metrics (`JoedCharWidth`) as other text functions for accurate measurements.
- **Line height:** Each line is 8 pixels tall (scaled by text style size), matching the font used by text drawing functions.

**Example:**
```lua
-- Measure simple text
local metrics = measuretextblock("Hello World", 200)
print(string.format("Width: %d, Height: %d, Lines: %d", 
    metrics.width, metrics.height, metrics.lineCount))

-- Measure wrapped text
local longText = "This is a very long text that will wrap to multiple lines"
local wrapped = measuretextblock(longText, 100)
print(string.format("Wrapped to %d lines, max width: %d", 
    wrapped.lineCount, wrapped.width))

-- Calculate layout before drawing
local text = "Status: OK\nHealth: 100\nScore: 5000"
local metrics = measuretextblock(text, 150)
local boxX = 10
local boxY = 10
local boxW = metrics.width + 8  -- Add padding
local boxH = metrics.height + 8

-- Draw background box
fillrect(boxX, boxY, boxW, boxH, 0x10)
drawrect(boxX, boxY, boxW, boxH, 0x20)

-- Draw text centered in box
drawtextwh(boxX + 4, boxY + 4, text, 0x20, boxW - 8, boxH - 8, 0)

-- Measure with text style
textstyle({size = 2.0})
local largeMetrics = measuretextblock("Large Text", 200)
print(string.format("Large text: width=%d, height=%d", 
    largeMetrics.width, largeMetrics.height))
textstyle({size = 1.0})  -- Reset

-- Measure with character spacing
textstyle({spacing = 2})
local spacedMetrics = measuretextblock("Spaced Text", 200)
print(string.format("Spaced text width: %d", spacedMetrics.width))
textstyle({spacing = 0})  -- Reset
```

### `drawtextbox`

**Signature:** `drawtextbox(x, y, width, height, text, color, bgColor, borderColor)`
Draws text in a bordered box with an optional background. This is useful for creating dialog boxes, message displays, info panels, and other UI elements that need visual boundaries.

**Parameters:**
- `x` (integer): X coordinate of the top-left corner of the box (0-255).
- `y` (integer): Y coordinate of the top-left corner of the box (0-239).
- `width` (integer): Width of the box in pixels. Must be positive.
- `height` (integer): Height of the box in pixels. Must be positive.
- `text` (string): Text string to display inside the box. Supports newline characters (`\n`) for multi-line text.
- `color` (integer): Text color. Valid range is 0x00-0x3F (automatically mapped to NES palette range).
- `bgColor` (integer, optional): Background color for the box interior. Valid range is 0x00-0x3F. If `nil` or omitted, no background is drawn.
- `borderColor` (integer, optional): Border color for the box. Valid range is 0x00-0x3F. If `nil` or omitted, no border is drawn. The border is 3 pixels thick.

**Returns:** Nothing

**Notes:**
- **Border thickness:** The border is always 3 pixels thick when `borderColor` is specified.
- **Text padding:** Text is drawn with 2 pixels of padding from the inner edges of the box (after accounting for the border).
- **Text wrapping:** Text automatically wraps within the available space inside the box.
- **Multi-line support:** Supports newline characters (`\n`) for explicit line breaks.
- **Optional parameters:** Both `bgColor` and `borderColor` are optional. You can specify:
  - Both: Full box with background and border
  - Only `bgColor`: Box with background but no border
  - Only `borderColor`: Box with border but no background (transparent interior)
  - Neither: Just text (no box, equivalent to `drawtext()`)
- **Coordinate clamping:** The box is automatically clamped to screen bounds if it extends beyond the visible area.
- **Text clipping:** Text that doesn't fit in the box is clipped to the available space.

**Example:**
```lua
-- Full box with background and border
drawtextbox(10, 10, 100, 40, "Dialog Box", 0x20, 0x10, 0x20)

-- Background only (no border)
drawtextbox(10, 60, 100, 40, "Info Panel", 0x20, 0x10, nil)

-- Border only (no background, transparent interior)
drawtextbox(10, 110, 100, 40, "Border Only", 0x20, nil, 0x20)

-- Multi-line text in a box
drawtextbox(120, 10, 100, 60, "Line 1\nLine 2\nLine 3", 0x20, 0x10, 0x20)
```

## Basic Drawing Functions

### `drawpixel`

**Signature:** `drawpixel(x, y, color)`
Draws a single pixel at the specified coordinates.

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn pixels appear above game graphics.
- Useful for drawing custom shapes, lines, or individual pixels for debugging.

**Example:**
```lua
-- Draw a diagonal line of pixels
for i = 0, 20 do
  drawpixel(10 + i, 10 + i, 0x39)  -- Yellow-green diagonal line
end

-- Draw a single pixel
drawpixel(128, 120, 0x20)  -- Bright white pixel at screen center
```

### `drawline`

**Signature:** `drawline(x1, y1, x2, y2, color)`
Draws a line between two points using Bresenham's line algorithm.

**Parameters:**
- `x1` (integer): Starting X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): Starting Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Ending X coordinate (0-255).
- `y2` (integer): Ending Y coordinate (0-239).
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn lines appear above game graphics.
- Supports all line directions: horizontal, vertical, diagonal, and any angle.
- Uses efficient Bresenham's algorithm for smooth, accurate lines.
- For lines with thickness greater than 1 pixel, use `drawthickline()`.

**Example:**
```lua
-- Draw a crosshair at screen center
drawline(108, 120, 148, 120, 0x20)  -- Horizontal line
drawline(128, 100, 128, 140, 0x20)  -- Vertical line

-- Draw a box using lines
drawline(200, 30, 250, 30, 0x39)    -- Top
drawline(250, 30, 250, 80, 0x39)    -- Right
drawline(250, 80, 200, 80, 0x39)    -- Bottom
drawline(200, 80, 200, 30, 0x39)    -- Left
```

### `drawthickline`

**Signature:** `drawthickline(x1, y1, x2, y2, thickness, color)`
Draws a thick line between two points with a specified thickness using perpendicular line segments.

**Parameters:**
- `x1` (integer): Starting X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): Starting Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Ending X coordinate (0-255).
- `y2` (integer): Ending Y coordinate (0-239).
- `thickness` (integer): Line thickness in pixels. Automatically clamped to 1-50 range for performance.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The line thickness is specified in pixels and represents the width of the line perpendicular to its direction.
- Thickness is automatically clamped to 1-50 range. Values below 1 are set to 1, values above 50 are set to 50.
- Uses Bresenham's line algorithm for the main line, with perpendicular line segments drawn at each point to create thickness.
- For very short lines or single points, draws a filled circle instead of a line.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn thick lines appear above game graphics.
- Supports all line directions: horizontal, vertical, diagonal, and any angle.
- For thin lines (thickness 1), `drawline()` is more efficient.
- Useful for drawing borders, arrows, indicators, or any graphics that require visible line width.

**Example:**
```lua
-- Draw horizontal thick lines with different thicknesses
drawthickline(20, 30, 80, 30, 1, 0x20)   -- Bright white, thickness 1 (same as drawline)
drawthickline(20, 40, 80, 40, 3, 0x39)   -- Yellow-green, thickness 3
drawthickline(20, 50, 80, 50, 5, 0x16)   -- Red / orange-red, thickness 5

-- Draw crosshair with thick lines
drawthickline(108, 120, 148, 120, 3, 0x20)  -- Horizontal thick line
drawthickline(128, 100, 128, 140, 3, 0x20)  -- Vertical thick line
```

## Rectangle Functions

### `drawrect`

**Signature:** `drawrect(x, y, w, h, color)`
Draws a rectangle outline (border only) at the specified position and size.

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `w` (integer): Width of the rectangle in pixels. Must be positive.
- `h` (integer): Height of the rectangle in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The rectangle is drawn as an outline only (border), not filled.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn rectangles appear above game graphics.
- Useful for drawing borders, boxes, panels, or highlighting areas of the screen.
- For a filled rectangle, use `fillrect()` or draw multiple lines/pixels.

**Example:**
```lua
-- Draw a simple rectangle border
drawrect(10, 50, 60, 40, 0x20)  -- Bright white outline rectangle

-- Draw a border around an area
drawrect(5, 115, 120, 60, 0x20) -- Bright white border around panel
```

### `fillrect`

**Signature:** `fillrect(x, y, w, h, color)`
Draws a filled rectangle (solid color) at the specified position and size.

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `w` (integer): Width of the rectangle in pixels. Must be positive.
- `h` (integer): Height of the rectangle in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The rectangle is completely filled with the specified color (solid rectangle).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn rectangles appear above game graphics.
- Useful for drawing backgrounds, progress bars, panels, or any solid colored areas.
- Combine with `drawrect()` to create bordered panels: first fill, then draw border.

**Example:**
```lua
-- Draw a simple filled rectangle
fillrect(10, 50, 60, 40, 0x10)  -- Light gray filled rectangle

-- Draw a progress bar
local barWidth = 100  -- Progress percentage
fillrect(10, 100, barWidth, 8, 0x39)  -- Filled progress bar
drawrect(10, 100, 100, 8, 0x20)        -- Border around progress bar

-- Draw a background panel with border
fillrect(5, 115, 120, 60, 0x10)       -- Light gray background
drawrect(5, 115, 120, 60, 0x20)      -- Bright white border around panel
```

### `clearrect`

**Signature:** `clearrect(x, y, w, h)`
Clears a rectangle area, making it transparent (removes any overlay content in that region).

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `w` (integer): Width of the rectangle to clear in pixels. Must be positive.
- `h` (integer): Height of the rectangle to clear in pixels. Must be positive.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- Clearing sets pixels to 0 (transparent), which means they won't overwrite the NES frame during compositing.
- Useful for preventing ghosting when redrawing dynamic content that changes size or position.
- Best practice: Clear areas before redrawing text or panels that update each frame.
- Pixels cleared outside the visible area (0-255, 0-239) are ignored (silently clipped).
- Unlike `fillrect()`, this function doesn't require a color parameter - it always clears to transparent.

**Example:**
```lua
-- Clear the entire screen overlay
clearrect(0, 0, 256, 240)

-- Clear a specific panel area before redrawing
clearrect(5, 115, 120, 60)  -- Clear panel area
fillrect(5, 115, 120, 60, 0x10)  -- Redraw with new content
drawrect(5, 115, 120, 60, 0x20)

-- Clear text area before updating (prevents ghosting)
clearrect(6, 170, 80, 8)  -- Clear FPS text area
drawtext(6, 170, string.format("FPS: %.1f", fps), 0x39)
```

### `clearscreen`

**Signature:** `clearscreen()`
Clears the entire overlay screen, making it transparent (removes all overlay content).

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Clears the entire overlay buffer (256Ã—240 pixels) by setting all pixels to 0 (transparent).
- Equivalent to calling `clearrect(0, 0, 256, 240)`, but more efficient.
- Useful for full screen clears when you want to start fresh each frame.
- Clearing sets pixels to transparent, which means they won't overwrite the NES frame during compositing.
- Best practice: Call `clearscreen()` at the start of each frame if you're redrawing everything, or use `clearrect()` for partial clears.

**Example:**
```lua
-- Clear entire screen at start of frame
clearscreen()

-- Then draw your overlay content
drawtext(10, 10, "Score: " .. score, 0x39)
fillrect(10, 20, 100, 8, 0x16)
```

### `fillscreen`

**Signature:** `fillscreen(color)`
Fills the entire overlay screen with a specified color.

**Parameters:**
- `color` (integer): Palette color index (0x00-0x3F). NES palette has 64 colors.

**Returns:** Nothing

**Notes:**
- Fills the entire overlay buffer (256Ã—240 pixels) with the specified color.
- Equivalent to calling `fillrect(0, 0, 256, 240, color)`, but more efficient.
- Respects the current drawing mode (normal, add, subtract, multiply, alpha) set by `setdrawmode()`.
- Useful for screen overlays, fade effects, and full-screen color fills.
- The color is automatically mapped to the overlay format (0x00-0x3F â†’ 0x80-0xBF).
- Unlike `clearscreen()`, this fills with a visible color rather than making pixels transparent.

**Example:**
```lua
-- Fill screen with a solid color
fillscreen(0x16)  -- Red/orange fill

-- Create a fade effect using alpha blending
setdrawmode("alpha")
fillscreen(0x16)  -- Semi-transparent red overlay
setdrawmode("normal")  -- Reset to normal
```

### `screenshot`

**Signature:** `screenshot([filename])`
Takes a screenshot of the current frame, including all overlays and Lua-drawn content. Screenshots are saved as PNG files to the `game:\snaps` directory.

**Parameters:**
- `filename` (string, optional): Custom filename for the screenshot
  - If provided, saves to that name (adds `.png` extension if missing)
  - If `nil` or not provided, uses auto-generated name (e.g., "Game - 1.png")
  - Auto-generated names include the ROM name and an incrementing number

**Returns:**
- `string` - The filename of the saved screenshot (without full path)
  - Returns the actual filename used (may differ if auto-generated)
  - Returns `nil` on error (e.g., directory creation failed)

**Notes:**
- Screenshots are saved to `game:\snaps` directory
- The directory is automatically created if it doesn't exist
- Screenshots include the NES frame, all overlays, and all Lua-drawn content
- Useful for automated screenshots, recording, and script-controlled captures
- Screenshot format is PNG (Portable Network Graphics)
- Auto-generated filenames use the format: "ROM Name - N.png" where N is an incrementing number
- Custom filenames are used as-is (with `.png` extension added if missing)
- Screenshots are saved synchronously - the function returns after the file is written
- Useful for creating automated screenshot sequences, recording gameplay highlights, or capturing specific moments

**Example: Basic Usage:**
```lua
function script()
    -- Take screenshot with auto-generated name
    local filename = screenshot()
    if filename then
        drawtext(4, 4, string.format("Screenshot: %s", filename), 0x29)
    end
end
```

**Example: Custom Filename:**
```lua
function script()
    -- Take screenshot with custom name
    local filename = screenshot("my_screenshot")
    if filename then
        drawtext(4, 4, string.format("Saved: %s", filename), 0x29)
    end
end
```

**Example: Automated Screenshot Sequence:**
```lua
local screenshotCount = 0
local lastScreenshotFrame = 0

function script()
    local frame = getframecount()
    
    -- Take screenshot every 300 frames (~5 seconds at 60fps)
    if frame - lastScreenshotFrame >= 300 then
        screenshotCount = screenshotCount + 1
        local filename = screenshot(string.format("sequence_%03d", screenshotCount))
        if filename then
            print(string.format("Screenshot %d: %s", screenshotCount, filename))
        end
        lastScreenshotFrame = frame
    end
    
    -- Display screenshot counter
    drawtext(4, 4, string.format("Screenshots: %d", screenshotCount), 0x39)
end
```

**Example: Screenshot on Event:**
```lua
local lastHealth = 100

function script()
    local health = readbyte(0x006A)  -- Example health address
    
    -- Take screenshot when health drops below threshold
    if health < 50 and lastHealth >= 50 then
        local filename = screenshot(string.format("low_health_%d", getframecount()))
        if filename then
            print(string.format("Low health screenshot: %s", filename))
        end
    end
    
    lastHealth = health
end
```

### `screenshotregion`

**Signature:** `screenshotregion(x, y, w, h, path)`
Captures a partial screen region and saves it as a PNG file. The captured region includes both the NES frame and all Lua-drawn overlays, matching what is displayed on screen.

**Parameters:**
- `x` (integer): X coordinate of the top-left corner of the region to capture (0-255)
- `y` (integer): Y coordinate of the top-left corner of the region to capture (0-239)
- `w` (integer): Width of the region to capture (must be positive)
- `h` (integer): Height of the region to capture (must be positive)
- `path` (string): Filename for the screenshot (required)

**Returns:**
- `boolean` - `true` if the screenshot was saved successfully, `false` on failure

**Notes:**
- Screenshots are saved to `game:\snaps` directory
- The directory is automatically created if it doesn't exist
- The captured region includes the NES frame, all overlays, and all Lua-drawn content
- The `.png` extension is automatically added to the filename if not present
- The region must fit within the screen bounds (256x240)
- The saved image is 256x240 pixels with the captured region positioned at the top-left corner
- Useful for capturing specific areas of the screen, UI elements, or game regions
- Screenshots are saved synchronously - the function returns after the file is written
- Returns `false` (instead of throwing an error) if the screenshot fails to save

**Example: Basic Usage:**
```lua
function gui()
    -- Capture a 100x50 region starting at (10, 30)
    local success = screenshotregion(10, 30, 100, 50, "region1")
    if success then
        drawtext(4, 4, "Region captured!", 0x29)
    end
end
```

**Example: Capture Multiple Regions:**
```lua
local frameCount = 0
local lastScreenshotFrame = 0

function gui()
    local frame = getframecount()
    
    -- Capture different regions with delay between screenshots
    if frame - lastScreenshotFrame >= 60 then
        if frameCount == 0 then
            screenshotregion(0, 0, 64, 32, "top_left")
        elseif frameCount == 1 then
            screenshotregion(192, 208, 64, 32, "bottom_right")
        elseif frameCount == 2 then
            screenshotregion(10, 30, 100, 50, "center_region")
        end
        frameCount = frameCount + 1
        lastScreenshotFrame = frame
    end
end
```

**Example: Capture UI Element:**
```lua
function gui()
    -- Draw some UI
    fillrect(10, 10, 100, 30, 0x16)
    drawtext(15, 15, "Score: 1000", 0x20)
    
    -- Capture just the UI element
    local success = screenshotregion(10, 10, 100, 30, "ui_element")
    if success then
        print("UI element captured")
    end
end
```

## Image Functions

### `drawimage`

**Signature:** `drawimage(x, y, imageData, width, height)`
Draws an image from a table of color values (byte data).

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `imageData` (table): Table containing color values in row-major order. Each value must be a palette color index (0x00-0x3F). The table must contain at least `width * height` elements.
- `width` (integer): Width of the image in pixels. Must be positive.
- `height` (integer): Height of the image in pixels. Must be positive.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The image data table is read in row-major order: pixels are arranged left-to-right, top-to-bottom.
- Color values are automatically clamped to the valid range (0x00-0x3F) and mapped to the NES palette.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn images appear above game graphics.
- Useful for drawing custom sprites, icons, logos, or game-specific graphics.
- The function validates that the imageData table contains sufficient data (at least `width * height` elements).

**Example:**
```lua
-- Draw an 8x8 sprite (64 pixels total)
local spriteData = {
    0x20, 0x39, 0x20, 0x39, 0x20, 0x39, 0x20, 0x39,  -- Row 1
    0x39, 0x20, 0x39, 0x20, 0x39, 0x20, 0x39, 0x20,  -- Row 2
    -- ... more rows
}
drawimage(10, 10, spriteData, 8, 8)
```

### `drawimageindexed`

**Signature:** `drawimageindexed(x, y, imageData, palette, width, height)`
Draws an image using indexed palette mapping. The image data contains palette indices that are looked up in a separate palette table to get the actual color values.

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `imageData` (table): Table containing palette indices (1-based) in row-major order. Each value is an index into the `palette` table. The table must contain at least `width * height` elements.
- `palette` (table): Table containing color values. Each value must be a palette color index (0x00-0x3F). The palette table can contain up to 256 colors. Palette indices in `imageData` reference this table (1 = first color, 2 = second color, etc.).
- `width` (integer): Width of the image in pixels. Must be positive.
- `height` (integer): Height of the image in pixels. Must be positive.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The image data table is read in row-major order: pixels are arranged left-to-right, top-to-bottom.
- Palette indices in `imageData` use 1-based indexing (1, 2, 3...) to match Lua's table indexing convention.
- Color values in the palette table are automatically clamped to the valid range (0x00-0x3F) and mapped to the NES palette.
- Palette indices that are out of range are automatically clamped to valid palette entries.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn images appear above game graphics.
- Useful for efficient sprite drawing where the same sprite data can be reused with different palettes.
- This is more memory-efficient than `drawimage()` when you have multiple color variations of the same sprite.

**Example:**
```lua
-- Define a 4-color palette
local palette1 = {0x20, 0x39, 0x16, 0x3D}  -- Bright white, Yellow-green, Red / orange-red, Silver

-- 8x8 sprite data using palette indices (1-4, Lua 1-based)
local spriteData = {
    1, 2, 1, 2, 1, 2, 1, 2,  -- Row 1
    2, 1, 2, 1, 2, 1, 2, 1,  -- Row 2
    -- ... more rows
}

-- Draw sprite with palette1
drawimageindexed(10, 10, spriteData, palette1, 8, 8)

-- Reuse the same sprite data with a different palette
local palette2 = {0x20, 0x16, 0x26, 0x37}  -- Different color scheme
drawimageindexed(100, 10, spriteData, palette2, 8, 8)
```

### `drawimageex`

**Signature:** `drawimageex(img, x, y, options)`
Extended image drawing function with support for transformations including rotation, scaling, flipping, and tinting. This is a flexible single-call function for advanced image rendering.

**Parameters:**
- `img` (table): Image data table containing color values in row-major order. Each value must be a palette color index (0x00-0x3F). The table must contain at least `w * h` elements.
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `options` (table): Table containing drawing options:
  - `w` (integer, required): Source width of the image in pixels. Must be positive.
  - `h` (integer, required): Source height of the image in pixels. Must be positive.
  - `dstW` (integer, optional): Destination width for scaling. If not specified, defaults to `w` (no scaling).
  - `dstH` (integer, optional): Destination height for scaling. If not specified, defaults to `h` (no scaling).
  - `rot` (number, optional): Rotation angle in degrees. Positive values rotate clockwise. Default is 0 (no rotation).
  - `flipX` (boolean, optional): If `true`, flips the image horizontally. Default is `false`.
  - `flipY` (boolean, optional): If `true`, flips the image vertically. Default is `false`.
  - `tint` (integer, optional): Color index (0x00-0x3F) to tint the image. Applies a 50% blend with the tint color. If not specified, no tinting is applied.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The image data table is read in row-major order: pixels are arranged left-to-right, top-to-bottom.
- Rotation is applied around the center of the destination image.
- Scaling uses nearest-neighbor sampling for performance.
- Flipping is applied before rotation (flip, then rotate).
- Tinting blends the source colors with the tint color using a 50% mix.
- Color values are automatically clamped to the valid range (0x00-0x3F) and mapped to the NES palette.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn images appear above game graphics.
- All transformations are applied in the order: scaling → flipping → rotation → tinting.
- Useful for sprite animations, UI elements, and game graphics that need transformations.

**Example:**
```lua
-- Create an 8x8 checkerboard image
local imgData = {}
for y = 0, 7 do
    for x = 0, 7 do
        if ((x + y) % 2 == 0) then
            table.insert(imgData, 0x20)  -- White
        else
            table.insert(imgData, 0x16)  -- Red
        end
    end
end

-- Basic drawing (no transformations)
drawimageex(imgData, 10, 10, {w = 8, h = 8})

-- Scale to 2x size
drawimageex(imgData, 10, 30, {w = 8, h = 8, dstW = 16, dstH = 16})

-- Rotate 45 degrees
drawimageex(imgData, 10, 50, {w = 8, h = 8, rot = 45})

-- Flip horizontally
drawimageex(imgData, 10, 70, {w = 8, h = 8, flipX = true})

-- Apply blue tint
drawimageex(imgData, 10, 90, {w = 8, h = 8, tint = 0x01})

-- Combined: scale, rotate, and flip
drawimageex(imgData, 10, 110, {w = 8, h = 8, dstW = 12, dstH = 12, rot = 30, flipX = true, tint = 0x28})
```

### `drawtile`

**Signature:** `drawtile(x, y, tileIndex, paletteIndex)`
Draws a single NES tile (8x8 pixels) directly from the PPU pattern table.

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `tileIndex` (integer): Index of the tile in the pattern table (0-255). Each tile is 16 bytes (8x8 pixels with 2 bits per pixel).
- `paletteIndex` (integer): Background palette index (0-3). The NES has 4 background palettes, each with 3 colors plus a universal background color.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- Tile data is read directly from the NES PPU pattern table memory.
- The pattern table used depends on PPU register 0 bit 4 (BGAdrHI): $0000 if bit is 0, $1000 if bit is 1.
- Each tile is 8x8 pixels with 2-bit color depth (4 possible colors per tile).
- Transparent pixels (color index 0) are skipped and not drawn.
- Palette colors are read from the current NES palette RAM (PALRAM).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn tiles appear above game graphics.
- Useful for drawing game tiles directly, creating tile editors, level viewers, or displaying pattern table contents.
- This function reads actual NES tile data from PPU memory, so it will show whatever tiles are currently loaded in the pattern table.

**Example:**
```lua
-- Draw tile 0 with palette 0
drawtile(10, 10, 0, 0)

-- Draw a row of tiles
for i = 0, 15 do
    drawtile(10 + (i * 18), 26, i, i % 4)
end
```

### `drawchrtile`

**Signature:** `drawchrtile(x, y, tileIndex, paletteIndex)`
Draws a single NES tile (8x8 pixels) directly from the cartridge CHR-ROM data.

**Parameters:**
- `x` (integer): X coordinate of top-left corner (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate of top-left corner (0-239). NES vertical resolution is 240 pixels.
- `tileIndex` (integer): Index of the tile in CHR-ROM (0-255). Each tile is 16 bytes (8x8 pixels with 2 bits per pixel).
- `paletteIndex` (integer): Background palette index (0-3). The NES has 4 background palettes, each with 3 colors plus a universal background color.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- Tile data is read directly from the cartridge's CHR-ROM memory, showing the original cartridge graphics data.
- Unlike `drawtile()`, this function reads from the raw cartridge CHR-ROM data rather than the PPU pattern table (which may be modified at runtime).
- Each tile is 8x8 pixels with 2-bit color depth (4 possible colors per tile).
- Transparent pixels (color index 0) are skipped and not drawn.
- Palette colors are read from the current NES palette RAM (PALRAM).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn tiles appear above game graphics.
- Useful for displaying cartridge graphics, creating tile viewers, or examining CHR-ROM data.
- This function shows the original cartridge tile data, which is useful for tile editors and graphics viewers.

**Example:**
```lua
-- Draw CHR-ROM tile 0 with palette 0
drawchrtile(10, 10, 0, 0)

-- Draw a row of CHR-ROM tiles
for i = 0, 15 do
    drawchrtile(10 + (i * 18), 26, i, i % 4)
end
```

## Drawing State Functions

### `setdrawmode`

**Signature:** `setdrawmode(mode)`
Sets the drawing mode for all subsequent drawing operations. This allows you to control how pixels are blended when drawn on top of existing overlay content.

**Parameters:**
- `mode` (string): The drawing mode to use. Valid values are:
  - `"normal"` - Normal mode (default): Overwrites destination pixels completely. No blending.
  - `"add"` - Additive blending: Adds source and destination color values together, creating a brighter result. Clamped at maximum brightness.
  - `"sub"` - Subtractive blending: Subtracts source color from destination color, creating a darker result. Clamped at minimum brightness.
  - `"multiply"` - Multiply blending: Multiplies source and destination color values together, creating a darker blended result.
  - `"alpha"` - Alpha blending: Averages source and destination color values (50% mix), creating a smooth blend between the two.

**Returns:** Nothing

**Notes:**
- The drawing mode persists across all drawing function calls until changed by calling `setdrawmode()` again.
- Default mode is `"normal"` (overwrite mode) when the script starts.
- Blending modes only affect pixels when drawn on top of existing content. Transparent pixels (value 0) are always written directly without blending.
- All blend modes operate on the color index values (0-63) before mapping to the overlay range.
- **Additive mode** (`"add"`): Best for creating glow effects, highlighting, or brightening areas. When colors overlap, they become brighter.
- **Subtractive mode** (`"sub"`): Best for creating shadows, darkening effects, or dimming overlays. When colors overlap, they become darker.
- **Multiply mode** (`"multiply"`): Best for creating darker overlays, shadows, or dimming effects. Bright white (0x20) has no effect, darker colors darken more.
- **Alpha mode** (`"alpha"`): Best for creating smooth transitions, semi-transparent overlays, or blending effects. Creates a 50/50 mix of source and destination.
- The drawing mode applies to all drawing functions: `drawpixel`, `drawline`, `drawthickline`, `drawrect`, `fillrect`, `drawimage`, `drawimageindexed`, `drawtile`, `drawchrtile`, `drawcircle`, `fillcircle`, `drawtriangle`, `filltriangle`, `drawellipse`, `fillellipse`, `drawarc`, `fillarc`, `drawpolygon`, `fillpolygon`, and `drawpolyline`.
- Text rendering (`drawtext`, `drawtextwh`) does not use blend modes and always uses normal mode.
- For best visual results, draw base shapes first in normal mode, then draw overlapping shapes with blend modes.

**Example:**
```lua
-- Draw base shape in normal mode
setdrawmode("normal")
fillrect(10, 10, 80, 80, 0x20)  -- Bright white base rectangle

-- Draw overlapping shape with additive blending (brightens)
setdrawmode("add")
fillrect(50, 50, 80, 80, 0x16)  -- Red / orange-red overlapping (should brighten)

-- Reset to normal for text
setdrawmode("normal")
drawtext(4, 4, "Blend mode test", 0x39)
```

### `setclipregion`

**Signature:** `setclipregion(x, y, width, height)`
Sets a clipping region (scissor test) for all subsequent drawing operations. Pixels drawn outside the clipping region will be ignored, effectively creating a "window" where drawing is allowed.

**Parameters:**
- `x` (integer): X coordinate of the top-left corner of the clipping region (0-255).
- `y` (integer): Y coordinate of the top-left corner of the clipping region (0-239).
- `width` (integer): Width of the clipping region in pixels. Must be positive.
- `height` (integer): Height of the clipping region in pixels. Must be positive.

**Returns:** Nothing

**Notes:**
- The clipping region persists across all drawing function calls until changed by calling `setclipregion()` again or cleared with `clearclipregion()`.
- By default, clipping is disabled (all pixels are allowed). Setting a valid region enables clipping.
- The clipping region is automatically clamped to screen bounds (0-255, 0-239).
- If `width` or `height` is zero or negative, clipping is disabled (all pixels are allowed).
- Setting the clipping region to full screen (0, 0, 256, 240) effectively disables clipping.
- To explicitly disable clipping, use `clearclipregion()` instead of setting the region to full screen.
- The clipping region affects all drawing functions: `drawpixel`, `drawline`, `drawthickline`, `drawrect`, `fillrect`, `drawimage`, `drawimageindexed`, `drawtile`, `drawchrtile`, `drawcircle`, `fillcircle`, `drawtriangle`, `filltriangle`, `drawellipse`, `fillellipse`, `drawarc`, `fillarc`, `drawpolygon`, `fillpolygon`, and `drawpolyline`.
- Text rendering (`drawtext`, `drawtextwh`) does not use clipping regions.
- Useful for creating windowed drawing areas, UI panels, or restricting drawing to specific screen regions.
- Clipping is applied per-pixel, so shapes that extend beyond the clip region will be partially drawn (only the pixels inside the region are rendered).

**Example:**
```lua
-- Set a clipping region (only draw in this area)
setclipregion(50, 50, 100, 80)  -- Clip region: x=50, y=50, width=100, height=80

-- Draw a large rectangle - will be clipped to the region
fillrect(40, 40, 120, 100, 0x16)  -- Red / orange-red rectangle (clipped)

-- Reset clipping (disable by setting to full screen)
setclipregion(0, 0, 256, 240)
```

### `clearclipregion`

**Signature:** `clearclipregion()`
Clears the clipping region, disabling clipping for all subsequent drawing operations. After calling this function, drawing will work across the entire screen without any restrictions.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Disables clipping by clearing the current clipping region.
- After calling `clearclipregion()`, all drawing functions will work across the full screen (0-255, 0-239).
- This is equivalent to calling `setclipregion(0, 0, 256, 240)`, but more convenient and explicit.
- The clipping region remains disabled until `setclipregion()` is called again with a new region.
- Useful for resetting clipping after drawing within a restricted region, or for explicitly disabling clipping when you want to ensure full-screen drawing.

**Example:**
```lua
-- Set a clipping region
setclipregion(50, 50, 100, 80)

-- Draw a rectangle - will be clipped to the region
fillrect(40, 40, 120, 100, 0x16)  -- Red / orange-red rectangle (clipped)

-- Clear the clipping region
clearclipregion()

-- Now draw outside the previous clip region - should be visible
fillrect(10, 10, 30, 30, 0x20)  -- Bright white rectangle (now visible)
```

### `setdrawcolor`

**Signature:** `setdrawcolor(color)`
Sets the default drawing color for drawing functions that don't specify a color parameter. This provides a global color setting that can be used by drawing functions when color is optional.

**Parameters:**
- `color` (integer): Default color index to use. Valid range is 0x00-0x3F (NES palette range).

**Returns:** Nothing

**Notes:**
- Sets a global default color that persists across all drawing function calls until changed by calling `setdrawcolor()` again.
- Default color is 0x39 (yellow-green) when the script starts.
- The color value must be in the valid NES palette range (0x00-0x3F).
- Currently, all drawing functions require an explicit color parameter, so this function stores the default color for potential future use by functions that may make color optional.
- Useful for setting a preferred default color that can be used by drawing functions in the future, or for preparing a color value that you'll use multiple times.

**Example:**
```lua
-- Set default drawing color to red / orange-red
setdrawcolor(0x16)

-- Set default drawing color to yellow-green
setdrawcolor(0x39)

-- Set default drawing color to bright white
setdrawcolor(0x20)
```

### `pushdrawstate`

**Signature:** `pushdrawstate()`
Saves the current drawing state (drawing mode, default color, clipping region) to a stack. This allows you to temporarily modify drawing settings and restore them later with `popdrawstate()`.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Saves the current state of:
  - Drawing mode (normal, add, sub, multiply, alpha)
  - Default drawing color (set by `setdrawcolor()`)
  - Clipping region (set by `setclipregion()`)
  - Clipping enabled flag
  - Transform state (set by `settransform()`)
- The state is saved to a stack, allowing nested push/pop operations.
- Use `popdrawstate()` to restore the saved state.
- Useful for batch drawing operations where you need to temporarily change drawing settings and restore them afterward.
- Supports nested operations - you can push multiple states and pop them in reverse order.

**Example:**
```lua
-- Save current state
pushdrawstate()

-- Change drawing settings
setdrawmode("add")
setdrawcolor(0x16)
setclipregion(10, 10, 100, 80)

-- Draw with modified settings
fillrect(20, 20, 60, 40, 0x20)

-- Restore original state
popdrawstate()

-- Now drawing uses original settings again
fillrect(120, 20, 60, 40, 0x20)
```

### `popdrawstate`

**Signature:** `popdrawstate()`
Restores the most recently saved drawing state from the stack. This restores the drawing mode, default color, and clipping region that were saved with `pushdrawstate()`.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Restores the drawing state that was saved with the most recent `pushdrawstate()` call.
- Restores:
  - Drawing mode (normal, add, sub, multiply, alpha)
  - Default drawing color
  - Clipping region and enabled flag
  - Transform state (translation, scale, rotation)
- The state stack uses LIFO (Last In, First Out) order - the most recently pushed state is restored first.
- Throws an error if called when the stack is empty (no saved states to restore).
- Must be called in reverse order of `pushdrawstate()` calls for nested operations.

**Example:**
```lua
-- Nested push/pop example
pushdrawstate()  -- Save state 1
setdrawmode("multiply")

pushdrawstate()  -- Save state 2 (nested)
setclipregion(10, 10, 100, 80)

-- Draw with nested state
fillrect(20, 20, 60, 40, 0x20)

popdrawstate()  -- Restore to state 1 (multiply mode, no clipping)
fillrect(120, 20, 60, 40, 0x20)

popdrawstate()  -- Restore to original state
fillrect(200, 20, 40, 40, 0x20)
```

### `settransform`

**Signature:** `settransform(tx, ty, sx, sy, rot)`
Sets a global 2D transformation matrix that applies to all subsequent drawing operations. The transform applies rotation around the origin, then scaling, then translation.

**Parameters:**
- `tx` (number): Translation X offset in pixels
- `ty` (number): Translation Y offset in pixels
- `sx` (number): Scale factor for X axis (1.0 = no scaling)
- `sy` (number): Scale factor for Y axis (1.0 = no scaling)
- `rot` (number): Rotation angle in degrees (0 = no rotation)

**Returns:** Nothing

**Notes:**
- The transform is applied in order: rotation (around origin), then scale, then translation
- Rotation is applied around the origin (0, 0) before translation
- Transform persists across all drawing function calls until changed by `settransform()` again or reset with `resettransform()`
- Transform state is saved/restored by `pushdrawstate()` and `popdrawstate()`
- Useful for coordinate transformations, camera effects, and drawing objects at different positions/scales/rotations
- Transform is currently applied to point-based drawing functions (like `drawpixel()`)

**Example:**
```lua
-- Translation only (move everything 30 pixels right)
settransform(30, 0, 1, 1, 0)
fillrect(10, 10, 20, 20, 0x20)

-- Scale only (make everything 1.5x larger)
settransform(0, 0, 1.5, 1.5, 0)
fillrect(10, 10, 20, 20, 0x20)

-- Rotation only (rotate 45 degrees around origin, then translate)
settransform(128, 100, 1, 1, 45)
for i = 0, 10 do
    drawpixel(i, 0, 0x20)  -- Draws rotated line at center
end

-- Combined transform
settransform(50, 30, 1.2, 1.2, 30)  -- Translate, scale, rotate
fillrect(10, 10, 20, 20, 0x20)

-- Reset to no transform
resettransform()
```

### `resettransform`

**Signature:** `resettransform()`
Resets the global 2D transformation to identity (no transform). This disables all transformations and restores normal coordinate system.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Resets transform to identity: translation (0, 0), scale (1, 1), rotation (0 degrees)
- Disables the transform (sets `transformEnabled` to false)
- All subsequent drawing operations use normal coordinates until `settransform()` is called again
- Transform state is saved/restored by `pushdrawstate()` and `popdrawstate()`

**Example:**
```lua
-- Apply transform
settransform(30, 20, 1.5, 1.5, 45)
fillrect(10, 10, 20, 20, 0x20)  -- Transformed

-- Reset transform
resettransform()
fillrect(10, 10, 20, 20, 0x20)  -- Normal coordinates
```

### `beginbatch`

**Signature:** `beginbatch()`
Begins a batch of draw calls. This marks the start of a drawing batch that can be optimized to reduce state changes and improve performance when drawing multiple primitives.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Marks the beginning of a batch of drawing operations
- Supports nested calls - you can call `beginbatch()` multiple times and must call `endbatch()` the same number of times
- Batching state is saved/restored by `pushdrawstate()` and `popdrawstate()`
- Use `endbatch()` to mark the end of the batch
- The batching infrastructure allows drawing functions to optimize operations (e.g., reduce redundant state changes, batch similar operations)
- Useful for performance optimization when drawing many shapes with similar properties

**Example:**
```lua
-- Batch multiple draw calls
beginbatch()
fillrect(10, 10, 20, 20, 0x20)
fillrect(40, 10, 20, 20, 0x20)
fillrect(70, 10, 20, 20, 0x20)
endbatch()

-- Nested batching
beginbatch()
fillrect(10, 50, 20, 20, 0x16)
beginbatch()  -- Nested
fillrect(40, 50, 20, 20, 0x28)
endbatch()
fillrect(70, 50, 20, 20, 0x29)
endbatch()
```

### `endbatch`

**Signature:** `endbatch()`
Ends a batch of draw calls. This marks the end of a drawing batch that was started with `beginbatch()`.

**Parameters:** None

**Returns:** Nothing

**Notes:**
- Marks the end of a batch of drawing operations
- Must be called to match each `beginbatch()` call
- Supports nested calls - decrements the batch depth counter
- Batching is only disabled when the batch depth reaches 0
- Batching state is saved/restored by `pushdrawstate()` and `popdrawstate()`
- Always pair `endbatch()` with a corresponding `beginbatch()` call

**Example:**
```lua
-- Simple batch
beginbatch()
fillrect(10, 10, 20, 20, 0x20)
fillrect(40, 10, 20, 20, 0x20)
endbatch()

-- Batch with state changes
beginbatch()
setdrawmode("add")
fillrect(10, 50, 20, 20, 0x20)
fillrect(40, 50, 20, 20, 0x20)
setdrawmode("normal")
endbatch()
```

### `setimagescale`

**Signature:** `setimagescale(mode)`
Sets the image scaling mode for subsequent `drawimage()` calls. Controls how images are scaled when drawn at different sizes (when scaling is implemented in `drawimage()`).

**Parameters:**
- `mode` (string): Scaling mode. Valid values are:
  - `"nearest"` - Nearest-neighbor interpolation (pixelated, fast). Each pixel is mapped directly to the nearest source pixel.
  - `"linear"` - Linear interpolation (smooth, slower). Pixels are blended for smoother scaling.

**Returns:** Nothing

**Notes:**
- The scaling mode persists across all `drawimage()` calls until changed by `setimagescale()` again
- Image scale mode state is saved/restored by `pushdrawstate()` and `popdrawstate()`
- Default mode is `"nearest"` (fastest)
- Use `getimagescale()` to query the current scaling mode
- The actual scaling effect will be visible when `drawimage()` is enhanced to support scaling operations

**Example:**
```lua
-- Set to nearest-neighbor (pixelated)
setimagescale("nearest")
drawimage(10, 10, imgData, 4, 4)

-- Set to linear interpolation (smooth)
setimagescale("linear")
drawimage(10, 50, imgData, 4, 4)

-- Save and restore mode
pushdrawstate()
setimagescale("linear")
drawimage(10, 90, imgData, 4, 4)
popdrawstate()  -- Restores previous mode
```

### `getimagescale`

**Signature:** `getimagescale()`
Returns the current image scaling mode as a string.

**Parameters:** None

**Returns:**
- `string`: Current scaling mode, either `"nearest"` or `"linear"`

**Notes:**
- Useful for debugging and verifying that `setimagescale()` is working correctly
- Returns the mode that was last set with `setimagescale()`
- The returned value matches what was passed to `setimagescale()`

**Example:**
```lua
-- Set mode and verify
setimagescale("nearest")
local mode = getimagescale()  -- Returns "nearest"
drawtext(4, 4, "Mode: " .. mode, 0x20)

setimagescale("linear")
local mode2 = getimagescale()  -- Returns "linear"
drawtext(4, 12, "Mode: " .. mode2, 0x20)
```

### `createcanvas`

**Signature:** `createcanvas(w, h)`
Creates an offscreen rendering surface (canvas) that can be used for offscreen drawing operations and post-processing effects.

**Parameters:**
- `w` (integer): Width of the canvas in pixels. Must be positive.
- `h` (integer): Height of the canvas in pixels. Must be positive.

**Returns:**
- `integer`: Canvas handle (unique identifier). Use this handle with other canvas functions to draw to or draw from the canvas. Returns 0 on failure.

**Notes:**
- Maximum canvas size is 1024x1024 pixels to prevent excessive memory usage
- Canvas buffer is initialized to transparent (all pixels set to 0)
- Each canvas has a unique handle that persists until the canvas is destroyed
- Canvas handles are integers starting from 1 (0 is invalid)
- Canvas buffers use the same format as the overlay (uint8 per pixel, 0x00-0x3F color indices)
- Useful for offscreen rendering, post-processing effects, and caching rendered graphics
- Canvas memory is automatically managed - canvases are cleaned up when Lua state is reset
- Multiple canvases can be created simultaneously

**Example:**
```lua
-- Create a small canvas for a sprite
local spriteCanvas = createcanvas(16, 16)

-- Create a screen-sized canvas for post-processing
local screenCanvas = createcanvas(256, 240)

-- Create multiple canvases for different layers
local bgCanvas = createcanvas(256, 240)
local fgCanvas = createcanvas(256, 240)
local uiCanvas = createcanvas(256, 240)

-- Verify canvas was created
if spriteCanvas > 0 then
    drawtext(4, 4, "Canvas created: " .. spriteCanvas, 0x20)
end
```

### `setrendertarget`

**Signature:** `setrendertarget(canvas)`
Sets the render target to a canvas for offscreen rendering, or resets to screen rendering. All subsequent drawing operations will render to the specified canvas instead of the screen overlay.

**Parameters:**
- `canvas` (integer or nil): Canvas handle returned by `createcanvas()`, or `nil`/no argument to reset to screen rendering.

**Returns:** Nothing

**Notes:**
- When a canvas handle is provided, all drawing functions (like `drawpixel()`, `fillrect()`, `drawimage()`, etc.) will render to that canvas instead of the screen
- When `nil` or no argument is provided, rendering is reset to the screen overlay
- **Important:** Currently, only screen-sized canvases (256x240) are supported for safety. Smaller canvases may cause buffer overruns because drawing functions use screen dimensions for buffer offsets
- The render target is automatically reset to screen at the start of each frame
- Canvas must be created with `createcanvas()` before it can be used as a render target
- Invalid canvas handles will return an error
- Useful for offscreen rendering, post-processing effects, render-to-texture, and caching graphics

**Example:**
```lua
-- Create a canvas
local myCanvas = createcanvas(256, 240)

-- Set render target to canvas
setrendertarget(myCanvas)

-- Draw to canvas (offscreen)
fillrect(0, 0, 256, 240, 0x16)  -- Red background
fillcircle(128, 120, 50, 0x20)  -- White circle

-- Reset to screen rendering
setrendertarget(nil)

-- Now drawing goes to screen again
drawtext(4, 4, "Back to screen", 0x20)

-- Switch back to canvas
setrendertarget(myCanvas)
drawrect(10, 10, 100, 80, 0x28)  -- Yellow border on canvas

-- Reset to screen
setrendertarget(nil)
```

### `blit`

**Signature:** `blit(canvas, x, y)`
Blits (copies) a canvas to the screen at the specified position. This allows you to composite rendered surfaces onto the screen overlay.

**Parameters:**
- `canvas` (integer): Canvas handle returned by `createcanvas()`.
- `x` (integer): X coordinate on screen where the canvas will be drawn (0-255).
- `y` (integer): Y coordinate on screen where the canvas will be drawn (0-239).

**Returns:** Nothing

**Notes:**
- The canvas is copied pixel-by-pixel to the screen at the specified position
- Transparent pixels (value 0) in the canvas are skipped and do not overwrite the destination
- The canvas can be partially off-screen; only the visible portion will be drawn (automatic clipping)
- Blending mode is applied when copying pixels, so the canvas will blend with existing screen content
- Canvas pixels are already in overlay format (0x80-0xBF) since they were drawn using the same drawing functions
- Invalid canvas handles will return an error
- Useful for compositing rendered surfaces, post-processing effects, sprite rendering, and UI elements

**Example:**
```lua
-- Create a canvas and draw to it
local myCanvas = createcanvas(256, 240)
setrendertarget(myCanvas)
fillrect(0, 0, 256, 240, 0x16)  -- Red background
fillcircle(128, 120, 50, 0x20)  -- White circle
setrendertarget(nil)

-- Blit canvas to screen at (0, 0)
blit(myCanvas, 0, 0)

-- Blit the same canvas at different positions
blit(myCanvas, 50, 50)
blit(myCanvas, 100, 100)

-- Blit partially off-screen (will clip automatically)
blit(myCanvas, 200, 200)  -- Partially off right and bottom edges
```

### `lineargradient`

**Signature:** `lineargradient(x1, y1, x2, y2, ...)`
Creates a linear gradient fill that can be used to fill shapes with smooth color transitions. The gradient is defined by a start point, end point, and color stops.

**Parameters:**
- `x1` (number): X coordinate of the gradient start point.
- `y1` (number): Y coordinate of the gradient start point.
- `x2` (number): X coordinate of the gradient end point.
- `y2` (number): Y coordinate of the gradient end point.
- Additional parameters depend on the mode:
  - **Simple mode (6 args)**: `color1` (integer), `color2` (integer) - Two colors for a simple gradient
  - **Variable mode (6+ args)**: `color1`, `color2`, `color3`, ... - Multiple colors evenly spaced
  - **Table mode (5 args)**: `stops` (table) - Table of `{position, color}` pairs, e.g. `{{0.0, 0x16}, {0.5, 0x28}, {1.0, 0x20}}`

**Returns:**
- `integer`: Gradient handle (unique identifier). Use this handle with `fillrectgradient()` or other gradient drawing functions. Returns 0 on failure.

**Notes:**
- Start and end points cannot be the same (will return an error)
- Color values are palette indices (0x00-0x3F), automatically clamped to valid range
- Position values in table mode are normalized (0.0 to 1.0), automatically clamped
- Stops in table mode are automatically sorted by position
- Gradient handles are unique integers starting from 1 (0 is invalid)
- Gradients persist until Lua state is reset
- Useful for creating smooth color transitions in rectangles, backgrounds, and UI elements

**Example:**
```lua
-- Simple two-color horizontal gradient (red to white)
local grad1 = lineargradient(0, 0, 100, 0, 0x16, 0x20)

-- Simple two-color vertical gradient (blue to yellow)
local grad2 = lineargradient(0, 0, 0, 50, 0x01, 0x28)

-- Multiple colors evenly spaced (red, yellow, blue, white)
local grad3 = lineargradient(0, 0, 100, 0, 0x16, 0x28, 0x01, 0x20)

-- Table of stops with custom positions
local stops = {
    {0.0, 0x16},   -- Red at start
    {0.3, 0x28},   -- Yellow at 30%
    {0.7, 0x01},   -- Blue at 70%
    {1.0, 0x20}    -- White at end
}
local grad4 = lineargradient(0, 0, 100, 0, stops)

-- Diagonal gradient
local grad5 = lineargradient(0, 0, 80, 40, 0x28, 0x16)  -- Yellow to red
```

### `fillrectgradient`

**Signature:** `fillrectgradient(x, y, w, h, gradient)`
Fills a rectangle with a linear gradient. Each pixel's color is determined by projecting its position onto the gradient line and interpolating between color stops.

**Parameters:**
- `x` (integer): X coordinate of the rectangle's top-left corner (0-255).
- `y` (integer): Y coordinate of the rectangle's top-left corner (0-239).
- `w` (integer): Width of the rectangle in pixels. Must be positive.
- `h` (integer): Height of the rectangle in pixels. Must be positive.
- `gradient` (integer): Gradient handle returned by `lineargradient()` or `radialgradient()`.

**Returns:** Nothing

**Notes:**
- The gradient is applied by projecting each pixel onto the gradient line (for linear) or calculating distance from center (for radial)
- Colors are interpolated between stops based on the pixel's position
- Rectangle is automatically clipped to screen bounds (0-255, 0-239)
- Respects clipping regions set with `setclipregion()`
- Blending mode is applied (set with `setdrawmode()`)
- Invalid gradient handles will return an error
- Works with both linear gradients (from `lineargradient()`) and radial gradients (from `radialgradient()`)
- **Performance Warning:** Gradient fills can be very resource intensive, especially for large rectangles, complex gradients with many stops, or radial gradients. Each pixel requires calculations and color interpolation. This may cause significant FPS drops. Use sparingly, prefer smaller rectangles, or use simpler gradients with fewer stops for better performance.
- Useful for creating smooth color transitions in backgrounds, buttons, and UI elements

**Example:**
```lua
-- Create a horizontal gradient (red to white)
local myGradient = lineargradient(0, 0, 100, 0, 0x16, 0x20)

-- Fill a rectangle with the gradient
fillrectgradient(10, 10, 100, 50, myGradient)

-- Create a vertical gradient (blue to yellow)
local vertGradient = lineargradient(0, 0, 0, 50, 0x01, 0x28)

-- Fill a rectangle with vertical gradient
fillrectgradient(120, 10, 50, 100, vertGradient)

-- Create a multi-color gradient
local rainbow = lineargradient(0, 0, 200, 0, 0x16, 0x28, 0x01, 0x20)
fillrectgradient(10, 120, 200, 40, rainbow)
```

### `radialgradient`

**Signature:** `radialgradient(cx, cy, radius, ...)`
Creates a radial gradient fill that radiates outward from a center point. The gradient transitions colors in a circular pattern from the center to the edge.

**Parameters:**
- `cx` (number): X coordinate of the gradient center point.
- `cy` (number): Y coordinate of the gradient center point.
- `radius` (number): Radius of the gradient (distance from center to edge). Must be positive.
- Additional parameters depend on the mode:
  - **Simple mode (5 args)**: `color1` (integer), `color2` (integer) - Two colors (center to edge)
  - **Variable mode (5+ args)**: `color1`, `color2`, `color3`, ... - Multiple colors evenly spaced from center to edge
  - **Table mode (4 args)**: `stops` (table) - Table of `{position, color}` pairs, e.g. `{{0.0, 0x16}, {0.5, 0x28}, {1.0, 0x20}}`

**Returns:**
- `integer`: Gradient handle (unique identifier). Use this handle with `fillrectgradient()` or other gradient drawing functions. Returns 0 on failure.

**Notes:**
- Radius must be positive (will return an error if zero or negative)
- Color values are palette indices (0x00-0x3F), automatically clamped to valid range
- Position values in table mode are normalized (0.0 to 1.0), where 0.0 is the center and 1.0 is at the radius edge
- Stops in table mode are automatically sorted by position
- Gradient handles are unique integers starting from 1 (0 is invalid)
- Gradients persist until Lua state is reset
- **Performance Warning:** Radial gradients can be very resource intensive, especially when filling large areas or using many color stops. Each pixel requires distance calculations and color interpolation. This may cause significant FPS drops. Use sparingly, prefer smaller rectangles, or use simpler gradients with fewer stops for better performance.
- Useful for creating spotlight effects, circular color transitions, and radial backgrounds

**Example:**
```lua
-- Simple two-color radial gradient (red center to white edge)
local grad1 = radialgradient(128, 120, 50, 0x16, 0x20)

-- Simple two-color radial gradient (blue center to yellow edge)
local grad2 = radialgradient(64, 64, 40, 0x01, 0x28)

-- Multiple colors evenly spaced (red, yellow, blue, white)
local grad3 = radialgradient(128, 120, 60, 0x16, 0x28, 0x01, 0x20)

-- Table of stops with custom positions
local stops = {
    {0.0, 0x20},   -- White at center
    {0.2, 0x28},   -- Yellow at 20%
    {0.4, 0x16},   -- Red at 40%
    {0.6, 0x39},   -- Green at 60%
    {0.8, 0x01},   -- Blue at 80%
    {1.0, 0x00}    -- Black at edge
}
local grad4 = radialgradient(128, 120, 100, stops)

-- Use with fillrectgradient
fillrectgradient(0, 0, 256, 240, grad4)
```

## Shape Functions

### `drawcircle`

**Signature:** `drawcircle(x, y, radius, color)`
Draws a circle outline at the specified center position and radius.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `radius` (integer): Circle radius in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The circle is drawn as an outline only (border), not filled.
- Uses the midpoint circle algorithm for smooth, accurate circles.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn circles appear above game graphics.
- Useful for drawing circular indicators, markers, or decorative elements.

**Example:**
```lua
-- Draw circles at different positions
drawcircle(128, 120, 30, 0x39)    -- Yellow-green circle at center
drawcircle(50, 50, 10, 0x29)      -- Medium bright green circle
drawcircle(200, 180, 20, 0x16)    -- Red / orange-red circle

-- Draw multiple concentric circles
for i = 5, 25, 5 do
    drawcircle(128, 120, i, 0x20)  -- Bright white circles
end
```

### `fillcircle`

**Signature:** `fillcircle(x, y, radius, color)`
Draws a filled circle (solid color) at the specified center position and radius.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `radius` (integer): Circle radius in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The circle is completely filled with the specified color (solid circle).
- Uses distance calculation to determine which pixels are inside the circle radius.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn circles appear above game graphics.
- Useful for drawing solid circular indicators, markers, progress indicators, or decorative elements.
- For circle outlines only, use `drawcircle()`.

**Example:**
```lua
-- Draw filled circles at different positions
fillcircle(128, 120, 30, 0x39)    -- Filled yellow-green circle at center
fillcircle(50, 50, 10, 0x29)      -- Filled medium bright green circle

-- Draw concentric filled circles
fillcircle(128, 120, 25, 0x20)    -- Bright white circle
fillcircle(128, 120, 15, 0x16)    -- Red / orange-red circle inside
fillcircle(128, 120, 5, 0x20)     -- Bright white center
```

### `drawellipse`

**Signature:** `drawellipse(x, y, rx, ry, color)`
Draws an ellipse outline at the specified center position with separate horizontal and vertical radii.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `rx` (integer): Horizontal radius (semi-major axis) in pixels. Must be positive.
- `ry` (integer): Vertical radius (semi-minor axis) in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The ellipse is drawn as an outline only (border), not filled.
- Uses the midpoint ellipse algorithm for smooth, accurate ellipses.
- When `rx == ry`, the ellipse is a circle (same result as `drawcircle()`).
- When `rx > ry`, the ellipse is wider than tall (horizontal ellipse).
- When `rx < ry`, the ellipse is taller than wide (vertical ellipse).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn ellipses appear above game graphics.
- Useful for drawing oval indicators, markers, or decorative elements with non-circular shapes.

**Example:**
```lua
-- Draw horizontal ellipses (wide ovals)
drawellipse(128, 60, 50, 25, 0x20)    -- Wide bright white ellipse

-- Draw vertical ellipses (tall ovals)
drawellipse(50, 120, 20, 40, 0x29)     -- Tall medium bright green ellipse

-- Draw a circle (rx == ry, same as drawcircle)
drawellipse(128, 120, 30, 30, 0x39)    -- Circle (yellow-green)
```

### `fillellipse`

**Signature:** `fillellipse(x, y, rx, ry, color)`
Draws a filled ellipse (solid color) at the specified center position with separate horizontal and vertical radii.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `rx` (integer): Horizontal radius (semi-major axis) in pixels. Must be positive.
- `ry` (integer): Vertical radius (semi-minor axis) in pixels. Must be positive.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The ellipse is completely filled with the specified color (solid ellipse).
- Uses distance calculation with the ellipse equation to determine which pixels are inside the ellipse: `(dxÂ²/rxÂ²) + (dyÂ²/ryÂ²) â‰¤ 1`
- When `rx == ry`, the ellipse is a circle (same result as `fillcircle()`).
- When `rx > ry`, the ellipse is wider than tall (horizontal ellipse).
- When `rx < ry`, the ellipse is taller than wide (vertical ellipse).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn ellipses appear above game graphics.
- Useful for drawing solid oval indicators, markers, progress indicators, or decorative elements with non-circular shapes.
- For ellipse outlines only, use `drawellipse()`.

**Example:**
```lua
-- Draw filled horizontal ellipses (wide ovals)
fillellipse(128, 60, 50, 25, 0x20)    -- Filled wide bright white ellipse

-- Draw filled vertical ellipses (tall ovals)
fillellipse(50, 120, 20, 40, 0x29)     -- Filled tall medium bright green ellipse

-- Draw a filled circle (rx == ry, same as fillcircle)
fillellipse(128, 120, 30, 30, 0x39)    -- Filled circle (yellow-green)
```

### `drawarc`

**Signature:** `drawarc(x, y, radius, startAngle, endAngle, color)`
Draws a circular arc outline (portion of a circle) between two angles.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `radius` (integer): Circle radius in pixels. Must be positive.
- `startAngle` (integer): Starting angle in degrees (0-360). Angles wrap automatically.
- `endAngle` (integer): Ending angle in degrees (0-360). Angles wrap automatically.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The arc is drawn as an outline only (border), not filled.
- Uses the midpoint circle algorithm with angle filtering to draw only the arc segment.
- **Angle system:** 0Â° = right (east), 90Â° = down (south), 180Â° = left (west), 270Â° = up (north), 360Â° = right (same as 0Â°).
- Angles are normalized to 0-360 range automatically.
- Supports wrap-around arcs (e.g., arc from 350Â° to 10Â° crosses the 0Â°/360Â° boundary).
- When `startAngle == endAngle`, draws a full circle (same result as `drawcircle()`).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn arcs appear above game graphics.
- Useful for drawing progress indicators, dials, gauges, pie chart segments, or partial circular decorations.

**Example:**
```lua
-- Draw quadrant arcs (four corners)
drawarc(128, 120, 30, 0, 90, 0x20)    -- Top-right quadrant (0Â° to 90Â°)
drawarc(128, 120, 30, 90, 180, 0x26)  -- Top-left quadrant (90Â° to 180Â°)
drawarc(128, 120, 30, 180, 270, 0x29) -- Bottom-left quadrant (180Â° to 270Â°)
drawarc(128, 120, 30, 270, 360, 0x37) -- Bottom-right quadrant (270Â° to 360Â°)

-- Progress indicator (75% of circle)
drawarc(128, 120, 40, 0, 270, 0x39)    -- Large arc covering 270 degrees
```

### `fillarc`

**Signature:** `fillarc(x, y, radius, startAngle, endAngle, color)`
Draws a filled circular arc (pie slice) between two angles.

**Parameters:**
- `x` (integer): Center X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Center Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `radius` (integer): Circle radius in pixels. Must be positive.
- `startAngle` (integer): Starting angle in degrees (0-360). Angles wrap automatically.
- `endAngle` (integer): Ending angle in degrees (0-360). Angles wrap automatically.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The arc is drawn as a filled shape (pie slice), filling all pixels within the angle range and radius.
- Uses distance calculation and angle filtering to determine which pixels to fill.
- **Angle system:** 0Â° = right (east), 90Â° = down (south), 180Â° = left (west), 270Â° = up (north), 360Â° = right (same as 0Â°).
- Angles are normalized to 0-360 range automatically.
- Supports wrap-around arcs (e.g., arc from 350Â° to 10Â° crosses the 0Â°/360Â° boundary).
- When `startAngle == endAngle`, fills a full circle (same result as `fillcircle()`).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn arcs appear above game graphics.
- Useful for drawing progress indicators, pie charts, gauges, dials, or sector-based visualizations.

**Example:**
```lua
-- Draw four quadrant pie slices
fillarc(64, 60, 40, 0, 90, 0x20)    -- Top-right quadrant (bright white)
fillarc(192, 60, 40, 90, 180, 0x26)  -- Top-left quadrant (coral red)
fillarc(64, 180, 40, 180, 270, 0x29) -- Bottom-left quadrant (medium bright green)
fillarc(192, 180, 40, 270, 360, 0x37) -- Bottom-right quadrant (bright yellow)

-- Progress indicators (different percentages)
fillarc(50, 120, 30, 0, 90, 0x39)     -- 25% progress (yellow-green)
fillarc(206, 120, 30, 0, 180, 0x21)    -- 50% progress (light blue)
fillarc(128, 120, 30, 0, 270, 0x28)    -- 75% progress (yellow)
```

### `drawroundrect`

**Signature:** `drawroundrect(x, y, w, h, radius, color)`
Draws a rounded rectangle outline (rectangle with rounded corners).

**Parameters:**
- `x` (integer): Top-left X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Top-left Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `w` (integer): Rectangle width in pixels. Must be positive.
- `h` (integer): Rectangle height in pixels. Must be positive.
- `radius` (integer): Corner radius in pixels. Must be non-negative. Automatically clamped to not exceed half the width or height.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The rectangle is drawn as an outline only (border), not filled.
- Uses arc segments for rounded corners and straight lines for the edges.
- When `radius = 0`, draws a regular rectangle (same result as `drawrect()`).
- The corner radius is automatically clamped to `min(w/2, h/2)` to prevent invalid shapes.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn rounded rectangles appear above game graphics.
- Useful for drawing modern UI elements, buttons, panels, or decorative borders with rounded corners.

**Example:**
```lua
-- Small radius (subtle rounding)
drawroundrect(10, 10, 60, 40, 5, 0x20)   -- Bright white outline, radius 5

-- Medium radius (moderate rounding)
drawroundrect(80, 10, 60, 40, 10, 0x26)  -- Coral red outline, radius 10

-- Large radius (strong rounding)
drawroundrect(150, 10, 60, 40, 15, 0x29) -- Medium bright green outline, radius 15

-- Radius 0 (should draw as regular rectangle, same as drawrect)
drawroundrect(10, 170, 80, 30, 0, 0x2B)   -- Aqua-green outline, radius 0
```

### `fillroundrect`

**Signature:** `fillroundrect(x, y, w, h, radius, color)`
Draws a filled rounded rectangle (rectangle with rounded corners, filled interior).

**Parameters:**
- `x` (integer): Top-left X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Top-left Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `w` (integer): Rectangle width in pixels. Must be positive.
- `h` (integer): Rectangle height in pixels. Must be positive.
- `radius` (integer): Corner radius in pixels. Must be non-negative. Automatically clamped to not exceed half the width or height.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The rectangle is drawn as a filled shape (all interior pixels are colored), including the rounded corners.
- Uses filled arc segments for rounded corners and fills the center rectangle and edge areas.
- When `radius = 0`, draws a regular filled rectangle (same result as `fillrect()`).
- The corner radius is automatically clamped to `min(w/2, h/2)` to prevent invalid shapes.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn filled rounded rectangles appear above game graphics.
- Useful for drawing modern UI elements, buttons, panels, progress bars, or decorative filled shapes with rounded corners.

**Example:**
```lua
-- Small radius (subtle rounding)
fillroundrect(10, 10, 60, 40, 5, 0x20)   -- Bright white fill, radius 5

-- Medium radius (moderate rounding)
fillroundrect(80, 10, 60, 40, 10, 0x26)  -- Coral red fill, radius 10

-- Large radius (strong rounding)
fillroundrect(150, 10, 60, 40, 15, 0x29) -- Medium bright green fill, radius 15

-- Radius 0 (should fill as regular rectangle, same as fillrect)
fillroundrect(10, 170, 80, 30, 0, 0x2B)   -- Aqua-green fill, radius 0
```

### `drawtriangle`

**Signature:** `drawtriangle(x1, y1, x2, y2, x3, y3, color)`
Draws a triangle outline by connecting three vertices with lines.

**Parameters:**
- `x1` (integer): First vertex X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): First vertex Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Second vertex X coordinate (0-255).
- `y2` (integer): Second vertex Y coordinate (0-239).
- `x3` (integer): Third vertex X coordinate (0-255).
- `y3` (integer): Third vertex Y coordinate (0-239).
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The triangle is drawn as an outline only (three connected lines), not filled.
- Uses Bresenham's line algorithm to draw the three edges connecting the vertices.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn triangles appear above game graphics.
- Useful for drawing triangular indicators, markers, directional arrows, or decorative elements.
- The triangle can be oriented in any direction by specifying the three vertex positions.

**Example:**
```lua
-- Draw triangles pointing in different directions
drawtriangle(128, 50, 100, 80, 156, 80, 0x20)    -- Pointing up (bright white)
drawtriangle(128, 190, 100, 160, 156, 160, 0x39) -- Pointing down (yellow-green)
drawtriangle(50, 120, 80, 100, 80, 140, 0x16)     -- Pointing right (red / orange-red)
drawtriangle(206, 120, 176, 100, 176, 140, 0x29)  -- Pointing left (medium bright green)

-- Draw a diamond shape using two triangles
drawtriangle(128, 80, 148, 120, 108, 120, 0x20)   -- Top triangle
drawtriangle(128, 160, 148, 120, 108, 120, 0x20)  -- Bottom triangle
```

### `filltriangle`

**Signature:** `filltriangle(x1, y1, x2, y2, x3, y3, color)`
Draws a filled triangle (solid color) by filling the interior area defined by three vertices.

**Parameters:**
- `x1` (integer): First vertex X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): First vertex Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Second vertex X coordinate (0-255).
- `y2` (integer): Second vertex Y coordinate (0-239).
- `x3` (integer): Third vertex X coordinate (0-255).
- `y3` (integer): Third vertex Y coordinate (0-239).
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The triangle is completely filled with the specified color (solid triangle).
- Uses scanline fill algorithm to efficiently fill the triangle interior.
- Vertices are automatically sorted by Y coordinate for proper filling.
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn triangles appear above game graphics.
- Useful for drawing solid triangular indicators, markers, directional arrows, progress indicators, or decorative elements.
- The triangle can be oriented in any direction by specifying the three vertex positions.
- For triangle outlines only, use `drawtriangle()`.

**Example:**
```lua
-- Draw filled triangles pointing in different directions
filltriangle(128, 50, 100, 80, 156, 80, 0x20)    -- Pointing up (bright white)
filltriangle(128, 190, 100, 160, 156, 160, 0x39) -- Pointing down (yellow-green)
filltriangle(50, 120, 80, 100, 80, 140, 0x16)     -- Pointing right (red / orange-red)
filltriangle(206, 120, 176, 100, 176, 140, 0x29)  -- Pointing left (medium bright green)

-- Draw multiple filled triangles for decorative effects
for i = 1, 5 do
    local x = 40 + i * 35
    local size = 15
    filltriangle(x, 30, x + size, 30 + size, x - size/2, 30 + size, 0x37)  -- Bright yellow triangles
end

-- Draw a diamond shape using two filled triangles
filltriangle(128, 80, 148, 120, 108, 120, 0x20)   -- Top triangle
filltriangle(128, 160, 148, 120, 108, 120, 0x20)  -- Bottom triangle

-- Combine outline and filled for effect
filltriangle(100, 50, 156, 50, 128, 100, 0x16)     -- Filled red / orange-red triangle
drawtriangle(100, 50, 156, 50, 128, 100, 0x20)    -- Bright white outline on top
```

### `drawpolygon`

**Signature:** `drawpolygon(x1, y1, x2, y2, ..., color)`
Draws a polygon outline by connecting multiple vertices with lines and automatically closing the shape.

**Parameters:**
- `x1` (integer): First vertex X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): First vertex Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Second vertex X coordinate (0-255).
- `y2` (integer): Second vertex Y coordinate (0-239).
- `...` (integer pairs): Additional vertex coordinates as pairs of x, y values. Requires at least 2 points total.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range). Must be the last argument.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The polygon is drawn as an outline only (connected lines), not filled.
- Uses Bresenham's line algorithm to draw edges connecting consecutive vertices.
- The polygon is automatically closed (last vertex connects back to first vertex).
- Requires an odd number of arguments (pairs of x,y coordinates plus one color argument).
- Requires at least 2 points (minimum 4 arguments: x1, y1, x2, y2, color).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn polygons appear above game graphics.
- Useful for drawing complex shapes, stars, hexagons, pentagons, or any multi-sided outline shape.
- For open paths (non-closed lines), use `drawpolyline()`.
- For filled polygons, use `fillpolygon()`.
- For simple 3-point shapes, `drawtriangle()` is more efficient.

**Example:**
```lua
-- Draw a square (4 points)
drawpolygon(50, 50, 100, 50, 100, 100, 50, 100, 0x20)  -- Bright white square outline

-- Draw a pentagon (5 points)
drawpolygon(128, 30, 148, 60, 128, 90, 108, 60, 118, 30, 0x39)  -- Yellow-green pentagon

-- Draw a star shape (5 points)
drawpolygon(128, 20, 132, 50, 160, 50, 138, 70, 148, 100, 128, 80, 108, 100, 118, 70, 96, 50, 124, 50, 0x37)  -- Bright yellow star

-- Draw a hexagon (6 points)
local cx, cy, radius = 128, 120, 30
drawpolygon(
    cx, cy - radius,                    -- Top
    cx + radius * 0.866, cy - radius * 0.5,  -- Top-right
    cx + radius * 0.866, cy + radius * 0.5,  -- Bottom-right
    cx, cy + radius,                    -- Bottom
    cx - radius * 0.866, cy + radius * 0.5,  -- Bottom-left
    cx - radius * 0.866, cy - radius * 0.5,  -- Top-left
    0x29
)

-- Draw an irregular polygon
drawpolygon(50, 30, 80, 20, 100, 40, 90, 70, 60, 80, 40, 60, 0x16)  -- Red / orange-red irregular shape

-- Draw a triangle using drawpolygon (drawtriangle is more efficient for this)
drawpolygon(128, 50, 100, 80, 156, 80, 0x20)  -- Bright white triangle
```

### `drawpolyline`

**Signature:** `drawpolyline(x1, y1, x2, y2, ..., color)`
Draws an open polyline (connected line segments) by connecting multiple vertices with lines, but does NOT automatically close the shape.

**Parameters:**
- `x1` (integer): First vertex X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): First vertex Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Second vertex X coordinate (0-255).
- `y2` (integer): Second vertex Y coordinate (0-239).
- `...` (integer pairs): Additional vertex coordinates as pairs of x, y values. Requires at least 2 points total.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range). Must be the last argument.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The polyline is drawn as connected line segments (open path), not filled and not closed.
- Uses Bresenham's line algorithm to draw edges connecting consecutive vertices.
- The polyline does NOT automatically close (last vertex does NOT connect back to first vertex).
- This differs from `drawpolygon()` which automatically closes the shape.
- Requires an odd number of arguments (pairs of x,y coordinates plus one color argument).
- Requires at least 2 points (minimum 4 arguments: x1, y1, x2, y2, color).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn polylines appear above game graphics.
- Useful for drawing paths, routes, waveforms, arrows, or any open-line shape that shouldn't be closed.
- For closed shapes, use `drawpolygon()`.
- For simple single lines, `drawline()` is more efficient.

**Example:**
```lua
-- Draw a simple L-shaped path (doesn't close)
drawpolyline(20, 40, 60, 40, 60, 80, 0x20)  -- Bright white L-shape

-- Draw a zigzag pattern
drawpolyline(20, 120, 40, 100, 60, 120, 80, 100, 100, 120, 0x39)  -- Yellow-green zigzag

-- Draw a curved-looking path (multiple points)
drawpolyline(130, 40, 140, 50, 150, 45, 160, 55, 170, 50, 180, 60, 0x16)  -- Red / orange-red curved path

-- Draw an arrow shape (open path)
drawpolyline(200, 100, 220, 100, 220, 90, 230, 110, 220, 130, 220, 120, 200, 120, 0x29)  -- Medium bright green arrow

-- Draw a wave pattern
drawpolyline(50, 150, 70, 140, 90, 150, 110, 140, 130, 150, 150, 140, 0x37)  -- Bright yellow wave

-- Draw a simple path between points
drawpolyline(100, 50, 120, 70, 140, 50, 160, 70, 0x20)  -- Bright white connecting path

-- Note: drawpolyline does NOT close - compare with drawpolygon
drawpolyline(100, 100, 150, 100, 150, 150, 100, 150, 0x16)  -- Red / orange-red open square (missing top edge)
drawpolygon(100, 100, 150, 100, 150, 150, 100, 150, 0x39)   -- Yellow-green closed square (complete)
```

### `fillpolygon`

**Signature:** `fillpolygon(x1, y1, x2, y2, ..., color)`
Draws a filled polygon (solid color) by filling the interior area defined by multiple vertices.

**Parameters:**
- `x1` (integer): First vertex X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y1` (integer): First vertex Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `x2` (integer): Second vertex X coordinate (0-255).
- `y2` (integer): Second vertex Y coordinate (0-239).
- `...` (integer pairs): Additional vertex coordinates as pairs of x, y values. Requires at least 3 points total.
- `color` (integer): Palette color index. Valid range is 0x00-0x3F (automatically mapped to NES palette range). Must be the last argument.

**Returns:** Nothing

**Notes:**
- Coordinates (0, 0) represent the top-left corner of the screen.
- The polygon is completely filled with the specified color (solid polygon).
- Uses scanline fill algorithm with even-odd rule to efficiently fill the polygon interior.
- The polygon is automatically closed (last vertex connects back to first vertex).
- Requires an odd number of arguments (pairs of x,y coordinates plus one color argument).
- Requires at least 3 points (minimum 6 arguments: x1, y1, x2, y2, x3, y3, color).
- Pixels drawn outside the visible area (0-255, 0-239) are ignored (silently clipped).
- The overlay is composited on top of the NES frame, so Lua-drawn polygons appear above game graphics.
- Useful for drawing solid stars, hexagons, pentagons, or any filled multi-sided shape.
- Supports complex self-intersecting polygons using even-odd fill rule.
- For polygon outlines only, use `drawpolygon()`.
- For simple 3-point shapes, `filltriangle()` is more efficient.

**Example:**
```lua
-- Draw a filled square (4 points)
fillpolygon(50, 50, 100, 50, 100, 100, 50, 100, 0x20)  -- Bright white filled square

-- Draw a filled pentagon (5 points)
fillpolygon(128, 30, 148, 60, 128, 90, 108, 60, 118, 30, 0x39)  -- Yellow-green filled pentagon

-- Draw a filled star shape (10 points)
fillpolygon(
    128, 20, 132, 50, 160, 50, 138, 70, 148, 100,
    128, 80, 108, 100, 118, 70, 96, 50, 124, 50,
    0x37
)  -- Bright yellow filled star

-- Draw a filled hexagon (6 points)
local cx, cy, radius = 128, 120, 30
fillpolygon(
    cx, cy - radius,                    -- Top
    cx + radius * 0.866, cy - radius * 0.5,  -- Top-right
    cx + radius * 0.866, cy + radius * 0.5,  -- Bottom-right
    cx, cy + radius,                    -- Bottom
    cx - radius * 0.866, cy + radius * 0.5,  -- Bottom-left
    cx - radius * 0.866, cy - radius * 0.5,  -- Top-left
    0x29
)

-- Draw a filled irregular polygon
fillpolygon(50, 30, 80, 20, 100, 40, 90, 70, 60, 80, 40, 60, 0x16)  -- Red / orange-red filled irregular shape

-- Draw a filled triangle using fillpolygon (filltriangle is more efficient for this)
fillpolygon(128, 50, 100, 80, 156, 80, 0x20)  -- Bright white filled triangle
-- Combine outline and filled for effect
fillpolygon(128, 60, 148, 90, 128, 120, 108, 90, 0x16)  -- Red / orange-red filled pentagon
drawpolygon(128, 60, 148, 90, 128, 120, 108, 90, 0x20)  -- Bright white outline on top
```

## See Also

- **[Palette Reference](Palette-Reference)** - Complete NES palette color reference
- **[Color Functions](Color-Functions)** - Color manipulation functions
- **[Examples](Examples)** - Working example scripts
- **[Home](Home)** - Return to the main wiki page
