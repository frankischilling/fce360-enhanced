-- Test script for screenshot() function
-- Tests both auto-generated and custom filename modes

local frameCount = 0
local testPhase = 1
local phaseStartFrame = 0
local screenshotsTaken = {}

function gui()
	frameCount = frameCount + 1
	
	-- Phase 1: Auto-generated filename (no parameter)
	if testPhase == 1 then
		if frameCount == 60 then
			phaseStartFrame = frameCount
			local filename = screenshot()
			if filename then
				table.insert(screenshotsTaken, filename)
				print("Phase 1: Auto-generated screenshot saved as: " .. filename)
			else
				print("Phase 1: ERROR - screenshot() returned nil")
			end
		elseif frameCount == phaseStartFrame + 60 then
			testPhase = 2
			phaseStartFrame = frameCount
		end
	end
	
	-- Phase 2: Custom filename (with parameter)
	if testPhase == 2 then
		if frameCount == phaseStartFrame + 1 then
			local filename = screenshot("test_custom")
			if filename then
				table.insert(screenshotsTaken, filename)
				print("Phase 2: Custom screenshot saved as: " .. filename)
			else
				print("Phase 2: ERROR - screenshot() returned nil")
			end
		elseif frameCount == phaseStartFrame + 60 then
			testPhase = 3
			phaseStartFrame = frameCount
		end
	end
	
	-- Phase 3: Custom filename with .png extension
	if testPhase == 3 then
		if frameCount == phaseStartFrame + 1 then
			local filename = screenshot("test_with_ext.png")
			if filename then
				table.insert(screenshotsTaken, filename)
				print("Phase 3: Custom screenshot with extension saved as: " .. filename)
			else
				print("Phase 3: ERROR - screenshot() returned nil")
			end
		elseif frameCount == phaseStartFrame + 60 then
			testPhase = 4
			phaseStartFrame = frameCount
		end
	end
	
	-- Phase 4: Multiple screenshots with sequential names
	if testPhase == 4 then
		local shotNum = math.floor((frameCount - phaseStartFrame) / 30)
		if shotNum >= 1 and shotNum <= 3 then
			if (frameCount - phaseStartFrame) % 30 == 1 then
				local filename = screenshot("sequence_" .. shotNum)
				if filename then
					table.insert(screenshotsTaken, filename)
					print("Phase 4: Sequential screenshot " .. shotNum .. " saved as: " .. filename)
				end
			end
		elseif frameCount >= phaseStartFrame + 120 then
			testPhase = 5
			phaseStartFrame = frameCount
		end
	end
	
	-- Phase 5: Summary and display
	if testPhase == 5 then
		-- Display summary
		local y = 20
		drawtext(10, y, "Screenshot Test Complete", 0x2E)
		y = y + 12
		drawtext(10, y, "Total screenshots: " .. #screenshotsTaken, 0x2E)
		y = y + 12
		
		if #screenshotsTaken > 0 then
			drawtext(10, y, "Files saved:", 0x2E)
			y = y + 12
			for i, filename in ipairs(screenshotsTaken) do
				if y < 220 then
					drawtext(20, y, i .. ". " .. filename, 0x2C)
					y = y + 10
				end
			end
		else
			drawtext(10, y, "ERROR: No screenshots were saved!", 0x20)
		end
		
		-- Draw visual indicator
		local color = 0x2E
		if frameCount % 60 < 30 then
			color = 0x2F
		end
		fillrect(230, 10, 20, 20, color)
		drawtext(235, 12, "OK", 0x00)
	end
	
	-- Draw phase indicator
	local phaseNames = {
		[1] = "Auto-generated filename",
		[2] = "Custom filename",
		[3] = "Custom filename with .png",
		[4] = "Sequential screenshots",
		[5] = "Summary"
	}
	
	if testPhase <= 5 then
		drawtext(10, 4, "Phase " .. testPhase .. ": " .. (phaseNames[testPhase] or "Unknown"), 0x2E)
	end
end

print("Screenshot test script loaded")
print("Test will run for ~5 phases")
print("This script demonstrates:")
print("  - Auto-generated screenshot filenames")
print("  - Custom screenshot filenames")
print("  - Sequential screenshots")

