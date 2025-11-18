-- Test script for getrompath()
-- Displays full ROM path and logs changes

local lastPath = ""

function gui()
    local romName = getromname()
    local romPath = getrompath()
    local y = 4

    drawtext(4, y, "=== getrompath() Test ===", 0x3F)
    y = y + 10

    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Path: (none)", 0x2A)
        return
    end

    drawtext(4, y, "ROM: " .. romName, 0x2E)
    y = y + 10
    drawtext(4, y, "Path: " .. romPath, 0x20)

    if romPath ~= lastPath then
        print("ROM path: " .. romPath)
        lastPath = romPath
    end
end

