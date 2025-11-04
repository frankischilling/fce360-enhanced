function gui()
  -- Test filled arcs (pie slices) with different angles
  
  -- Four quadrant pie slices
  fillarc(64, 60, 40, 0, 90, 0x20)    -- Top-right quadrant (white)
  fillarc(192, 60, 40, 90, 180, 0x26)  -- Top-left quadrant (orange)
  fillarc(64, 180, 40, 180, 270, 0x29) -- Bottom-left quadrant (green)
  fillarc(192, 180, 40, 270, 360, 0x37) -- Bottom-right quadrant (yellow)
  
  -- Half circle pie slices
  fillarc(128, 40, 35, 0, 180, 0x16)     -- Top half (red)
  fillarc(128, 200, 35, 180, 360, 0x1C)  -- Bottom half (cyan)
  
  -- Progress indicators (different percentages)
  fillarc(50, 120, 30, 0, 90, 0x2E)     -- 25% progress (black)
  fillarc(206, 120, 30, 0, 180, 0x21)    -- 50% progress (light blue)
  fillarc(128, 120, 30, 0, 270, 0x28)    -- 75% progress (yellow)
  
  -- Small diagonal pie slices
  fillarc(32, 220, 15, 45, 135, 0x23)   -- Small diagonal slice (light purple)
  fillarc(224, 220, 15, 225, 315, 0x24)  -- Small opposite diagonal (pink)
  
  -- Full circle (should fill entire circle when angles span 360)
  fillarc(128, 120, 25, 0, 360, 0x2B)   -- Full circle (gray/blue)
end

