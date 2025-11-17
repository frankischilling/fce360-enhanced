-- Test script for trimrecording function
-- SELECT to start/stop recording
-- After recording:
--   A button: Set trim start frame (current frame count)
--   B button: Set trim end frame (current frame count)
--   X button: Trim recording to selected range
--   Y button: Reset trim selection
--   START button: Show recording info

print("=== test_trimrecording.lua script loaded ===")

local frameCount = 0
local isRecording = false
local recordedData = nil
local lastSelectState = false
local lastAState = false
local lastBState = false
local lastXState = false
local lastYState = false
local lastStartState = false
local recordFrames = 0
local status = ""
local statusFrame = 0
local trimStartFrame = -1  -- -1 means not set
local trimEndFrame = -1    -- -1 means not set
local originalLength = 0
local trimmedLength = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states
    local selectPressed = isbuttonpressed(0, "SELECT")
    local aPressed = isxboxbuttonpressed(0, "A")
    local bPressed = isxboxbuttonpressed(0, "B")
    local xPressed = isxboxbuttonpressed(0, "X")
    local yPressed = isxboxbuttonpressed(0, "Y")
    local startPressed = isbuttonpressed(0, "START")
    
    -- Toggle recording with SELECT (edge detect)
    if selectPressed and not lastSelectState then
        if not isRecording then
            -- Start recording
            if startinputrecording() then
                isRecording = true
                recordFrames = frameCount
                recordedData = nil
                status = "Recording started - Press SELECT to stop"
                statusFrame = frameCount
                trimStartFrame = -1
                trimEndFrame = -1
                originalLength = 0
                trimmedLength = 0
                print("Recording started")
            end
        else
            -- Stop recording
            recordedData = stopinputrecording()
            isRecording = false
            
            -- Calculate recording length
            trimmedLength = 0
            if recordedData and recordedData.player0 then
                for i = 1, 10000 do
                    if recordedData.player0[i] == nil then
                        trimmedLength = i - 1
                        break
                    end
                end
            end
            
            -- If we haven't set originalLength yet (first time stopping), set it
            if originalLength == 0 then
                originalLength = trimmedLength
            end
            
            status = string.format("Recording stopped - %d frames", trimmedLength)
            if originalLength > 0 and trimmedLength ~= originalLength then
                status = status .. string.format(" (trimmed from %d)", originalLength)
            end
            statusFrame = frameCount
            print(string.format("Recording stopped - %d frames recorded", trimmedLength))
        end
    end
    lastSelectState = selectPressed
    
    -- Allow trimming during recording
    if isRecording then
        -- A button: Set trim start frame (current frame in recording)
        if aPressed and not lastAState then
            -- Get current frame count in recording
            -- We need to estimate based on how long we've been recording
            local currentFrame = frameCount - recordFrames - 1  -- -1 because frame hasn't been recorded yet
            if currentFrame < 0 then currentFrame = 0 end
            
            trimStartFrame = currentFrame
            
            status = string.format("Trim start set to frame %d", trimStartFrame)
            statusFrame = frameCount
            print(string.format("Trim start frame set to %d", trimStartFrame))
        end
        lastAState = aPressed
        
        -- B button: Set trim end frame (current frame in recording)
        if bPressed and not lastBState then
            -- Get current frame count in recording
            local currentFrame = frameCount - recordFrames - 1  -- -1 because frame hasn't been recorded yet
            if currentFrame < 0 then currentFrame = 0 end
            
            trimEndFrame = currentFrame
            
            status = string.format("Trim end set to frame %d", trimEndFrame)
            statusFrame = frameCount
            print(string.format("Trim end frame set to %d", trimEndFrame))
        end
        lastBState = bPressed
        
        -- X button: Trim recording
        if xPressed and not lastXState then
            if trimStartFrame >= 0 and trimEndFrame >= 0 then
                if trimStartFrame <= trimEndFrame then
                    -- Trim the recording
                    local success = trimrecording(trimStartFrame, trimEndFrame)
                    if success then
                        local framesKept = trimEndFrame - trimStartFrame + 1
                        status = string.format("Trimmed! Kept frames %d-%d (%d frames)", trimStartFrame, trimEndFrame, framesKept)
                        statusFrame = frameCount
                        print(string.format("Recording trimmed: kept frames %d to %d (%d frames)", trimStartFrame, trimEndFrame, framesKept))
                        
                        -- Update trimmed length estimate
                        trimmedLength = framesKept
                    else
                        status = "Trim failed! Check frame range."
                        statusFrame = frameCount
                        print("ERROR: trimrecording failed")
                    end
                else
                    status = "Error: Start frame > End frame"
                    statusFrame = frameCount
                    print("ERROR: Start frame must be <= End frame")
                end
            else
                status = "Error: Set start and end frames first (A and B buttons)"
                statusFrame = frameCount
                print("ERROR: Set trim start and end frames first")
            end
        end
        lastXState = xPressed
        
        -- Y button: Reset trim selection
        if yPressed and not lastYState then
            trimStartFrame = -1
            trimEndFrame = -1
            status = "Trim selection reset"
            statusFrame = frameCount
            print("Trim selection reset")
        end
        lastYState = yPressed
        
        -- START button: Show recording info
        if startPressed and not lastStartState then
            local currentFrames = frameCount - recordFrames
            local info = string.format("Recording: %d frames", currentFrames)
            if trimStartFrame >= 0 and trimEndFrame >= 0 then
                info = info .. string.format("\nTrim range: %d to %d (%d frames)", trimStartFrame, trimEndFrame, trimEndFrame - trimStartFrame + 1)
            end
            status = info
            statusFrame = frameCount
            print(info)
        end
        lastStartState = startPressed
    elseif recordedData ~= nil then
        -- Show info when not recording
        -- START button: Show recording info
        if startPressed and not lastStartState then
            local info = string.format("Recorded: %d frames", trimmedLength)
            if originalLength > 0 and trimmedLength ~= originalLength then
                info = info .. string.format(" (trimmed from %d)", originalLength)
            end
            status = info
            statusFrame = frameCount
            print(info)
        end
        lastStartState = startPressed
        
        -- Reset button states
        lastAState = false
        lastBState = false
        lastXState = false
        lastYState = false
    else
        -- Reset button states when no recording
        lastAState = false
        lastBState = false
        lastXState = false
        lastYState = false
        lastStartState = false
    end
    
    -- Clear status after 3 seconds (180 frames at 60fps)
    if statusFrame > 0 and frameCount - statusFrame > 180 then
        status = ""
        statusFrame = 0
    end
end

function script()
    -- Draw status
    local y = 4
    if status ~= "" then
        drawtext(4, y, status, 0x39)
        y = y + 8
    end
    
    -- Draw recording status
    if isRecording then
        local framesRecorded = frameCount - recordFrames
        drawtext(4, y, string.format("Recording: %d frames", framesRecorded), 0x2E)
        y = y + 8
        
        -- Show trim selection
        if trimStartFrame >= 0 and trimEndFrame >= 0 then
            drawtext(4, y, string.format("Trim: frames %d to %d", trimStartFrame, trimEndFrame), 0x39)
            y = y + 8
            drawtext(4, y, string.format("Will keep %d frames (press X to trim)", trimEndFrame - trimStartFrame + 1), 0x39)
            y = y + 8
        elseif trimStartFrame >= 0 then
            drawtext(4, y, string.format("Trim start: frame %d (press B to set end)", trimStartFrame), 0x39)
            y = y + 8
        elseif trimEndFrame >= 0 then
            drawtext(4, y, string.format("Trim end: frame %d (press A to set start)", trimEndFrame), 0x39)
            y = y + 8
        else
            drawtext(4, y, "Press A to set trim start, B to set trim end", 0x30)
            y = y + 8
        end
    elseif recordedData ~= nil then
        drawtext(4, y, string.format("Recorded: %d frames", trimmedLength), 0x20)
        y = y + 8
        if originalLength > 0 and trimmedLength ~= originalLength then
            drawtext(4, y, string.format("(trimmed from %d frames)", originalLength), 0x30)
            y = y + 8
        end
    else
        drawtext(4, y, "Press SELECT to start recording", 0x30)
        y = y + 8
    end
    
    -- Draw controls
    y = y + 4
    drawtext(4, y, "Controls:", 0x30)
    y = y + 8
    drawtext(4, y, "SELECT: Start/stop recording", 0x30)
    y = y + 8
    if isRecording then
        drawtext(4, y, "A: Set trim start frame", 0x30)
        y = y + 8
        drawtext(4, y, "B: Set trim end frame", 0x30)
        y = y + 8
        drawtext(4, y, "X: Trim recording (while recording)", 0x30)
        y = y + 8
        drawtext(4, y, "Y: Reset trim selection", 0x30)
        y = y + 8
        drawtext(4, y, "START: Show info", 0x30)
    elseif recordedData ~= nil then
        drawtext(4, y, "START: Show info", 0x30)
    end
end

