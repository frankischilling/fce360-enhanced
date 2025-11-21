-- Test script for getbuttonheldms(btn)
-- Displays how long buttons are held and prints thresholds when exceeded

local trackedButtons = {
    -- Digital buttons supported by isxboxbuttonpressed()
    { name = "A", label = "A Button", useXboxCheck = true },
    { name = "B", label = "B Button", useXboxCheck = true },
    { name = "X", label = "X Button", useXboxCheck = true },
    { name = "Y", label = "Y Button", useXboxCheck = true },
    { name = "START", label = "Start", useXboxCheck = true },
    { name = "BACK", label = "Back", useXboxCheck = true },
    { name = "LEFT_SHOULDER", label = "Left Shoulder", useXboxCheck = true },
    { name = "RIGHT_SHOULDER", label = "Right Shoulder", useXboxCheck = true },
    { name = "LEFT_THUMB", label = "Left Stick Click", useXboxCheck = true },
    { name = "RIGHT_THUMB", label = "Right Stick Click", useXboxCheck = true },
    { name = "DPAD_UP", label = "D-Pad Up", useXboxCheck = true },
    { name = "DPAD_DOWN", label = "D-Pad Down", useXboxCheck = true },
    { name = "DPAD_LEFT", label = "D-Pad Left", useXboxCheck = true },
    { name = "DPAD_RIGHT", label = "D-Pad Right", useXboxCheck = true },

    -- Analog triggers and stick directions (use getbuttonheldms only)
    { name = "LT", label = "Left Trigger", useXboxCheck = false },
    { name = "RT", label = "Right Trigger", useXboxCheck = false },
    { name = "LS_UP", label = "Left Stick Up", useXboxCheck = false },
    { name = "LS_DOWN", label = "Left Stick Down", useXboxCheck = false },
    { name = "LS_LEFT", label = "Left Stick Left", useXboxCheck = false },
    { name = "LS_RIGHT", label = "Left Stick Right", useXboxCheck = false },
    { name = "RS_UP", label = "Right Stick Up", useXboxCheck = false },
    { name = "RS_DOWN", label = "Right Stick Down", useXboxCheck = false },
    { name = "RS_LEFT", label = "Right Stick Left", useXboxCheck = false },
    { name = "RS_RIGHT", label = "Right Stick Right", useXboxCheck = false },
}

local thresholdMs = 1000
local lastThresholdPrint = {}
local playerIndex = 0

function gui()
    clearrect(0, 0, 256, 240)

    drawtext(4, 4, "Xbox getbuttonheldms() test", 0x39)
    drawtext(4, 12, "Hold Xbox 360 buttons to see duration (ms)", 0x20)
    drawtext(4, 20, string.format("Threshold: %d ms (prints once per hold)", thresholdMs), 0x20)

    local y = 36
    for i, button in ipairs(trackedButtons) do
        local heldMs = getbuttonheldms(button.name)
        local isPressed
        if button.useXboxCheck then
            isPressed = isxboxbuttonpressed(playerIndex, button.name)
        else
            -- Treat analog inputs as "pressed" if held time is non-zero
            isPressed = heldMs > 0
        end

        local color = isPressed and 0x2A or 0x20
        drawtext(10, y, string.format("%-16s: %5d ms %s",
            button.label, heldMs, isPressed and " [PRESSED]" or ""), color)

        if heldMs >= thresholdMs and isPressed then
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

