# fce360-enhanced (FCEUX-360 UI tweaks)

Enhanced Xbox 360 port of the FCEUX NES emulator focused on front-end responsiveness. Core emulation code remains intact; improvements are limited to the Xbox UI layer (XUI scenes, input cadence, list scrolling).

* Toolchain: Visual Studio 2008 SP1
* SDK: Xbox 360 XDK 2.0.7645.1 (Nov 2008)
* Target: Xbox 360 (RGH/JTAG), retail-runnable `.xex`
* Current release: **v0.2** — *hold-to-accelerate scrolling + held paging*

## What’s new (v0.2)

* **Time-based acceleration on Right Stick (RS):** Start precise, then ramp speed the longer you hold in one direction. Deflection magnitude also scales step size. Hard caps prevent runaway scroll on huge libraries.
* **Held paging (LB/RB):** Hold a shoulder button to page up/down at a steady cadence.
* **Sane UX guards:** Minimum dwell to prevent double-steps, direction/neutral resets, and a deadzone so tiny bumps don’t spam moves.
* **Precision preserved:** D-pad / Left Stick keep XUI’s native single-step behavior for fine selection.
* No changes to the emulator core (APU/PPU/CPU/mappers).

## Repository layout (excerpt)

* `fceux/` – Visual Studio 2008 solution and Xbox 360 project.
* `fceux/xbox/` – Xbox front-end (UI, input, filesystem glue).
* `fceux/xbox/ui/mainui.cpp` – XUI scenes (ROM browser, OSD). **Scrolling changes live here.**
* `fceux/media/` – Static assets (XUI skin `ui.xzp`, font `xarialuni.ttf`, textures).
* Core emulation lives under `fceux/fceux/` and is intentionally untouched.

## Build

1. Open `fceux\fceux.sln` in Visual Studio 2008 SP1.
2. Select Configuration: `Release_LTCG` and Platform: `Xbox 360`.
3. Build the `fceux` project.

Notes

* The post-build step may fail with:

  * `xbecopy: error X1001: Could not connect to Xbox 360 ''`
  * This is expected if Neighborhood isn’t configured. The `.xex` still builds fine.
  * To silence it entirely, clear **Project Properties → Build Events → Post-Build**.
* Typical warnings for this era/toolchain are harmless here:

  * `/GR-` RTTI disabled warnings where `dynamic_cast` appears in core I/O types.
  * `FASTCALL` macro redefinition between XDK headers and `fceux/fceux/ppu.h`.

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
* Launch `fceux.xex` from Aurora/FSD/XEXMenu. The first time you change a setting, `fceui.ini` will be written next to the `.xex`.

## Controls (front-end)

* **Right Stick (hold up/down):** *Time-based acceleration* of selection.
* **LB / RB (hold):** Page up / page down at a steady cadence.
* **D-pad / Left Stick:** Single-step precision (native XUI behavior).
* **A:** Load game & start emulation.
* **B:** Back.
* **OSD:** Save/Load State and Reset; leaving OSD resumes gameplay. “Load Game” returns to the ROM browser.

## Advanced tuning (optional)

All tunables live in the ROM list scene (`LoadGame` in `fceux/xbox/ui/mainui.cpp`):

* **Deadzone:** ignore tiny RS deflections
  `const float RS_DEADZONE = 0.30f;`
* **Held paging cadence (LB/RB):**
  `const DWORD pageRepeatMs = ~100;`
* **General dwell/response:**

  ```
  m_initialDelayMs   = 180;   // initial repeat delay
  m_repeatIntervalMs = 70;    // baseline cadence (non-RS path)
  m_minDwellMs       = 50;    // minimum time between injected moves
  ```
* **Acceleration tiers (RS hold time → interval/steps cap):**
  Ramps from ~160 ms (1 step) down to ~35 ms (3 steps) after ~2.6s hold, with deflection-based scaling (light touch stays precise).

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

## Troubleshooting

* **“Holding longer doesn’t speed up”**: Acceleration is on the **Right Stick**. D-pad/Left Stick remain single-step by design. Ensure your controller’s RS is calibrated and not stuck near the deadzone (~0.30).
* **Black UI or missing text**: Verify `media\ui.xzp` and `media\xarialuni.ttf` are present next to `fceux.xex` under `media\`.
* **Empty ROM list**: Place `.nes`/`.zip` files under `roms\`.
* **Post-build copy error**: Expected without Neighborhood; deploy via FTP manually.

## Changelog

* **v0.2**

  * feat: Time-based RS acceleration with deflection scaling + hard caps.
  * feat: Continuous paging while holding LB/RB.
  * ux: Debounce/min-dwell & direction resets to eliminate double-moves.
  * refactor: Movement injection isolated in `UpdatePerFrame()`; no heap churn on hot path.
* **v0.1**

  * Initial enhanced UI pass; core emulation untouched.

---
