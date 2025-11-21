-- drawtextscaled() verification script
-- Tests various scaling factors and validates argument handling with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "normalScale",
        desc = "Normal 1.0x scale",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 20, "1.0x scale", 0x29, 1.0, 1.0)
        end,
    },
    {
        id = "smallScale",
        desc = "Small 0.5x scale (min)",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 35, "0.5x scale", 0x20, 0.5, 0.5)
        end,
    },
    {
        id = "largeScale",
        desc = "Large 2.0x scale",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 50, "2.0x LARGE", 0x37, 2.0, 2.0)
        end,
    },
    {
        id = "maxScale",
        desc = "Maximum 4.0x scale",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 80, "4.0x MAX", 0x16, 4.0, 4.0)
        end,
    },
    {
        id = "asymmetricScale",
        desc = "Asymmetric X/Y scaling",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 120, "Wide 2x1", 0x21, 2.0, 1.0)
        end,
    },
    {
        id = "autoClampLow",
        desc = "Auto-clamp scale < 0.5 (should clamp to 0.5)",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 140, "Clamp 0.1->0.5", 0x2E, 0.1, 0.1)
        end,
    },
    {
        id = "autoClampHigh",
        desc = "Auto-clamp scale > 4.0 (should clamp to 4.0)",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 160, "Clamp 10->4", 0x2E, 10.0, 10.0)
        end,
    },
    {
        id = "missingArgs",
        desc = "Missing required arguments throws error",
        expectSuccess = false,
        fn = function()
            drawtextscaled(10, 10, "oops", 0x20) -- Missing scaleX, scaleY
        end,
    },
    {
        id = "badScaleType",
        desc = "Non-numeric scale throws error",
        expectSuccess = false,
        fn = function()
            drawtextscaled(10, 10, "bad", 0x20, "two", 1.0)
        end,
    },
    {
        id = "zeroScale",
        desc = "Zero scale (should clamp to 0.5)",
        expectSuccess = true,
        fn = function()
            drawtextscaled(10, 180, "Zero->0.5", 0x29, 0.0, 0.0)
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
    print("----- drawtextscaled() QA -----")
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
        print("[RESULT] drawtextscaled(): ✅ PASS")
    else
        print("[RESULT] drawtextscaled(): ⚠️ FAIL (check console log)")
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
        print("=== Running drawtextscaled tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("drawtextscaled QA (frame %d)", frame), 0x37)
    drawtext(4, 200, "Check console for test results", 0x20)
    
    -- Draw live examples each frame to show scaling visually
    drawtextscaled(120, 20, "1.0x", 0x29, 1.0, 1.0)
    drawtextscaled(120, 40, "0.5x", 0x20, 0.5, 0.5)
    drawtextscaled(120, 60, "2.0x", 0x37, 2.0, 2.0)
    drawtextscaled(120, 100, "4.0x", 0x16, 4.0, 4.0)
    drawtextscaled(120, 150, "Wide", 0x21, 2.5, 1.0)
    drawtextscaled(120, 170, "Tall", 0x2E, 1.0, 2.5)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

