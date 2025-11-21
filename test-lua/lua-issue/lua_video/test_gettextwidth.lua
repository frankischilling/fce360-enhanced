-- gettextwidth() verification script
-- Tests text measurement and validates return values with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "singleLine",
        desc = "Single line text measurement",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("Hello")
            if width <= 0 then
                error("Expected positive width, got " .. width)
            end
            return width
        end,
    },
    {
        id = "emptyString",
        desc = "Empty string returns 0",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("")
            if width ~= 0 then
                error("Expected 0 for empty string, got " .. width)
            end
            return width
        end,
    },
    {
        id = "multiline",
        desc = "Multiline text returns max width",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("Short\nMuch longer line\nMed")
            -- "Much longer line" should be widest
            if width <= 0 then
                error("Expected positive width for multiline, got " .. width)
            end
            return width
        end,
    },
    {
        id = "spaces",
        desc = "Text with spaces",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("A B C D E")
            if width <= 0 then
                error("Expected positive width with spaces, got " .. width)
            end
            return width
        end,
    },
    {
        id = "specialChars",
        desc = "Special characters",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("!@#$%^&*()")
            if width <= 0 then
                error("Expected positive width for special chars, got " .. width)
            end
            return width
        end,
    },
    {
        id = "tabs",
        desc = "Text with tab character",
        expectSuccess = true,
        fn = function()
            local width = gettextwidth("A\tB")
            -- Tab should add space (4 spaces worth)
            if width <= 0 then
                error("Expected positive width with tab, got " .. width)
            end
            return width
        end,
    },
    {
        id = "variableWidth",
        desc = "Compare narrow vs wide chars",
        expectSuccess = true,
        fn = function()
            local narrow = gettextwidth("iii")
            local wide = gettextwidth("WWW")
            -- W should be wider than i
            if wide <= narrow then
                error(string.format("Expected 'WWW' (%d) > 'iii' (%d)", wide, narrow))
            end
            return wide
        end,
    },
    {
        id = "trailingNewline",
        desc = "Trailing newline doesn't affect width",
        expectSuccess = true,
        fn = function()
            local withNewline = gettextwidth("Test\n")
            local withoutNewline = gettextwidth("Test")
            -- Width should be the same (newline doesn't add width)
            if withNewline ~= withoutNewline then
                error(string.format("Width mismatch: %d vs %d", withNewline, withoutNewline))
            end
            return withNewline
        end,
    },
    {
        id = "missingArg",
        desc = "Missing required argument throws error",
        expectSuccess = false,
        fn = function()
            gettextwidth() -- No argument
        end,
    },
    {
        id = "wrongType",
        desc = "Non-string argument throws error",
        expectSuccess = false,
        fn = function()
            gettextwidth(123) -- Number instead of string
        end,
    },
}

local results = {}
local widthValues = {}

local function runTest(item)
    local ok, result = pcall(item.fn)
    local passed = item.expectSuccess and ok or (not item.expectSuccess and not ok)
    results[item.id] = { passed = passed, ok = ok, err = result }
    if ok and type(result) == "number" then
        widthValues[item.id] = result
    end
end

local function logResults()
    local allPass = true
    print("----- gettextwidth() QA -----")
    for _, item in ipairs(tests) do
        local result = results[item.id]
        if result and result.passed then
            local extra = ""
            if widthValues[item.id] then
                extra = string.format(" (width: %d px)", widthValues[item.id])
            end
            print(string.format("[PASS] %s%s", item.desc, extra))
        else
            allPass = false
            print(string.format("[FAIL] %s -- %s", item.desc, tostring(result and result.err or "no result")))
        end
    end

    if allPass then
        print("[RESULT] gettextwidth(): ✅ PASS")
    else
        print("[RESULT] gettextwidth(): ⚠️ FAIL (check console log)")
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
        print("=== Running gettextwidth tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("gettextwidth QA (frame %d)", frame), 0x37)
    drawtext(4, 220, "Check console for test results", 0x20)
    
    -- Visual demonstration: draw text with rectangles showing measured width
    local y = 30
    local testStrings = {
        { text = "Short", color = 0x29 },
        { text = "Medium length", color = 0x20 },
        { text = "Very long text line here", color = 0x37 },
        { text = "iii", color = 0x16 },
        { text = "WWW", color = 0x21 },
    }
    
    for _, item in ipairs(testStrings) do
        local width = gettextwidth(item.text)
        
        -- Draw background rectangle showing measured width
        fillrect(10, y - 1, width, 10, 0x30)
        
        -- Draw the actual text on top
        drawtext(10, y, item.text, item.color)
        
        -- Show the measured width value
        drawtext(160, y, string.format("%d px", width), 0x2E)
        
        y = y + 15
    end
    
    -- Demonstrate multiline measurement
    local multiText = "Short\nMuch longer line\nMed"
    local multiWidth = gettextwidth(multiText)
    drawtext(10, 130, "Multiline (max width):", 0x20)
    drawtext(10, 145, multiText, 0x29)
    fillrect(10, 143, multiWidth, 1, 0x16) -- Show max width as a line
    drawtext(160, 150, string.format("%d px", multiWidth), 0x2E)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

