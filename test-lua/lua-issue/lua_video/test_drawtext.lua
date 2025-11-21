-- drawtext() verification script for lua_video module
-- Ensures core drawtext paths execute without errors and reports PASS/FAIL to the console.

local frame = 0
local testsRan = false
local testStatus = {
    basic = { ok = false, err = nil, desc = "Basic draw with explicit color" },
    defaultColor = { ok = false, err = nil, desc = "Draw using default color parameter" },
    clampBounds = { ok = false, err = nil, desc = "Draw near lower edge (clamping / skip logic)" },
}

local function runTest(name, fn)
    local ok, err = pcall(fn)
    testStatus[name].ok = ok
    testStatus[name].err = err
end

local function logResults()
    local allPass = true
    print("----- drawtext() QA -----")
    for name, result in pairs(testStatus) do
        if result.ok then
            print(string.format("[PASS] %s", result.desc))
        else
            allPass = false
            print(string.format("[FAIL] %s -- %s", result.desc, tostring(result.err or "unknown error")))
        end
    end

    if allPass then
        print("[RESULT] drawtext(): ✅ PASS")
    else
        print("[RESULT] drawtext(): ⚠️ FAIL (see individual entries above)")
    end
end

local function performTests()
    runTest("basic", function()
        drawtext(8, 8, "drawtext() QA - basic", 0x29)
    end)

    runTest("defaultColor", function()
        drawtext(8, 24, "default color argument", nil) -- rely on default 0x20
    end)

    runTest("clampBounds", function()
        -- Place text near bottom to ensure function safely skips instead of crashing
        drawtext(8, 232, "edge clamp test", 0x2E)
    end)

    logResults()
end

local function drawOverlay()
    frame = frame + 1

    if not testsRan then
        performTests()
        testsRan = true
    end

    -- Minimal overlay so testers can see the script is running.
    drawtext(4, 4, string.format("drawtext QA running (frame %d)", frame), 0x37)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

