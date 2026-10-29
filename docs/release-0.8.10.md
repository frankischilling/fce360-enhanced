# Version 0.8.10

This release addresses [issue #8](https://github.com/frankischilling/fce360-enhanced/issues/8),
the hitch when taking the first controller screenshot.

Controller captures now copy the visible indexed frame, palette, and filename
into a single pending job. A worker started before gameplay handles filename
search, PNG compression, and disk writes at a lower thread priority. A second
capture request is ignored while the writer is busy. Leaving the emulator
drains any pending capture before the worker closes.

The old warmup no longer writes or deletes a file at screenshot number 99999.
Numbered captures reserve a new file before writing and keep a separate counter
for each ROM path. A failed write removes only the file that capture created.

Lua `screenshot()` and `screenshotregion()` calls remain synchronous. Numbered
Lua captures share the controller writer's filename allocation so they do not
choose the same slot. Explicit filenames retain their existing behavior.

The package contains a newly compiled `fceux.xex`, rebuilt UI archive, and the
required media files. The font comes from the published 0.8.5 media package.
Copy the XEX and media folder together. Keep your existing configuration,
scripts, ROMs, BIOS files, screenshots, saves, and states.

Native component tests exercise the production worker with SDK substitutes,
including blocked writes, capture ownership, busy handling, numbering, I/O
failures, synchronous sharing, and shutdown. Separate PNG checks verify lossless
pixels, palettes, dimensions, and chunk CRCs. CI runs these tests with address
and undefined behavior sanitizers.

The Xbox 360 build uses Visual Studio 2008 SP1 and XDK 2.0.9328.0. A clean
Release_LTCG build completed with zero errors and seven compiler warnings.
Xbox 360 timing and gameplay testing remain outstanding.
