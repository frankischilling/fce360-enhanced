# fce360-enhanced (FCEUX-360 tweaks)

Enhanced Xbox 360 port of the FCEUX NES emulator focused on front-end responsiveness. Core emulation code remains intact; improvements are limited to the Xbox UI layer (XUI scenes, input cadence, list scrolling).

> **Note:** Code hasn't been touched since around 2016, so I'm giving it some love with UI improvements and modern features while preserving the original emulation core.

> **Note:** This can also be built with newer versions of Visual Studio and the Xbox 360 SDK/XDK. If you want to update from Visual Studio 2008, try using Visual Studio 2010. Confirmed working on Xbox 360 SDK 21256.3.

> **Warning:** You might run into build errors or incompatibilities when using newer toolchains or SDK/XDK versions. Building outside of the original environment (VS2008 + Nov 2008 XDK) may require extra fixes or changes.

* Toolchain: Visual Studio 2008 SP1
* SDK: Xbox 360 XDK 2.0.7645.1 (Nov 2008)
* Also builds on Xbox 360 SDK 21256.3
* Target: Xbox 360 (RGH/JTAG), retail-runnable `.xex`
* Current release: **v0.8.6** *Game Genie Support Returns: Toggle an Xbox keyboard prompt before launch, enter one or more codes, and watch them decode, activate, and save automatically just like the original hardware magic.*

---

## Table of Contents

- [Features Showcase](#features-showcase)
- [What's New](#whats-new)
  - [v0.8.6 - Game Genie Support Returns](#whats-new-v086)
  - [v0.8.5 - Battery Save Fixes and ROM Information Functions](#whats-new-v085)
  - [v0.8.4 - Enhanced Input Recording Functions](#whats-new-v084)
  - [v0.8.3 - Input Recording Save/Load API](#whats-new-v083)
  - [v0.8.2 - Enhanced Input API](#whats-new-v082)
  - [v0.8.1 - Enhanced Drawing API](#whats-new-v081)
  - [v0.8.0 - Complete File I/O API Suite](#whats-new-v080)
  - [v0.7.9 - Complete Audio API Suite](#whats-new-v079)
  - [v0.7.8 - Audio API Functions and Screenshot Performance Fix](#whats-new-v078)
  - [v0.7.7 - State Management and Xbox 360 Input Lua API Functions](#whats-new-v077)
  - [v0.7.6 - New Overlay Functions, Screenshot Improvements, and Text Rendering Updates](#whats-new-v076)
  - [v0.7.5 - Timing, Screen Info, and Color Manipulation Lua API Functions](#whats-new-v075)
  - [v0.7.4 - Game State, Game Genie, and Timing Lua API Functions](#whats-new-v074)
  - [v0.7.3 - ROM Information Lua API Functions](#whats-new-v073)
  - [v0.7.2 - New Lua API Functions](#whats-new-v072)
  - [v0.7.1 - Text Measurement and Rotation API Functions](#whats-new-v071)
  - [v0.7.0 - ROM Counter Display](#whats-new-v070)
  - [v0.6.9 - Famicom Disk System Support](#whats-new-v069)
  - [v0.6.8.1 - VSync Synchronization Hotfix](#whats-new-v0681)
  - [v0.6.8 - VSync Frame Pacing Fix](#whats-new-v068)
  - [v0.6.7 - Advanced Memory API Functions](#whats-new-v067)
  - [v0.6.6 - Enhanced Lua Console Scrolling](#whats-new-v066)
  - [v0.6.5 - Lua Console and Script Timing Controls](#whats-new-v065)
  - [v0.6.4 - Complete Memory API and Script Callback Rename](#whats-new-v064)
  - [v0.6.3 - Expanded Drawing API and Crash Prevention Fixes](#whats-new-v063)
  - [v0.6.2 - Rewind and Fast-Forward Input Fixes](#whats-new-v062)
  - [v0.6.1 - Lua Drawing API Upgrade](#whats-new-v061)
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
  - [Setup](https://github.com/frankischilling/fce360-enhanced/wiki/Setup)
  - [API Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Home#api-reference)
  - [Callbacks](https://github.com/frankischilling/fce360-enhanced/wiki/Callbacks)
  - [Complete Examples](https://github.com/frankischilling/fce360-enhanced/wiki/Examples)
  - [Script Loading Behavior](https://github.com/frankischilling/fce360-enhanced/wiki/Technical-Details#script-loading-behavior)
  - [Technical Details](https://github.com/frankischilling/fce360-enhanced/wiki/Technical-Details)
  - [Troubleshooting](https://github.com/frankischilling/fce360-enhanced/wiki/Troubleshooting)
  - [Advanced: Multiple Scripts](https://github.com/frankischilling/fce360-enhanced/wiki/Technical-Details#script-loading-behavior)
- [Advanced Tuning](#advanced-tuning-optional)
- [Packaging Builds](#packaging-builds-for-github-releases)
- [Troubleshooting](#troubleshooting)

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

## What's New

*Current release: **v0.8.6** Game Genie Support Returns: Flip on the keyboard prompt, punch in classic codes, and they fire instantly just like the cartridge add-on.*

---

## What's new (v0.8.6)

* **Game Genie is officially back on Xbox 360**
  * Enable `[cheat]` -> `enable=1` in `game:\fceui.ini` to light up the new pre-launch keyboard prompt.
  * Mash in your favorite NES-era codes before the ROM even boots�cancel or submit nothing if you just want a vanilla launch.

* **Stack codes, hit start, and everything just works**
  * Feed the prompt multiple 6- or 8-character codes separated by spaces, commas, or hyphen chains like `YSAOPE-YEAOZA-YEAPYA`.
  * Every valid entry is decoded through the built-in Game Genie interpreter, activated on the fly, and saved into the per-ROM `.cht` so they persist just like native cheats.
  * Empty input skips the feature for that launch, and bad characters trigger a friendly OSD toast instead of blowing up the emulator.

---

## What's new (v0.8.5)

* **Major Update: Fixed Battery Saves**
  * **Critical Fix:** Battery saves now work correctly! This update makes FCE360 Enhanced a proper emulator for long-term gameplay.
  * **Previous Issue:** Saves were being written to `game:sav/test.sav` instead of using the ROM name, and saves weren't persisting correctly.
  * **Fixed:**
    * Save files now use the actual ROM name (e.g., `game:\sav\The Legend of Zelda.sav`, `game:\sav\Kirbys Adventure.sav`)
    * All save slots now work correctly (not just the first slot)
    * Saves properly persist across game sessions
    * Save directory (`game:\sav\`) is automatically created on startup
    * Fixed path resolution for Xbox device paths (`game:`, `hdd1:`, etc.)
    * Fixed FileBase extraction from archive paths and device paths
    * Ensured saves are written correctly on game close

* **New ROM Information Functions:** Added **5 powerful ROM information functions** for ROM analysis, identification, and file management!

  * **ROM Hashing:**
    * `getromhash(algorithm)` - Gets ROM hash using specified algorithm
      * Parameters: algorithm (string: "crc32", "crc", "md5", "sum", "sum16", "xor")
      * Returns: hexadecimal string representation
      * Case-insensitive algorithm names
      * Returns empty string if no ROM is loaded
      * Throws error for unsupported algorithms (sha1, sha256, sha512)
      * Use case: ROM identification, verification, database lookups
      * Example: `getromhash("crc32")` - returns 8-character hex string
      * Example: `getromhash("md5")` - returns 32-character hex string

  * **iNES Header Analysis:**
    * `getinesheader()` - Gets full iNES header dump as a Lua table
      * Parameters: none
      * Returns: table with comprehensive ROM header information, or nil if no ROM is loaded
      * Table fields: id, rom_size, vrom_size, rom_type, rom_type2, mapper, mirroring, mirroring_string, has_battery, has_trainer, four_screen, vs_system, playchoice10, nes2_format, raw_header, reserve
      * Use case: ROM analysis, mapper detection, header validation, compatibility checking
      * Example: `local header = getinesheader(); print("Mapper: " .. header.mapper)`

  * **Region Detection:**
    * `getregion()` - Gets ROM region (NTSC, PAL, or Dendy)
      * Parameters: none
      * Returns: string ("NTSC", "PAL", or "Dendy")
      * Reads region from iNES header TV system bits
      * Supports both iNES 1.0 and NES 2.0 formats
      * Returns "NTSC" as default if no ROM is loaded
      * Use case: Region-specific behavior, compatibility checks
      * **NOTE:** This feature requires updated UI files (LoadGame.xui, LoadGame.xur, ui.xzp) to be pushed
      * Example: `local region = getregion(); print("Region: " .. region)`

  * **ROM Path Information:**
    * `getrompath()` - Gets current ROM file path (full path including device and filename)
      * Parameters: none
      * Returns: string (full file path), or empty string if no ROM is loaded
      * Handles archive paths (path.zip|internal.nes format)
      * Use case: File operations relative to ROM location, path-based scripts
      * Example: `local path = getrompath(); print("ROM path: " .. path)`

  * **Save File Path:**
    * `getsavepath()` - Gets save file path for current ROM
      * Parameters: none
      * Returns: string (save file path), or empty string if no ROM is loaded
      * Path format: `game:\sav\<ROMName>.sav`
      * Use case: Save file management, backup operations, save file inspection
      * Example: `local savePath = getsavepath(); print("Save path: " .. savePath)`

* **UI Changes:**
  * Removed PAL/NTSC region toggle from LoadGame.xui, LoadGame - Copie.xui, and GameConfig.xui
  * Region is now automatically detected from ROM header
  * Updated default focus in config scenes to skip removed region controls

* **Technical Details:**
  * Modified `DetermineFileBase()` to properly handle Xbox device paths
  * Updated `FCEU_SaveGameSave()` and `FCEU_LoadGameSave()` to refresh FileBase from GameInfo before saving/loading
  * Changed `FCEUI_LoadGameVirtual()` to use fullFilename instead of filename for FileBase extraction
  * Added automatic creation of `game:\sav\` directory in `Cemulator::LoadGame()`
  * Removed Xbox-specific code that was preventing saves from being written
  * Fixed save path resolution for both regular ROMs and ROMs in ZIP archives

* **Documentation:**
  * Complete API documentation added to `wiki/ROM-Info-Functions.md` for all new functions
  * Updated `wiki/Home.md` with links to all five new functions
  * All functions include detailed examples and use cases
  * Function signatures formatted for clickable wiki links

* **Testing:**
  * Created `test_getromhash.lua` - Tests all hash algorithms, case insensitivity, error handling
  * Created `test_getinesheader.lua` - Tests header parsing and displays all fields
  * Created `test_getregion.lua` - Tests region detection for different ROM types
  * Created `test_getrompath.lua` - Tests path extraction for various ROM locations
  * Created `test_getsavepath.lua` - Tests save path generation

* **Use Cases:**
  * **ROM Identification:** Use `getromhash()` to identify ROMs by CRC32 or MD5 hash
  * **ROM Analysis:** Use `getinesheader()` to analyze ROM structure, mapper, and features
  * **Region Detection:** Use `getregion()` for region-specific behavior or compatibility checks
  * **File Management:** Use `getrompath()` and `getsavepath()` for file operations relative to ROM location
  * **Save Management:** Use `getsavepath()` to backup, restore, or inspect save files

* **Includes Previous Features:**
  * All v0.8.4 features: Enhanced Input Recording Functions (markers, playback navigation, speed control, trimming)
  * All v0.8.3 features: Input Recording Save/Load API (saveinputrecording, loadinputrecording)
  * All v0.8.2 features: Enhanced Input API (button callbacks, hold timing, haptic feedback, input remapping)
  * All v0.8.1 features: Enhanced Drawing API (2D transforms, canvas rendering, gradients, advanced text styling, partial screenshot capture)
  * All v0.8.0 features: Complete File I/O API Suite (readfile, writefile, listfiles, etc.)
  * All v0.7.9 features: Complete Audio API Suite
  * All v0.7.8 features: Audio API Functions and Screenshot Performance Fix
  * All v0.7.7 features: State Management and Xbox 360 Input Lua API Functions
  * All prior features from v0.7.6–v0.6.1

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
* **RIGHT_THUMB CLICK** **Screenshot** — Press simultaneously to capture a screenshot. Saved to `game:\snaps\` using ROM filename (e.g., `SuperMario-0.png`). *Note: Screenshot combo takes precedence over rewind.*
* **START + BACK:** Open **OSD** (auto-pause).
* **OSD actions:** Save/Load State (with slots), Reset Game, GFX options (experimental). Exiting OSD resumes gameplay; "Load Game" returns to ROM browser.

---

## Lua Scripting API

FCE360 Enhanced includes full Lua 5.1 scripting support for custom overlays, automation, and game enhancements.

> **📚 Complete API Documentation:** All Lua API functions, callbacks, examples, and technical details are now available in the **[GitHub Wiki](https://github.com/frankischilling/fce360-enhanced/wiki)**.

### Quick Start

1. **Create the Lua directory** in your game folder (same location as `fceux.xex`)
2. **Place your scripts** in the `lua\` folder as `.lua` files
3. **Scripts auto-load** when a game starts - no manual loading required!

**Search Paths:**
- `hdd1:\fce360-enhanced\lua\` (recommended - user-writable)
- `game:\lua\` (game folder - may be read-only in packages)
- `usb0:\lua\` (USB storage)

### Quick Example

```lua
function script()
    -- Draw FPS counter
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    
    -- Draw a status message
    drawtext(4, 12, "Lua Active", 0x20)
end
```

### Documentation Links

**Getting Started:**
- **[Setup](https://github.com/frankischilling/fce360-enhanced/wiki/Setup)** - How to set up Lua scripting
- **[Technical Details](https://github.com/frankischilling/fce360-enhanced/wiki/Technical-Details)** - Lua version, update frequency, rendering details, script timing
- **[Troubleshooting](https://github.com/frankischilling/fce360-enhanced/wiki/Troubleshooting)** - Common issues and solutions

**API Reference:**
- **[Drawing Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Drawing-Functions)** - Text, shapes, images, and graphics primitives
- **[Memory Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Memory-Functions)** - Read, write, and scan memory
- **[Audio Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Audio-Functions)** - Audio analysis, filtering, and format conversion
- **[File I/O Functions](https://github.com/frankischilling/fce360-enhanced/wiki/File-IO-Functions)** - File and directory management
- **[Input Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Input-Functions)** - Controller input and manipulation
- **[Input Recording Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Input-Recording-Functions)** - Capture and replay controller input
- **[State Management Functions](https://github.com/frankischilling/fce360-enhanced/wiki/State-Management-Functions)** - Save and load game states
- **[Monitoring Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Monitoring-Functions)** - Performance and timing
- **[ROM Information Functions](https://github.com/frankischilling/fce360-enhanced/wiki/ROM-Info-Functions)** - Game and cartridge information
- **[Color Functions](https://github.com/frankischilling/fce360-enhanced/wiki/Color-Functions)** - Color manipulation and palette

**Callbacks and Examples:**
- **[Callbacks](https://github.com/frankischilling/fce360-enhanced/wiki/Callbacks)** - Required and optional callback functions
- **[Complete Examples](https://github.com/frankischilling/fce360-enhanced/wiki/Examples)** - Working example scripts
- **[Palette Reference](https://github.com/frankischilling/fce360-enhanced/wiki/Palette-Reference)** - Complete NES palette color reference

---

**Note:** The detailed function documentation has been moved to the [GitHub Wiki](https://github.com/frankischilling/fce360-enhanced/wiki) for better organization and easier maintenance. All functions, parameters, return values, notes, and examples are available there.

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
* **Game Genie codes:** Add a `[cheat]` section to `game:\fceui.ini` and set `enable=1`. With the toggle on, launching a ROM opens the Xbox keyboard before the game starts so you can enter one or more 6- or 8-character Game Genie codes (hyphens optional, case-insensitive). Separate multiple codes with spaces, commas, or by finishing a code and typing another hyphen (e.g. `YSAOPE-YEAOZA-YEAPYA`). Every valid code is decoded, applied immediately, and saved into the per-ROM `.cht` file alongside your normal cheats. Submit an empty entry to skip cheats for that launch.
