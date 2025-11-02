# fce360-enhanced (FCEUX-360 tweaks)

Enhanced Xbox 360 port of the FCEUX NES emulator focused on front-end responsiveness. Core emulation code remains intact; improvements are limited to the Xbox UI layer (XUI scenes, input cadence, list scrolling).

> **Note:** Code hasn't been touched since around 2016, so I'm giving it some love with UI improvements and modern features while preserving the original emulation core.

* Toolchain: Visual Studio 2008 SP1
* SDK: Xbox 360 XDK 2.0.7645.1 (Nov 2008)
* Target: Xbox 360 (RGH/JTAG), retail-runnable `.xex`
* Current release: **v0.6.0** — *Lua scripting support for custom overlays and automation, favorite games list, frame-perfect rewind with speed ramping, recent games list, ROM search with Xbox keyboard UI, screenshot capture, fast forward (RT trigger), in-game OSD (pause menu), save states/slots, quick reset, + prior scrolling upgrades*

---

## Table of Contents

- [Features Showcase](#features-showcase)
- [What's New](#whats-new-v060)
  - [v0.6.0 - Lua Scripting Support](#whats-new-v060)
  - [v0.5.3 - Favorite Games List](#whats-new-v053)
  - [v0.5.2 - Rewind System](#whats-new-v052)
  - [v0.5.1 - Recent Games List](#whats-new-v051)
  - [v0.5.0 - ROM Search](#whats-new-v050)
  - [v0.4.0 - Screenshot Capture](#whats-new-v040)
  - [v0.3.1 - Fast Forward](#whats-new-v031)
  - [v0.3.0 - In-Game OSD](#whats-new-v030)
  - [v0.2 - Fast Scrolling](#whats-new-v02)
- [Repository Layout](#repository-layout-excerpt)
- [Build](#build)
- [Deploy to Xbox 360](#deploy-to-xbox-360-rghjtag)
- [Controls](#controls-front-end--osd)
  - [ROM Browser](#rom-browser)
  - [In-Game](#in-game)
- [Lua Scripting API](#lua-scripting-api)
  - [Setup](#setup)
  - [API Functions](#api-functions)
  - [Callbacks](#callbacks)
  - [Complete Examples](#complete-examples)
  - [Script Loading Behavior](#script-loading-behavior)
  - [Technical Details](#technical-details)
  - [Troubleshooting](#troubleshooting-1)
  - [Advanced: Multiple Scripts](#advanced-multiple-scripts)
- [Advanced Tuning](#advanced-tuning-optional)
- [Packaging Builds](#packaging-builds-for-github-releases)
- [Troubleshooting](#troubleshooting)
- [Changelog](#changelog)

---

## Features Showcase

### Recent Games List (v0.5.1)
![Recent Games List](img/recentGames.jpeg)

### ROM Search (v0.5.0)
![ROM Search](img/searchRoms.jpeg)

### Fast Scrolling (v0.2)

📹 **[Watch Fast Scrolling Demo](https://github.com/frankischilling/fce360-enhanced/raw/main/img/fastScrolling.mp4)** (MP4 video)

*Note: Click the link above to view the video demonstration. GitHub README files don't support embedded video playback.*

---

## What's new (v0.6.0)

* **Lua Scripting Support:** Full Lua 5.1 scripting engine for custom overlays and automation! Create your own HUDs, FPS counters, timers, and more.
  * **Automatic Script Loading:** Place `.lua` scripts in `lua\` folder - they auto-load when games start
  * **Rich API:** `drawtext()` for custom overlays, `getfps()` for frame rate monitoring, `joypad()` callback for input manipulation
  * **Double-Buffered Overlays:** Smooth 60Hz rendering with 30Hz Lua updates prevents flicker
  * **Multiple Scripts:** Load multiple scripts simultaneously - organize your overlays however you want
  * **Error Handling:** Script errors won't crash the emulator - safe and robust
  * See the **[Lua Scripting API](#lua-scripting-api)** section below for complete documentation and examples!

---

## What's new (v0.5.3)

* **Favorite Games List:** Press **X button** in the ROM browser to add or remove games from your favorites. Favorite games appear below recent games with a `[Favorite]` prefix and separator line. Favorites persist across sessions and are saved to `fceui.ini`. Favorite games are included in search results and the list automatically refreshes when toggling favorites.

---
## What's new (v0.5.2)

* **Rewind System:** Hold **LT (Left Trigger)** during gameplay to rewind through recent frames. Speed automatically ramps up the longer you hold: starts at 1x (frame-by-frame), then 2x after 0.25s, 4x after 0.75s, and 8x after 1.5s. The system stores up to 300 frames (~5 seconds at 60fps) in a circular buffer. Rewind stops when you release LT or reach the oldest saved state. Screenshot combo (LEFT_THUMB + LT) takes precedence over rewind.

---

## What's new (v0.5.1)

* **Recent Games List:** Automatically tracks the last 15 played ROMs and displays them at the top of the ROM browser with a `[Recent]` prefix. Recent games persist across sessions and are saved to `fceui.ini`. A visual separator (`---`) distinguishes recent games from the full ROM list. Recent games are included in search results and the list automatically refreshes when returning to the ROM browser.

![Recent Games List](img/recentGames.jpeg)

---

## What's new (v0.5.0)

* **ROM Search:** Press **Y button** in the ROM browser to open the Xbox 360 on-screen keyboard. Enter a game name to filter the ROM list in real-time with case-insensitive partial matching. Empty search shows all ROMs.

![ROM Search](img/searchRoms.jpeg)

---

## What's new (v0.4.0)

* **Screenshot capture:** Press **LEFT_THUMB (left stick click) + LT trigger** simultaneously during gameplay to capture screenshots. Screenshots are saved to `game:\snaps\` as PNG files using the ROM filename (e.g., `SuperMario-0.png`). Latch mechanism prevents multiple screenshots per button press.

---

## What's new (v0.3.1)

* **Fast Forward:** Press and hold **RT (Right Trigger)** during gameplay to speed up emulation at 2x speed. Release to return to normal speed. No configuration needed.

---

## What's new (v0.3.0)

* **In-game OSD & auto-pause:** Press **START + BACK** during gameplay to open the OSD. Emulation **pauses on entry** and **resumes on exit**.
* **Save states with slots:** Save/Load using a slot selector (0–9) from the OSD.
* **Quick Reset:** Restart the current game from the OSD.
* **GFX settings (experimental):** Toggle fullscreen/TV bezel and software filter presets. *Known to be a little buggy; see “Known Issues.”*
* No changes to the emulator core (APU/PPU/CPU/mappers).

---

## What's new (v0.2)

* **Time-based acceleration on Right Stick (RS):** Start precise, then ramp speed the longer you hold in one direction. Deflection magnitude also scales step size; hard caps prevent runaway scroll on huge libraries.
* **Held paging (LB/RB):** Hold a shoulder button to page up/down at a steady cadence.
* **Sane UX guards:** Minimum dwell to prevent double-steps, direction/neutral resets, and a deadzone so tiny bumps don't spam moves.
* **Precision preserved:** D-pad / Left Stick keep XUI's native single-step behavior for fine selection.

📹 **[Watch Fast Scrolling Demo](https://github.com/frankischilling/fce360-enhanced/raw/main/img/fastScrolling.mp4)** (MP4 video)

*Fast scrolling demonstration*

---

## Repository layout (excerpt)

* `fceux/` – Visual Studio 2008 solution and Xbox 360 project.
* `fceux/xbox/` – Xbox front-end (UI, input, filesystem glue).
* `fceux/xbox/ui/mainui.cpp` – XUI scenes (ROM browser, **OSD**, emulation runner). **Scrolling & OSD glue live here.**
* `fceux/media/` – Static assets (XUI skin `ui.xzp`, font `xarialuni.ttf`, textures).
* Core emulation lives under `fceux/fceux/` and is intentionally untouched.

---

## Build

1. Open `fceux\fceux.sln` in Visual Studio 2008 SP1.
2. Select Configuration: `Release_LTCG` and Platform: `Xbox 360`.
3. Build the `fceux` project.

Notes

* Post-build may warn:

  * `xbecopy: error X1001: Could not connect to Xbox 360 ''`
  * Expected if Neighborhood isn’t configured. The `.xex` still builds.
  * To silence, clear **Project Properties → Build Events → Post-Build**.
* Typical era/toolchain warnings are harmless here (e.g., `/GR-` RTTI notes, `FASTCALL` macro noise).

---

## Deploy to Xbox 360 (RGH/JTAG)

Create this layout on the console (e.g., `HDD1:\Emulators\FCEUX360\`):

```
FCEUX360\
├── media\          # copy everything from repo fceux\media\
├── roms\           # put .nes/.zip here
├── snaps\          # screenshots (optional)
├── states\         # save-states
├── fceui.ini       # created at runtime; can be an empty file initially
└── fceux.xex       # from fceux\Release_LTCG\
```

Steps

* Copy `fceux\Release_LTCG\fceux.xex` to the folder above.
* Copy all contents of `fceux\media\` into `media\` next to the `.xex` (**must** include `ui.xzp` and `xarialuni.ttf`).
* Launch `fceux.xex` from Aurora/FSD/XEXMenu. The first time you change a setting, `fceui.ini` will be written.

---

## Controls (front-end & OSD)

### ROM Browser

* **Recent Games:** Last 15 played ROMs appear at the top with `[Recent]` prefix and separator line. Automatically updated when games are loaded.
* **Favorite Games:** User-selected favorite games appear below recent games with `[Favorite]` prefix and separator line. Persist across sessions.
* **X:** **Toggle Favorite** — Add or remove the selected game from favorites (only in ROM browser, not during gameplay).
* **Y:** **Search** — Open Xbox keyboard to search ROMs by name. Filters list in real-time with case-insensitive partial matching.
* **Right Stick (hold up/down):** *Time-based acceleration* of selection.
* **LB / RB (hold):** Page up / page down at a steady cadence.
* **D-pad / Left Stick:** Single-step precision (native XUI behavior).
* **A:** Load game & start emulation.
* **B:** Back.

### In-Game

* **LT (Left Trigger):** **Rewind** — Hold to rewind gameplay. Speed ramps automatically: 1x → 2x → 4x → 8x based on hold duration. Stores up to ~5 seconds of gameplay history.
* **RT (Right Trigger):** **Fast Forward** — Hold to speed up emulation at 2x speed. Release to return to normal speed.
* **LEFT_THUMB + LT:** **Screenshot** — Press simultaneously to capture a screenshot. Saved to `game:\snaps\` using ROM filename (e.g., `SuperMario-0.png`). *Note: Screenshot combo takes precedence over rewind.*
* **START + BACK:** Open **OSD** (auto-pause).
* **OSD actions:** Save/Load State (with slots), Reset Game, GFX options (experimental). Exiting OSD resumes gameplay; "Load Game" returns to ROM browser.

---

## Lua Scripting API

FCE360 Enhanced includes full Lua 5.1 scripting support for custom overlays, automation, and game enhancements.

### Table of Contents (Lua API)

- [Setup](#setup)
- [Search Paths](#search-paths)
- [API Functions](#api-functions)
  - [Drawing Functions](#drawing-functions)
    - [`drawtext(x, y, text [, color])`](#drawtextx-y-text--color)
      - Parameters, Returns, Notes, Examples, Common Colors
    - [`drawtextwh(x, y, text, color, max_w, max_h, border)`](#drawtextwhx-y-text-color-max_w-max_h-border)
      - Parameters (including border details), Returns, Notes, Border Styles, Examples
    - [`drawpixel(x, y, color)`](#drawpixelx-y-color)
      - Parameters, Returns, Notes, Examples
    - [`drawline(x1, y1, x2, y2, color)`](#drawlinex1-y1-x2-y2-color)
      - Parameters, Returns, Notes, Examples
    - [`drawrect(x, y, w, h, color)`](#drawrectx-y-w-h-color)
      - Parameters, Returns, Notes, Examples
    - [`fillrect(x, y, w, h, color)`](#fillrectx-y-w-h-color)
      - Parameters, Returns, Notes, Examples
    - [`clearrect(x, y, w, h)`](#clearrectx-y-w-h)
      - Parameters, Returns, Notes, Examples
  - [Monitoring Functions](#monitoring-functions)
    - [`getfps()`](#getfps)
      - Parameters, Returns, Notes, Basic & Advanced Examples
- [Callbacks](#callbacks)
  - [`gui()`](#gui) - **Required callback**
    - When Called, Important Notes, Basic & Advanced Examples
  - [`joypad(player, buttons)`](#joypadplayer-buttons-optional) - *Optional callback*
    - Button Bitmask Reference, Bitwise Operations, Multiple Examples
- [Complete Examples](#complete-examples)
  - [FPS Display](#fps-display)
  - [On-Screen Timer](#on-screen-timer)
  - [Multi-Line Status Display](#multi-line-status-display)
- [Script Loading Behavior](#script-loading-behavior)
- [Technical Details](#technical-details)
  - Lua Version, Update Frequency, Rendering, Coordinate System, Color Palette, Performance
- [Troubleshooting](#troubleshooting-1)
  - Script not loading, Text not appearing, Script errors, Performance issues
- [Advanced: Multiple Scripts](#advanced-multiple-scripts)

---

### Setup

1. **Create the Lua directory** in your game folder (same location as `fceux.xex`):
   ```
   FCEUX360\
   ├── lua\          # Create this folder for Lua scripts
   ├── media\
   ├── roms\
   └── fceux.xex
   ```

2. **Place your scripts** in the `lua\` folder as `.lua` files.

3. **Scripts auto-load** when a game starts - no manual loading required!

### Search Paths
- `hdd1:\fce360-enhanced\lua\` (recommended - user-writable)
- `game:\lua\` (game folder - may be read-only in packages)
- `usb0:\lua\` (USB storage)

### API Functions

#### Drawing Functions

##### `drawtext(x, y, text [, color])`
Draws **borderless** text on the screen overlay using FCEUX's built-in font renderer. This function draws only the glyph pixels (characters) with no background, outline, or shadow.

**Parameters:**
- `x` (integer): X coordinate (0-255). NES horizontal resolution is 256 pixels.
- `y` (integer): Y coordinate (0-239). NES vertical resolution is 240 pixels.
- `text` (string): Text string to display. Multi-line text is not directly supported - use `drawtextwh()` for multi-line support.
- `color` (integer, optional): Palette color index (default: `0x20`). Valid range is 0x00-0x3F (automatically mapped to NES palette range).

**Returns:** Nothing

**Notes:**
- Text is drawn using an 8×8 pixel font. Each character occupies 8 pixels horizontally.
- **Borderless rendering:** This function draws only the glyph pixels with no background, outline, or shadow effects. For text with borders/outlines, use `drawtextwh()`.
- Coordinates (0, 0) represent the top-left corner of the screen.
- Text drawn outside the visible area (0-255, 0-239) will be clipped automatically.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- The entire 8-pixel height row is cleared before drawing to prevent ghosting from previous frames.

**Example:**
```lua
drawtext(4, 4, "Hello World!", 0x20)        -- White text at top-left (no border)
drawtext(100, 120, "Score: 1000", 0x2E)     -- Yellow/green text centered (no border)
drawtext(4, 232, "Bottom text", 0x0F)       -- Near bottom of screen (no border)
```

##### `drawtextwh(x, y, text, color, max_w, max_h, border)`
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
- Text is drawn using an 8×8 pixel font. Each character occupies 8 pixels horizontally.
- **Multi-line support:** Use newline characters (`\n`) in the text string to create multi-line displays. Text automatically wraps within `max_w` pixels.
- **Border rendering:** When `border > 0`, the function draws a dimmed background (using palette indices 0xC1, 0xD1, 0xCF) around text for improved contrast and readability. Border style 2 provides the thickest outline for maximum visibility.
- **Borderless mode (`border = 0`):** When border is 0, the function proactively clears the specified `max_w × max_h` area before drawing to prevent ghosting from previous frames, then draws only the glyph pixels.
- Coordinates (0, 0) represent the top-left corner of the screen.
- Text drawn outside the visible area (0-255, 0-239) will be clipped automatically.
- The overlay is composited on top of the NES frame, so Lua-drawn text appears above game graphics.
- For simple single-line text without borders, `drawtext()` is more efficient and automatically handles ghosting prevention.

**Example:**
```lua
-- Borderless text (same as drawtext but with size limits)
drawtextwh(4, 4, "FPS: 60.0", 0x2E, 200, 16, 0)

-- Multi-line text with thin border for better visibility
drawtextwh(10, 50, "Line 1\nLine 2\nLine 3", 0x20, 150, 32, 1)

-- Text with thick border for maximum contrast
drawtextwh(10, 100, "IMPORTANT!", 0x0F, 200, 16, 2)

-- Wrapped text with border in a panel
fillrect(5, 115, 120, 60, 0x10)           -- Dark background panel
drawrect(5, 115, 120, 60, 0x20)           -- White border
drawtextwh(10, 120, "Status:\nHealth: 100\nScore: 5000", 0x2E, 110, 50, 1)
```

**Border Style Comparison:**
- **`border = 0`:** Clean glyph-only rendering, no background interference. Fastest, least visual impact.
- **`border = 1`:** Thin outline provides good contrast on most backgrounds. Slightly dimmed background.
- **`border = 2`:** Thick outline with enhanced background for maximum readability. Best for text over complex or moving backgrounds.

##### `drawpixel(x, y, color)`
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
  drawpixel(10 + i, 10 + i, 0x2E)  -- Yellow/green diagonal line
end

-- Draw a single pixel
drawpixel(128, 120, 0x20)  -- White pixel at screen center
```

##### `drawline(x1, y1, x2, y2, color)`
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

**Example:**
```lua
-- Draw a crosshair at screen center
drawline(108, 120, 148, 120, 0x3F)  -- Horizontal line
drawline(128, 100, 128, 140, 0x3F)  -- Vertical line

-- Draw a box using lines
drawline(200, 30, 250, 30, 0x2E)    -- Top
drawline(250, 30, 250, 80, 0x2E)    -- Right
drawline(250, 80, 200, 80, 0x2E)    -- Bottom
drawline(200, 80, 200, 30, 0x2E)    -- Left
```

##### `drawrect(x, y, w, h, color)`
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
drawrect(10, 50, 60, 40, 0x20)  -- White outline rectangle

-- Draw multiple rectangles with different colors
drawrect(10, 50, 60, 40, 0x20)   -- White outline
drawrect(80, 50, 60, 40, 0x2E)   -- Yellow/green outline
drawrect(150, 50, 50, 30, 0x3F)  -- Bright outline

-- Draw a border around an area (you can use multiple drawrect calls for nested borders)
drawrect(5, 115, 120, 60, 0x20) -- White border around panel
```

##### `fillrect(x, y, w, h, color)`
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
fillrect(10, 50, 60, 40, 0x10)  -- Dark gray filled rectangle

-- Draw a progress bar
local barWidth = 100  -- Progress percentage
fillrect(10, 100, barWidth, 8, 0x2E)  -- Filled progress bar
drawrect(10, 100, 100, 8, 0x3F)        -- Border around progress bar

-- Draw a background panel with border
fillrect(5, 115, 120, 60, 0x10)       -- Dark background
drawrect(5, 115, 120, 60, 0x20)      -- White border around panel

-- Draw multiple filled rectangles with varying colors
for i = 0, 7 do
    local x = 180 + (i % 4) * 18
    local y = 170 + math.floor(i / 4) * 18
    fillrect(x, y, 15, 15, 0x20 + i)  -- Varying colors
    drawrect(x, y, 15, 15, 0x3F)      -- Border on each
end
```

##### `clearrect(x, y, w, h)`
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
drawtext(6, 170, string.format("FPS: %.1f", fps), 0x2E)

-- Clear a region that changes size
local barWidth = math.floor(progress * 100)
clearrect(10, 100, 100, 8)  -- Clear entire bar area
fillrect(10, 100, barWidth, 8, 0x2E)  -- Redraw with new width
```

**Common Color Values:**
- `0x20` - White
- `0x2E` - Yellow/Green
- `0x0F` - Red/Pink
- `0x30` - Light gray
- `0x00` - Black (rarely visible on overlay)

#### Monitoring Functions

##### `getfps()`
Returns the current frame rate as a floating-point number. The FPS is recalculated every second.

**Parameters:** None

**Returns:** 
- `number` - Current FPS value (typically 60.0 for normal speed, 120.0 for 2× fast-forward, etc.)

**Notes:**
- The FPS value is updated once per second, so rapid calls within the same second will return the same value.
- During fast-forward (RT trigger), FPS will reflect the increased emulation speed.
- During pause, FPS should remain stable (may show the last calculated value before pause).

**Example:**
```lua
local fps = getfps()
drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
```

**Advanced Example:**
```lua
local lastFPS = 0
local fpsHistory = {}

function gui()
    local fps = getfps()
    
    -- Track FPS history for averaging
    table.insert(fpsHistory, fps)
    if #fpsHistory > 60 then
        table.remove(fpsHistory, 1)
    end
    
    -- Calculate average
    local sum = 0
    for i = 1, #fpsHistory do
        sum = sum + fpsHistory[i]
    end
    local avgFPS = sum / #fpsHistory
    
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
    drawtext(4, 12, string.format("Avg: %.1f", avgFPS), 0x30)
end
```

### Callbacks

Callbacks are functions that your script defines, which the emulator will call automatically at specific times.

#### `gui()`
**Required callback** - Called every ~33ms (~30Hz) to draw overlay content. This is your main drawing function and must be defined in your script.

**Parameters:** None

**Returns:** Nothing (return values are ignored)

**When Called:**
- Automatically called by the emulator during each frame rendering cycle
- Runs at approximately 30Hz (every 33 milliseconds) for performance
- The overlay is double-buffered and composited at 60Hz to prevent flicker
- Called even when emulation is paused (if a game is loaded)

**Important Notes:**
- Your script **must** define this function, or nothing will be drawn
- Keep this function lightweight - heavy computations can impact emulation performance
- All drawing functions (`drawtext`, etc.) must be called from within `gui()` to appear on screen
- The function can be empty if you only use `joypad()` for input modification

**Basic Example:**
```lua
function gui()
    -- Draw FPS counter
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
    
    -- Draw a status message
    drawtext(4, 12, "Lua Active", 0x20)
end
```

**Advanced Example with State:**
```lua
local startTime = os.clock()
local frameCounter = 0

function gui()
    frameCounter = frameCounter + 1
    
    -- FPS display
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
    
    -- Frame counter
    drawtext(4, 12, string.format("Frames: %d", frameCounter), 0x30)
    
    -- Elapsed time (approximate)
    local elapsed = os.clock() - startTime
    local minutes = math.floor(elapsed / 60)
    local seconds = math.floor(elapsed % 60)
    drawtext(4, 20, string.format("Time: %02d:%02d", minutes, seconds), 0x20)
end
```

#### `joypad(player, buttons)` *(Optional)*
Optional callback - Called when the emulator reads controller input. Allows you to intercept and modify button presses before they reach the game.

**Parameters:**
- `player` (integer): Player number (0-based)
  - `0` = Player 1 (first controller)
  - `1` = Player 2 (second controller, if connected)
- `buttons` (integer): Current raw button state as a bitmask

**Returns:** 
- `integer` - Modified button state that will be used by the game

**Button Bitmask Reference:**
The `buttons` parameter is a bitmask where each bit represents a button:
- Bit 0 (0x01): Right
- Bit 1 (0x02): Left
- Bit 2 (0x04): Down
- Bit 3 (0x08): Up
- Bit 4 (0x10): Start
- Bit 5 (0x20): Select
- Bit 6 (0x40): B
- Bit 7 (0x80): A

**Notes:**
- This function is called every frame when reading controller input
- If you don't define this function, input passes through unchanged
- You can use bitwise operations to modify buttons:
  - `buttons | mask` - Press a button (set bit)
  - `buttons & ~mask` - Release a button (clear bit)
  - `buttons ^ mask` - Toggle a button (flip bit)
- Modifications apply immediately to the game
- Can be used for auto-fire, button combinations, macros, etc.

**Example: Auto-Fire A Button:**
```lua
local autoFireFrame = 0

function joypad(player, buttons)
    -- Auto-fire for player 1 only
    if player == 0 then
        autoFireFrame = autoFireFrame + 1
        -- Press A button every other frame (30 Hz auto-fire)
        if (autoFireFrame % 2) == 0 then
            return buttons | 0x80  -- Set A button (bit 7)
        end
    end
    return buttons  -- Pass through unmodified
end
```

**Example: Turbo Mode (All Buttons Auto-Fire):**
```lua
local turboFrame = 0

function joypad(player, buttons)
    turboFrame = turboFrame + 1
    if player == 0 then
        -- Fast turbo (every 2 frames)
        if (turboFrame % 2) == 0 then
            -- Toggle A and B buttons for turbo effect
            return buttons ^ (0x80 | 0x40)
        end
    end
    return buttons
end
```

**Example: Button Combo (Hold B + Right = Run):**
```lua
function joypad(player, buttons)
    if player == 0 then
        -- If B is pressed, also press Right (useful for some games)
        if (buttons & 0x40) ~= 0 then  -- B button pressed
            return buttons | 0x01  -- Also press Right
        end
    end
    return buttons
end
```

**Example: Input Passthrough (No Modification):**
```lua
function joypad(player, buttons)
    -- You can log or monitor input without modifying it
    -- Just return the original buttons value
    return buttons
end
```

### Complete Examples

#### FPS Display
```lua
function gui()
    local fps = getfps()
    drawtext(2, 2, string.format("FPS: %.1f", fps), 0x2E)
end
```

#### On-Screen Timer
```lua
local startTime = 0
local running = false

function gui()
    if running then
        local elapsed = getfps() * (os.clock() - startTime)  -- Approximate
        local minutes = math.floor(elapsed / 3600)
        local seconds = math.floor((elapsed % 3600) / 60)
        drawtext(4, 4, string.format("Time: %02d:%02d", minutes, seconds), 0x2E)
    end
end
```

#### Multi-Line Status Display
```lua
function gui()
    local fps = getfps()
    local lineHeight = 10
    
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x2E)
    drawtext(4, 4 + lineHeight, "Status: Running", 0x20)
    drawtext(4, 4 + lineHeight * 2, "Press LT to rewind", 0x0F)
end
```

### Script Loading Behavior

- **Automatic Loading:** All `.lua` and `.LUA` files in the `lua\` directories are automatically loaded when a game starts.
- **Multiple Scripts:** You can place multiple scripts - they will all be loaded and their `gui()` functions called.
- **Reload Behavior:** Scripts are loaded fresh each time you start a game.
- **Error Handling:** Script errors are logged (visible via debug output). Scripts that error won't crash the emulator, but won't draw anything.

### Technical Details

- **Lua Version:** Lua 5.1 (compatible with standard Lua 5.1 scripts)
- **Update Frequency:** `gui()` is called at ~30Hz (every ~33ms) for performance
- **Rendering:** Overlay is double-buffered and composited at 60Hz to prevent flicker
- **Coordinate System:** 
  - Origin (0, 0) is top-left
  - X increases rightward (0-255)
  - Y increases downward (0-239)
- **Color Palette:** Uses FCEUX's NES palette system. Color index 0x2E is typically yellow/green, 0x20 is white.
- **Performance:** Scripts run on the main emulation thread. Keep `gui()` functions fast to maintain 60 FPS.

### Troubleshooting

**Script not loading:**
- Verify the `lua\` folder exists in the same directory as `fceux.xex`
- Check file extension is `.lua` (not `.txt`)
- Ensure script file is not empty
- Check debug output for load errors

**Text not appearing:**
- Verify `gui()` function is defined in your script
- Check coordinates are within bounds (0-255, 0-239)
- Try a simple test: `drawtext(4, 4, "TEST", 0x2E)`
- Ensure script loaded successfully (check debug output)

**Script errors:**
- Check debug output for Lua error messages
- Verify function names match exactly (`gui`, `drawtext`, `getfps`)
- Test with a minimal script first

**Performance issues:**
- Keep `gui()` functions simple - avoid heavy calculations
- Don't call expensive string operations every frame
- Use local variables for frequently accessed values

### Advanced: Multiple Scripts

You can create multiple `.lua` files, each with its own `gui()` function. All `gui()` functions will be called in load order. Each script maintains its own global state.

**Example:**
```
lua\
├── fps.lua      # Shows FPS counter
├── timer.lua    # Shows game timer
└── hud.lua      # Custom HUD elements
```

All three will load and execute simultaneously!

---

## Advanced tuning (optional)

All tunables live in the ROM list scene (`LoadGame` in `fceux/xbox/ui/mainui.cpp`):

* **Deadzone (RS):** `const float RS_DEADZONE = ~0.28–0.30f`
* **Held paging cadence (LB/RB):** `const DWORD pageRepeatMs = ~100;`
* **General dwell/response:**

  ```
  m_initialDelayMs   = 180;  // initial repeat delay
  m_repeatIntervalMs = 70;   // baseline cadence (non-RS path)
  m_minDwellMs       = 50;   // minimum time between injected moves
  ```
* **Acceleration tiers (RS hold time):** ramps from ~150–160 ms (1 step) down to ~35 ms (3 steps) after ~2.6s hold; deflection magnitude scales steps.

---

## Packaging builds for GitHub Releases

Attach a zip containing:

```
FCEUX360-<version>-xex.zip
└── FCEUX360\
    ├── fceux.xex
    ├── fceui.ini              # optional seed (empty)
    ├── media\                 # from repo
    ├── roms\                  # empty
    ├── snaps\                 # empty
    └── states\                # empty
```

---

## Troubleshooting

* **OSD doesn’t open:** Must be *in-game* (emulation scene active). Press **START + BACK** simultaneously. Ensure `ui.xzp` contains the OSD scenes and that your tab order puts OSD reachable from the emulation scene (default uses `GoToNext()`).
* **GFX settings revert or don’t apply:** Known issue; sometimes UI state and renderer can desync on scene changes. Work is in progress to harden state propagation and persistence.
* **“Holding longer doesn’t speed up”:** Acceleration is on **Right Stick**; D-pad/Left Stick remain single-step. Check the RS deadzone and stick calibration.
* **Black UI or missing text:** Verify `media\ui.xzp` and `media\xarialuni.ttf` are present.
* **Empty ROM list:** Place `.nes`/`.zip` files under `roms\`.
* **Screenshots not saving:** Ensure you're pressing LEFT_THUMB (stick click, not movement) + LT trigger simultaneously. Verify `game:\snaps\` directory exists and has write permissions.
* **Post-build copy error:** Expected without Neighborhood; deploy via FTP manually.

---

## Changelog

* **v0.6.0**

  * feat(lua): Full Lua 5.1 scripting engine integration for custom overlays and automation.
  * feat(lua): Automatic script loading from `lua\` directories (`hdd1:\fce360-enhanced\lua\`, `game:\lua\`, `usb0:\lua\`).
  * feat(lua): API functions: `drawtext(x, y, text [, color])` for overlay drawing, `getfps()` for frame rate monitoring.
  * feat(lua): Callback system: `gui()` function called every frame (~30Hz) for drawing, optional `joypad()` for input manipulation.
  * feat(lua): Double-buffered overlay system with 60Hz composition prevents flicker during 30Hz Lua updates.
  * feat(lua): Support for multiple simultaneous scripts - all `.lua` files in `lua\` folders are auto-loaded.
  * feat(lua): Robust error handling - script errors logged but don't crash emulator.
  * tech(lua): Lua 5.1 compatibility - works with standard Lua scripts.
  * tech(lua): Performance optimized - Lua runs at 30Hz while overlay composites at 60Hz for smooth rendering.
  * tech(lua): Status message system shows "Lua: Loaded..." when scripts are successfully loaded.
  * fix(lua): Fixed overlay ghosting issues with improved clearing logic for status messages.
  * fix(lua): Prevented blank overlay publishing when scripts early-return without drawing.
  * docs(lua): Complete Lua API documentation added to README with examples and troubleshooting guide.

* **v0.5.3**

  * feat(favorites): Favorite games list via X button in ROM browser.
  * feat(favorites): Favorite games displayed below recent games with `[Favorite]` prefix.
  * feat(favorites): Visual separators (`---`) between recent, favorites, and full ROM list.
  * feat(favorites): Favorites persist across sessions via `fceui.ini` config file.
  * feat(favorites): Auto-cleans deleted ROMs from favorites list on load.
  * feat(favorites): Favorite games included in search results.
  * feat(favorites): List refreshes automatically when toggling favorites.
  * tech: Favorite games stored in `[favorites]` section of config as `game0`, `game1`, etc.
  * tech: Optimized save performance to prevent UI freezing when adding favorites.
  * fix(favorites): X button only works in ROM browser, prevents freeze during gameplay.
  * fix(favorites): Favorites persist across builds, matching recent games behavior.

* **v0.5.2**

  * feat(rewind): Frame-perfect rewind system via LT trigger with automatic speed ramping.
  * feat(rewind): Stores up to 300 frames (~5 seconds at 60fps) in circular buffer.
  * feat(rewind): Speed ramping: 1x → 2x (0.25s) → 4x (0.75s) → 8x (1.5s) based on hold duration.
  * feat(rewind): Continuous rewind while LT is held; stops when released or reaching oldest state.
  * fix(rewind): Fixed premature stopping bug where rewind would pause every frame.
  * fix(input): Prevents LT input from leaking through to NES gamepad during rewind.
  * tech: Rewind buffer saves states every frame using FCEU save/load system.
  * tech: Audio skipped during rewind for better performance.
  * tech: Proper initialization and cleanup of rewind state variables.
  * tech: Rewind buffer cleared when loading new games.
* **v0.5.1**

  * feat(recent): Recent games list tracks last 15 played ROMs automatically.
  * feat(recent): Recent games displayed at top of ROM browser with `[Recent]` prefix.
  * feat(recent): Visual separator (`---`) between recent and full ROM list.
  * feat(recent): Recent games persist across sessions via `fceui.ini` config file.
  * feat(recent): Auto-cleans deleted ROMs from recent list on load.
  * feat(recent): Recent games included in search results.
  * feat(recent): List refreshes automatically when returning to ROM browser.
  * tech: Recent games stored in `[recent]` section of config as `game0`, `game1`, etc.
  * tech: Added `OnEnterTab` handler to reload recent games when scene becomes active.
  * tech: File existence validation removes stale entries from recent list.
* **v0.5.0**

  * feat(search): ROM search functionality with Xbox 360 on-screen keyboard (Y button).
  * feat(search): Case-insensitive partial matching (e.g., "mario" matches "Super Mario Bros").
  * feat(search): Real-time filtering of ROM list as you type.
  * feat(search): Non-blocking async implementation keeps UI responsive.
  * tech: Uses XShowKeyboardUI with overlapped I/O for native Xbox keyboard interface.
  * tech: Maintains full ROM list for filtering; filtered results displayed in XUI list.
  * tech: Search filter persists until cleared or modified.
* **v0.4.0**

  * feat(screenshot): Screenshot capture via LEFT_THUMB button + LT trigger during gameplay.
  * feat(screenshot): Screenshots saved to `game:\snaps\` with automatic directory creation.
  * feat(screenshot): Screenshots use actual ROM filename instead of generic "game" naming (e.g., `SuperMario-0.png`).
  * tech: Manually extracts filename from ROM path and sets FileBase for proper snapshot naming.
  * tech: Supports ROMs from .nes files and .zip archives.
  * fix(screenshot): Fixed button detection to use current frame state (wButtons) instead of previous frame.
  * fix(screenshot): Made snapshot directory string static to ensure pointer validity.
* **v0.3.1**

  * feat(emulation): Fast forward via RT trigger at fixed 2x speed multiplier.
  * tech: Added fast forward detection in main emulation loop; runs multiple frames per render cycle when active.
  * tech: Optimized audio/video processing to only occur on final frame during fast forward.
  * fix(ui): Removed C++11 in-class initialization in `mainui.cpp` for compatibility with build configuration.
* **v0.3.0**

  * feat(osd): in-game OSD via **START + BACK**, auto-pause on entry, resume on exit.
  * feat(osd): Save/Load State with slot selector; Quick Reset.
  * feat(ui): (Experimental) GFX switches for fullscreen/TV bezel and software filter presets.
  * tech: Added `UpdatePerFrame()` to `XuiRunner` to poll combo; uses a latch to prevent re-trigger while held.
  * tech: Registered `XuiRunner` instance globally and invoked from `RenderXui()` for per-frame checks.
  * note: Core emulation remains untouched.
* **v0.2**

  * feat: Time-based Right Stick acceleration with deflection scaling + hard caps.
  * feat: Continuous paging while holding LB/RB.
  * ux: Debounce/min-dwell & direction resets to eliminate double-moves.
  * refactor: Movement injection isolated in `UpdatePerFrame()`; no heap churn on hot path.
* **v0.1**

  * Initial enhanced UI pass; core emulation untouched.

---
