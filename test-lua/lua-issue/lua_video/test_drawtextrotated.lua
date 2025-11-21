-- drawtextrotated() verification script
-- Tests various rotation angles and validates argument handling with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "noRotation",
        desc = "No rotation (0 degrees)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(20, 30, "0deg", 0x29, 0)
        end,
    },
    {
        id = "rotate45",
        desc = "45 degree rotation",
        expectSuccess = true,
        fn = function()
            drawtextrotated(60, 50, "45deg", 0x20, 45)
        end,
    },
    {
        id = "rotate90",
        desc = "90 degree rotation (vertical)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(100, 70, "90deg", 0x37, 90)
        end,
    },
    {
        id = "rotate180",
        desc = "180 degree rotation (upside down)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(140, 90, "180deg", 0x16, 180)
        end,
    },
    {
        id = "rotate270",
        desc = "270 degree rotation",
        expectSuccess = true,
        fn = function()
            drawtextrotated(180, 110, "270deg", 0x21, 270)
        end,
    },
    {
        id = "negativeAngle",
        desc = "Negative angle (should normalize)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(20, 130, "-45deg", 0x2E, -45)
        end,
    },
    {
        id = "largeAngle",
        desc = "Angle > 360 (should wrap with modulo)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(60, 150, "405deg", 0x29, 405) -- 405 % 360 = 45
        end,
    },
    {
        id = "multipleRotations",
        desc = "Multiple full rotations (720 degrees)",
        expectSuccess = true,
        fn = function()
            drawtextrotated(100, 170, "720deg", 0x20, 720) -- 720 % 360 = 0
        end,
    },
    {
        id = "missingArgs",
        desc = "Missing required arguments throws error",
        expectSuccess = false,
        fn = function()
            drawtextrotated(10, 10, "oops", 0x20) -- Missing angleDeg
        end,
    },
    {
        id = "badAngleType",
        desc = "Non-numeric angle throws error",
        expectSuccess = false,
        fn = function()
            drawtextrotated(10, 10, "bad", 0x20, "ninety")
        end,
    },
}

local results = {}

local function runTest(item)
    local ok, err = pcall(item.fn)
    local passed = item.expectSuccess and ok or (not item.expectSuccess and not ok)
    results[item.id] = { passed = passed, ok = ok, err = err }
end

local function logResults()
    local allPass = true
    print("----- drawtextrotated() QA -----")
    for _, item in ipairs(tests) do
        local result = results[item.id]
        if result and result.passed then
            print(string.format("[PASS] %s", item.desc))
        else
            allPass = false
            print(string.format("[FAIL] %s -- %s", item.desc, tostring(result and result.err or "no result")))
        end
    end

    if allPass then
        print("[RESULT] drawtextrotated(): ✅ PASS")
    else
        print("[RESULT] drawtextrotated(): ⚠️ FAIL (check console log)")
    end
end

local function perform()
    for _, item in ipairs(tests) do
        runTest(item)
    end
    logResults()
end

local function drawOverlay()
    frame = frame + 1
    if not ran then
        print("=== Running drawtextrotated tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("drawtextrotated QA (frame %d)", frame), 0x37)
    drawtext(4, 220, "Check console for test results", 0x20)
    
    -- Draw live examples each frame to show rotation visually
    -- Center point for rotation demo
    local cx, cy = 200, 80
    
    -- Draw a reference cross to show rotation center
    fillrect(cx - 2, cy, 4, 1, 0x30) -- horizontal line
    fillrect(cx, cy - 2, 1, 4, 0x30) -- vertical line
    
    -- Draw text at various angles around the center
    drawtextrotated(cx, cy - 20, "0deg", 0x29, 0)
    drawtextrotated(cx + 15, cy - 15, "45deg", 0x20, 45)
    drawtextrotated(cx + 20, cy, "90deg", 0x37, 90)
    drawtextrotated(cx + 15, cy + 15, "135deg", 0x16, 135)
    drawtextrotated(cx, cy + 20, "180deg", 0x21, 180)
    drawtextrotated(cx - 15, cy + 15, "225deg", 0x2E, 225)
    drawtextrotated(cx - 20, cy, "270deg", 0x29, 270)
    drawtextrotated(cx - 15, cy - 15, "315deg", 0x20, 315)
    
    -- Animated rotating text
    local animAngle = (frame * 3) % 360
    drawtextrotated(200, 150, "ROTATING", 0x37, animAngle)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

