# Technical Details

## Lua Version

- **Lua Version:** Lua 5.1 (compatible with standard Lua 5.1 scripts)
- All standard Lua 5.1 features are available
- Compatible with existing Lua 5.1 libraries and scripts

## Update Frequency

- **`script()` / `gui()` callback:** Called at ~30Hz (every ~33ms) for performance
- **Rendering:** Overlay is double-buffered and composited at 60Hz to prevent flicker
- This means your drawing code runs at 30Hz, but the display updates at 60Hz for smooth rendering

## Rendering

- **Double-buffered overlay:** Prevents flicker and ensures smooth rendering
- **Compositing:** Lua-drawn content is composited on top of the NES frame
- **Coordinate System:** 
  - Origin (0, 0) is top-left
  - X increases rightward (0-255)
  - Y increases downward (0-239)
- **Clipping:** Content drawn outside the visible area (0-255, 0-239) is automatically clipped

## Color Palette

- **Palette System:** Uses FCEUX's NES palette system
- **Color Range:** 0x00-0x3F (64 colors total)
- **Common Colors:**
  - `0x39` - Yellow-green (good for text)
  - `0x20` - Bright white
  - `0x2E` - Black (transparent)
  - `0x16` - Red/orange-red
  - `0x29` - Medium bright green
- See [Palette Reference](Palette-Reference) for complete color table

## Script Timing

Control how often your `script()` callback runs. The overlay still composites at 60 Hz; this only changes the `script()` cadence.

<a id="setscriptinterval"></a>`r`n### `setscriptinterval(ms)`
Sets the callback interval for `script()`.

**Parameters:**
- `ms` (integer): Interval in milliseconds. Clamped to 16â€“1000 ms.

**Returns:** Nothing

**Notes:**
- Default is 33 ms (~30 Hz).
- Heavier scripts can set a slower interval (e.g., 100â€“250 ms). Lighter scripts can use 16 ms.
- The interval resets to 33 ms when Lua is (re)initialized.

**Example:**
```lua
-- Run script at ~10 Hz
setscriptinterval(100)

-- Later, restore to default
setscriptinterval(33)
```

<a id="getscriptinterval"></a>`r`n### `getscriptinterval()`
Returns the current `script()` interval in milliseconds.

**Returns:**
- (integer): Current interval in milliseconds

**Example:**
```lua
local ms = getscriptinterval()
drawtext(4, 4, string.format("Interval: %d ms", ms), 0x39)
```

## Performance

- **Thread:** Scripts run on the main emulation thread
- **Performance Impact:** Keep `script()` / `gui()` functions fast to maintain 60 FPS
- **Best Practices:**
  - Avoid heavy calculations in the main callback
  - Don't call expensive string operations every frame
  - Use local variables for frequently accessed values
  - Cache expensive computations when possible

## Script Loading Behavior

- **Automatic Loading:** All `.lua` and `.LUA` files in the `lua\` directories are automatically loaded when a game starts
- **Multiple Scripts:** You can place multiple scripts - they will all be loaded and their `script()` / `gui()` functions called
- **Reload Behavior:** Scripts are loaded fresh each time you start a game
- **Error Handling:** Script errors are logged (visible via debug output). Scripts that error won't crash the emulator, but won't draw anything
- **Load Order:** Scripts are loaded in alphabetical order by filename

## Search Paths

Scripts are automatically searched in these locations (in order):
1. `hdd1:\fce360-enhanced\lua\` (recommended - user-writable)
2. `game:\lua\` (game folder - may be read-only in packages)
3. `usb0:\lua\` (USB storage)

## Memory Access

- **Address Space:** Full NES 16-bit address space (0x0000-0xFFFF)
- **Memory Regions:**
  - RAM (0x0000-0x1FFF): Work RAM (mirrored)
  - PPU Registers (0x2000-0x3FFF): PPU I/O registers (mirrored)
  - APU and I/O (0x4000-0x401F): Audio processing unit and I/O registers
  - Expansion ROM (0x4020-0x5FFF): Expansion area
  - Cartridge RAM (0x6000-0x7FFF): Save RAM
  - Cartridge ROM (0x8000-0xFFFF): Program ROM (PRG)
- **Memory Mapping:** Uses FCEUX's memory mapping system, which handles all memory mapping correctly for different mappers and regions

## Audio Processing

- **Sample Rate:** Audio is processed at the NES APU sample rate
- **Channels:** 5 channels available (Pulse 1, Pulse 2, Triangle, Noise, DMC)
- **FFT Support:** Frequency domain analysis available via `getaudiofft()` and `getaudiochannelfft()`
- **Filtering:** Real-time audio filtering available via `setaudiofilter()`

## File I/O

- **File Paths:** Supports relative and absolute paths
- **Path Normalization:** Forward slashes automatically converted to backslashes
- **Search Locations:** Files are searched in multiple locations automatically
- **Binary Mode:** File reading uses binary mode to support any file type

## Input Handling

- **Button Bitmask:** Input is represented as a bitmask (0x00-0xFF)
- **Player Support:** Supports up to 4 players (0-3)
- **Hardware Input:** Can read hardware input even when using `setjoypad()`
- **Frame Accuracy:** Use `beforeframe()` callback for frame-accurate input control

## Codebase Structure

The Lua API bindings are organized into modular C++ files for maintainability and clarity. Each API category is implemented in its own module:

**Core Integration:**
- `fceux/fceux/fceulua.cpp` – Main Lua integration, script loading, and lifecycle management
- `fceux/fceux/lua_bindings.h` – Consolidated header including all module headers
- `fceux/fceux/lua_helpers.h/.cpp` – Centralized helper utilities (argument validation, error reporting, data conversion)
- `fceux/fceux/lua_shared_state.h` – Shared state structures and constants

**API Modules:**
- `lua_video.cpp` – Drawing functions (text, shapes, images, canvas)
- `lua_memory.cpp` – Memory reading, writing, scanning
- `lua_audio.cpp` – Audio analysis and processing
- `lua_fileio.cpp` – File and directory operations
- `lua_input.cpp` – Controller input and manipulation
- `lua_movie.cpp` – Input recording and state management
- `lua_profiler.cpp` – Performance monitoring and profiling
- `lua_emulator.cpp` – Emulation state and timing
- `lua_rom.cpp` – ROM information functions
- `lua_palette.cpp` – Color and palette operations
- `lua_runtime.cpp` – Runtime utilities
- `lua_gamegenie.cpp` – Game Genie code encoding/decoding

Each module uses:
- **Table-driven registration** (`static const luaL_Reg k<Domain>Funcs[]`)
- **Centralized helpers** from `lua_helpers.h` for validation and error reporting
- **Shared state structures** from `lua_shared_state.h` where appropriate

For detailed contributor guidelines, see [Contributing](Contributing).

## See Also

- **[Setup](Setup)** - How to set up Lua scripting
- **[Troubleshooting](Troubleshooting)** - Common issues and solutions
- **[Contributing](Contributing)** - Codebase structure and development guidelines
- **[Home](Home)** - Return to the main wiki page