# Complete Examples

This page contains complete, working example scripts that demonstrate various Lua API features.

## FPS Display

Simple FPS counter overlay:

```lua
function script()
    local fps = getfps()
    drawtext(2, 2, string.format("FPS: %.1f", fps), 0x39)
end
```

## On-Screen Timer

Timer that tracks elapsed time:

```lua
local startTime = 0
local running = false

function script()
    if running then
        local elapsed = getfps() * (os.clock() - startTime)  -- Approximate
        local minutes = math.floor(elapsed / 3600)
        local seconds = math.floor((elapsed % 3600) / 60)
        drawtext(4, 4, string.format("Time: %02d:%02d", minutes, seconds), 0x39)
    end
end
```

## Multi-Line Status Display

Display multiple lines of information:

```lua
function script()
    local fps = getfps()
    local lineHeight = 10
    
    drawtext(4, 4, string.format("FPS: %.1f", fps), 0x39)
    drawtext(4, 4 + lineHeight, "Status: Running", 0x20)
    drawtext(4, 4 + lineHeight * 2, "Press LT to rewind", 0x0F)
end
```

## Game State HUD (Super Mario Bros 1)

Display game information from memory:

```lua
function script()
    -- Super Mario Bros 1 memory addresses
    local livesRaw = readbyte(0x075A)
    local lives = livesRaw + 1  -- Convert to displayed value
    local coins = readbyte(0x075E)
    local worldLevel = readbyte(0x075F)
    
    -- Decode world/level (bits 4-7 = world, bits 0-3 = level)
    local world = (worldLevel >> 4) + 1
    local level = (worldLevel & 0x0F) + 1
    
    -- Display game info
    drawtext(4, 4, string.format("Lives: %d", lives), 0x20)
    drawtext(4, 12, string.format("Coins: %d", coins), 0x37)
    drawtext(4, 20, string.format("World %d-%d", world, level), 0x39)
    
    -- Read score (multi-byte value)
    local scoreHigh = readbyte(0x07DE)  -- Tens of thousands
    local scoreMid = readbyte(0x07DD)   -- Hundreds
    local scoreLow = readbyte(0x07DC)   -- Ones/tens
    local score = scoreHigh * 10000 + scoreMid * 100 + scoreLow
    drawtext(4, 28, string.format("Score: %05d", score), 0x20)
end
```

## Auto-Fire Script

Automatic button pressing:

```lua
local autoFireFrame = 0

function joypad(player, buttons)
    -- Auto-fire for player 1 only
    if player == 0 then
        autoFireFrame = autoFireFrame + 1
        -- Press A button every other frame (30 Hz auto-fire)
        if (autoFireFrame % 2) == 0 then
            return buttons | 0x01  -- Set A button (bit 0)
        end
    end
    return buttons  -- Pass through unmodified
end

function script()
    drawtext(4, 4, "Auto-Fire Active", 0x29)
end
```

## Progress Bar

Visual progress indicator:

```lua
function script()
    local progress = 0.75  -- 75% progress
    local barWidth = 100
    local barHeight = 8
    local barX = 10
    local barY = 100
    
    -- Draw background
    fillrect(barX, barY, barWidth, barHeight, 0x10)
    drawrect(barX, barY, barWidth, barHeight, 0x20)
    
    -- Draw progress
    local filledWidth = math.floor(barWidth * progress)
    fillrect(barX, barY, filledWidth, barHeight, 0x39)
    
    -- Draw percentage text
    drawtext(barX + barWidth + 5, barY, string.format("%d%%", math.floor(progress * 100)), 0x20)
end
```

## Audio Channel Monitor

Monitor which audio channels are active:

```lua
local activeChannels = {false, false, false, false, false}
local channelNames = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"}

function onaudiochannelchange(channel, enabled)
    activeChannels[channel + 1] = enabled
end

function script()
    if not getaudioenabled() then
        return
    end
    
    local y = 4
    drawtext(4, y, "Active Channels:", 0x29)
    y = y + 12
    
    for i = 0, 4 do
        local active = activeChannels[i + 1]
        local color = active and 0x27 or 0x10
        drawtext(4, y, string.format("%s: %s", 
            channelNames[i + 1], active and "ACTIVE" or "INACTIVE"), color)
        y = y + 10
    end
end
```

## File I/O Example

Read and write configuration files:

```lua
local configFile = "config.txt"
local config = {}

-- Load configuration on startup
local configData = readfile(configFile)
if configData then
    -- Simple config parsing (key=value format)
    for line in configData:gmatch("[^\r\n]+") do
        local key, value = line:match("([^=]+)=(.+)")
        if key and value then
            config[key:match("^%s*(.-)%s*$")] = value:match("^%s*(.-)%s*$")
        end
    end
    print("Configuration loaded")
else
    -- Default configuration
    config.color = "0x39"
    config.position = "top-left"
    print("Using default configuration")
end

function script()
    local color = tonumber(config.color) or 0x39
    drawtext(4, 4, "Config loaded!", color)
end
```

## TAS Script Example

Frame-accurate input control:

```lua
local frameCount = 0
local autoMode = false
local lastSelectState = false

function beforeframe()
    -- Detect real hardware input for toggle
    local hw = gethardwarejoypad(0)
    local selectMask = getbuttonmask("SELECT")
    local selectPressed = (math.floor(hw / selectMask) % 2 == 1)
    
    -- Toggle on rising edge
    if selectPressed and not lastSelectState then
        autoMode = not autoMode
    end
    lastSelectState = selectPressed
    
    if autoMode then
        frameCount = frameCount + 1
        local buttons = getbuttonmask("RIGHT") + getbuttonmask("B")
        if frameCount % 60 == 0 then
            buttons = buttons + getbuttonmask("A")
        end
        setjoypad(0, buttons)
    else
        clearjoypad(0)
    end
end

function script()
    drawtext(4, 4, autoMode and "AUTO MODE: ON" or "AUTO MODE: OFF", 0x29)
    if autoMode then
        drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    end
end
```

## See Also

- [Home](Home) - Overview of all API functions
- [Setup](Setup) - How to set up Lua scripting
- [Callbacks](Callbacks) - Callback function reference

