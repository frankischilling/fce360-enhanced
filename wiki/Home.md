# FCE360 Enhanced - Lua Scripting API

Welcome to the FCE360 Enhanced Lua Scripting API documentation! This wiki provides comprehensive documentation for all Lua API functions available in FCE360 Enhanced.

FCE360 Enhanced includes full Lua 5.1 scripting support for custom overlays, automation, and game enhancements.

## Quick Start

1. **Create the Lua directory** in your game folder (same location as `fceux.xex`)
2. **Place your scripts** in the `lua\` folder as `.lua` files
3. **Scripts auto-load** when a game starts - no manual loading required!

See [Setup](Setup) for detailed instructions.

## Documentation Pages

### Getting Started
- **[Setup](Setup)** - How to set up Lua scripting
- **[Technical Details](Technical-Details)** - Lua version, update frequency, rendering details, script timing ([`setscriptinterval()`](Technical-Details#setscriptinterval), [`getscriptinterval()`](Technical-Details#getscriptinterval)), console spacing ([`setconsolespacing()`](Utility-Functions#setconsolespacing))
- **[Troubleshooting](Troubleshooting)** - Common issues and solutions

### API Reference

#### Drawing Functions
- **[Drawing Functions](Drawing-Functions)** - Text, shapes, images, and graphics primitives
  - **Text rendering:** [`drawtext()`](Drawing-Functions#drawtext), [`textstyle()`](Drawing-Functions#textstyle), [`drawtextwh()`](Drawing-Functions#drawtextwh), [`drawtextscaled()`](Drawing-Functions#drawtextscaled), [`drawtextrotated()`](Drawing-Functions#drawtextrotated), [`gettextwidth()`](Drawing-Functions#gettextwidth), [`gettextheight()`](Drawing-Functions#gettextheight), [`measuretextblock()`](Drawing-Functions#measuretextblock), [`drawtextbox()`](Drawing-Functions#drawtextbox)
  - **Basic drawing:** [`drawpixel()`](Drawing-Functions#drawpixel), [`drawline()`](Drawing-Functions#drawline), [`drawthickline()`](Drawing-Functions#drawthickline)
  - **Rectangles:** [`drawrect()`](Drawing-Functions#drawrect), [`fillrect()`](Drawing-Functions#fillrect), [`clearrect()`](Drawing-Functions#clearrect)
  - **Screen operations:** [`clearscreen()`](Drawing-Functions#clearscreen), [`fillscreen()`](Drawing-Functions#fillscreen), [`screenshot()`](Drawing-Functions#screenshot), [`screenshotregion()`](Drawing-Functions#screenshotregion)
  - **Images:** [`drawimage()`](Drawing-Functions#drawimage), [`drawimageindexed()`](Drawing-Functions#drawimageindexed), [`drawimageex()`](Drawing-Functions#drawimageex), [`drawtile()`](Drawing-Functions#drawtile), [`drawchrtile()`](Drawing-Functions#drawchrtile)
  - **Drawing state:** [`setdrawmode()`](Drawing-Functions#setdrawmode), [`setclipregion()`](Drawing-Functions#setclipregion), [`clearclipregion()`](Drawing-Functions#clearclipregion), [`setdrawcolor()`](Drawing-Functions#setdrawcolor), [`pushdrawstate()`](Drawing-Functions#pushdrawstate), [`popdrawstate()`](Drawing-Functions#popdrawstate), [`settransform()`](Drawing-Functions#settransform), [`resettransform()`](Drawing-Functions#resettransform), [`beginbatch()`](Drawing-Functions#beginbatch), [`endbatch()`](Drawing-Functions#endbatch), [`setimagescale()`](Drawing-Functions#setimagescale), [`getimagescale()`](Drawing-Functions#getimagescale)
  - **Canvas operations:** [`createcanvas()`](Drawing-Functions#createcanvas), [`setrendertarget()`](Drawing-Functions#setrendertarget), [`blit()`](Drawing-Functions#blit)
  - **Gradients:** [`lineargradient()`](Drawing-Functions#lineargradient), [`radialgradient()`](Drawing-Functions#radialgradient), [`fillrectgradient()`](Drawing-Functions#fillrectgradient)
  - **Shapes:** [`drawcircle()`](Drawing-Functions#drawcircle), [`fillcircle()`](Drawing-Functions#fillcircle), [`drawellipse()`](Drawing-Functions#drawellipse), [`fillellipse()`](Drawing-Functions#fillellipse), [`drawarc()`](Drawing-Functions#drawarc), [`fillarc()`](Drawing-Functions#fillarc), [`drawroundrect()`](Drawing-Functions#drawroundrect), [`fillroundrect()`](Drawing-Functions#fillroundrect), [`drawtriangle()`](Drawing-Functions#drawtriangle), [`filltriangle()`](Drawing-Functions#filltriangle), [`drawpolygon()`](Drawing-Functions#drawpolygon), [`drawpolyline()`](Drawing-Functions#drawpolyline), [`fillpolygon()`](Drawing-Functions#fillpolygon)

#### Memory Functions
- **[Memory Reading Functions](Memory-Functions#memory-reading-functions)** - Read and scan memory
  - **Reading:** [`readbyte()`](Memory-Functions#readbyte), [`readword()`](Memory-Functions#readword), [`readbytes()`](Memory-Functions#readbytes), [`readram()`](Memory-Functions#readram)
  - **Memory info:** [`getmemorytype()`](Memory-Functions#getmemorytype), [`ismemorywritable()`](Memory-Functions#ismemorywritable)
  - **Scanning:** [`scanbyte()`](Memory-Functions#scanbyte), [`scanword()`](Memory-Functions#scanword), [`scanbytes()`](Memory-Functions#scanbytes), [`findpattern()`](Memory-Functions#findpattern), [`scanchanged()`](Memory-Functions#scanchanged)
  - **Watchpoints:** [`watchbyte()`](Memory-Functions#watchbyte), [`unwatchbyte()`](Memory-Functions#unwatchbyte), [`getmemorysnapshot()`](Memory-Functions#getmemorysnapshot)
- **[Memory Writing Functions](Memory-Functions#memory-writing-functions)** - Modify memory
  - **Writing:** [`writebyte()`](Memory-Functions#writebyte), [`writeword()`](Memory-Functions#writeword), [`writebytes()`](Memory-Functions#writebytes), [`writeprg()`](Memory-Functions#writeprg)
  - **Bulk operations:** [`fillbytes()`](Memory-Functions#fillbytes), [`copybytes()`](Memory-Functions#copybytes), [`comparebytes()`](Memory-Functions#comparebytes)
  - **Backup/restore:** [`backupbytes()`](Memory-Functions#backupbytes), [`restorebytes()`](Memory-Functions#restorebytes)
  - **Bit operations:** [`setbit()`](Memory-Functions#setbit), [`clearbit()`](Memory-Functions#clearbit), [`togglebit()`](Memory-Functions#togglebit), [`testbit()`](Memory-Functions#testbit)

#### Audio Functions
- **[Audio Functions](Audio-Functions)** - Audio analysis and processing
  - **Status:** [`getaudioenabled()`](Audio-Functions#getaudioenabled)
  - **Mixed audio:** [`getaudiosample()`](Audio-Functions#getaudiosample), [`getaudiobuffer()`](Audio-Functions#getaudiobuffer), [`getaudiosampleleft()`](Audio-Functions#getaudiosampleleft), [`getaudiosampleright()`](Audio-Functions#getaudiosampleright), [`getaudiofft()`](Audio-Functions#getaudiofft)
  - **Channel-specific:** [`getaudiochannel()`](Audio-Functions#getaudiochannel), [`getaudiochannelsample()`](Audio-Functions#getaudiochannelsample), [`getaudiochannelfft()`](Audio-Functions#getaudiochannelfft)
  - **Filtering:** [`setaudiofilter()`](Audio-Functions#setaudiofilter), [`getaudiofilter()`](Audio-Functions#getaudiofilter), [`getaudiofiltered()`](Audio-Functions#getaudiofiltered)
  - **Format conversion:** [`audiosampletofloat()`](Audio-Functions#audiosampletofloat), [`floattosample()`](Audio-Functions#floattosample), [`audiosampletouint8()`](Audio-Functions#audiosampletouint8), [`uint8tosample()`](Audio-Functions#uint8tosample), [`normalizeaudiosample()`](Audio-Functions#normalizeaudiosample), [`monotostereo()`](Audio-Functions#monotostereo), [`stereotomono()`](Audio-Functions#stereotomono)

#### File I/O Functions
- **[File I/O Functions](File-IO-Functions)** - File and directory management
  - **Reading:** [`readfile()`](File-IO-Functions#readfile)
  - **Writing:** [`writefile()`](File-IO-Functions#writefile)
  - **File info:** [`fileexists()`](File-IO-Functions#fileexists), [`listfiles()`](File-IO-Functions#listfiles), [`listdir()`](File-IO-Functions#listdir)
  - **Directory management:** [`mkdir()`](File-IO-Functions#mkdir), [`rmdir()`](File-IO-Functions#rmdir), [`rmfile()`](File-IO-Functions#rmfile)

#### Input Functions
- **[Input Functions](Input-Functions)** - Controller input and manipulation
  - **Reading:** [`getjoypad()`](Input-Functions#getjoypad), [`isbuttonpressed()`](Input-Functions#isbuttonpressed), [`isxboxbuttonpressed()`](Input-Functions#isxboxbuttonpressed), [`gethardwarejoypad()`](Input-Functions#gethardwarejoypad), [`getbuttonheldms()`](Input-Functions#getbuttonheldms)
  - **Writing:** [`setjoypad()`](Input-Functions#setjoypad), [`clearjoypad()`](Input-Functions#clearjoypad), [`pressbutton()`](Input-Functions#pressbutton), [`releasebutton()`](Input-Functions#releasebutton)
  - **Remapping:** [`mapinput()`](Input-Functions#mapinput) - Per-script input remapping for custom control schemes
  - **Feedback:** [`setrumble()`](Input-Functions#setrumble) - Controller haptic feedback
  - **Callbacks:** [`onbuttonpress()`](Input-Functions#onbuttonpress), [`onbuttonrelease()`](Input-Functions#onbuttonrelease)
  - **Utilities:** [`getbuttonname()`](Input-Functions#getbuttonname), [`getbuttonmask()`](Input-Functions#getbuttonmask)
- **[Input Recording Functions](Input-Recording-Functions)** - Capture and replay controller input
  - [`startinputrecording()`](Input-Recording-Functions#startinputrecording), [`stopinputrecording()`](Input-Recording-Functions#stopinputrecording), [`playinputrecording(data)`](Input-Recording-Functions#playinputrecordingdata), [`saveinputrecording(path)`](Input-Recording-Functions#saveinputrecordingpath), [`loadinputrecording(path)`](Input-Recording-Functions#loadinputrecordingpath), [`setrecordingmarker(name)`](Input-Recording-Functions#setrecordingmarkername), [`jumptorecordingmarker(name)`](Input-Recording-Functions#jumptorecordingmarkername), [`setplaybackspeed(mult)`](Input-Recording-Functions#setplaybackspeedmult), [`trimrecording(startFrame, endFrame)`](Input-Recording-Functions#trimrecordingstartframe-endframe)
  - Recording utilities integrate with [`gethardwarejoypad()`](Input-Functions#gethardwarejoypad) for toggles and [`setjoypad()`](Input-Functions#setjoypad) for scripted overrides

#### State Management Functions
- **[State Management Functions](State-Management-Functions)** - Save and load game states
  - **Slot-based:** [`savestate()`](State-Management-Functions#savestate), [`loadstate()`](State-Management-Functions#loadstate), [`hasstate()`](State-Management-Functions#hasstate)
  - **File-based:** [`savestatefile()`](State-Management-Functions#savestatefile), [`loadstatefile()`](State-Management-Functions#loadstatefile)

#### Monitoring Functions
- **[Monitoring Functions](Monitoring-Functions)** - Performance, timing, frame/jitter metrics, Lua memory inspection, garbage collection, and profiling helpers.
  - **FPS:** [`getfps()`](Monitoring-Functions#getfps)
  - **Frame info:** [`getframecount()`](Monitoring-Functions#getframecount), [`getelapsedframes()`](Monitoring-Functions#getelapsedframes), [`getframecycles()`](Monitoring-Functions#getframecycles), [`getppucycles()`](Monitoring-Functions#getppucycles), [`getapucycles()`](Monitoring-Functions#getapucycles), [`getelapsedtime()`](Monitoring-Functions#getelapsedtime)
  - **Frame timing:** [`getframetime_ms()`](Monitoring-Functions#getframetime_ms), [`getjitter_ms()`](Monitoring-Functions#getjitter_ms)
  - **Lua memory & GC:** [`getluamem()`](Monitoring-Functions#getluamem), [`collectgarbage_now()`](Monitoring-Functions#collectgarbage_now)
  - **Profiling:** [`beginprofile()`](Monitoring-Functions#beginprofiletag), [`endprofile()`](Monitoring-Functions#endprofiletag)
  - **Timing helpers:** [`gettime()`](Monitoring-Functions#gettime), [`gettimedelta()`](Monitoring-Functions#gettimedelta), [`sleepframes()`](Monitoring-Functions#sleepframes)
  - **Screen info:** [`getscreenwidth()`](Monitoring-Functions#getscreenwidth), [`getscreenheight()`](Monitoring-Functions#getscreenheight), [`getscreensize()`](Monitoring-Functions#getscreensize)

#### ROM Information Functions
- **[ROM Information Functions](ROM-Info-Functions)** - Game and cartridge information
  - **ROM info:** [`getromname()`](ROM-Info-Functions#getromname), [`getrompath()`](ROM-Info-Functions#getrompath), [`getromhash(algorithm)`](ROM-Info-Functions#getromhashalgorithm), [`getinesheader()`](ROM-Info-Functions#getinesheader), [`getregion()`](ROM-Info-Functions#getregion), [`getromsize()`](ROM-Info-Functions#getromsize), [`getprgsize()`](ROM-Info-Functions#getprgsize), [`getchrsize()`](ROM-Info-Functions#getchrsize)
  - **Mapper:** [`getmapper()`](ROM-Info-Functions#getmapper), [`getmapperstring()`](ROM-Info-Functions#getmapperstring)
  - **Features:** [`hasbattery()`](ROM-Info-Functions#hasbattery)
  - **Saves:** [`getsavepath()`](ROM-Info-Functions#getsavepath) ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â returns `<rom>.sav` (or legacy `game.sav` if thatÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢s the only file), empty string if no save exists
  - **Emulation state:** [`isframeadvancing()`](ROM-Info-Functions#isframeadvancing), [`isrewinding()`](ROM-Info-Functions#isrewinding), [`isfastforwarding()`](ROM-Info-Functions#isfastforwarding)
  - **Game Genie:** [`getgamegeniecode()`](ROM-Info-Functions#getgamegeniecode), [`decodegamegenie()`](ROM-Info-Functions#decodegamegenie)

#### Color and Palette Functions
- **[Color Functions](Color-Functions)** - Color manipulation and palette
  - **Palette:** [`getpalettecolor()`](Color-Functions#getpalettecolor), [`setpalettecolor()`](Color-Functions#setpalettecolor), [`getnescolor()`](Color-Functions#getnescolor)
  - **Color utilities:** [`getcolorrgb()`](Color-Functions#getcolorrgb), [`blendcolors()`](Color-Functions#blendcolors)

#### Utility Functions
- **[Utility Functions](Utility-Functions)** - Debugging and console output
  - **Console output:** [`print(...)`](Utility-Functions#print), [`log(...)`](Utility-Functions#log)
  - **Console settings:** [`setconsolespacing(pixels)`](Utility-Functions#setconsolespacingpixels)

### Callbacks and Scripting
- **[Callbacks](Callbacks)** - Required and optional callback functions
  - [`script()`](Callbacks#script) - Required main callback (formerly `gui()`)
  - [`joypad()`](Callbacks#joypad) - Optional input modification callback
  - [`onaudiochannelchange()`](Callbacks#onaudiochannelchange) - Optional audio event callback
  - [`beforeframe()`](Callbacks#beforeframe) - Optional pre-frame callback

### Examples and Reference
- **[Complete Examples](Examples)** - Working example scripts
- **[Palette Reference](Palette-Reference)** - Complete NES palette color reference
- **[Script Loading Behavior](Technical-Details#script-loading-behavior)** - How scripts are loaded

## Search Paths

Scripts are automatically searched in these locations (in order):
- `hdd1:\fce360-enhanced\lua\` (recommended - user-writable)
- `game:\lua\` (game folder - may be read-only in packages)
- `usb0:\lua\` (USB storage)

## Quick Example

```lua
function script()
    -- Draw FPS counter
    local fps = getfps()
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    
    -- Draw a status message
    drawtext(4, 12, "Lua Active", 0x20)
end
```

## Need Help?

- Check the [Troubleshooting](Troubleshooting) page for common issues
- Review [Complete Examples](Examples) for working code samples
- See [Technical Details](Technical-Details) for implementation specifics
