-- Test script for gettextheight function
-- Demonstrates text height measurement (number of lines × 8 pixels)

-- Run tests once on script load
local function runTests()
  print("=== gettextheight Test Results ===")
  
  local passed = 0
  local failed = 0
  
  local function test(name, text, expectedHeight)
    local height = gettextheight(text)
    local pass = (height == expectedHeight)
    
    if pass then
      passed = passed + 1
      print(string.format('  [PASS] %s: "%s" = %d px', name, text:gsub("\n", "\\n"):gsub("\t", "\\t"), height))
    else
      failed = failed + 1
      print(string.format('  [FAIL] %s: "%s" = %d px (expected %d px)', name, text:gsub("\n", "\\n"):gsub("\t", "\\t"), height, expectedHeight))
    end
  end
  
  -- Test single-line text
  print("\n--- Single-line text ---")
  test("Single line", "Hello", 8)  -- 1 line = 8 pixels
  test("Single char", "A", 8)      -- 1 line = 8 pixels
  test("Empty string", "", 0)      -- 0 lines = 0 pixels
  
  -- Test multi-line text
  print("\n--- Multi-line text ---")
  test("Two lines", "Line 1\nLine 2", 16)  -- 2 lines = 16 pixels
  test("Three lines", "Line 1\nLine 2\nLine 3", 24)  -- 3 lines = 24 pixels
  test("Four lines", "A\nB\nC\nD", 32)  -- 4 lines = 32 pixels
  
  -- Test trailing newline (should add extra line)
  print("\n--- Trailing newline ---")
  test("Trailing \\n", "Hello\n", 16)  -- 2 lines (trailing \n counts)
  test("Two trailing \\n", "Hello\n\n", 24)  -- 3 lines
  test("Empty line", "\n", 16)  -- 2 lines (empty line)
  
  -- Test mixed cases
  print("\n--- Mixed cases ---")
  test("Single line with tab", "Tab\tTest", 8)  -- Tabs don't affect height
  test("Multi-line with tabs", "Line 1\nTab\tLine 2", 16)  -- 2 lines
  test("Only newlines", "\n\n\n", 32)  -- 4 lines = 32 pixels
  
  -- Test edge cases
  print("\n--- Edge cases ---")
  test("Newline at start", "\nHello", 16)  -- 2 lines
  test("Newline in middle", "A\nB", 16)  -- 2 lines
  test("Many lines", "1\n2\n3\n4\n5\n6\n7\n8", 64)  -- 8 lines = 64 pixels
  
  -- Test consistency
  print("\n--- Consistency checks ---")
  local text1 = "Line 1\nLine 2"
  local height1 = gettextheight(text1)
  local height2 = gettextheight(text1)
  if height1 == height2 then
    passed = passed + 1
    print(string.format('  [PASS] Consistency: Same text returns same height (%d px)', height1))
  else
    failed = failed + 1
    print(string.format('  [FAIL] Consistency: Same text returned different heights (%d vs %d)', height1, height2))
  end
  
  -- Test that height matches expected (8 pixels per line)
  local testText = "A\nB\nC"
  local testHeight = gettextheight(testText)
  local expectedLines = 3
  local expectedHeight = expectedLines * 8
  if testHeight == expectedHeight then
    passed = passed + 1
    print(string.format('  [PASS] Line calculation: %d lines = %d px (8 px per line)', expectedLines, testHeight))
  else
    failed = failed + 1
    print(string.format('  [FAIL] Line calculation: Expected %d px (%d lines × 8), got %d px', expectedHeight, expectedLines, testHeight))
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
  drawtext(4, 4, "gettextheight Test", 0x3F)
  drawtext(4, 16, "Check console/log", 0x2E)
  drawtext(4, 28, "for test results", 0x2E)
  
  -- Show examples on screen
  local example1 = "Single line"
  local height1 = gettextheight(example1)
  drawtext(4, 50, string.format('"%s" = %d px', example1, height1), 0x20)
  
  local example2 = "Line 1\nLine 2"
  local height2 = gettextheight(example2)
  drawtext(4, 70, string.format('2 lines = %d px', height2), 0x39)
  
  local example3 = "A\nB\nC"
  local height3 = gettextheight(example3)
  drawtext(4, 90, string.format('3 lines = %d px', height3), 0x2A)
end

