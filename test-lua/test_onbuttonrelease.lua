-- Test script for onbuttonrelease(btn, cb)
-- Verifies registration, replacement, and unregistration behavior for release callbacks

local tests = {
    { label = "Register release callback", status = "WAIT" },
    { label = "Replace release callback", status = "WAIT" },
    { label = "Unregister stops release callback", status = "WAIT" }
}

local stage = 1
local awaitingNoCallback = false
local summaryPrinted = false
local lastHardwareA = false

local function printResult(index, result, message)
    print(string.format("Test %d: %s - %s", index, result, message))
end

local function updateStatus(index, status)
    tests[index].status = status
end

local stage2Handler -- forward declaration

local function stage1Handler(player, button)
    if stage ~= 1 then
        return
    end
    updateStatus(1, "PASS")
    printResult(1, "PASS", string.format("Release callback fired from player %d (%s)", player + 1, button))
    stage = 2
    onbuttonrelease("A", stage2Handler)
    print("Test 2: Press and release A again to verify callback replacement")
end

stage2Handler = function(player, button)
    if stage ~= 2 then
        if stage == 3 and tests[3].status ~= "PASS" then
            if tests[3].status ~= "FAIL" then
                updateStatus(3, "FAIL")
                printResult(3, "FAIL", "Release callback fired after unregistration")
            end
            awaitingNoCallback = false
        end
        return
    end

    updateStatus(2, "PASS")
    printResult(2, "PASS", string.format("Replacement callback fired from player %d (%s)", player + 1, button))
    stage = 3
    awaitingNoCallback = true
    onbuttonrelease("A", nil)
    print("Test 3: Press and release A once more. Callback should NOT fire after unregistration.")
end

onbuttonrelease("A", stage1Handler)
print("Test 1: Press and release the A button to trigger the registered release callback")

function gui()
    local frame = getframecount()
    clearrect(0, 0, 256, 240)

    drawtext(4, 4, "onbuttonrelease() test", 0x39)
    drawtext(4, 12, string.format("Frame: %d", frame), 0x20)

    local y = 30
    for i, test in ipairs(tests) do
        drawtext(10, y, string.format("%d. %s - %s", i, test.label, test.status), 0x20)
        y = y + 12
    end

    if stage == 1 then
        drawtext(10, 80, "Press and then release A to trigger the callback", 0x20)
    elseif stage == 2 then
        drawtext(10, 80, "Press and release A again to test replacement", 0x20)
    elseif stage == 3 then
        drawtext(10, 80, "Press and release A again. Callback should not run.", 0x20)
    else
        drawtext(10, 80, "All tests completed", 0x20)
    end

    local hardwareA = isxboxbuttonpressed(0, "A")
    local releasedThisFrame = (not hardwareA) and lastHardwareA

    if stage == 3 and awaitingNoCallback and releasedThisFrame then
        if tests[3].status ~= "FAIL" then
            updateStatus(3, "PASS")
            printResult(3, "PASS", "No release callback fired after unregistration")
        end
        awaitingNoCallback = false
        stage = 4
    end

    lastHardwareA = hardwareA

    if not summaryPrinted then
        local allDone = true
        local passCount = 0
        local failCount = 0
        for _, test in ipairs(tests) do
            if test.status == "WAIT" then
                allDone = false
            elseif test.status == "PASS" then
                passCount = passCount + 1
            elseif test.status == "FAIL" then
                failCount = failCount + 1
            end
        end
        if allDone then
            print(string.format("Summary: %d PASS, %d FAIL", passCount, failCount))
            print("=== onbuttonrelease() Test Suite Complete ===")
            summaryPrinted = true
        end
    end
end

