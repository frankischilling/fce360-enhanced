-- Test script for drawtextscaled function
-- Demonstrates text drawing with custom X and Y scaling
-- Scale range: 0.5 to 4.0

function gui()
  local fps = getfps()
  
  -- Title
  drawtext(4, 4, "drawtextscaled Test", 0x3F)
  drawtext(4, 12, string.format("FPS: %.1f", fps), 0x2E)
  
  -- ===== NORMAL SIZE (scale 1.0) =====
  drawtextscaled(4, 24, "Normal (1.0x)", 0x20, 1.0, 1.0)
  
  -- ===== SCALED DOWN (0.5x) =====
  drawtextscaled(4, 36, "Small (0.5x)", 0x30, 0.5, 0.5)
  
  -- ===== SCALED UP (2.0x) =====
  drawtextscaled(4, 48, "Large (2.0x)", 0x2E, 2.0, 2.0)
  
  -- ===== WIDE TEXT (2.0x X, 1.0x Y) =====
  drawtextscaled(4, 80, "Wide (2.0x, 1.0x)", 0x39, 2.0, 1.0)
  
  -- ===== TALL TEXT (1.0x X, 2.0x Y) =====
  drawtextscaled(4, 100, "Tall", 0x37, 1.0, 2.0)
  drawtextscaled(4, 120, "(1.0x, 2.0x)", 0x37, 1.0, 2.0)
  
  -- ===== EXTREME SCALE (4.0x) =====
  drawtextscaled(4, 150, "HUGE!", 0x0F, 4.0, 4.0)
  
  -- ===== MEDIUM SCALE (1.5x) =====
  drawtextscaled(120, 24, "Medium", 0x28, 1.5, 1.5)
  drawtextscaled(120, 40, "(1.5x)", 0x28, 1.5, 1.5)
  
  -- ===== ANISOTROPIC SCALING =====
  drawtextscaled(120, 60, "Aniso", 0x26, 1.5, 2.5)
  drawtextscaled(120, 85, "1.5x2.5", 0x26, 1.5, 2.5)
  
  -- ===== COMPARISON: Same text, different scales =====
  drawtextscaled(4, 200, "A", 0x20, 0.5, 0.5)  -- Small A
  drawtextscaled(12, 200, "A", 0x20, 1.0, 1.0)  -- Normal A
  drawtextscaled(24, 200, "A", 0x20, 1.5, 1.5)  -- Medium A
  drawtextscaled(40, 200, "A", 0x20, 2.0, 2.0)  -- Large A
  drawtextscaled(64, 200, "A", 0x20, 3.0, 3.0)  -- Very Large A
  
  -- Status
  drawtext(4, 232, "Scale: 0.5-4.0 range", 0x20)
end

