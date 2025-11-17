-- Test script for setrumble function
-- Tests controller haptic feedback (rumble/vibration)
--
-- Note: This test requires a connected Xbox 360 controller to feel the rumble effect

-- Run tests once on script load
local function runTests()
  print("=== setrumble Test Results ===")
  
  local passed = 0
  local failed = 0
  
  local function test(name, ms, intensity, shouldPass)
    local success, result = pcall(function()
      setrumble(ms, intensity)
      return true
    end)
    
    local pass = false
    if shouldPass then
      pass = success
    else
      pass = not success  -- Should fail
    end
    
    if pass then
      passed = passed + 1
      if shouldPass then
        print(string.format('  [PASS] %s: setrumble(%d, %.2f) - executed successfully', name, ms, intensity))
      else
        print(string.format('  [PASS] %s: setrumble(%d, %.2f) correctly rejected', name, ms, intensity))
      end
    else
      failed = failed + 1
      local expected = shouldPass and "should execute" or "should reject"
      local actual = success and "executed" or "rejected"
      print(string.format('  [FAIL] %s: setrumble(%d, %.2f) %s (expected %s)', name, ms, intensity, actual, expected))
    end
  end
  
  -- Test valid parameters
  print("\n--- Valid parameters ---")
  test("Short duration, low intensity", 100, 0.1, true)
  test("Medium duration, medium intensity", 500, 0.5, true)
  test("Long duration, high intensity", 1000, 1.0, true)
  test("Zero duration (should be valid)", 0, 0.5, true)
  test("Minimum intensity", 200, 0.0, true)
  test("Maximum intensity", 200, 1.0, true)
  test("Fractional intensity", 300, 0.25, true)
  test("Fractional intensity", 300, 0.75, true)
  
  -- Test invalid parameters (should reject or clamp)
  print("\n--- Invalid parameters ---")
  test("Negative duration", -100, 0.5, false)
  test("Negative intensity (should clamp to 0.0)", -0.5, 0.5, true)  -- May clamp
  test("Intensity > 1.0 (should clamp to 1.0)", 200, 1.5, true)  -- May clamp
  test("Intensity > 1.0 (should clamp to 1.0)", 200, 2.0, true)  -- May clamp
  test("Very large intensity (should clamp)", 200, 10.0, true)  -- May clamp
  
  -- Test edge cases
  print("\n--- Edge cases ---")
  test("Very short duration", 1, 0.5, true)
  test("Very long duration", 5000, 0.5, true)
  test("Exact 0.5 intensity", 250, 0.5, true)
  test("Very small intensity", 200, 0.01, true)
  test("Very small intensity", 200, 0.99, true)
  
  -- Test that function can be called multiple times
  print("\n--- Multiple calls ---")
  local success1 = pcall(function() setrumble(100, 0.3) end)
  local success2 = pcall(function() setrumble(200, 0.6) end)
  local success3 = pcall(function() setrumble(150, 0.9) end)
  if success1 and success2 and success3 then
    passed = passed + 1
    print('  [PASS] Multiple calls: All executed successfully')
  else
    failed = failed + 1
    print('  [FAIL] Multiple calls: Some calls failed')
  end
  
  -- Test that function returns nothing (void)
  print("\n--- Return value check ---")
  local success, result = pcall(function()
    local ret = setrumble(100, 0.5)
    return ret
  end)
  if success then
    if result == nil then
      passed = passed + 1
      print('  [PASS] Return value: Returns nothing (nil) as expected')
    else
      failed = failed + 1
      print(string.format('  [FAIL] Return value: Expected nil, got %s', tostring(result)))
    end
  else
    failed = failed + 1
    print('  [FAIL] Return value: Function call failed')
  end
  
  -- Summary
  print("\n=== Test Summary ===")
  print(string.format("Passed: %d", passed))
  print(string.format("Failed: %d", failed))
  print(string.format("Total: %d", passed + failed))
  
  if failed == 0 then
    print("\n[ALL TESTS PASSED]")
  else
    print(string.format("\n[%d TEST(S) FAILED]", failed))
  end
  
  print("\n=== Interactive Test ===")
  print("Press A button to trigger short rumble (100ms, 0.5 intensity)")
  print("Press B button to trigger medium rumble (500ms, 0.7 intensity)")
  print("Press START button to trigger long rumble (1000ms, 1.0 intensity)")
  print("Press SELECT button to trigger very light rumble (200ms, 0.1 intensity)")
end

-- Run tests when script loads
runTests()

local frameCount = 0
local lastRumbleTime = 0
local rumbleActive = false
local rumbleStartTime = 0
local rumbleDuration = 0
local rumbleIntensity = 0.0

-- Track button states for edge detection
local lastA = false
local lastB = false
local lastStart = false
local lastSelect = false

function gui()
  frameCount = frameCount + 1
  
  -- Get current button states
  local buttons = getjoypad(0)
  local aPressed = (math.floor(buttons / 0x01) % 2 == 1)
  local bPressed = (math.floor(buttons / 0x02) % 2 == 1)
  local startPressed = (math.floor(buttons / 0x08) % 2 == 1)
  local selectPressed = (math.floor(buttons / 0x04) % 2 == 1)
  
  -- Edge detection: trigger rumble on button press (rising edge)
  if aPressed and not lastA then
    setrumble(100, 0.5)
    rumbleStartTime = gettime()
    rumbleDuration = 100
    rumbleIntensity = 0.5
    rumbleActive = true
    print("Rumble triggered: 100ms, 0.5 intensity (A button)")
  elseif bPressed and not lastB then
    setrumble(500, 0.7)
    rumbleStartTime = gettime()
    rumbleDuration = 500
    rumbleIntensity = 0.7
    rumbleActive = true
    print("Rumble triggered: 500ms, 0.7 intensity (B button)")
  elseif startPressed and not lastStart then
    setrumble(1000, 1.0)
    rumbleStartTime = gettime()
    rumbleDuration = 1000
    rumbleIntensity = 1.0
    rumbleActive = true
    print("Rumble triggered: 1000ms, 1.0 intensity (START button)")
  elseif selectPressed and not lastSelect then
    setrumble(200, 0.1)
    rumbleStartTime = gettime()
    rumbleDuration = 200
    rumbleIntensity = 0.1
    rumbleActive = true
    print("Rumble triggered: 200ms, 0.1 intensity (SELECT button)")
  end
  
  -- Update last button states
  lastA = aPressed
  lastB = bPressed
  lastStart = startPressed
  lastSelect = selectPressed
  
  -- Check if rumble is still active
  local currentTime = gettime()
  if rumbleActive then
    local elapsed = currentTime - rumbleStartTime
    if elapsed >= rumbleDuration then
      rumbleActive = false
    end
  end
  
  -- Display test info on screen
  drawtext(4, 4, "setrumble Test", 0x20)
  drawtext(4, 16, "Check console/log", 0x2D)
  drawtext(4, 28, "for test results", 0x2D)
  
  -- Display interactive controls
  drawtext(4, 50, "Controls:", 0x20)
  drawtext(4, 62, "  A: Short rumble (100ms, 0.5)", 0x2D)
  drawtext(4, 74, "  B: Medium rumble (500ms, 0.7)", 0x2D)
  drawtext(4, 86, "  START: Long rumble (1000ms, 1.0)", 0x2D)
  drawtext(4, 98, "  SELECT: Light rumble (200ms, 0.1)", 0x2D)
  
  -- Display rumble status
  local y = 120
  if rumbleActive then
    local elapsed = currentTime - rumbleStartTime
    local remaining = math.max(0, rumbleDuration - elapsed)
    local progress = math.min(1.0, elapsed / rumbleDuration)
    
    drawtext(4, y, "Rumble Status: ACTIVE", 0x29)
    y = y + 12
    drawtext(4, y, string.format("  Duration: %d ms", rumbleDuration), 0x2D)
    y = y + 10
    drawtext(4, y, string.format("  Intensity: %.2f", rumbleIntensity), 0x2D)
    y = y + 10
    drawtext(4, y, string.format("  Elapsed: %d ms", elapsed), 0x2D)
    y = y + 10
    drawtext(4, y, string.format("  Remaining: %d ms", remaining), 0x2D)
    y = y + 10
    
    -- Visual progress bar
    local barWidth = 100
    local barHeight = 8
    local barX = 4
    local barY = y
    local filledWidth = math.floor(barWidth * progress)
    
    -- Draw background
    fillrect(barX, barY, barWidth, barHeight, 0x2D)
    -- Draw filled portion
    if filledWidth > 0 then
      fillrect(barX, barY, filledWidth, barHeight, 0x29)
    end
    y = y + 12
  else
    drawtext(4, y, "Rumble Status: INACTIVE", 0x2D)
    y = y + 12
  end
  
  -- Display current button states
  y = y + 10
  drawtext(4, y, "Button States:", 0x20)
  y = y + 12
  
  local buttonColors = {
    aPressed and 0x29 or 0x2D,
    bPressed and 0x29 or 0x2D,
    startPressed and 0x29 or 0x2D,
    selectPressed and 0x29 or 0x2D
  }
  
  drawtext(4, y, string.format("  A: %s", aPressed and "PRESSED" or "released"), buttonColors[1])
  y = y + 10
  drawtext(4, y, string.format("  B: %s", bPressed and "PRESSED" or "released"), buttonColors[2])
  y = y + 10
  drawtext(4, y, string.format("  START: %s", startPressed and "PRESSED" or "released"), buttonColors[3])
  y = y + 10
  drawtext(4, y, string.format("  SELECT: %s", selectPressed and "PRESSED" or "released"), buttonColors[4])
  
  -- Show frame counter
  drawtext(4, 230, string.format("Frame: %d", frameCount), 0x2D)
  
  -- Show time
  local time = gettime()
  drawtext(150, 230, string.format("Time: %d ms", time), 0x2D)
end

