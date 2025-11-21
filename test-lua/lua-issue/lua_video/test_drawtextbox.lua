-- drawtextbox() verification script
-- Tests text boxes with backgrounds, borders, and validates argument handling with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "fullBox",
        desc = "Full box with text, background, and border",
        expectSuccess = true,
        fn = function()
            drawtextbox(10, 20, 100, 40, "Full Box", 0x29, 0x0F, 0x30)
        end,
    },
    {
        id = "noBg",
        desc = "Box with border but no background",
        expectSuccess = true,
        fn = function()
            drawtextbox(120, 20, 100, 40, "No BG", 0x20, -1, 0x16)
        end,
    },
    {
        id = "noBorder",
        desc = "Box with background but no border",
        expectSuccess = true,
        fn = function()
            drawtextbox(10, 70, 100, 40, "No Border", 0x37, 0x0F, -1)
        end,
    },
    {
        id = "textOnly",
        desc = "Text only (no background or border)",
        expectSuccess = true,
        fn = function()
            drawtextbox(120, 70, 100, 40, "Text Only", 0x16, -1, -1)
        end,
    },
    {
        id = "multiline",
        desc = "Multiline text in box",
        expectSuccess = true,
        fn = function()
            drawtextbox(10, 120, 100, 50, "Line 1\nLine 2\nLine 3", 0x21, 0x0F, 0x29)
        end,
    },
    {
        id = "smallBox",
        desc = "Small box (edge case)",
        expectSuccess = true,
        fn = function()
            drawtextbox(120, 120, 40, 20, "Small", 0x2E, 0x0F, 0x16)
        end,
    },
    {
        id = "nilBgBorder",
        desc = "Optional params as nil (not -1)",
        expectSuccess = true,
        fn = function()
            drawtextbox(10, 180, 80, 30, "Nil opts", 0x29, nil, nil)
        end,
    },
    {
        id = "minimalArgs",
        desc = "Minimal args (6 params, no optional)",
        expectSuccess = true,
        fn = function()
            drawtextbox(100, 180, 80, 30, "Minimal", 0x20)
        end,
    },
    {
        id = "missingArgs",
        desc = "Missing required arguments throws error",
        expectSuccess = false,
        fn = function()
            drawtextbox(10, 10, 100, 40, "oops") -- Missing color
        end,
    },
    {
        id = "zeroDimensions",
        desc = "Zero width/height (should skip drawing)",
        expectSuccess = true,
        fn = function()
            drawtextbox(10, 10, 0, 0, "Hidden", 0x29, 0x0F, 0x30)
            -- Should succeed but not draw anything
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
    print("----- drawtextbox() QA -----")
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
        print("[RESULT] drawtextbox(): ✅ PASS")
    else
        print("[RESULT] drawtextbox(): ⚠️ FAIL (check console log)")
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
        print("=== Running drawtextbox tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("drawtextbox QA (frame %d)", frame), 0x37)
    
    -- Visual demonstration: draw various box configurations
    -- Full box with all features
    drawtextbox(10, 20, 100, 40, "Full Box\nBG+Border", 0x29, 0x0F, 0x30)
    
    -- Border only (no background)
    drawtextbox(120, 20, 100, 40, "Border Only\nNo BG", 0x20, -1, 0x16)
    
    -- Background only (no border)
    drawtextbox(10, 70, 100, 40, "BG Only\nNo Border", 0x37, 0x0F, -1)
    
    -- Text only (no bg or border)
    drawtextbox(120, 70, 100, 40, "Text Only\nNo BG/Border", 0x16, -1, -1)
    
    -- Multiline with padding demonstration
    drawtextbox(10, 120, 100, 50, "Multiline\nTest Box\nLine 3", 0x21, 0x0F, 0x29)
    
    -- Small box
    drawtextbox(120, 120, 80, 35, "Small Box\nTest", 0x2E, 0x0F, 0x16)
    
    -- Animated box (changing color)
    local animColor = (frame % 60 < 30) and 0x29 or 0x16
    drawtextbox(10, 180, 100, 30, "Animated!", 0x30, 0x0F, animColor)
    
    -- Instructions
    drawtext(4, 220, "Check console for test results", 0x20)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

