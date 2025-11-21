-- setconsolespacing() verification script
-- Runs success + failure cases and logs PASS/FAIL to console.

local frame = 0
local testsRan = false

local testMatrix = {
    { id = "validDefault", desc = "Set spacing to 2 (default)", expectSuccess = true, fn = function()
        setconsolespacing(2)
    end },
    { id = "clampLow", desc = "Clamp negative spacing to 0", expectSuccess = true, fn = function()
        setconsolespacing(-20)
    end },
    { id = "clampHigh", desc = "Clamp large spacing to 8", expectSuccess = true, fn = function()
        setconsolespacing(64)
    end },
    { id = "missingArg", desc = "Missing required argument triggers error", expectSuccess = false, fn = function()
        -- Deliberately omit argument
        -- luacheck: ignore 212
        setconsolespacing()
    end },
    { id = "badType", desc = "Non-numeric argument triggers error", expectSuccess = false, fn = function()
        setconsolespacing("abc")
    end },
}

local results = {}

local function runTest(entry)
    local ok, err = pcall(entry.fn)
    local passed = entry.expectSuccess and ok or (not entry.expectSuccess and not ok)
    results[entry.id] = { passed = passed, rawOk = ok, err = err }
end

local function logResults()
    local allPass = true
    print("----- setconsolespacing() QA -----")
    for _, entry in ipairs(testMatrix) do
        local result = results[entry.id]
        if result and result.passed then
            print(string.format("[PASS] %s", entry.desc))
        else
            allPass = false
            local errMsg = result and result.err or "no result"
            print(string.format("[FAIL] %s -- %s", entry.desc, tostring(errMsg)))
        end
    end

    if allPass then
        print("[RESULT] setconsolespacing(): ✅ PASS")
    else
        print("[RESULT] setconsolespacing(): ⚠️ FAIL (see entries above)")
    end
end

local function performTests()
    for _, entry in ipairs(testMatrix) do
        runTest(entry)
    end
    logResults()
end

local function drawOverlay()
    frame = frame + 1

    if not testsRan then
        performTests()
        testsRan = true
    end

    drawtext(4, 4, string.format("setconsolespacing QA (frame %d)", frame), 0x37)
    drawtext(4, 14, "Console logs results; inspect spacing visually.", 0x20)
end

function script()
    drawOverlay()
end

function gui()
    drawOverlay()
end

