-- Test script for drawellipse and fillellipse functions

function gui()
    local centerX = 128
    local centerY = 120
    local spacing = 45
    
    -- Row 1: Draw ellipses (outlines) - Horizontal ellipses (wide ovals)
    drawellipse(30, 30, 35, 18, 0x20)   -- Wide white ellipse
    drawellipse(30 + spacing, 30, 30, 15, 0x26)  -- Wide coral red ellipse
    
    -- Row 1: Fill ellipses (filled) - Horizontal ellipses
    fillellipse(30 + spacing * 2, 30, 35, 18, 0x20)   -- Filled wide white ellipse
    fillellipse(30 + spacing * 3, 30, 30, 15, 0x26)  -- Filled wide coral red ellipse
    
    -- Row 2: Draw ellipses - Vertical ellipses (tall ovals)
    drawellipse(30, 80, 18, 35, 0x29)   -- Tall green ellipse
    drawellipse(30 + spacing, 80, 15, 30, 0x37)  -- Tall yellow ellipse
    
    -- Row 2: Fill ellipses - Vertical ellipses
    fillellipse(30 + spacing * 2, 80, 18, 35, 0x29)   -- Filled tall green ellipse
    fillellipse(30 + spacing * 3, 80, 15, 30, 0x37)  -- Filled tall yellow ellipse
    
    -- Row 3: Circle (rx == ry) - both draw and fill
    drawellipse(centerX - 40, centerY, 25, 25, 0x2E)  -- Circle outline
    fillellipse(centerX + 40, centerY, 25, 25, 0x2E)  -- Filled circle
    
    -- Row 4: Different sized ellipses
    drawellipse(centerX - 80, centerY + 50, 20, 12, 0x16)  -- Small horizontal outline
    fillellipse(centerX - 40, centerY + 50, 12, 20, 0x1C)  -- Small vertical filled
    drawellipse(centerX, centerY + 50, 18, 25, 0x29)  -- Medium vertical outline
    fillellipse(centerX + 50, centerY + 50, 25, 18, 0x37)  -- Medium horizontal filled
end

