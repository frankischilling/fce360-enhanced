-- Test script for getsavepath()
-- Displays the battery save path and logs it when it changes

local lastPath = ""

function gui()
    local romName = getromname()
    local savePath = getsavepath()
    local y = 4

    drawtext(4, y, "=== getsavepath() Test ===", 0x3F)
    y = y + 10

    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        y = y + 10
        drawtext(4, y, "Save path: (none)", 0x2A)
        return
    end

    drawtext(4, y, "ROM: " .. romName, 0x2E)
    y = y + 10
    drawtext(4, y, "Save path:", 0x20)
    y = y + 10
    if savePath == "" then
        drawtext(4, y, "(no save file yet)", 0x2A)
    else
        drawtext(4, y, savePath, 0x29)
    end

    if savePath ~= lastPath then
        if savePath == "" then
            print("Battery save path: (none yet)")
        else
            print("Battery save path: " .. savePath)
        end
        lastPath = savePath
    end
end
