-- Test script for getbuttonheldms(btn) focusing on NES buttons
-- Displays hold time for NES A/B/Select/Start and D-Pad

local nesButtons = {
    { name = "NES_A", label = "NES A" },
    { name = "NES_B", label = "NES B" },
    { name = "NES_SELECT", label = "NES Select" },
    { name = "NES_START", label = "NES Start" },
    { name = "NES_UP", label = "NES Up" },
    { name = "NES_DOWN", label = "NES Down" },
    { name = "NES_LEFT", label = "NES Left" },
    { name = "NES_RIGHT", label = "NES Right" },
}

local thresholdMs = 1000
local printedThreshold = {}

function gui()
    clearrect(0, 0, 256, 240)

    drawtext(4, 4, "NES getbuttonheldms() test", 0x39)
    drawtext(4, 12, "Hold NES-mapped buttons to see duration", 0x20)
    drawtext(4, 20, string.format("Threshold: %d ms (console prints once)", thresholdMs), 0x20)

    local y = 36
    for _, button in ipairs(nesButtons) do
        local heldMs = getbuttonheldms(button.name)
        drawtext(10, y, string.format("%-12s: %5d ms", button.label, heldMs), 0x20)

        if heldMs >= thresholdMs then
            if not printedThreshold[button.name] then
                print(string.format("%s held for %d ms (threshold %d ms)",
                    button.name, heldMs, thresholdMs))
                printedThreshold[button.name] = true
            end
        else
            printedThreshold[button.name] = false
        end

        y = y + 10
        if y > 220 then
            y = 36
        end
    end
end

