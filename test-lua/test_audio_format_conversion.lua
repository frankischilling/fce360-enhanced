-- Test script for audio format conversion functions
-- Tests all audio format conversion utilities

print("========================================")
print("Audio Format Conversion Test Script")
print("========================================")
print("Testing audio format conversion functions:")
print("  - audiosampletofloat(sample)")
print("  - floattosample(floatValue)")
print("  - audiosampletouint8(sample)")
print("  - uint8tosample(uint8Value)")
print("  - normalizeaudiosample(sample, [maxValue])")
print("  - monotostereo(monoSample)")
print("  - stereotomono(leftSample, rightSample)")
print("========================================")

-- Test counters
local testFrameCount = 0
local logInterval = 60  -- Log every 60 frames
local testsRun = false  -- Flag to ensure tests only run once

-- Test results tracking
local testResults = {}
local totalTests = 0
local passedTests = 0
local failedTests = 0

-- Helper function to add test result
local function addTestResult(testName, passed, details)
    totalTests = totalTests + 1
    if passed then
        passedTests = passedTests + 1
    else
        failedTests = failedTests + 1
    end
    table.insert(testResults, {
        name = testName,
        passed = passed,
        details = details or ""
    })
end

-- Test function availability on load
function onload()
    print("[TEST] Checking audio format conversion functions availability...")
    
    local functions = {
        "audiosampletofloat",
        "floattosample",
        "audiosampletouint8",
        "uint8tosample",
        "normalizeaudiosample",
        "monotostereo",
        "stereotomono"
    }
    
    for i = 1, #functions do
        local funcName = functions[i]
        local isAvailable = type(_G[funcName]) == "function"
        if isAvailable then
            print(string.format("  [OK] %s() is available", funcName))
            addTestResult(string.format("%s() available", funcName), true)
        else
            print(string.format("  [FAIL] %s() is NOT available", funcName))
            addTestResult(string.format("%s() available", funcName), false, "Function not found")
        end
    end
    
    -- Test basic conversions
    print("\n[TEST] Testing basic conversions...")
    
    -- Test audiosampletofloat
    local success, result = pcall(function()
        return audiosampletofloat(16384)  -- Half of max positive
    end)
    if success then
        local expected = 0.5
        local diff = math.abs(result - expected)
        local passed = diff < 0.01  -- Allow small floating point error
        print(string.format("  [%s] audiosampletofloat(16384) = %.6f (expected ~0.5, diff: %.6f)", 
            passed and "OK" or "FAIL", result, diff))
        addTestResult("audiosampletofloat(16384)", passed, string.format("Got %.6f, expected ~0.5", result))
    else
        print(string.format("  [FAIL] audiosampletofloat() error: %s", tostring(result)))
        addTestResult("audiosampletofloat(16384)", false, tostring(result))
    end
    
    -- Test floattosample
    local success2, result2 = pcall(function()
        return floattosample(0.5)
    end)
    if success2 then
        local expected = 16384
        local diff = math.abs(result2 - expected)
        local passed = diff < 100  -- Allow small rounding error
        print(string.format("  [%s] floattosample(0.5) = %d (expected ~16384, diff: %d)", 
            passed and "OK" or "FAIL", result2, diff))
        addTestResult("floattosample(0.5)", passed, string.format("Got %d, expected ~16384", result2))
    else
        print(string.format("  [FAIL] floattosample() error: %s", tostring(result2)))
        addTestResult("floattosample(0.5)", false, tostring(result2))
    end
    
    -- Test round-trip conversion
    local originalSample = 12345
    local floatVal = audiosampletofloat(originalSample)
    local convertedBack = floattosample(floatVal)
    local roundTripDiff = math.abs(originalSample - convertedBack)
    local roundTripPassed = roundTripDiff < 100  -- Allow small rounding error
    print(string.format("  [%s] Round-trip: %d -> %.6f -> %d (diff: %d)", 
        roundTripPassed and "OK" or "FAIL", originalSample, floatVal, convertedBack, roundTripDiff))
    addTestResult("Float round-trip conversion", roundTripPassed, 
        string.format("Diff: %d (should be < 100)", roundTripDiff))
    
    -- Test uint8 conversion
    local success3, result3 = pcall(function()
        return audiosampletouint8(0)  -- Zero should map to 128
    end)
    if success3 then
        local passed = result3 == 128
        print(string.format("  [%s] audiosampletouint8(0) = %d (expected 128)", 
            passed and "OK" or "FAIL", result3))
        addTestResult("audiosampletouint8(0)", passed, string.format("Got %d, expected 128", result3))
    else
        print(string.format("  [FAIL] audiosampletouint8() error: %s", tostring(result3)))
        addTestResult("audiosampletouint8(0)", false, tostring(result3))
    end
    
    -- Test uint8 round-trip
    local originalUint8 = 200
    local sampleFromUint8 = uint8tosample(originalUint8)
    local uint8Back = audiosampletouint8(sampleFromUint8)
    local uint8Diff = math.abs(originalUint8 - uint8Back)
    local uint8RoundTripPassed = uint8Diff <= 1  -- Allow 1 unit difference due to precision
    print(string.format("  [%s] Uint8 round-trip: %d -> %d -> %d (diff: %d)", 
        uint8RoundTripPassed and "OK" or "FAIL", originalUint8, sampleFromUint8, uint8Back, uint8Diff))
    addTestResult("Uint8 round-trip conversion", uint8RoundTripPassed, 
        string.format("Diff: %d (should be <= 1)", uint8Diff))
    
    -- Test monotostereo
    local success4, result4 = pcall(function()
        return monotostereo(1000)
    end)
    if success4 and result4 then
        local passed = result4.left == 1000 and result4.right == 1000
        print(string.format("  [%s] monotostereo(1000) = {left: %d, right: %d}", 
            passed and "OK" or "FAIL", result4.left, result4.right))
        addTestResult("monotostereo(1000)", passed, 
            string.format("Got {left: %d, right: %d}, expected both 1000", result4.left, result4.right))
    else
        print(string.format("  [FAIL] monotostereo() error: %s", tostring(result4)))
        addTestResult("monotostereo(1000)", false, tostring(result4))
    end
    
    -- Test stereotomono
    local success5, result5 = pcall(function()
        return stereotomono(1000, 2000)
    end)
    if success5 then
        local expected = 1500
        local passed = result5 == expected
        print(string.format("  [%s] stereotomono(1000, 2000) = %d (expected 1500)", 
            passed and "OK" or "FAIL", result5))
        addTestResult("stereotomono(1000, 2000)", passed, 
            string.format("Got %d, expected 1500", result5))
    else
        print(string.format("  [FAIL] stereotomono() error: %s", tostring(result5)))
        addTestResult("stereotomono(1000, 2000)", false, tostring(result5))
    end
    
    -- Test edge cases
    print("\n[TEST] Testing edge cases...")
    
    -- Test max value
    local maxFloat = audiosampletofloat(32767)
    local maxPassed = maxFloat >= 0.99 and maxFloat <= 1.0
    addTestResult("audiosampletofloat(32767) max", maxPassed, string.format("Got %.6f", maxFloat))
    
    -- Test min value
    local minFloat = audiosampletofloat(-32768)
    local minPassed = minFloat >= -1.0 and minFloat <= -0.99
    addTestResult("audiosampletofloat(-32768) min", minPassed, string.format("Got %.6f", minFloat))
    
    -- Test zero
    local zeroFloat = audiosampletofloat(0)
    local zeroPassed = math.abs(zeroFloat) < 0.001
    addTestResult("audiosampletofloat(0) zero", zeroPassed, string.format("Got %.6f", zeroFloat))
    
    -- Print summary
    print("\n========================================")
    print(string.format("TEST SUMMARY: %d/%d PASSED, %d FAILED", passedTests, totalTests, failedTests))
    if failedTests == 0 then
        print("*** ALL TESTS PASSED! ***")
    else
        print("*** SOME TESTS FAILED ***")
    end
    print("========================================")
end

function script()
    -- Run tests on first frame if not already run
    if not testsRun then
        testsRun = true
        print("[TEST] Running tests in script()...")
        
        -- Check function availability
        local functions = {
            "audiosampletofloat",
            "floattosample",
            "audiosampletouint8",
            "uint8tosample",
            "normalizeaudiosample",
            "monotostereo",
            "stereotomono"
        }
        
        for i = 1, #functions do
            local funcName = functions[i]
            local isAvailable = type(_G[funcName]) == "function"
            if isAvailable then
                print(string.format("  [OK] %s() is available", funcName))
                addTestResult(string.format("%s() available", funcName), true)
            else
                print(string.format("  [FAIL] %s() is NOT available", funcName))
                addTestResult(string.format("%s() available", funcName), false, "Function not found")
            end
        end
        
        -- Test basic conversions
        print("\n[TEST] Testing basic conversions...")
        
        -- Test audiosampletofloat
        local success, result = pcall(function()
            return audiosampletofloat(16384)  -- Half of max positive
        end)
        if success then
            local expected = 0.5
            local diff = math.abs(result - expected)
            local passed = diff < 0.01  -- Allow small floating point error
            print(string.format("  [%s] audiosampletofloat(16384) = %.6f (expected ~0.5, diff: %.6f)", 
                passed and "OK" or "FAIL", result, diff))
            addTestResult("audiosampletofloat(16384)", passed, string.format("Got %.6f, expected ~0.5", result))
        else
            print(string.format("  [FAIL] audiosampletofloat() error: %s", tostring(result)))
            addTestResult("audiosampletofloat(16384)", false, tostring(result))
        end
        
        -- Test floattosample
        local success2, result2 = pcall(function()
            return floattosample(0.5)
        end)
        if success2 then
            local expected = 16384
            local diff = math.abs(result2 - expected)
            local passed = diff < 100  -- Allow small rounding error
            print(string.format("  [%s] floattosample(0.5) = %d (expected ~16384, diff: %d)", 
                passed and "OK" or "FAIL", result2, diff))
            addTestResult("floattosample(0.5)", passed, string.format("Got %d, expected ~16384", result2))
        else
            print(string.format("  [FAIL] floattosample() error: %s", tostring(result2)))
            addTestResult("floattosample(0.5)", false, tostring(result2))
        end
        
        -- Test round-trip conversion
        local originalSample = 12345
        local floatVal = audiosampletofloat(originalSample)
        local convertedBack = floattosample(floatVal)
        local roundTripDiff = math.abs(originalSample - convertedBack)
        local roundTripPassed = roundTripDiff < 100  -- Allow small rounding error
        print(string.format("  [%s] Round-trip: %d -> %.6f -> %d (diff: %d)", 
            roundTripPassed and "OK" or "FAIL", originalSample, floatVal, convertedBack, roundTripDiff))
        addTestResult("Float round-trip conversion", roundTripPassed, 
            string.format("Diff: %d (should be < 100)", roundTripDiff))
        
        -- Test uint8 conversion
        local success3, result3 = pcall(function()
            return audiosampletouint8(0)  -- Zero should map to 128
        end)
        if success3 then
            local passed = result3 == 128
            print(string.format("  [%s] audiosampletouint8(0) = %d (expected 128)", 
                passed and "OK" or "FAIL", result3))
            addTestResult("audiosampletouint8(0)", passed, string.format("Got %d, expected 128", result3))
        else
            print(string.format("  [FAIL] audiosampletouint8() error: %s", tostring(result3)))
            addTestResult("audiosampletouint8(0)", false, tostring(result3))
        end
        
        -- Test uint8 round-trip
        local originalUint8 = 200
        local sampleFromUint8 = uint8tosample(originalUint8)
        local uint8Back = audiosampletouint8(sampleFromUint8)
        local uint8Diff = math.abs(originalUint8 - uint8Back)
        local uint8RoundTripPassed = uint8Diff <= 1  -- Allow 1 unit difference due to precision
        print(string.format("  [%s] Uint8 round-trip: %d -> %d -> %d (diff: %d)", 
            uint8RoundTripPassed and "OK" or "FAIL", originalUint8, sampleFromUint8, uint8Back, uint8Diff))
        addTestResult("Uint8 round-trip conversion", uint8RoundTripPassed, 
            string.format("Diff: %d (should be <= 1)", uint8Diff))
        
        -- Test monotostereo
        local success4, result4 = pcall(function()
            return monotostereo(1000)
        end)
        if success4 and result4 then
            local passed = result4.left == 1000 and result4.right == 1000
            print(string.format("  [%s] monotostereo(1000) = {left: %d, right: %d}", 
                passed and "OK" or "FAIL", result4.left, result4.right))
            addTestResult("monotostereo(1000)", passed, 
                string.format("Got {left: %d, right: %d}, expected both 1000", result4.left, result4.right))
        else
            print(string.format("  [FAIL] monotostereo() error: %s", tostring(result4)))
            addTestResult("monotostereo(1000)", false, tostring(result4))
        end
        
        -- Test stereotomono
        local success5, result5 = pcall(function()
            return stereotomono(1000, 2000)
        end)
        if success5 then
            local expected = 1500
            local passed = result5 == expected
            print(string.format("  [%s] stereotomono(1000, 2000) = %d (expected 1500)", 
                passed and "OK" or "FAIL", result5))
            addTestResult("stereotomono(1000, 2000)", passed, 
                string.format("Got %d, expected 1500", result5))
        else
            print(string.format("  [FAIL] stereotomono() error: %s", tostring(result5)))
            addTestResult("stereotomono(1000, 2000)", false, tostring(result5))
        end
        
        -- Test edge cases
        print("\n[TEST] Testing edge cases...")
        
        -- Test max value
        local maxFloat = audiosampletofloat(32767)
        local maxPassed = maxFloat >= 0.99 and maxFloat <= 1.0
        addTestResult("audiosampletofloat(32767) max", maxPassed, string.format("Got %.6f", maxFloat))
        
        -- Test min value
        local minFloat = audiosampletofloat(-32768)
        local minPassed = minFloat >= -1.0 and minFloat <= -0.99
        addTestResult("audiosampletofloat(-32768) min", minPassed, string.format("Got %.6f", minFloat))
        
        -- Test zero
        local zeroFloat = audiosampletofloat(0)
        local zeroPassed = math.abs(zeroFloat) < 0.001
        addTestResult("audiosampletofloat(0) zero", zeroPassed, string.format("Got %.6f", zeroFloat))
        
        -- Print summary
        print("\n========================================")
        print(string.format("TEST SUMMARY: %d/%d PASSED, %d FAILED", passedTests, totalTests, failedTests))
        if failedTests == 0 then
            print("*** ALL TESTS PASSED! ***")
        else
            print("*** SOME TESTS FAILED ***")
        end
        print("========================================")
    end
    
    if not getaudioenabled() then
        drawtext(4, 4, "Audio is disabled", 0x10)
        return
    end
    
    testFrameCount = testFrameCount + 1
    
    -- Get current audio sample
    local sample = getaudiosample()
    local leftSample = getaudiosampleleft()
    local rightSample = getaudiosampleright()
    
    -- Test conversions on real audio data
    if testFrameCount % logInterval == 0 then
        print(string.format("[TEST] Frame %d - Real-time conversions:", testFrameCount))
        
        -- Float conversion
        local floatVal = audiosampletofloat(sample)
        local sampleFromFloat = floattosample(floatVal)
        print(string.format("  Sample: %d -> Float: %.6f -> Sample: %d", 
            sample, floatVal, sampleFromFloat))
        
        -- Uint8 conversion
        local uint8Val = audiosampletouint8(sample)
        local sampleFromUint8 = uint8tosample(uint8Val)
        print(string.format("  Sample: %d -> Uint8: %d -> Sample: %d", 
            sample, uint8Val, sampleFromUint8))
        
        -- Normalize to different ranges
        local normalized8 = normalizeaudiosample(sample, 127)
        local normalized16 = normalizeaudiosample(sample, 32767)
        print(string.format("  Normalized to 8-bit range: %d, 16-bit range: %d", 
            normalized8, normalized16))
        
        -- Stereo conversions
        local stereo = monotostereo(sample)
        local mono = stereotomono(leftSample, rightSample)
        print(string.format("  Mono->Stereo: {left: %d, right: %d}, Stereo->Mono: %d", 
            stereo.left, stereo.right, mono))
    end
    
    -- Render GUI
    local y = 4
    
    -- Title
    drawtext(4, y, "Audio Format Conversion Test", 0x27)
    y = y + 12
    
    -- Overall test status (big and clear)
    local allPassed = false
    local statusColor = 0x29  -- Default yellow/gray
    local statusText = "Tests not run yet..."
    
    if totalTests > 0 then
        allPassed = failedTests == 0
        statusColor = allPassed and 0x27 or 0x10  -- Green if all pass, red if any fail
        statusText = allPassed and "*** ALL TESTS PASSED ***" or "*** SOME TESTS FAILED ***"
    end
    
    drawtext(4, y, statusText, statusColor)
    y = y + 12
    
    -- Test summary
    if totalTests > 0 then
        drawtext(4, y, string.format("Tests: %d/%d PASSED, %d FAILED", passedTests, totalTests, failedTests), 
            allPassed and 0x37 or 0x17)
    else
        drawtext(4, y, "Waiting for tests to run...", 0x29)
    end
    y = y + 15
    
    -- Test results list
    if #testResults > 0 then
        drawtext(4, y, "Test Results:", 0x27)
        y = y + 12
        
        -- Display first 10 test results (or all if less than 10)
        local maxDisplay = math.min(10, #testResults)
        for i = 1, maxDisplay do
            local test = testResults[i]
            local resultColor = test.passed and 0x37 or 0x10  -- Green for pass, red for fail
            local resultSymbol = test.passed and "[PASS]" or "[FAIL]"
            local displayText = string.format("%s %s", resultSymbol, test.name)
            
            -- Truncate if too long
            if string.len(displayText) > 35 then
                displayText = string.sub(displayText, 1, 32) .. "..."
            end
            
            drawtext(4, y, displayText, resultColor)
            y = y + 10
            
            -- Stop if we're running out of screen space
            if y > 220 then
                break
            end
        end
        
        if #testResults > maxDisplay then
            drawtext(4, y, string.format("... and %d more tests (check console)", #testResults - maxDisplay), 0x29)
            y = y + 10
        end
    else
        drawtext(4, y, "No test results yet. Check console.", 0x29)
        y = y + 10
    end
    
    y = y + 10
    
    -- Separator
    drawtext(4, y, "--- Live Audio Data ---", 0x29)
    y = y + 12
    
    -- Display original sample
    drawtext(4, y, string.format("Original Sample: %d", sample), 0x37)
    y = y + 10
    
    -- Float conversion
    local floatVal = audiosampletofloat(sample)
    local sampleFromFloat = floattosample(floatVal)
    drawtext(4, y, string.format("Float: %.4f -> Sample: %d", floatVal, sampleFromFloat), 0x37)
    y = y + 10
    
    -- Uint8 conversion
    local uint8Val = audiosampletouint8(sample)
    local sampleFromUint8 = uint8tosample(uint8Val)
    drawtext(4, y, string.format("Uint8: %d -> Sample: %d", uint8Val, sampleFromUint8), 0x37)
    y = y + 10
    
    -- Normalization
    local normalized8 = normalizeaudiosample(sample, 127)
    local normalized16 = normalizeaudiosample(sample, 32767)
    drawtext(4, y, string.format("Normalized (8-bit): %d, (16-bit): %d", normalized8, normalized16), 0x37)
    y = y + 10
    
    -- Stereo info
    drawtext(4, y, string.format("Stereo: L=%d, R=%d", leftSample, rightSample), 0x37)
    y = y + 10
    
    -- Mono->Stereo conversion
    local stereo = monotostereo(sample)
    drawtext(4, y, string.format("Mono->Stereo: L=%d, R=%d", stereo.left, stereo.right), 0x37)
    y = y + 10
    
    -- Stereo->Mono conversion
    local mono = stereotomono(leftSample, rightSample)
    drawtext(4, y, string.format("Stereo->Mono: %d", mono), 0x37)
    y = y + 10
    
    -- Instructions
    y = y + 5
    drawtext(4, y, "Check console for detailed logs", 0x29)
    y = y + 10
    drawtext(4, y, string.format("Frame: %d", testFrameCount), 0x37)
    
    -- Test edge cases display
    y = y + 15
    drawtext(4, y, "Edge Cases:", 0x27)
    y = y + 10
    
    -- Test maximum values
    local maxFloat = audiosampletofloat(32767)
    local maxUint8 = audiosampletouint8(32767)
    drawtext(4, y, string.format("Max (32767): Float=%.4f, Uint8=%d", maxFloat, maxUint8), 0x37)
    y = y + 10
    
    -- Test minimum values
    local minFloat = audiosampletofloat(-32768)
    local minUint8 = audiosampletouint8(-32768)
    drawtext(4, y, string.format("Min (-32768): Float=%.4f, Uint8=%d", minFloat, minUint8), 0x37)
    y = y + 10
    
    -- Test zero
    local zeroFloat = audiosampletofloat(0)
    local zeroUint8 = audiosampletouint8(0)
    drawtext(4, y, string.format("Zero (0): Float=%.4f, Uint8=%d", zeroFloat, zeroUint8), 0x37)
end

