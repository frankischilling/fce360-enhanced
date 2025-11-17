-- Test script for loadinputrecording function
-- X button to load test_recording.txt
-- Y button to load with custom filename
-- A button to load and show file list

print("=== test_loadinputrecording.lua script loaded ===")

local frameCount = 0
local lastXState = false
local lastYState = false
local lastAState = false
local loadStatus = ""
local loadStatusFrame = 0
local playbackActive = false
local playbackFrame = 0
local lastPlaybackCheck = 0

function beforeframe()
    frameCount = frameCount + 1
    
    -- Get button states
    local xPressed = isxboxbuttonpressed(0, "X")
    local yPressed = isxboxbuttonpressed(0, "Y")
    local aPressed = isxboxbuttonpressed(0, "A")
    
    -- Load default recording with X button (edge detect)
    if xPressed and not lastXState then
        local filename = "test_recording.txt"
        local success = loadinputrecording(filename)
        
        if success then
            loadStatus = "Loaded: " .. filename
            playbackActive = true
            playbackFrame = frameCount
            print("*** FILE LOADED: " .. filename .. " ***")
        else
            loadStatus = "Load FAILED: " .. filename
            playbackActive = false
        end
        loadStatusFrame = frameCount
    end
    lastXState = xPressed
    
    -- Load with custom filename using Y button (edge detect)
    -- Searches for numbered recordings (recording_1.txt, recording_846.txt, etc.) and custom names
    if yPressed and not lastYState then
        local found = false
        local loadedFilename = ""
        
        -- First, try numbered recordings around current frame (likely to be recent saves)
        -- Search from current frame backwards, then forwards, then 1-1000
        local searchOrder = {}
        
        -- Add current frame and nearby frames first (most likely to exist)
        for offset = 0, 500, 50 do
            local testFrame = frameCount - offset
            if testFrame > 0 then
                table.insert(searchOrder, testFrame)
            end
        end
        
        -- Add forward frames
        for offset = 50, 500, 50 do
            local testFrame = frameCount + offset
            if testFrame <= 10000 then
                table.insert(searchOrder, testFrame)
            end
        end
        
        -- Add sequential 1-1000
        for i = 1, 1000 do
            table.insert(searchOrder, i)
        end
        
        -- Try all numbered recordings in search order
        for _, num in ipairs(searchOrder) do
            local filename = string.format("recording_%d.txt", num)
            if loadinputrecording(filename) then
                loadedFilename = filename
                found = true
                print("*** FILE LOADED: " .. filename .. " ***")
                break
            end
        end
        
        -- If no numbered file found, try custom test names (but NOT test_recording.txt)
        if not found then
            local customNames = {
                "my_recording.txt",
                "custom_recording.txt",
                "recording.txt"
            }
            
            for i, filename in ipairs(customNames) do
                if loadinputrecording(filename) then
                    loadedFilename = filename
                    found = true
                    print("*** FILE LOADED: " .. filename .. " ***")
                    break
                end
            end
        end
        
        if found then
            loadStatus = "Loaded: " .. loadedFilename
            playbackActive = true
            playbackFrame = frameCount
        else
            loadStatus = "No recording files found"
            playbackActive = false
        end
        loadStatusFrame = frameCount
    end
    lastYState = yPressed
    
    -- Load with user input using A button (edge detect)
    -- Tries common test filenames
    if aPressed and not lastAState then
        local testFiles = {
            "test_recording.txt",
            "my_recording.txt",
            "custom_recording.txt",
            "recording.txt"
        }
        
        local loaded = false
        for i, filename in ipairs(testFiles) do
            if loadinputrecording(filename) then
                loadStatus = "Loaded: " .. filename
                playbackActive = true
                playbackFrame = frameCount
                loaded = true
                print("*** FILE LOADED: " .. filename .. " ***")
                break
            end
        end
        
        if not loaded then
            loadStatus = "No files found to load"
            playbackActive = false
        end
        loadStatusFrame = frameCount
    end
    lastAState = aPressed
    
    -- Clear load status after 3 seconds (180 frames at 60fps)
    if loadStatus ~= "" and (frameCount - loadStatusFrame) > 180 then
        loadStatus = ""
    end
end

function gui()
    -- Display status
    drawtext(4, 4, "Load Input Recording Test", 0x20)
    drawtext(4, 12, string.format("Frame: %d", frameCount), 0x2D)
    
    -- Load status
    if loadStatus ~= "" then
        local color = 0x37  -- Green for success
        if string.find(loadStatus, "FAILED") or string.find(loadStatus, "No files") then
            color = 0x29  -- Red for error
        end
        drawtext(4, 20, loadStatus, color)
        -- Show the attempted filename for debugging
        if string.find(loadStatus, "FAILED") then
            local attemptedFile = string.match(loadStatus, "FAILED: (.+)")
            if attemptedFile then
                drawtext(4, 44, "Tried: " .. attemptedFile, 0x2D)
            end
        end
    else
        drawtext(4, 20, "Ready to load", 0x2D)
    end
    
    -- Playback status
    if playbackActive then
        local elapsedFrames = frameCount - playbackFrame
        drawtext(4, 28, "PLAYBACK ACTIVE", 0x37)
        drawtext(4, 36, string.format("Playing for: %d frames", elapsedFrames), 0x39)
    else
        drawtext(4, 28, "No playback", 0x2D)
        if loadStatus ~= "" and not string.find(loadStatus, "FAILED") and not string.find(loadStatus, "No files") then
            -- If we loaded successfully but playback isn't active, something might be wrong
            drawtext(4, 36, "Check console for errors", 0x29)
        end
    end
    
    -- Instructions
    drawtext(4, 52, "X: Load test_recording.txt", 0x2D)
    drawtext(4, 60, "Y: Search numbered & custom", 0x2D)
    drawtext(4, 68, "A: Try common test names", 0x2D)
    
    -- Show current button state
    local buttons = getjoypad(0)
    local buttonNames = getbuttonname(buttons)
    if buttonNames == "" then
        buttonNames = "(none)"
    end
    drawtext(4, 84, "Buttons: " .. buttonNames, 0x39)
    
    -- Debug: Show button detection status
    local xStatus = isxboxbuttonpressed(0, "X") and "PRESSED" or "released"
    local yStatus = isxboxbuttonpressed(0, "Y") and "PRESSED" or "released"
    local aStatus = isxboxbuttonpressed(0, "A") and "PRESSED" or "released"
    drawtext(4, 92, string.format("X: %s  Y: %s", xStatus, yStatus), 0x2D)
    drawtext(4, 100, string.format("A: %s", aStatus), 0x2D)
    
    -- Visual indicator
    if playbackActive then
        drawtext(4, 108, ">>> PLAYBACK <<<", 0x37)
    end
    
    -- Show file path info
    drawtext(4, 124, "Files loaded from:", 0x2D)
    drawtext(4, 132, "hdd1:\\fce360-enhanced\\lua\\recordings\\", 0x39)
    
    -- Tips
    drawtext(4, 148, "Tip: Use saveinputrecording", 0x2D)
    drawtext(4, 156, "to create test files first", 0x2D)
end

