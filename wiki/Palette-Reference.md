# NES Palette Reference for Lua Overlays

Complete reference for the NES 64-color palette used in Lua drawing functions.

## General Notes

- **Valid range:** `0x00–0x3F` (64 colors total)
- **Internally mapped to:** `0x80–0xBF` for overlay rendering
- **Coordinates and drawing functions:** 256×240 pixels resolution
- **Transparent/black values:** Some palette indices (notably `0x0D, 0x0E, 0x0F, 0x1E, 0x1F, 0x2F`) are near-black or transparent and will render invisibly—avoid these for text or outlines
- **Palette variation:** Colors vary slightly depending on the current NTSC tint/hue settings, but their relative brightness and hue ordering are fixed

## Recommended Defaults

| Use Case                   | Suggested Colors                                             |
| -------------------------- | ------------------------------------------------------------ |
| **Text / HUD**             | `0x20` (bright white), `0x39` (yellow-green), `0x3D` (silver) |
| **Panels / Backgrounds**   | `0x10` (light gray), `0x2D` (light gray)                     |
| **Outlines / Borders**     | `0x20` (bright white), `0x3D` (silver)                       |
| **Warnings / Alerts**      | `0x16` (red / orange-red), `0x26` (coral red), `0x37` (bright yellow)    |
| **Highlights / Status OK** | `0x29` (medium bright green), `0x39` (yellow-green)                  |

## Complete Palette Table

### Row 0 — Dark (0x00–0x0F)
- `0x00` - dark gray
- `0x01` - midnight navy
- `0x02` - deep blue
- `0x03` - indigo
- `0x04` - deep violet
- `0x05` - wine / dark magenta
- `0x06` - maroon
- `0x07` - very dark red
- `0x08` - brown
- `0x09` - deep green
- `0x0A` - dark green
- `0x0B` - teal-green
- `0x0C` - dark cyan-blue
- `0x0D` - black (transparent)
- `0x0E` - black (transparent)
- `0x0F` - black (transparent)

### Row 1 — Medium-Dark (0x10–0x1F)
- `0x10` - light gray
- `0x11` - light blue
- `0x12` - blue
- `0x13` - violet
- `0x14` - light purple
- `0x15` - salmon 
- `0x16` - red / orange-red
- `0x17` - orange
- `0x18` - yellow-brown
- `0x19` - dark leaf green
- `0x1A` - medium green
- `0x1B` - bright green
- `0x1C` - cyan
- `0x1D` - black (transparent)
- `0x1E` - black (transparent)
- `0x1F` - black (transparent)

### Row 2 — Medium-Bright (0x20–0x2F)
- `0x20` - bright white
- `0x21` - light blue
- `0x22` - baby blue 
- `0x23` - sky blue
- `0x24` - lavander
- `0x25` - light pink
- `0x26` - coral red
- `0x27` - orange
- `0x28` - yellow
- `0x29` - medium bright green
- `0x2A` - bright neon green
- `0x2B` - aqua-green
- `0x2C` - cyan
- `0x2D` - light gray
- `0x2E` - black (transparent)
- `0x2F` - black (transparent)

### Row 3 — Bright (0x30–0x3F)
- `0x30` - very light gray
- `0x31` - very light blue
- `0x32` - light gray blue
- `0x33` - periwinkle
- `0x34` - very light lavander
- `0x35` - light salmon
- `0x36` - peach
- `0x37` - bright yellow
- `0x38` - golden yellow
- `0x39` - yellow-green
- `0x3A` - bright green
- `0x3B` - aqua-green
- `0x3C` - light cyan
- `0x3D` - silver
- `0x3E` - black (transparent)
- `0x3F` - black (transparent)

## Example: Displaying the Palette in Lua

You can create a visual palette reference using `fillrect`:

```lua
function script()
    local x0, y0, w, h = 8, 8, 12, 12
    local i = 0
    
    for row = 0, 3 do
        for col = 0, 15 do
            local idx = i
            fillrect(x0 + col * (w + 1), y0 + row * (h + 1), w, h, idx)
            i = i + 1
        end
    end
    
    drawtext(8, y0 + 4 * (h + 1) + 6, "NES Palette 0x00–0x3F", 0x20)
end
```

This will display all 64 colors from the NES palette as an overlay grid.

## See Also

- **[Color Functions](Color-Functions)** - Color manipulation functions
- **[Drawing Functions](Drawing-Functions)** - Drawing functions that use colors
- **[Home](Home)** - Return to the main wiki page