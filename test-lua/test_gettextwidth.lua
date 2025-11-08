-- Test script for gettextwidth function
-- Demonstrates text width measurement using the same font metrics as drawing functions

-- Run tests once on script load
local function runTests()
  print("=== gettextwidth Test Results ===")
  
  local passed = 0
  local failed = 0
  
  local function test(name, text, expectedMin, expectedMax)
    local width = gettextwidth(text)
    local pass = false
    
    if expectedMin and expectedMax then
      pass = (width >= expectedMin and width <= expectedMax)
    elseif expectedMin then
      pass = (width == expectedMin)
    else
      -- Just check it's non-negative and reasonable
      pass = (width >= 0 and width < 1000)
    end
    
    if pass then
      passed = passed + 1
      print(string.format('  [PASS] %s: "%s" = %d px', name, text:gsub("\n", "\\n"):gsub("\t", "\\t"), width))
    else
      failed = failed + 1
      local expected = expectedMin and (expectedMax and string.format("%d-%d", expectedMin, expectedMax) or tostring(expectedMin)) or "valid"
      print(string.format('  [FAIL] %s: "%s" = %d px (expected %s)', name, text:gsub("\n", "\\n"):gsub("\t", "\\t"), width, expected))
    end
  end
  
  -- Test various strings
  print("\n--- Single-line strings ---")
  test("Single char", "A", 3, 6)  -- Most chars are 3-6 pixels wide
  test("Short text", "Hi", 6, 12)
  test("Normal text", "Hello", 20, 40)
  test("Longer text", "Wide Text", 40, 80)
  test("Numbers", "1234567890", 30, 60)
  test("Special chars", "!@#$%", 15, 30)
  
  -- Test multi-line text (should return longest line)
  print("\n--- Multi-line text ---")
  -- Calculate expected width by measuring the longest line directly
  local longestLine = "Line 3 is longer"
  local expectedMultiWidth = gettextwidth(longestLine)
  test("Multi-line", "Line 1\nLine 2\nLine 3 is longer", expectedMultiWidth, expectedMultiWidth)  -- Should match longest line width
  
  -- Test with tabs (tab = 4 spaces)
  print("\n--- Tab handling ---")
  local tabText = "Tab\tTest"
  local tabWidth = gettextwidth(tabText)
  local spaceWidth = gettextwidth(" ")
  local expectedTabWidth = gettextwidth("Tab") + (spaceWidth * 4) + gettextwidth("Test")
  test("Tab expansion", tabText, expectedTabWidth - 2, expectedTabWidth + 2)  -- Allow small variance
  
  -- Test empty string
  print("\n--- Edge cases ---")
  test("Empty string", "", 0, 0)
  
  -- Test that width is consistent
  print("\n--- Consistency checks ---")
  local text1 = "Test"
  local width1 = gettextwidth(text1)
  local width2 = gettextwidth(text1)
  if width1 == width2 then
    passed = passed + 1
    print(string.format('  [PASS] Consistency: Same text returns same width (%d px)', width1))
  else
    failed = failed + 1
    print(string.format('  [FAIL] Consistency: Same text returned different widths (%d vs %d)', width1, width2))
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
end

-- Run tests when script loads
runTests()

function gui()
  -- Display test info on screen
  drawtext(4, 4, "gettextwidth Test", 0x3F)
  drawtext(4, 16, "Check console/log", 0x2E)
  drawtext(4, 28, "for test results", 0x2E)
  
  -- Show a simple example on screen
  local exampleText = "Example"
  local exampleWidth = gettextwidth(exampleText)
  drawtext(4, 50, string.format('"%s" = %d px', exampleText, exampleWidth), 0x20)
end

