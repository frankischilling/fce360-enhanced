-- Test script for drawarc function

function gui()
    local centerX = 128
    local centerY = 120
    local radius = 30
    
    -- Draw arcs in different quadrants
    -- Top-right quadrant (0° to 90°)
    drawarc(centerX - 50, centerY - 50, radius, 0, 90, 0x20)   -- White arc
    
    -- Top-left quadrant (90° to 180°)
    drawarc(centerX + 50, centerY - 50, radius, 90, 180, 0x26)  -- Coral red arc
    
    -- Bottom-left quadrant (180° to 270°)
    drawarc(centerX + 50, centerY + 50, radius, 180, 270, 0x29) -- Green arc
    
    -- Bottom-right quadrant (270° to 360°)
    drawarc(centerX - 50, centerY + 50, radius, 270, 360, 0x37)  -- Yellow arc
    
    -- Half circles
    drawarc(centerX, centerY - 60, radius, 180, 0, 0x16)  -- Top half circle (crosses 0°)
    drawarc(centerX, centerY + 60, radius, 0, 180, 0x1C)  -- Bottom half circle
    
    -- Progress indicator style arcs (0° to 270° = 75% of circle)
    drawarc(centerX, centerY, 40, 0, 270, 0x2E)  -- Large yellow/green arc
    
    -- Small arcs at center
    drawarc(centerX, centerY, 20, 45, 135, 0x20)  -- Small arc (diagonal)
    drawarc(centerX, centerY, 15, 225, 315, 0x26) -- Small arc (opposite diagonal)
end

