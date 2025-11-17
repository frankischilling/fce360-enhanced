-- Test script for jumptorecordingmarker function
-- SELECT to start recording
-- B button to stop recording
-- Y button to start playback (after recording)
-- During recording:
--   A button to set marker "marker_A"
--   START button to set marker "marker_START"
--   UP button to set marker "marker_UP"
--   DOWN button to set marker "marker_DOWN"
-- During playback:
--   A button to jump to "marker_A"
--   START button to jump to "marker_START"
--   UP button to jump to "marker_UP"
--   DOWN button to jump to "marker_DOWN"

print("=== test_jumptorecordingmarker.lua script loaded ===")

local frameCount = 0
local isRecording = false
local recordedData = nil
local isPlayback = false
local lastSelectState = false
local lastAState = false
local lastBState = false
local lastStartState = false
local lastUpState = false
local lastDownState = false
local lastYState = false
local recordFrames = 0
local status = ""
local statusFrame = 0
local markersSet = {}  -- Track markers we've set (for display)
local playbackStartFrame = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states
    local selectPressed = isbuttonpressed(0, "SELECT")
    local aPressed = isbuttonpressed(0, "A")
    local bPressed = isbuttonpressed(0, "B")
    local startPressed = isbuttonpressed(0, "START")
    local upPressed = isbuttonpressed(0, "UP")
    local downPressed = isbuttonpressed(0, "DOWN")
    local yPressed = isxboxbuttonpressed(0, "Y")
    
    -- Start recording with SELECT (edge detect)
    if selectPressed and not lastSelectState then
        if not isRecording and not isPlayback then
            -- Start recording
            if startinputrecording() then
                isRecording = true
                recordFrames = frameCount
                recordedData = nil
                status = "Recording started - Press B to stop"
                markersSet = {}  -- Clear markers when starting new recording
                statusFrame = frameCount
                print("Recording started")
            end
        end
    end
    lastSelectState = selectPressed
    
    -- Stop recording with B button (edge detect)
    if bPressed and not lastBState then
        if isRecording then
            -- Stop recording
            recordedData = stopinputrecording()
            isRecording = false
            status = "Recording stopped - Press Y to play"
            statusFrame = frameCount
            print("Recording stopped")
        end
    end
    lastBState = bPressed
    
    -- Set markers during recording (edge detect) - using NES controls
    if isRecording then
        -- A button sets "marker_A"
        if aPressed and not lastAState then
            setrecordingmarker("marker_A")
            local currentFrame = frameCount - recordFrames
            status = string.format("Marker 'marker_A' set at frame %d", currentFrame)
            statusFrame = frameCount
            table.insert(markersSet, {name = "marker_A", frame = currentFrame})
            print(string.format("Marker 'marker_A' set at frame %d", currentFrame))
        end
        lastAState = aPressed
        
        -- START button sets "marker_START"
        if startPressed and not lastStartState then
            setrecordingmarker("marker_START")
            local currentFrame = frameCount - recordFrames
            status = string.format("Marker 'marker_START' set at frame %d", currentFrame)
            statusFrame = frameCount
            table.insert(markersSet, {name = "marker_START", frame = currentFrame})
            print(string.format("Marker 'marker_START' set at frame %d", currentFrame))
        end
        lastStartState = startPressed
        
        -- UP button sets "marker_UP"
        if upPressed and not lastUpState then
            setrecordingmarker("marker_UP")
            local currentFrame = frameCount - recordFrames
            status = string.format("Marker 'marker_UP' set at frame %d", currentFrame)
            statusFrame = frameCount
            table.insert(markersSet, {name = "marker_UP", frame = currentFrame})
            print(string.format("Marker 'marker_UP' set at frame %d", currentFrame))
        end
        lastUpState = upPressed
        
        -- DOWN button sets "marker_DOWN"
        if downPressed and not lastDownState then
            setrecordingmarker("marker_DOWN")
            local currentFrame = frameCount - recordFrames
            status = string.format("Marker 'marker_DOWN' set at frame %d", currentFrame)
            statusFrame = frameCount
            table.insert(markersSet, {name = "marker_DOWN", frame = currentFrame})
            print(string.format("Marker 'marker_DOWN' set at frame %d", currentFrame))
        end
        lastDownState = downPressed
    end
    
    -- Start playback with Y button (edge detect)
    if yPressed and not lastYState then
        if recordedData ~= nil and not isRecording and not isPlayback then
            playinputrecording(recordedData)
            isPlayback = true
            playbackStartFrame = frameCount
            status = "Playback started - Press A/START/UP/DOWN to jump to markers"
            statusFrame = frameCount
            print("Playback started")
        elseif isRecording then
            status = "Stop recording first!"
            statusFrame = frameCount
        elseif recordedData == nil then
            status = "No recording to play!"
            statusFrame = frameCount
        end
    end
    lastYState = yPressed
    
    -- Jump to markers during playback (edge detect) - using NES controls
    if isPlayback then
        -- A button jumps to "marker_A"
        if aPressed and not lastAState then
            local success = jumptorecordingmarker("marker_A")
            if success then
                status = "Jumped to marker_A"
                print("Jumped to marker_A")
            else
                status = "Failed to jump to marker_A"
                print("Failed to jump to marker_A")
            end
            statusFrame = frameCount
        end
        lastAState = aPressed
        
        -- START button jumps to "marker_START"
        if startPressed and not lastStartState then
            local success = jumptorecordingmarker("marker_START")
            if success then
                status = "Jumped to marker_START"
                print("Jumped to marker_START")
            else
                status = "Failed to jump to marker_START"
                print("Failed to jump to marker_START")
            end
            statusFrame = frameCount
        end
        lastStartState = startPressed
        
        -- UP button jumps to "marker_UP"
        if upPressed and not lastUpState then
            local success = jumptorecordingmarker("marker_UP")
            if success then
                status = "Jumped to marker_UP"
                print("Jumped to marker_UP")
            else
                status = "Failed to jump to marker_UP"
                print("Failed to jump to marker_UP")
            end
            statusFrame = frameCount
        end
        lastUpState = upPressed
        
        -- DOWN button jumps to "marker_DOWN"
        if downPressed and not lastDownState then
            local success = jumptorecordingmarker("marker_DOWN")
            if success then
                status = "Jumped to marker_DOWN"
                print("Jumped to marker_DOWN")
            else
                status = "Failed to jump to marker_DOWN"
                print("Failed to jump to marker_DOWN")
            end
            statusFrame = frameCount
        end
        lastDownState = downPressed
        
        -- Check if playback finished
        -- Note: We can't directly check playback state, but we can detect when it ends
        -- by checking if enough time has passed (rough estimate)
    else
        -- Reset button states when not in playback
        lastAState = false
        lastStartState = false
        lastUpState = false
        lastDownState = false
    end
    
    -- Clear status after 3 seconds (180 frames at 60fps)
    if status ~= "" and (frameCount - statusFrame) > 180 then
        status = ""
    end
end

function gui()
    -- Display status
    drawtext(4, 4, "Jump to Recording Marker Test", 0x20)
    drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    
    -- Recording status
    if isRecording then
        drawtext(4, 20, "RECORDING...", 0x29)
        local frames = frameCount - recordFrames
        drawtext(4, 28, string.format("Recording Frames: %d", frames), 0x39)
        
        -- Show marker status
        if status ~= "" then
            drawtext(4, 36, status, 0x37)
        else
            drawtext(4, 36, "Press A/START/UP/DOWN to set markers", 0x2D)
        end
        
        -- Display markers set during this recording
        if #markersSet > 0 then
            drawtext(4, 52, "Markers set:", 0x2D)
            local y = 60
            for i = 1, math.min(#markersSet, 5) do  -- Show up to 5 markers
                local marker = markersSet[i]
                drawtext(4, y, string.format("  %s at frame %d", marker.name, marker.frame), 0x37)
                y = y + 8
            end
            if #markersSet > 5 then
                drawtext(4, y, string.format("  ... and %d more", #markersSet - 5), 0x2D)
            end
        else
            drawtext(4, 52, "No markers set yet", 0x2D)
        end
    elseif isPlayback then
        drawtext(4, 20, "PLAYBACK ACTIVE", 0x39)
        local playbackFrames = frameCount - playbackStartFrame
        drawtext(4, 28, string.format("Playback Frames: %d", playbackFrames), 0x39)
        
        -- Show jump status
        if status ~= "" then
            drawtext(4, 36, status, 0x37)
        else
            drawtext(4, 36, "Press A/START/UP/DOWN to jump to markers", 0x2D)
        end
        
        -- Display available markers
        if #markersSet > 0 then
            drawtext(4, 52, "Available markers:", 0x2D)
            local y = 60
            for i = 1, math.min(#markersSet, 5) do
                local marker = markersSet[i]
                drawtext(4, y, string.format("  %s (frame %d)", marker.name, marker.frame), 0x37)
                y = y + 8
            end
        end
    else
        drawtext(4, 20, "Not Recording/Playing", 0x2D)
        if recordedData ~= nil then
            -- Get length of recorded data (player 0)
            local p0Data = recordedData.player0
            local length = 0
            if p0Data then
                for i = 1, 10000 do  -- Count frames (reasonable limit)
                    if p0Data[i] == nil then
                        length = i - 1
                        break
                    end
                end
            end
            drawtext(4, 28, string.format("Recorded: %d frames", length), 0x37)
            
            -- Show final marker count
            if #markersSet > 0 then
                drawtext(4, 36, string.format("Total markers: %d", #markersSet), 0x39)
            end
            
            if status ~= "" then
                drawtext(4, 44, status, 0x2D)
            end
        else
            drawtext(4, 28, "No recording", 0x2D)
        end
    end
    
    -- Instructions
    if isRecording then
        drawtext(4, 120, "B: Stop recording", 0x2D)
        drawtext(4, 128, "A: Set marker 'marker_A'", 0x2D)
        drawtext(4, 136, "START: Set marker 'marker_START'", 0x2D)
        drawtext(4, 144, "UP: Set marker 'marker_UP'", 0x2D)
        drawtext(4, 152, "DOWN: Set marker 'marker_DOWN'", 0x2D)
    elseif isPlayback then
        drawtext(4, 120, "A: Jump to marker_A", 0x2D)
        drawtext(4, 128, "START: Jump to marker_START", 0x2D)
        drawtext(4, 136, "UP: Jump to marker_UP", 0x2D)
        drawtext(4, 144, "DOWN: Jump to marker_DOWN", 0x2D)
    else
        drawtext(4, 120, "SELECT: Start recording", 0x2D)
        if recordedData ~= nil then
            drawtext(4, 128, "Y: Start playback", 0x2D)
        end
    end
    
    -- Visual indicator
    if isRecording then
        drawtext(4, 168, ">>> RECORDING <<<", 0x29)
    elseif isPlayback then
        drawtext(4, 168, ">>> PLAYBACK <<<", 0x39)
    end
    
    -- Show current button state
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    if buttonNames == "" then
        buttonNames = "(none)"
    end
    drawtext(4, 184, "Buttons: " .. buttonNames, 0x39)
end

