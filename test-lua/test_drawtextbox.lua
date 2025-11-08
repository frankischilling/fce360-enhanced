-- Test script for drawtextbox(x, y, width, height, text, color, bgColor, borderColor)
-- Tests various combinations of background and border

local frameCount = 0

function gui()
	frameCount = frameCount + 1
	
	-- Test 1: Full box with background and border
	drawtextbox(10, 10, 100, 40, "Test 1: Full Box", 0x3F, 0x10, 0x3F)
	
	-- Test 2: Background only (no border)
	drawtextbox(10, 60, 100, 40, "Test 2: Bg Only", 0x3F, 0x20, nil)
	
	-- Test 3: Border only (no background) - 3 pixel border
	drawtextbox(10, 110, 100, 40, "Test 3: Border Only", 0x3F, nil, 0x3F)
	
	-- Test 4: Text only (no background, no border)
	drawtextbox(10, 160, 100, 40, "Test 4: Text Only", 0x3F, nil, nil)
	
	-- Test 5: Multi-line text
	drawtextbox(120, 10, 100, 60, "Test 5:\nMulti-line\nText Box", 0x3F, 0x10, 0x3F)
	
	-- Test 6: Small box
	drawtextbox(120, 80, 60, 30, "Small", 0x3F, 0x20, 0x3F)
	
	-- Test 7: Large box with long text
	drawtextbox(120, 120, 120, 50, "This is a longer text that should wrap inside the box", 0x3F, 0x10, 0x3F)
	
	-- Test 8: Different colors
	drawtextbox(10, 210, 100, 40, "Test 8: Colors", 0x3F, 0x30, 0x0F)
	
	-- Print status
	drawtext(0, 0, string.format("Frame: %d", frameCount), 0x3F)
end

