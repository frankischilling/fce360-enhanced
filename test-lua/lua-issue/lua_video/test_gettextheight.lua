-- gettextheight() verification script
-- Tests text height measurement and validates return values with PASS/FAIL logging

local frame = 0
local ran = false

local tests = {
    {
        id = "singleLine",
        desc = "Single line text (8 pixels)",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("Hello")
            if height ~= 8 then
                error(string.format("Expected 8 pixels, got %d", height))
            end
            return height
        end,
    },
    {
        id = "emptyString",
        desc = "Empty string returns 0",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("")
            if height ~= 0 then
                error(string.format("Expected 0 for empty string, got %d", height))
            end
            return height
        end,
    },
    {
        id = "twoLines",
        desc = "Two lines with newline (16 pixels)",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("Line 1\nLine 2")
            if height ~= 16 then
                error(string.format("Expected 16 pixels (2 lines), got %d", height))
            end
            return height
        end,
    },
    {
        id = "threeLines",
        desc = "Three lines (24 pixels)",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("One\nTwo\nThree")
            if height ~= 24 then
                error(string.format("Expected 24 pixels (3 lines), got %d", height))
            end
            return height
        end,
    },
    {
        id = "trailingNewline",
        desc = "Trailing newline adds extra line",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("Text\n")
            if height ~= 16 then
                error(string.format("Expected 16 pixels (2 lines with trailing \\n), got %d", height))
            end
            return height
        end,
    },
    {
        id = "multipleNewlines",
        desc = "Multiple consecutive newlines",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("A\n\n\nB")
            -- "A\n\n\nB" = 4 lines (A, empty, empty, B)
            if height ~= 32 then
                error(string.format("Expected 32 pixels (4 lines), got %d", height))
            end
            return height
        end,
    },
    {
        id = "onlyNewlines",
        desc = "String with only newlines",
        expectSuccess = true,
        fn = function()
            local height = gettextheight("\n\n")
            -- "\n\n" = 3 lines (empty + 2 newlines)
            if height ~= 24 then
                error(string.format("Expected 24 pixels (3 lines), got %d", height))
            end
            return height
        end,
    },
    {
        id = "longMultiline",
        desc = "Long multiline block (10 lines)",
        expectSuccess = true,
        fn = function()
            local text = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10"
            local height = gettextheight(text)
            if height ~= 80 then
                error(string.format("Expected 80 pixels (10 lines), got %d", height))
            end
            return height
        end,
    },
    {
        id = "missingArg",
        desc = "Missing required argument throws error",
        expectSuccess = false,
        fn = function()
            gettextheight() -- No argument
        end,
    },
    {
        id = "wrongType",
        desc = "Non-string argument (Lua may auto-convert)",
        expectSuccess = true,
        fn = function()
            -- Lua often coerces numbers to strings, so this may succeed
            local height = gettextheight(456)
            -- If it succeeds, verify it returns a valid number
            if type(height) ~= "number" then
                error("Expected number return, got " .. type(height))
            end
            return height
        end,
    },
}

local results = {}
local heightValues = {}

local function runTest(item)
    local ok, result = pcall(item.fn)
    local passed = item.expectSuccess and ok or (not item.expectSuccess and not ok)
    results[item.id] = { passed = passed, ok = ok, err = result }
    if ok and type(result) == "number" then
        heightValues[item.id] = result
    end
end

local function logResults()
    local allPass = true
    print("----- gettextheight() QA -----")
    for _, item in ipairs(tests) do
        local result = results[item.id]
        if result and result.passed then
            local extra = ""
            if heightValues[item.id] then
                extra = string.format(" (height: %d px)", heightValues[item.id])
            end
            print(string.format("[PASS] %s%s", item.desc, extra))
        else
            allPass = false
            print(string.format("[FAIL] %s -- %s", item.desc, tostring(result and result.err or "no result")))
        end
    end

    if allPass then
        print("[RESULT] gettextheight(): ✅ PASS")
    else
        print("[RESULT] gettextheight(): ⚠️ FAIL (check console log)")
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
        print("=== Running gettextheight tests on frame " .. frame .. " ===")
        perform()
        ran = true
    end

    drawtext(4, 4, string.format("gettextheight QA (frame %d)", frame), 0x37)
    drawtext(4, 220, "Check console for test results", 0x20)
    
    -- Visual demonstration: draw text blocks with rectangles showing measured height
    local x = 10
    local y = 30
    
    local testStrings = {
        { text = "Single", label = "1 line", color = 0x29 },
        { text = "Line 1\nLine 2", label = "2 lines", color = 0x20 },
        { text = "A\nB\nC", label = "3 lines", color = 0x37 },
        { text = "1\n2\n3\n4\n5", label = "5 lines", color = 0x16 },
    }
    
    for _, item in ipairs(testStrings) do
        local height = gettextheight(item.text)
        
        -- Draw background rectangle showing measured height
        fillrect(x - 2, y - 1, 70, height + 2, 0x30)
        
        -- Draw the actual text on top
        drawtext(x, y, item.text, item.color)
        
        -- Show the label and measured height value
        drawtext(x + 75, y, item.label, 0x2E)
        drawtext(x + 75, y + 8, string.format("%d px", height), 0x20)
        
        y = y + height + 10
    end
    
    -- Demonstrate that trailing newline adds a line
    local withTrailing = "Text\n"
    local withoutTrailing = "Text"
    local heightWith = gettextheight(withTrailing)
    local heightWithout = gettextheight(withoutTrailing)
    
    drawtext(150, 30, "Trailing \\n test:", 0x20)
    drawtext(150, 40, string.format("'Text' = %d px", heightWithout), 0x29)
    drawtext(150, 50, string.format("'Text\\n' = %d px", heightWith), 0x37)
    if heightWith == heightWithout + 8 then
        drawtext(150, 60, "Trailing \\n adds 8px ✓", 0x29)
    else
        drawtext(150, 60, "ERROR: Height mismatch!", 0x16)
    end
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

