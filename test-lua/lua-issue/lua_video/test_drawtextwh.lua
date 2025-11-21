-- drawtextwh() verification script
-- Covers normal usage plus argument validation paths and logs PASS/FAIL to console.

local frame = 0
local ran = false

local tests = {
    {
        id = "basicBox",
        desc = "Basic textbox with border",
        expectSuccess = true,
        fn = function()
            drawtextwh(16, 32, "drawtextwh basic\nline wrap test", 0x29, 120, 40, 1)
        end,
    },
    {
        id = "noBorder",
        desc = "Borderless textbox (border=0)",
        expectSuccess = true,
        fn = function()
            drawtextwh(16, 80, "No border variant", 0x20, 100, 20, 0)
        end,
    },
    {
        id = "clampHeight",
        desc = "Textbox clamped near bottom edge",
        expectSuccess = true,
        fn = function()
            drawtextwh(16, 220, "Clamp test", 0x37, 80, 40, 1)
        end,
    },
    {
        id = "missingArgs",
        desc = "Missing required args throws error",
        expectSuccess = false,
        fn = function()
            -- Intentionally omit params
            drawtextwh(10, 10, "oops")
        end,
    },
    {
        id = "badTypes",
        desc = "Non-numeric width triggers error",
        expectSuccess = false,
        fn = function()
            drawtextwh(10, 10, "bad width", 0x29, "wide", 20, 1)
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
    print("----- drawtextwh() QA -----")
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
        print("[RESULT] drawtextwh(): ✅ PASS")
    else
        print("[RESULT] drawtextwh(): ⚠️ FAIL (check console log)")
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
        print("=== Running tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("drawtextwh QA (frame %d)", frame), 0x37)
    drawtext(4, 14, "Inspect overlay + console output.", 0x20)
    
    -- Debug: Draw colored rectangles to verify basic drawing works
    fillrect(140, 32, 100, 40, 0x16)  -- Red box
    fillrect(140, 80, 100, 20, 0x29)  -- Green box
    fillrect(140, 120, 100, 20, 0x21) -- Blue box
    
    -- Debug: Try drawing a regular box first to verify drawing works
    if frame <= 5 then
        print("Frame " .. frame .. ": Attempting to draw test boxes...")
    end
    
    -- Draw the test boxes directly each frame so they're visible
    local ok1 = pcall(function() drawtextwh(16, 32, "drawtextwh basic\nline wrap test", 0x29, 120, 40, 1) end)
    local ok2 = pcall(function() drawtextwh(16, 80, "No border variant", 0x20, 100, 20, 0) end)
    local ok3 = pcall(function() drawtextwh(16, 120, "Another test", 0x37, 80, 20, 2) end)
    
    if frame <= 5 then
        print(string.format("  Box 1: %s, Box 2: %s, Box 3: %s", tostring(ok1), tostring(ok2), tostring(ok3)))
    end
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

