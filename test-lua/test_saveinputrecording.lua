-- Test script for saveinputrecording function
-- SELECT to start/stop recording
-- X button to save recording to file
-- Y button to save with custom filename

local frameCount = 0
local isRecording = false
local recordedData = nil
local lastSelectState = false
local lastXState = false
local lastYState = false
local recordFrames = 0
local saveStatus = ""
local saveStatusFrame = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states using isbuttonpressed (more reliable)
    local selectPressed = isbuttonpressed(0, "SELECT")
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
                saveStatus = ""
            end
        else
            -- Stop recording
            recordedData = stopinputrecording()
            isRecording = false
            saveStatus = ""
        end
    end
    lastSelectState = selectPressed
    
    -- Save recording with default name using X button (edge detect)
    if xPressed and not lastXState then
        if recordedData ~= nil and not isRecording then
            local success = saveinputrecording("test_recording.txt")
            if success then
                saveStatus = "Saved: test_recording.txt"
            else
                saveStatus = "Save FAILED!"
            end
            saveStatusFrame = frameCount
        elseif isRecording then
            saveStatus = "Stop recording first!"
            saveStatusFrame = frameCount
        elseif recordedData == nil then
            saveStatus = "No recording to save!"
            saveStatusFrame = frameCount
        end
    end
    lastXState = xPressed
    
    -- Save recording with custom name using Y button (edge detect)
    if yPressed and not lastYState then
        if recordedData ~= nil and not isRecording then
            -- Generate filename with timestamp (frame count)
            local filename = string.format("recording_%d.txt", frameCount)
            local success = saveinputrecording(filename)
            if success then
                saveStatus = "Saved: " .. filename
            else
                saveStatus = "Save FAILED: " .. filename
            end
            saveStatusFrame = frameCount
        elseif isRecording then
            saveStatus = "Stop recording first!"
            saveStatusFrame = frameCount
        elseif recordedData == nil then
            saveStatus = "No recording to save!"
            saveStatusFrame = frameCount
        end
    end
    lastYState = yPressed
    
    -- Clear save status after 3 seconds (180 frames at 60fps)
    if saveStatus ~= "" and (frameCount - saveStatusFrame) > 180 then
        saveStatus = ""
    end
end

function gui()
    -- Display status
    drawtext(4, 4, "Save Input Recording Test", 0x20)
    drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    
    -- Recording status
    if isRecording then
        drawtext(4, 20, "RECORDING...", 0x29)
        local frames = frameCount - recordFrames
        drawtext(4, 28, string.format("Frames: %d", frames), 0x39)
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
        else
            drawtext(4, 28, "No recording", 0x2D)
        end
    end
    
    -- Save status
    if saveStatus ~= "" then
        local color = 0x37  -- Green for success
        if string.find(saveStatus, "FAILED") or string.find(saveStatus, "!") then
            color = 0x29  -- Red for error
        end
        drawtext(4, 36, saveStatus, color)
    else
        if recordedData ~= nil then
            drawtext(4, 36, "Ready to save", 0x2D)
        else
            drawtext(4, 36, "No data to save", 0x2D)
        end
    end
    
    -- Instructions
    drawtext(4, 52, "SELECT: Start/Stop recording", 0x2D)
    drawtext(4, 60, "X: Save to test_recording.txt", 0x2D)
    drawtext(4, 68, "Y: Save with custom filename", 0x2D)
    
    -- Show current button state
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    if buttonNames == "" then
        buttonNames = "(none)"
    end
    drawtext(4, 84, "Buttons: " .. buttonNames, 0x39)
    
    -- Debug: Show button detection status
    local selectStatus = isbuttonpressed(0, "SELECT") and "PRESSED" or "released"
    local xStatus = isxboxbuttonpressed(0, "X") and "PRESSED" or "released"
    local yStatus = isxboxbuttonpressed(0, "Y") and "PRESSED" or "released"
    drawtext(4, 92, string.format("SELECT: %s", selectStatus), isbuttonpressed(0, "SELECT") and 0x29 or 0x2D)
    drawtext(4, 100, string.format("X: %s  Y: %s", xStatus, yStatus), 0x2D)
    
    -- Visual indicator
    if isRecording then
        drawtext(4, 108, ">>> RECORDING <<<", 0x29)
    end
    
    -- Show file path info
    drawtext(4, 124, "Files saved to:", 0x2D)
    drawtext(4, 132, "hdd1:\\fce360-enhanced\\lua\\recordings\\", 0x39)
end

