-- Test script for getregion() function
-- Displays current ROM region and logs transitions

local lastRegion = ""
local frameCount = 0

function script()
    frameCount = frameCount + 1

    local romName = getromname()
    local region = getregion()
    local y = 4

    drawtext(4, y, "=== getregion() Test ===", 0x3F)
    y = y + 10

    if romName == "" then
        drawtext(4, y, "No ROM loaded", 0x37)
        return
    end

    drawtext(4, y, "ROM: " .. romName, 0x2E)
    y = y + 10
    drawtext(4, y, "Region: " .. region, 0x20)

    if region ~= lastRegion then
        print("getregion(): " .. region)
        lastRegion = region
    end

    if frameCount % 300 == 0 then
        print(string.format("[frame %d] Region = %s", frameCount, region))
    end
end

