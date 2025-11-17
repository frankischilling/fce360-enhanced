-- Test script for setrecordingmarker function
-- SELECT to start/stop recording
-- A button to set marker "marker_A"
-- B button to set marker "marker_B"
-- X button to set marker "marker_X"
-- Y button to set marker "marker_Y"

local frameCount = 0
local isRecording = false
local recordedData = nil
local lastSelectState = false
local lastAState = false
local lastBState = false
local lastXState = false
local lastYState = false
local recordFrames = 0
local markerStatus = ""
local markerStatusFrame = 0
local markersSet = {}  -- Track markers we've set (for display)

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states
    local selectPressed = isbuttonpressed(0, "SELECT")
    local aPressed = isxboxbuttonpressed(0, "A")
    local bPressed = isxboxbuttonpressed(0, "B")
    local xPressed = isxboxbuttonpressed(0, "X")
    local yPressed = isxboxbuttonpressed(0, "Y")
    
    -- Toggle recording with SELECT (edge detect)
    if selectPressed and not lastSelectState then
        if not isRecording then
            -- Start recording
            if startinputrecording() then
                isRecording = true
                recordFrames = frameCount
                recordedData = nil
                markerStatus = ""
                markersSet = {}  -- Clear markers when starting new recording
            end
        else
            -- Stop recording
            recordedData = stopinputrecording()
            isRecording = false
            markerStatus = "Recording stopped"
            markerStatusFrame = frameCount
        end
    end
    lastSelectState = selectPressed
    
    -- Set markers during recording (edge detect)
    if isRecording then
        -- A button sets "marker_A"
        if aPressed and not lastAState then
            setrecordingmarker("marker_A")
            local currentFrame = frameCount - recordFrames
            markerStatus = string.format("Marker 'marker_A' set at frame %d", currentFrame)
            markerStatusFrame = frameCount
            table.insert(markersSet, {name = "marker_A", frame = currentFrame})
            print(string.format("Marker 'marker_A' set at frame %d", currentFrame))
        end
        lastAState = aPressed
        
        -- B button sets "marker_B"
        if bPressed and not lastBState then
            setrecordingmarker("marker_B")
            local currentFrame = frameCount - recordFrames
            markerStatus = string.format("Marker 'marker_B' set at frame %d", currentFrame)
            markerStatusFrame = frameCount
            table.insert(markersSet, {name = "marker_B", frame = currentFrame})
            print(string.format("Marker 'marker_B' set at frame %d", currentFrame))
        end
        lastBState = bPressed
        
        -- X button sets "marker_X"
        if xPressed and not lastXState then
            setrecordingmarker("marker_X")
            local currentFrame = frameCount - recordFrames
            markerStatus = string.format("Marker 'marker_X' set at frame %d", currentFrame)
            markerStatusFrame = frameCount
            table.insert(markersSet, {name = "marker_X", frame = currentFrame})
            print(string.format("Marker 'marker_X' set at frame %d", currentFrame))
        end
        lastXState = xPressed
        
        -- Y button sets "marker_Y"
        if yPressed and not lastYState then
            setrecordingmarker("marker_Y")
            local currentFrame = frameCount - recordFrames
            markerStatus = string.format("Marker 'marker_Y' set at frame %d", currentFrame)
            markerStatusFrame = frameCount
            table.insert(markersSet, {name = "marker_Y", frame = currentFrame})
            print(string.format("Marker 'marker_Y' set at frame %d", currentFrame))
        end
        lastYState = yPressed
    else
        -- Reset button states when not recording
        lastAState = false
        lastBState = false
        lastXState = false
        lastYState = false
    end
    
    -- Clear marker status after 3 seconds (180 frames at 60fps)
    if markerStatus ~= "" and (frameCount - markerStatusFrame) > 180 then
        markerStatus = ""
    end
end

function gui()
    -- Display status
    drawtext(4, 4, "Set Recording Marker Test", 0x20)
    drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    
    -- Recording status
    if isRecording then
        drawtext(4, 20, "RECORDING...", 0x29)
        local frames = frameCount - recordFrames
        drawtext(4, 28, string.format("Recording Frames: %d", frames), 0x39)
        
        -- Show marker status
        if markerStatus ~= "" then
            drawtext(4, 36, markerStatus, 0x37)
        else
            drawtext(4, 36, "Press A/B/X/Y to set markers", 0x2D)
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
    else
        drawtext(4, 20, "Not Recording", 0x2D)
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
        else
            drawtext(4, 28, "No recording", 0x2D)
        end
    end
    
    -- Instructions
    drawtext(4, 120, "SELECT: Start/Stop recording", 0x2D)
    drawtext(4, 128, "A: Set marker 'marker_A'", 0x2D)
    drawtext(4, 136, "B: Set marker 'marker_B'", 0x2D)
    drawtext(4, 144, "X: Set marker 'marker_X'", 0x2D)
    drawtext(4, 152, "Y: Set marker 'marker_Y'", 0x2D)
    
    -- Visual indicator
    if isRecording then
        drawtext(4, 168, ">>> RECORDING <<<", 0x29)
    end
    
    -- Show current button state
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    if buttonNames == "" then
        buttonNames = "(none)"
    end
    drawtext(4, 184, "Buttons: " .. buttonNames, 0x39)
end

