fce360-enhanced (FCEUX-360 UI tweaks)
====================================

Enhanced Xbox 360 port of the FCEUX NES emulator focused on front-end responsiveness. Core emulation code remains intact; improvements are limited to the Xbox UI layer (XUI scenes, input cadence, list scrolling).

- Toolchain: Visual Studio 2008 SP1
- SDK: Xbox 360 XDK 2.0.7645.1 (Nov 2008)
- Target: Xbox 360 (RGH/JTAG), retail-runnable .xex

What's new
----------
- Faster ROM browser scrolling:
  - Reduced initial repeat delay and repeat interval for D-pad/analog.
  - Deflection-based acceleration (larger step sizes the longer/stronger you hold).
  - Viewport follows selection; the highlighted row stays visible while you scroll.
- No changes to the emulator core (APU/PPU/CPU/mappers).

Repository layout (excerpt)
---------------------------
- `fceux/` – Visual Studio 2008 solution and Xbox 360 project.
- `fceux/xbox/` – Xbox front-end (UI, input, filesystem glue).
- `fceux/xbox/ui/mainui.cpp` – XUI scenes (ROM browser, OSD). Scrolling changes live here.
- `fceux/media/` – Static assets (XUI skin `ui.xzp`, font `xarialuni.ttf`, textures).
- Core emulation lives under `fceux/fceux/` and is intentionally untouched.

Build
-----
1) Open `fceux\fceux.sln` in Visual Studio 2008 SP1.
2) Select Configuration: `Release_LTCG` and Platform: `Xbox 360`.
3) Build the `fceux` project.

Notes
- The post-build step may fail with:
  - `xbecopy: error X1001: Could not connect to Xbox 360 ''`
  - This is expected if Neighborhood isn’t configured. The .xex still builds fine.
  - To suppress the error entirely, clear Project Properties → Build Events → Post-Build.
- Typical warnings for this era/toolchain are harmless here:
  - `/GR-` RTTI disabled warns where `dynamic_cast` appears in core I/O types.
  - `FASTCALL` macro redefinition between XDK headers and `fceux/fceux/ppu.h`.

Deploy to Xbox 360 (RGH/JTAG)
-----------------------------
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
- Copy `fceux\Release_LTCG\fceux.xex` to the folder above.
- Copy all contents of `fceux\media\` into `media\` next to the .xex (must include `ui.xzp` and `xarialuni.ttf`).
- Launch `fceux.xex` from Aurora/FSD/XEXMenu. The first time you change a setting, `fceui.ini` will be written next to the .xex.

Packaging builds for GitHub Releases
------------------------------------
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

Controls (front-end)
--------------------
- ROM list: D-pad or left stick to scroll; hold to accelerate.
- A: select; B: back.
- In-game OSD: Save/Load State and Reset; leaving OSD resumes gameplay. "Load Game" returns to the ROM browser.

Development notes
-----------------
- Scrolling changes are constrained to `fceux/xbox/ui/mainui.cpp`.
- No exceptions/RTTI are introduced; compatible with VS2008 + XDK 7645.
- Minimal heap churn; input and list updates run once per frame.

Troubleshooting
---------------
- Black UI or missing text: verify `media\ui.xzp` and `media\xarialuni.ttf` exist next to `fceux.xex` under `media\`.
- Empty ROM list: place `.nes`/`.zip` files under `roms\`.
- Post-build copy error: expected without Neighborhood; deploy via FTP manually.


