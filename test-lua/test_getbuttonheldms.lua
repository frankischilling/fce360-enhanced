-- Test script for getbuttonheldms(btn)
-- Displays how long buttons are held and prints thresholds when exceeded

local trackedButtons = {
    { name = "A", label = "A Button" },
    { name = "B", label = "B Button" },
    { name = "X", label = "X Button" },
    { name = "Y", label = "Y Button" },
    { name = "DPAD_UP", label = "D-Pad Up" },
    { name = "DPAD_DOWN", label = "D-Pad Down" },
    { name = "DPAD_LEFT", label = "D-Pad Left" },
    { name = "DPAD_RIGHT", label = "D-Pad Right" },
    { name = "START", label = "Start" },
    { name = "BACK", label = "Back" },
    { name = "LEFT_SHOULDER", label = "Left Shoulder" },
    { name = "RIGHT_SHOULDER", label = "Right Shoulder" },
    { name = "LEFT_THUMB", label = "Left Stick Click" },
    { name = "RIGHT_THUMB", label = "Right Stick Click" },
    { name = "LS_UP", label = "Left Stick Up" },
    { name = "LS_DOWN", label = "Left Stick Down" },
    { name = "LS_LEFT", label = "Left Stick Left" },
    { name = "LS_RIGHT", label = "Left Stick Right" },
    { name = "RS_UP", label = "Right Stick Up" },
    { name = "RS_DOWN", label = "Right Stick Down" },
    { name = "RS_LEFT", label = "Right Stick Left" },
    { name = "RS_RIGHT", label = "Right Stick Right" },
    { name = "LT", label = "Left Trigger" },
    { name = "RT", label = "Right Trigger" },
    { name = "NES_A", label = "NES A Button" },
    { name = "NES_B", label = "NES B Button" },
    { name = "NES_SELECT", label = "NES Select" },
    { name = "NES_START", label = "NES Start" },
    { name = "NES_UP", label = "NES Up" },
    { name = "NES_DOWN", label = "NES Down" },
    { name = "NES_LEFT", label = "NES Left" },
    { name = "NES_RIGHT", label = "NES Right" },
}

local thresholdMs = 1000
local lastThresholdPrint = {}

function gui()
    clearrect(0, 0, 256, 240)

    drawtext(4, 4, "getbuttonheldms() test", 0x39)
    drawtext(4, 12, "Hold buttons to see duration (ms)", 0x20)
    drawtext(4, 20, string.format("Threshold: %d ms (prints once per hold)", thresholdMs), 0x20)

    local y = 36
    for i, button in ipairs(trackedButtons) do
        local heldMs = getbuttonheldms(button.name)
        drawtext(10, y, string.format("%-16s: %5d ms", button.label, heldMs), 0x20)

        if heldMs >= thresholdMs then
            if not lastThresholdPrint[button.name] then
                print(string.format("%s held for %d ms (threshold %d ms)",
                    button.name, heldMs, thresholdMs))
                lastThresholdPrint[button.name] = true
            end
        else
            lastThresholdPrint[button.name] = false
        end

        y = y + 10
        if y > 220 then
            y = 36
        end
    end
end

