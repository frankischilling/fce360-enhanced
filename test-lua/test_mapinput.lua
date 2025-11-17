-- Test script for mapinput function
-- Tests per-script input remapping (virtual button names to physical inputs)

-- Run tests once on script load
local function runTests()
  print("=== mapinput Test Results ===")
  
  local passed = 0
  local failed = 0
  
  local function test(name, virtualBtn, physicalSpec, shouldPass)
    local success, result = pcall(function()
      mapinput(virtualBtn, physicalSpec)
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
        print(string.format('  [PASS] %s: mapinput("%s", "%s") - executed successfully', name, virtualBtn, physicalSpec))
      else
        print(string.format('  [PASS] %s: mapinput("%s", "%s") correctly rejected', name, virtualBtn, physicalSpec))
      end
    else
      failed = failed + 1
      local expected = shouldPass and "should execute" or "should reject"
      local actual = success and "executed" or "rejected"
      print(string.format('  [FAIL] %s: mapinput("%s", "%s") %s (expected %s)', name, virtualBtn, physicalSpec, actual, expected))
    end
  end
  
  -- Test valid NES button mappings
  print("\n--- Valid NES button mappings ---")
  test("JUMP to A", "JUMP", "A", true)
  test("ATTACK to B", "ATTACK", "B", true)
  test("PAUSE to START", "PAUSE", "START", true)
  test("MENU to SELECT", "MENU", "SELECT", true)
  test("MOVE_UP to UP", "MOVE_UP", "UP", true)
  test("MOVE_DOWN to DOWN", "MOVE_DOWN", "DOWN", true)
  test("MOVE_LEFT to LEFT", "MOVE_LEFT", "LEFT", true)
  test("MOVE_RIGHT to RIGHT", "MOVE_RIGHT", "RIGHT", true)
  
  -- Test valid Xbox button mappings
  print("\n--- Valid Xbox button mappings ---")
  test("FIRE to X", "FIRE", "X", true)
  test("GRAB to Y", "GRAB", "Y", true)
  test("SHOULDER_L to LEFT_SHOULDER", "SHOULDER_L", "LEFT_SHOULDER", true)
  test("SHOULDER_R to RIGHT_SHOULDER", "SHOULDER_R", "RIGHT_SHOULDER", true)
  test("STICK_L to LEFT_THUMB", "STICK_L", "LEFT_THUMB", true)
  test("STICK_R to RIGHT_THUMB", "STICK_R", "RIGHT_THUMB", true)
  test("BACK_BTN to BACK", "BACK_BTN", "BACK", true)
  
  -- Test case-insensitive
  print("\n--- Case-insensitive mappings ---")
  test("Lowercase virtual", "jump", "A", true)
  test("Mixed case virtual", "Attack", "B", true)
  test("Lowercase physical", "FIRE", "a", true)
  test("Mixed case physical", "GRAB", "X", true)
  
  -- Test invalid mappings
  print("\n--- Invalid mappings ---")
  test("Empty virtual button", "", "A", false)
  test("Empty physical spec", "JUMP", "", false)
  test("Invalid physical spec", "JUMP", "INVALID", false)
  test("Invalid physical spec 2", "ATTACK", "Z", false)
  test("Invalid physical spec 3", "FIRE", "BUTTON_X", false)
  
  -- Test that mappings can be overwritten
  print("\n--- Mapping overwrite ---")
  local success1 = pcall(function() mapinput("JUMP", "A") end)
  local success2 = pcall(function() mapinput("JUMP", "B") end)  -- Overwrite
  local success3 = pcall(function() mapinput("JUMP", "X") end)  -- Overwrite again
  if success1 and success2 and success3 then
    passed = passed + 1
    print('  [PASS] Mapping overwrite: All overwrites executed successfully')
  else
    failed = failed + 1
    print('  [FAIL] Mapping overwrite: Some overwrites failed')
  end
  
  -- Test that function returns nothing (void)
  print("\n--- Return value check ---")
  local success, result = pcall(function()
    local ret = mapinput("TEST", "A")
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
  print("Setting up test mappings:")
  print("  JUMP -> A button")
  print("  ATTACK -> B button")
  print("  FIRE -> X button (Xbox)")
  print("  GRAB -> Y button (Xbox)")
  print("")
  print("Press the mapped buttons and check if virtual names work with isbuttonpressed()")
end

-- Run tests when script loads
runTests()

-- Set up test mappings
mapinput("JUMP", "A")
mapinput("ATTACK", "B")
mapinput("FIRE", "X")
mapinput("GRAB", "Y")
mapinput("PAUSE", "START")
mapinput("MENU", "SELECT")

local frameCount = 0
local lastJump = false
local lastAttack = false
local lastFire = false
local lastGrab = false

function gui()
  frameCount = frameCount + 1
  
  -- Test virtual button mappings with isbuttonpressed
  local jumpPressed = isbuttonpressed(0, "JUMP")
  local attackPressed = isbuttonpressed(0, "ATTACK")
  local firePressed = isbuttonpressed(0, "FIRE")
  local grabPressed = isbuttonpressed(0, "GRAB")
  local pausePressed = isbuttonpressed(0, "PAUSE")
  local menuPressed = isbuttonpressed(0, "MENU")
  
  -- Also check physical buttons for comparison
  local aPressed = isbuttonpressed(0, "A")
  local bPressed = isbuttonpressed(0, "B")
  local xPressed = isxboxbuttonpressed(0, "X")
  local yPressed = isxboxbuttonpressed(0, "Y")
  local startPressed = isbuttonpressed(0, "START")
  local selectPressed = isbuttonpressed(0, "SELECT")
  
  -- Edge detection for visual feedback
  if jumpPressed and not lastJump then
    print("JUMP (virtual) pressed! (mapped to A)")
  end
  if attackPressed and not lastAttack then
    print("ATTACK (virtual) pressed! (mapped to B)")
  end
  if firePressed and not lastFire then
    print("FIRE (virtual) pressed! (mapped to X)")
  end
  if grabPressed and not lastGrab then
    print("GRAB (virtual) pressed! (mapped to Y)")
  end
  
  lastJump = jumpPressed
  lastAttack = attackPressed
  lastFire = firePressed
  lastGrab = grabPressed
  
  -- Display test info on screen
  drawtext(4, 4, "mapinput Test", 0x20)
  drawtext(4, 16, "Check console/log", 0x2D)
  drawtext(4, 28, "for test results", 0x2D)
  
  -- Display mappings
  drawtext(4, 50, "Mappings:", 0x20)
  drawtext(4, 62, "  JUMP -> A", 0x2D)
  drawtext(4, 74, "  ATTACK -> B", 0x2D)
  drawtext(4, 86, "  FIRE -> X (Xbox)", 0x2D)
  drawtext(4, 98, "  GRAB -> Y (Xbox)", 0x2D)
  drawtext(4, 110, "  PAUSE -> START", 0x2D)
  drawtext(4, 122, "  MENU -> SELECT", 0x2D)
  
  -- Display virtual button states
  local y = 140
  drawtext(4, y, "Virtual Buttons:", 0x20)
  y = y + 12
  
  local virtualColors = {
    jumpPressed and 0x29 or 0x2D,
    attackPressed and 0x29 or 0x2D,
    firePressed and 0x29 or 0x2D,
    grabPressed and 0x29 or 0x2D,
    pausePressed and 0x29 or 0x2D,
    menuPressed and 0x29 or 0x2D
  }
  
  drawtext(4, y, string.format("  JUMP: %s", jumpPressed and "PRESSED" or "released"), virtualColors[1])
  y = y + 10
  drawtext(4, y, string.format("  ATTACK: %s", attackPressed and "PRESSED" or "released"), virtualColors[2])
  y = y + 10
  drawtext(4, y, string.format("  FIRE: %s", firePressed and "PRESSED" or "released"), virtualColors[3])
  y = y + 10
  drawtext(4, y, string.format("  GRAB: %s", grabPressed and "PRESSED" or "released"), virtualColors[4])
  y = y + 10
  drawtext(4, y, string.format("  PAUSE: %s", pausePressed and "PRESSED" or "released"), virtualColors[5])
  y = y + 10
  drawtext(4, y, string.format("  MENU: %s", menuPressed and "PRESSED" or "released"), virtualColors[6])
  
  -- Display physical button states for comparison
  y = y + 12
  drawtext(4, y, "Physical Buttons (for comparison):", 0x20)
  y = y + 12
  
  local physicalColors = {
    aPressed and 0x29 or 0x2D,
    bPressed and 0x29 or 0x2D,
    xPressed and 0x29 or 0x2D,
    yPressed and 0x29 or 0x2D,
    startPressed and 0x29 or 0x2D,
    selectPressed and 0x29 or 0x2D
  }
  
  drawtext(4, y, string.format("  A: %s", aPressed and "PRESSED" or "released"), physicalColors[1])
  y = y + 10
  drawtext(4, y, string.format("  B: %s", bPressed and "PRESSED" or "released"), physicalColors[2])
  y = y + 10
  drawtext(4, y, string.format("  X (Xbox): %s", xPressed and "PRESSED" or "released"), physicalColors[3])
  y = y + 10
  drawtext(4, y, string.format("  Y (Xbox): %s", yPressed and "PRESSED" or "released"), physicalColors[4])
  y = y + 10
  drawtext(4, y, string.format("  START: %s", startPressed and "PRESSED" or "released"), physicalColors[5])
  y = y + 10
  drawtext(4, y, string.format("  SELECT: %s", selectPressed and "PRESSED" or "released"), physicalColors[6])
  
  -- Verification: Check that virtual buttons match their physical mappings
  y = y + 12
  drawtext(4, y, "Verification:", 0x20)
  y = y + 12
  
  local verify1 = (jumpPressed == aPressed) and 0x29 or 0x37
  local verify2 = (attackPressed == bPressed) and 0x29 or 0x37
  local verify3 = (firePressed == xPressed) and 0x29 or 0x37
  local verify4 = (grabPressed == yPressed) and 0x29 or 0x37
  local verify5 = (pausePressed == startPressed) and 0x29 or 0x37
  local verify6 = (menuPressed == selectPressed) and 0x29 or 0x37
  
  drawtext(4, y, string.format("  JUMP==A: %s", (jumpPressed == aPressed) and "OK" or "MISMATCH"), verify1)
  y = y + 10
  drawtext(4, y, string.format("  ATTACK==B: %s", (attackPressed == bPressed) and "OK" or "MISMATCH"), verify2)
  y = y + 10
  drawtext(4, y, string.format("  FIRE==X: %s", (firePressed == xPressed) and "OK" or "MISMATCH"), verify3)
  y = y + 10
  drawtext(4, y, string.format("  GRAB==Y: %s", (grabPressed == yPressed) and "OK" or "MISMATCH"), verify4)
  y = y + 10
  drawtext(4, y, string.format("  PAUSE==START: %s", (pausePressed == startPressed) and "OK" or "MISMATCH"), verify5)
  y = y + 10
  drawtext(4, y, string.format("  MENU==SELECT: %s", (menuPressed == selectPressed) and "OK" or "MISMATCH"), verify6)
  
  -- Show frame counter
  drawtext(4, 230, string.format("Frame: %d", frameCount), 0x2D)
  
  -- Show example usage
  if jumpPressed then
    drawtext(150, 50, "JUMP!", 0x29)
  end
  if attackPressed then
    drawtext(150, 60, "ATTACK!", 0x29)
  end
  if firePressed then
    drawtext(150, 70, "FIRE!", 0x29)
  end
  if grabPressed then
    drawtext(150, 80, "GRAB!", 0x29)
  end
end

