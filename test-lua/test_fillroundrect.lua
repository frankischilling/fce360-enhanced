function gui()
  -- Test filled rounded rectangles with different radii
  
  -- Small radius (subtle rounding)
  fillroundrect(10, 10, 60, 40, 5, 0x20)   -- White fill, radius 5
  
  -- Medium radius (moderate rounding)
  fillroundrect(80, 10, 60, 40, 10, 0x26)  -- Orange fill, radius 10
  
  -- Large radius (strong rounding)
  fillroundrect(150, 10, 60, 40, 15, 0x29) -- Green fill, radius 15
  
  -- Very large radius (almost pill-shaped)
  fillroundrect(10, 60, 100, 30, 15, 0x37)  -- Yellow fill, radius 15
  
  -- Square with small rounding
  fillroundrect(120, 60, 50, 50, 8, 0x16)   -- Red fill, radius 8
  
  -- Wide rectangle with medium rounding
  fillroundrect(10, 120, 180, 40, 12, 0x1C) -- Cyan fill, radius 12
  
  -- Tall rectangle with small rounding
  fillroundrect(200, 10, 40, 100, 8, 0x23)  -- Light purple fill, radius 8
  
  -- Radius 0 (should fill as regular rectangle, same as fillrect)
  fillroundrect(10, 170, 80, 30, 0, 0x2B)   -- Gray fill, radius 0
  
  -- Very small rectangle with radius
  fillroundrect(100, 170, 30, 20, 5, 0x21)  -- Light blue fill, radius 5
  
  -- Multiple sizes with same radius
  fillroundrect(140, 170, 50, 25, 8, 0x24)  -- Pink fill
  fillroundrect(200, 170, 40, 35, 8, 0x28)  -- Yellow fill
end

