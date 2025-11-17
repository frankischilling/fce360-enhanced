-- Test script for setplaybackspeed function
-- SELECT to start/stop recording
-- Y button to start playback (after recording)
-- During playback:
--   A button: Set speed to 0.5x (half speed)
--   B button: Set speed to 1.0x (normal speed)
--   X button: Set speed to 2.0x (double speed)
--   START button: Set speed to 0.25x (quarter speed)
--   UP button: Set speed to 4.0x (quadruple speed)

print("=== test_setplaybackspeed.lua script loaded ===")

local frameCount = 0
local isRecording = false
local recordedData = nil
local isPlayback = false
local lastPlaybackState = false
local lastSelectState = false
local lastAState = false
local lastBState = false
local lastXState = false
local lastStartState = false
local lastUpState = false
local lastYState = false
local recordFrames = 0
local status = ""
local statusFrame = 0
local playbackStartFrame = 0
local currentSpeed = 1.0
local autoRestart = true  -- Automatically restart playback when it finishes
local recordingLength = 0  -- Length of recorded data in frames

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states
    local selectPressed = isbuttonpressed(0, "SELECT")
    local aPressed = isbuttonpressed(0, "A")
    local bPressed = isbuttonpressed(0, "B")
    local xPressed = isxboxbuttonpressed(0, "X")
    local startPressed = isbuttonpressed(0, "START")
    local upPressed = isbuttonpressed(0, "UP")
    local yPressed = isxboxbuttonpressed(0, "Y")
    
    -- Toggle recording with SELECT (edge detect)
    if selectPressed and not lastSelectState then
        if not isRecording and not isPlayback then
            -- Start recording
            if startinputrecording() then
                isRecording = true
                recordFrames = frameCount
                recordedData = nil
                status = "Recording started - Press SELECT to stop"
                statusFrame = frameCount
                print("Recording started")
            end
        elseif isRecording then
            -- Stop recording
            recordedData = stopinputrecording()
            isRecording = false
            
            -- Calculate recording length
            recordingLength = 0
            if recordedData and recordedData.player0 then
                for i = 1, 10000 do
                    if recordedData.player0[i] == nil then
                        recordingLength = i - 1
                        break
                    end
                end
            end
            
            status = "Recording stopped - Press Y to play"
            statusFrame = frameCount
            print(string.format("Recording stopped - %d frames recorded", recordingLength))
        end
    end
    lastSelectState = selectPressed
    
    -- Start/stop playback with Y button (edge detect)
    if yPressed and not lastYState then
        if recordedData ~= nil and not isRecording then
            if not isPlayback then
                -- Start playback
                playinputrecording(recordedData)
                isPlayback = true
                playbackStartFrame = frameCount
                currentSpeed = 1.0  -- Reset to normal speed
                setplaybackspeed(1.0)
                status = "Playback started (1.0x) - Press A/B/X/START/UP to change speed"
                statusFrame = frameCount
                print("Playback started at 1.0x speed")
            else
                -- Stop playback (by restarting with same data, which resets it)
                -- Actually, we can't stop playback directly, so just note it
                status = "Press Y again after playback finishes to restart"
                statusFrame = frameCount
            end
        elseif isRecording then
            status = "Stop recording first!"
            statusFrame = frameCount
        elseif recordedData == nil then
            status = "No recording to play!"
            statusFrame = frameCount
        end
    end
    lastYState = yPressed
    
    -- Change playback speed during playback (edge detect)
    if isPlayback then
        -- A button: 0.5x speed (half speed)
        if aPressed and not lastAState then
            currentSpeed = 0.5
            setplaybackspeed(0.5)
            status = "Playback speed: 0.5x (half speed)"
            statusFrame = frameCount
            print("Playback speed set to 0.5x")
        end
        lastAState = aPressed
        
        -- B button: 1.0x speed (normal speed)
        if bPressed and not lastBState then
            currentSpeed = 1.0
            setplaybackspeed(1.0)
            status = "Playback speed: 1.0x (normal speed)"
            statusFrame = frameCount
            print("Playback speed set to 1.0x")
        end
        lastBState = bPressed
        
        -- X button: 2.0x speed (double speed)
        if xPressed and not lastXState then
            currentSpeed = 2.0
            setplaybackspeed(2.0)
            status = "Playback speed: 2.0x (double speed)"
            statusFrame = frameCount
            print("Playback speed set to 2.0x")
        end
        lastXState = xPressed
        
        -- START button: 0.25x speed (quarter speed)
        if startPressed and not lastStartState then
            currentSpeed = 0.25
            setplaybackspeed(0.25)
            status = "Playback speed: 0.25x (quarter speed)"
            statusFrame = frameCount
            print("Playback speed set to 0.25x")
        end
        lastStartState = startPressed
        
        -- UP button: 4.0x speed (quadruple speed)
        if upPressed and not lastUpState then
            currentSpeed = 4.0
            setplaybackspeed(4.0)
            status = "Playback speed: 4.0x (quadruple speed)"
            statusFrame = frameCount
            print("Playback speed set to 4.0x")
        end
        lastUpState = upPressed
    else
        -- Reset button states when not in playback
        lastAState = false
        lastBState = false
        lastXState = false
        lastStartState = false
        lastUpState = false
    end
    
    -- Detect when playback finishes and automatically restart
    -- Use time-based detection since we can't directly check C++ playback state
    if isPlayback and recordedData ~= nil and recordingLength > 0 then
        local playbackElapsed = frameCount - playbackStartFrame
        -- Estimate when playback should finish (accounting for speed)
        -- At normal speed (1.0x), playback takes recordingLength frames
        -- At 4.0x speed, it takes recordingLength/4 frames, etc.
        local estimatedPlaybackFrames = math.ceil(recordingLength / math.max(currentSpeed, 0.1))
        
        -- If we've played longer than estimated + buffer, playback likely finished
        -- Use smaller buffer for high speeds (they finish faster)
        local buffer = currentSpeed >= 2.0 and 3 or 5
        if playbackElapsed >= estimatedPlaybackFrames + buffer then
            -- Playback likely finished, restart it
            playinputrecording(recordedData)
            isPlayback = true
            playbackStartFrame = frameCount
            -- Keep current speed (don't reset to 1.0)
            status = string.format("Playback restarted (%.2fx speed)", currentSpeed)
            statusFrame = frameCount
            print(string.format("Playback automatically restarted at %.2fx speed", currentSpeed))
        end
    elseif not isPlayback and recordedData ~= nil and not isRecording and autoRestart and lastPlaybackState then
        -- Playback was active but now isn't (detected by state change), restart immediately
        playinputrecording(recordedData)
        isPlayback = true
        playbackStartFrame = frameCount
        -- Keep current speed (don't reset to 1.0)
        status = string.format("Playback restarted (%.2fx speed)", currentSpeed)
        statusFrame = frameCount
        print(string.format("Playback automatically restarted at %.2fx speed", currentSpeed))
    end
    
    -- Update last playback state
    lastPlaybackState = isPlayback
    
    -- Clear status after 3 seconds (180 frames at 60fps)
    if status ~= "" and (frameCount - statusFrame) > 180 then
        status = ""
    end
end

function gui()
    -- Display status
    drawtext(4, 4, "Set Playback Speed Test", 0x20)
    drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    
    -- Recording status
    if isRecording then
        drawtext(4, 20, "RECORDING...", 0x29)
        local frames = frameCount - recordFrames
        drawtext(4, 28, string.format("Recording Frames: %d", frames), 0x39)
        
        if status ~= "" then
            drawtext(4, 36, status, 0x37)
        else
            drawtext(4, 36, "Press SELECT to stop recording", 0x2D)
        end
    elseif isPlayback then
        drawtext(4, 20, "PLAYBACK ACTIVE", 0x39)
        local playbackFrames = frameCount - playbackStartFrame
        drawtext(4, 28, string.format("Playback Frames: %d", playbackFrames), 0x39)
        
        -- Show current speed
        drawtext(4, 36, string.format("Current Speed: %.2fx", currentSpeed), 0x37)
        
        -- Show speed status
        if status ~= "" then
            drawtext(4, 44, status, 0x37)
        else
            drawtext(4, 44, "Press A/B/X/START/UP to change speed", 0x2D)
        end
        
        -- Speed descriptions
        local speedDesc = ""
        if currentSpeed == 0.25 then
            speedDesc = "Quarter speed (very slow)"
        elseif currentSpeed == 0.5 then
            speedDesc = "Half speed (slow motion)"
        elseif currentSpeed == 1.0 then
            speedDesc = "Normal speed"
        elseif currentSpeed == 2.0 then
            speedDesc = "Double speed (fast forward)"
        elseif currentSpeed == 4.0 then
            speedDesc = "Quadruple speed (very fast)"
        else
            speedDesc = string.format("%.2fx speed", currentSpeed)
        end
        drawtext(4, 52, speedDesc, 0x2D)
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
            
            if status ~= "" then
                drawtext(4, 36, status, 0x2D)
            end
        else
            drawtext(4, 28, "No recording", 0x2D)
        end
    end
    
    -- Instructions
    if isRecording then
        drawtext(4, 120, "SELECT: Stop recording", 0x2D)
    elseif isPlayback then
        drawtext(4, 120, "A: 0.5x (half speed)", 0x2D)
        drawtext(4, 128, "B: 1.0x (normal speed)", 0x2D)
        drawtext(4, 136, "X: 2.0x (double speed)", 0x2D)
        drawtext(4, 144, "START: 0.25x (quarter speed)", 0x2D)
        drawtext(4, 152, "UP: 4.0x (quadruple speed)", 0x2D)
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
        -- Show speed indicator
        local speedColor = 0x37  -- Green for normal
        if currentSpeed < 1.0 then
            speedColor = 0x39  -- Yellow for slow
        elseif currentSpeed > 1.0 then
            speedColor = 0x29  -- Red for fast
        end
        drawtext(4, 176, string.format("Speed: %.2fx", currentSpeed), speedColor)
    end
    
    -- Show current button state
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    if buttonNames == "" then
        buttonNames = "(none)"
    end
    drawtext(4, 192, "Buttons: " .. buttonNames, 0x39)
end

