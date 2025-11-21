-- textstyle() verification script
-- Tests text styling options and validates that styles apply to drawtext() with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "sizeSmall",
        desc = "Set size to 0.5x (min)",
        expectSuccess = true,
        fn = function()
            textstyle({ size = 0.5 })
            textstyle({ size = 1.0 }) -- Reset after test
        end,
    },
    {
        id = "sizeLarge",
        desc = "Set size to 4.0x (max)",
        expectSuccess = true,
        fn = function()
            textstyle({ size = 4.0 })
            textstyle({ size = 1.0 }) -- Reset
        end,
    },
    {
        id = "alignLeft",
        desc = "Set align to 'left'",
        expectSuccess = true,
        fn = function()
            textstyle({ align = "left" })
        end,
    },
    {
        id = "alignCenter",
        desc = "Set align to 'center'",
        expectSuccess = true,
        fn = function()
            textstyle({ align = "center" })
        end,
    },
    {
        id = "alignRight",
        desc = "Set align to 'right'",
        expectSuccess = true,
        fn = function()
            textstyle({ align = "right" })
        end,
    },
    {
        id = "outlineTrue",
        desc = "Enable outline (boolean)",
        expectSuccess = true,
        fn = function()
            textstyle({ outline = true })
            textstyle({ outline = 0 }) -- Reset
        end,
    },
    {
        id = "outlineThick",
        desc = "Set outline to 2 (thick)",
        expectSuccess = true,
        fn = function()
            textstyle({ outline = 2 })
            textstyle({ outline = 0 }) -- Reset
        end,
    },
    {
        id = "shadowTrue",
        desc = "Enable shadow",
        expectSuccess = true,
        fn = function()
            textstyle({ shadow = true })
            textstyle({ shadow = false }) -- Reset
        end,
    },
    {
        id = "spacingPositive",
        desc = "Set positive spacing",
        expectSuccess = true,
        fn = function()
            textstyle({ spacing = 5 })
            textstyle({ spacing = 0 }) -- Reset
        end,
    },
    {
        id = "spacingNegative",
        desc = "Set negative spacing",
        expectSuccess = true,
        fn = function()
            textstyle({ spacing = -2 })
            textstyle({ spacing = 0 }) -- Reset
        end,
    },
    {
        id = "multipleOptions",
        desc = "Set multiple options at once",
        expectSuccess = true,
        fn = function()
            textstyle({ size = 2.0, outline = 1, shadow = true, spacing = 2 })
            textstyle({ size = 1.0, outline = 0, shadow = false, spacing = 0 }) -- Reset
        end,
    },
    {
        id = "emptyTable",
        desc = "Empty table (no changes)",
        expectSuccess = true,
        fn = function()
            textstyle({})
        end,
    },
    {
        id = "missingArg",
        desc = "Missing table argument throws error",
        expectSuccess = false,
        fn = function()
            textstyle() -- No argument
        end,
    },
    {
        id = "wrongType",
        desc = "Non-table argument throws error",
        expectSuccess = false,
        fn = function()
            textstyle("not a table")
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
    print("----- textstyle() QA -----")
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
        print("[RESULT] textstyle(): ✅ PASS")
    else
        print("[RESULT] textstyle(): ⚠️ FAIL (check console log)")
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
        print("=== Running textstyle tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    -- Reset to defaults for header
    textstyle({ size = 1.0, outline = 0, shadow = false, spacing = 0 })
    drawtext(4, 4, string.format("textstyle QA (frame %d)", frame), 0x37)
    
    local y = 20
    
    -- Size variations
    textstyle({ size = 0.5 })
    drawtext(10, y, "0.5x Small", 0x29)
    y = y + 10
    
    textstyle({ size = 1.0 })
    drawtext(10, y, "1.0x Normal", 0x20)
    y = y + 10
    
    textstyle({ size = 2.0 })
    drawtext(10, y, "2.0x Large", 0x37)
    y = y + 20
    
    -- Reset size
    textstyle({ size = 1.0 })
    y = y + 5
    
    -- Outline variations
    drawtext(10, y, "No outline", 0x16)
    y = y + 10
    
    textstyle({ outline = 1 })
    drawtext(10, y, "Thin outline", 0x21)
    y = y + 10
    
    textstyle({ outline = 2 })
    drawtext(10, y, "Thick outline", 0x2E)
    y = y + 10
    
    -- Reset outline
    textstyle({ outline = 0 })
    y = y + 5
    
    -- Shadow
    textstyle({ shadow = true })
    drawtext(10, y, "With shadow", 0x29)
    y = y + 10
    
    textstyle({ shadow = false })
    y = y + 5
    
    -- Spacing variations
    textstyle({ spacing = -2 })
    drawtext(10, y, "Tight -2", 0x20)
    y = y + 10
    
    textstyle({ spacing = 0 })
    drawtext(10, y, "Normal 0", 0x20)
    y = y + 10
    
    textstyle({ spacing = 5 })
    drawtext(10, y, "Wide 5", 0x20)
    y = y + 10
    
    -- Reset spacing
    textstyle({ spacing = 0 })
    y = y + 5
    
    -- Combined styles
    textstyle({ size = 1.5, outline = 1, shadow = true })
    drawtext(10, y, "Combined!", 0x37)
    
    -- Instructions
    textstyle({ size = 1.0, outline = 0, shadow = false, spacing = 0 })
    drawtext(4, 220, "Check console for test results", 0x20)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

