# Troubleshooting

Common issues and solutions for Lua scripting in FCE360 Enhanced.

## Script not loading

**Symptoms:** Script doesn't run, no output, no errors visible

**Solutions:**
- Verify the `lua\` folder exists in the same directory as `fceux.xex`
- Check file extension is `.lua` (not `.txt` or `.lua.txt`)
- Ensure script file is not empty
- Check debug output for load errors (if available)
- Verify script syntax is valid Lua 5.1
- Make sure the script defines a `script()` or `gui()` function

## Text not appearing

**Symptoms:** Script loads but nothing is drawn on screen

**Solutions:**
- Verify `script()` or `gui()` function is defined in your script
- Check coordinates are within bounds (0-255, 0-239)
- Try a simple test: `drawtext(4, 4, "TEST", 0x39)`
- Ensure script loaded successfully (check debug output)
- Verify color index is valid (0x00-0x3F)
- Check that you're calling drawing functions from within `script()` / `gui()`, not from `beforeframe()`

## Script errors

**Symptoms:** Script crashes, error messages in console

**Solutions:**
- Check debug output for Lua error messages
- Verify function names match exactly (`script`, `drawtext`, `getfps`, etc.)
- Test with a minimal script first
- Check for typos in function names or variable names
- Verify all required parameters are provided
- Check for nil values before using them

## Performance issues

**Symptoms:** Emulation slows down, frame rate drops

**Solutions:**
- Keep `script()` / `gui()` functions simple - avoid heavy calculations
- Don't call expensive string operations every frame
- Use local variables for frequently accessed values
- Cache expensive computations when possible
- Avoid reading large amounts of memory every frame
- Consider reducing update frequency for non-critical displays

## Input not working

**Symptoms:** `setjoypad()` or `clearjoypad()` not working

**Solutions:**
- **Must use `beforeframe()` callback** - calling these in `script()` / `gui()` will cause input to be applied one frame late
- Verify you're using the correct player number (0-3)
- Check button bitmask values are correct
- Use `gethardwarejoypad()` to verify hardware input is being read

## Memory reading issues

**Symptoms:** `readbyte()` returns wrong values or errors

**Solutions:**
- Verify address is in valid range (0x0000-0xFFFF)
- Check that a game is loaded
- Memory addresses vary by game - verify addresses for your specific ROM version
- Some games store values in non-obvious formats - check encoding
- Use `getmemorytype()` to verify memory region type
- Use `ismemorywritable()` to check if address is writable

## File I/O issues

**Symptoms:** `readfile()` or `writefile()` fails

**Solutions:**
- Check file path is correct (relative or absolute)
- Verify directory exists for write operations
- Check file permissions (some directories may be read-only)
- Use `fileexists()` to verify file exists before reading
- Check that path uses correct separators (backslashes on Xbox 360)
- Verify file is not too large for available memory

## Audio functions not working

**Symptoms:** Audio functions return nil or zero

**Solutions:**
- Check that audio is enabled using `getaudioenabled()`
- Verify channel number is in valid range (0-4)
- Check that a game is loaded and audio is playing
- Some audio functions require audio to be actively playing

## Color/palette issues

**Symptoms:** Colors don't appear as expected

**Solutions:**
- Verify color index is in valid range (0x00-0x3F)
- Some palette indices are near-black or transparent - avoid these for text
- Check [Palette Reference](Palette-Reference) for color values
- Colors may vary slightly depending on NTSC tint/hue settings

## Multiple scripts not working

**Symptoms:** Only one script runs, or scripts conflict

**Solutions:**
- Verify all scripts define `script()` or `gui()` function
- Check that scripts don't have conflicting global variables
- Scripts are loaded in alphabetical order by filename
- Each script maintains its own global state

## Callback not being called

**Symptoms:** Optional callback (`joypad()`, `onaudiochannelchange()`, `beforeframe()`) not executing

**Solutions:**
- Verify function name matches exactly (case-sensitive)
- Check that function is defined at global scope (not inside another function)
- For `joypad()`: Function is called every frame when input is read
- For `onaudiochannelchange()`: Only called when channel state actually changes
- For `beforeframe()`: Called before input polling each frame

## Still having issues?

- Check the [Examples](Examples) page for working code samples
- Review [Technical Details](Technical-Details) for implementation specifics
- Verify your script matches the examples in the documentation
- Test with a minimal script first, then add features incrementally

## See Also

- [Setup](Setup) - How to set up Lua scripting
- [Technical Details](Technical-Details) - Implementation specifics
- [Examples](Examples) - Working code samples

