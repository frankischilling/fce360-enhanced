# Screenshots

Click the right stick during gameplay to request a PNG screenshot. Files go to
`game:\snaps` and use the ROM name followed by a number.

From version 0.8.10, controller captures copy the current visible frame and its
palette before returning to emulation. A worker handles filename search,
compression, and disk writes. One capture can be pending; another request while
the worker is busy is ignored. Let the current write finish before taking
another screenshot. Closing the emulator waits for the pending capture.

Numbered captures reserve a new filename and preserve existing screenshots.
The old warmup no longer writes or deletes screenshot number 99999. If storage
fails, the worker reports the error through debug output and removes only the
file it created for that attempt.

Lua [`screenshot()`](Drawing-Functions#screenshot) and
[`screenshotregion()`](Drawing-Functions#screenshotregion) remain synchronous.
They return after writing the file. A numbered Lua capture shares the filename
allocator and can wait for a controller capture already being written. Custom
filenames keep their existing behavior.

If a capture does not appear, check that the `snaps` directory is writable and
that the writer is not busy. Use the XEX and media folder from the same release.

Component tests cover blocked writes, frame ownership, busy handling, numbering,
I/O failures, synchronous captures, and shutdown. PNG checks verify pixels,
palettes, dimensions, and CRCs. Xbox 360 timing and gameplay still need hardware
testing; the source change alone does not establish a measured FPS result.
