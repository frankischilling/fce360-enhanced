function gui()
  -- Test rounded rectangles with different radii
  
  -- Small radius (subtle rounding)
  drawroundrect(10, 10, 60, 40, 5, 0x20)   -- White outline, radius 5
  
  -- Medium radius (moderate rounding)
  drawroundrect(80, 10, 60, 40, 10, 0x26)  -- Orange outline, radius 10
  
  -- Large radius (strong rounding)
  drawroundrect(150, 10, 60, 40, 15, 0x29) -- Green outline, radius 15
  
  -- Very large radius (almost pill-shaped)
  drawroundrect(10, 60, 100, 30, 15, 0x37)  -- Yellow outline, radius 15
  
  -- Square with small rounding
  drawroundrect(120, 60, 50, 50, 8, 0x16)   -- Red outline, radius 8
  
  -- Wide rectangle with medium rounding
  drawroundrect(10, 120, 180, 40, 12, 0x1C) -- Cyan outline, radius 12
  
  -- Tall rectangle with small rounding
  drawroundrect(200, 10, 40, 100, 8, 0x23)  -- Light purple outline, radius 8
  
  -- Radius 0 (should draw as regular rectangle)
  drawroundrect(10, 170, 80, 30, 0, 0x2B)   -- Gray outline, radius 0
  
  -- Very small rectangle with radius
  drawroundrect(100, 170, 30, 20, 5, 0x21)  -- Light blue outline, radius 5
  
  -- Multiple sizes with same radius
  drawroundrect(140, 170, 50, 25, 8, 0x24)  -- Pink outline
  drawroundrect(200, 170, 40, 35, 8, 0x28)  -- Yellow outline
end

