-- Test script for getscreensize()
local size = getscreensize()
print(string.format("Screen: %d x %d", size.width, size.height))

function gui()
    drawtext(4, 4, "OK", 0x2E)
end

