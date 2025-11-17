# File I/O Functions

The File I/O Functions provide comprehensive file and directory management capabilities. These functions allow you to read and write files, check file existence, list directory contents, and manage directories. All functions support both relative and absolute paths, with automatic path normalization and search path resolution.

## File Reading and Writing Functions

### `readfile`

**Signature:** `readfile(filename)`
Reads an entire file as a string. This function reads files from the game directory and common alternative locations, making it useful for reading configuration files, data files, and text files.

**Parameters:**
- `filename` (string, required): File path to read
  - Can be relative to game directory (e.g., `"config.txt"`)
  - Can be absolute path (e.g., `"game:\\lua\\config.txt"`)
  - Path separators are normalized (`/` converted to `\`)
  - If relative, searches in: `game:\`, `game:\lua\`, `game:\Lua\`, `hdd1:\fce360-enhanced\lua\`, etc.

**Returns:**
- `string` - File contents as a string, or `nil` on error
  - Returns the entire file contents as a Lua string
  - Supports both text and binary files (uses binary mode)
  - Returns empty string (`""`) for empty files
  - Returns `nil` if file is not found or cannot be read
  - File size is limited by available memory

**Notes:**
- Opens files in binary mode (`"rb"`) to support any file type
- Searches multiple locations if file not found in initial path:
  - `game:\` (base directory)
  - `game:\lua\` and `game:\Lua\`
  - `hdd1:\fce360-enhanced\lua\` and `hdd1:\fce360-enhanced\Lua\`
  - `game:\` (fallback)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Useful for:
  - Reading configuration files
  - Loading data files
  - Reading text files
  - Loading script data
- File contents are loaded entirely into memory
- Returns `nil` on any error (file not found, read error, memory allocation failure)
- Empty files return empty string (`""`), not `nil`

**Example: Basic Usage:**
```lua
-- Read a config file
local config = readfile("config.txt")
if config then
    print(string.format("Config file loaded: %d bytes", string.len(config)))
    -- Process config content
else
    print("Config file not found")
end
```

**Example: Reading Configuration:**
```lua
function onload()
    -- Try to read settings file
    local settings = readfile("settings.txt")
    
    if settings then
        -- Parse settings (example: simple key=value format)
        for line in string.gmatch(settings, "[^\r\n]+") do
            local key, value = string.match(line, "([^=]+)=(.+)")
            if key and value then
                print(string.format("Setting: %s = %s", key, value))
            end
        end
    else
        print("Settings file not found, using defaults")
    end
end
```

**Example: Loading Data File:**
```lua
function script()
    -- Load level data
    local levelData = readfile("level1.dat")
    
    if levelData then
        -- Process binary or text data
        local dataSize = string.len(levelData)
        drawtext(4, 4, string.format("Level data: %d bytes", dataSize), 0x37)
        
        -- Example: Read first byte as number
        if dataSize > 0 then
            local firstByte = string.byte(levelData, 1)
            drawtext(4, 14, string.format("First byte: %d", firstByte), 0x37)
        end
    end
end
```

**Example: Reading from Lua Directory:**
```lua
-- Read a file from the lua directory
local scriptData = readfile("lua\\helper.lua")
if scriptData then
    print("Helper script loaded")
    -- Could execute or parse the script
end
```

**Example: Error Handling:**
```lua
local content = readfile("myfile.txt")
if content then
    -- File was successfully read
    print(string.format("File content (%d bytes):", string.len(content)))
    print(content)
else
    -- File not found or error reading
    print("Error: Could not read myfile.txt")
    print("Check that file exists in game: directory or lua subdirectory")
end
```

**Example: Reading Text File Line by Line:**
```lua
local fileContent = readfile("data.txt")
if fileContent then
    -- Split into lines
    local lines = {}
    for line in string.gmatch(fileContent, "[^\r\n]+") do
        table.insert(lines, line)
    end
    
    print(string.format("Read %d lines from data.txt", #lines))
    
    -- Process each line
    for i = 1, #lines do
        print(string.format("Line %d: %s", i, lines[i]))
    end
end
```

### `writefile`

**Signature:** `writefile(filename, data)`
Writes a string to a file. This function writes files to writable locations, automatically creating parent directories as needed. Uses Win32 API for better compatibility with Xbox 360 paths like `hdd1:`.

**Parameters:**
- `filename` (string, required): File path to write to
  - Can be relative to writable directory (e.g., `"output.txt"`)
  - Can be absolute path (e.g., `"hdd1:\\fce360-enhanced\\lua\\output.txt"` or `"game:\\output.txt"`)
  - Path separators are normalized (`/` converted to `\`)
  - If relative, writes to: `hdd1:\fce360-enhanced\lua\` (preferred, always writable)
  - Falls back to `game:\` if hdd1: fails (for relative paths only)
- `data` (string, required): Data to write to file
  - Can be any string content (text or binary data)
  - Empty string (`""`) creates an empty file
  - String can contain any binary data (null bytes, etc.)

**Returns:**
- `boolean` - `true` if write was successful, `false` on error
  - Returns `true` if file was successfully written
  - Returns `false` if file cannot be opened, written, or on any error
  - Empty files (data = `""`) return `true` if file is created successfully

**Notes:**
- Uses Win32 API (`CreateFileA`, `WriteFile`, `CloseHandle`) for better Xbox 360 compatibility
- Automatically creates parent directories recursively if they don't exist
- Opens files in binary mode (`"wb"`) to support any file type
- Overwrites existing files (uses `CREATE_ALWAYS` mode)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- For relative paths, prefers `hdd1:\fce360-enhanced\lua\` as it's always writable
- Falls back to `game:\` only if:
  - Path was relative (not absolute)
  - hdd1: write failed
- Useful for:
  - Saving configuration files
  - Writing log files
  - Saving game data
  - Creating output files
  - Storing script-generated data
- Directory creation: All parent directories in the path are created automatically
- File overwrite: Existing files are completely replaced (not appended to)
- Returns `false` on any error (file cannot be opened, write fails, directory creation fails)

**Example: Basic Usage:**
```lua
-- Write a simple text file
local success = writefile("output.txt", "Hello, world!")
if success then
    print("File written successfully")
else
    print("Failed to write file")
end
```

**Example: Saving Configuration:**
```lua
function onload()
    -- Save settings to file
    local settings = "volume=80\nfullscreen=true\nlanguage=en"
    local success = writefile("settings.txt", settings)
    
    if success then
        print("Settings saved successfully")
    else
        print("Failed to save settings")
    end
end
```

**Example: Writing to hdd1: Location:**
```lua
-- Write to hdd1: (always writable on Xbox 360)
local data = "This file is saved to hdd1:"
local success = writefile("hdd1:\\fce360-enhanced\\lua\\saved_data.txt", data)

if success then
    print("File saved to hdd1: successfully")
    -- Read it back to verify
    local content = readfile("hdd1:\\fce360-enhanced\\lua\\saved_data.txt")
    if content == data then
        print("Write and read verified!")
    end
else
    print("Failed to write to hdd1:")
end
```

**Example: Writing Log File:**
```lua
local logEntries = {}
local logCount = 0

function script()
    -- Add log entry
    logCount = logCount + 1
    table.insert(logEntries, string.format("[Frame %d] Game state: %s", logCount, "running"))
    
    -- Save log every 60 frames
    if logCount % 60 == 0 then
        local logContent = table.concat(logEntries, "\n")
        local success = writefile("game_log.txt", logContent)
        if success then
            print(string.format("Log saved: %d entries", #logEntries))
        end
    end
end
```

**Example: Writing Binary Data:**
```lua
-- Write binary data (e.g., save game state)
local saveData = ""
for i = 1, 100 do
    saveData = saveData .. string.char(i % 256)  -- Binary data
end

local success = writefile("savegame.dat", saveData)
if success then
    print(string.format("Save game written: %d bytes", string.len(saveData)))
end
```

**Example: Error Handling:**
```lua
local data = "Important data to save"
local success = writefile("important.txt", data)

if success then
    print("File saved successfully")
    -- Verify by reading it back
    local content = readfile("important.txt")
    if content == data then
        print("Write verified: content matches")
    else
        print("Warning: Written content doesn't match!")
    end
else
    print("Error: Could not write file")
    print("Check that the directory is writable")
    print("Try using hdd1: path for guaranteed writability")
end
```

**Example: Writing Empty File:**
```lua
-- Create an empty file (e.g., marker file)
local success = writefile("marker.txt", "")
if success then
    print("Empty marker file created")
    -- Verify it's empty
    local content = readfile("marker.txt")
    if content == "" then
        print("File is correctly empty")
    end
end
```

## File Information Functions

### `fileexists`

**Signature:** `fileexists(filename)`
Checks if a file exists. This function efficiently checks for file existence without opening the file, making it useful for conditional file operations and validation before reading or writing files.

**Parameters:**
- `filename` (string, required): File path to check
  - Can be relative to game directory (e.g., `"config.txt"`)
  - Can be absolute path (e.g., `"game:\\lua\\config.txt"` or `"hdd1:\\fce360-enhanced\\lua\\config.txt"`)
  - Path separators are normalized (`/` converted to `\`)
  - If relative, searches in: `game:\`, `game:\lua\`, `game:\Lua\`, `hdd1:\fce360-enhanced\lua\`, etc.
  - Empty string (`""`) returns `false`

**Returns:**
- `boolean` - `true` if file exists, `false` otherwise
  - Returns `true` if file exists and is a file (not a directory)
  - Returns `false` if file doesn't exist, is a directory, or on error
  - Returns `false` for empty filename

**Notes:**
- Uses Win32 API (`GetFileAttributesA`) for efficient file existence checking
- More efficient than trying to open the file with `fopen` or `readfile`
- Searches multiple locations if file not found in initial path:
  - `game:\` (base directory)
  - `game:\lua\` and `game:\Lua\`
  - `hdd1:\fce360-enhanced\lua\` and `hdd1:\fce360-enhanced\Lua\`
  - `game:\` (fallback)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Directory check: Returns `false` if path points to a directory (only files return `true`)
- Useful for:
  - Checking if config files exist before reading
  - Validating file paths before operations
  - Conditional file operations
  - Avoiding errors when reading non-existent files
  - Checking if save files exist
- Returns `false` on any error (file not found, path is directory, invalid path)
- Does not distinguish between "file doesn't exist" and "path is a directory" - both return `false`

**Example: Basic Usage:**
```lua
-- Check if a file exists
if fileexists("config.txt") then
    print("Config file exists")
    local config = readfile("config.txt")
    -- Process config
else
    print("Config file not found, using defaults")
end
```

**Example: Conditional File Reading:**
```lua
function onload()
    -- Only read file if it exists
    if fileexists("settings.txt") then
        local settings = readfile("settings.txt")
        if settings then
            print("Settings loaded from file")
            -- Parse settings
        end
    else
        print("Settings file not found, using default settings")
    end
end
```

**Example: Checking Before Writing:**
```lua
-- Check if file exists before overwriting
local filename = "important_data.txt"
if fileexists(filename) then
    print("Warning: File already exists, will be overwritten")
    -- Ask for confirmation or backup
end

local success = writefile(filename, "new data")
if success then
    print("File written successfully")
end
```

**Example: Save File Check:**
```lua
function script()
    -- Check if save file exists
    local saveFile = "savegame.dat"
    if fileexists(saveFile) then
        drawtext(4, 4, "Save file exists", 0x37)
        -- Offer to load save
    else
        drawtext(4, 4, "No save file found", 0x29)
        -- Show "New Game" option
    end
end
```

**Example: Multiple File Checks:**
```lua
-- Check multiple files
local filesToCheck = {"config.txt", "data.dat", "log.txt"}
local existingFiles = {}

for i, filename in ipairs(filesToCheck) do
    if fileexists(filename) then
        table.insert(existingFiles, filename)
        print(string.format("Found: %s", filename))
    else
        print(string.format("Missing: %s", filename))
    end
end

print(string.format("Found %d of %d files", #existingFiles, #filesToCheck))
```

**Example: Error Prevention:**
```lua
-- Avoid errors by checking file existence first
local filename = "user_data.txt"
if fileexists(filename) then
    local data = readfile(filename)
    if data then
        -- Process data safely
        print(string.format("Loaded %d bytes from %s", string.len(data), filename))
    end
else
    print(string.format("File %s does not exist, skipping read", filename))
end
```

**Example: With Absolute Paths:**
```lua
-- Check file in specific location
local absolutePath = "hdd1:\\fce360-enhanced\\lua\\saved_data.txt"
if fileexists(absolutePath) then
    print("Save file found in hdd1:")
    local data = readfile(absolutePath)
    -- Process saved data
else
    print("No save file in hdd1:, starting fresh")
end
```

**Example: Directory vs File Check:**
```lua
-- Note: fileexists returns false for directories
local path = "game:\\lua"
if fileexists(path) then
    print("Path is a file")
else
    -- Could be a directory or doesn't exist
    print("Path is not a file (may be directory or doesn't exist)")
end
```

## Directory Listing Functions

### `listfiles`

**Signature:** `listfiles([path])`
Lists files in a directory. This function returns a table of filenames found in the specified directory, making it useful for directory browsing, file discovery, and dynamic file operations.

**Parameters:**
- `path` (string, optional, default: `"game:\"`): Directory path to list
  - If not provided or empty string, defaults to `"game:\"`
  - Can be relative to game directory (e.g., `"lua"` becomes `"game:\lua\"`)
  - Can be absolute path (e.g., `"game:\lua\"` or `"hdd1:\fce360-enhanced\lua\"`)
  - Path separators are normalized (`/` converted to `\`)
  - Trailing backslash is optional (automatically added if missing)

**Returns:**
- `table` - 1-indexed array of filename strings
  - Returns a Lua table with filenames (not full paths, just filenames)
  - Table is 1-indexed (Lua array style)
  - Each element is a string containing a filename
  - Returns empty table (`{}`) if directory doesn't exist, is empty, or on error
  - Only includes files (directories are excluded)
  - Skips "." and ".." entries

**Notes:**
- Uses Win32 API (`FindFirstFileA`, `FindNextFileA`, `FindClose`) for directory enumeration
- Only returns filenames, not full paths or directories
- Files are returned in the order found by the filesystem (not sorted)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Relative paths are resolved relative to `game:\`
- Directory existence: Returns empty table if directory doesn't exist (no error thrown)
- File filtering: Only includes files (excludes directories and special entries)
- Useful for:
  - Discovering available files in a directory
  - Building file browsers
  - Dynamic file loading
  - Checking what files are available
  - Iterating through directory contents
- Returns empty table on any error (directory not found, access denied, etc.)
- Does not return subdirectories - only files in the specified directory

**Example: Basic Usage:**
```lua
-- List files in default directory (game:\)
local files = listfiles()
print(string.format("Found %d files", #files))

for i = 1, #files do
    print(string.format("  %d. %s", i, files[i]))
end
```

**Example: List Files in Specific Directory:**
```lua
-- List files in lua directory
local luaFiles = listfiles("lua")
print(string.format("Found %d Lua files", #luaFiles))

for i = 1, #luaFiles do
    print(string.format("  - %s", luaFiles[i]))
end
```

**Example: With Absolute Path:**
```lua
-- List files using absolute path
local files = listfiles("hdd1:\\fce360-enhanced\\lua\\")
print(string.format("Files in hdd1: directory: %d", #files))

for i = 1, #files do
    print(files[i])
end
```

**Example: File Discovery:**
```lua
function onload()
    -- Discover available config files
    local configFiles = {}
    local allFiles = listfiles("game:\\")
    
    for i = 1, #allFiles do
        local filename = allFiles[i]
        -- Check if it's a config file
        if string.find(filename, "%.txt$") or string.find(filename, "%.cfg$") then
            table.insert(configFiles, filename)
        end
    end
    
    print(string.format("Found %d config files", #configFiles))
    for i = 1, #configFiles do
        print(string.format("  - %s", configFiles[i]))
    end
end
```

**Example: Dynamic File Loading:**
```lua
-- Load all data files from a directory
local dataFiles = listfiles("data")
local loadedData = {}

for i = 1, #dataFiles do
    local filename = dataFiles[i]
    local content = readfile("data\\" .. filename)
    if content then
        loadedData[filename] = content
        print(string.format("Loaded: %s (%d bytes)", filename, string.len(content)))
    end
end

print(string.format("Loaded %d data files", #dataFiles))
```

**Example: File Browser:**
```lua
function script()
    local files = listfiles()
    local y = 4
    
    drawtext(4, y, string.format("Files in directory: %d", #files), 0x27)
    y = y + 12
    
    -- Display first 10 files
    for i = 1, math.min(#files, 10) do
        local filename = files[i]
        -- Truncate long filenames
        if string.len(filename) > 30 then
            filename = string.sub(filename, 1, 27) .. "..."
        end
        drawtext(4, y, string.format("%d. %s", i, filename), 0x37)
        y = y + 10
    end
    
    if #files > 10 then
        drawtext(4, y, string.format("... and %d more", #files - 10), 0x29)
    end
end
```

**Example: Check if Directory Has Files:**
```lua
-- Check if a directory contains any files
local files = listfiles("saves")
if #files > 0 then
    print(string.format("Found %d save files:", #files))
    for i = 1, #files do
        print(string.format("  - %s", files[i]))
    end
else
    print("No save files found")
end
```

**Example: Filter Files by Extension:**
```lua
-- List only .lua files
local allFiles = listfiles("lua")
local luaFiles = {}

for i = 1, #allFiles do
    local filename = allFiles[i]
    if string.find(filename, "%.lua$") then
        table.insert(luaFiles, filename)
    end
end

print(string.format("Found %d Lua scripts", #luaFiles))
for i = 1, #luaFiles do
    print(string.format("  - %s", luaFiles[i]))
end
```

**Example: Error Handling:**
```lua
-- Safely list files, handling errors
local files = listfiles("some_directory")
if files then
    if #files > 0 then
        print(string.format("Found %d files", #files))
        -- Process files
    else
        print("Directory is empty or doesn't exist")
    end
else
    print("Error listing files")
end
```

**Example: Integration with fileexists:**
```lua
-- List files and verify they exist
local files = listfiles("game:\\")
local existingFiles = {}

for i = 1, #files do
    if fileexists(files[i]) then
        table.insert(existingFiles, files[i])
    end
end

print(string.format("Listed %d files, %d verified to exist", #files, #existingFiles))
```

### `listdir`

**Signature:** `listdir([path])`
Lists directories in a directory. This function returns a table of directory names found in the specified directory, making it useful for directory browsing, subdirectory discovery, and navigating directory structures.

**Parameters:**
- `path` (string, optional, default: `"game:\"`): Directory path to list
  - If not provided or empty string, defaults to `"game:\"`
  - Can be relative to game directory (e.g., `"lua"` becomes `"game:\lua\"`)
  - Can be absolute path (e.g., `"game:\lua\"` or `"hdd1:\fce360-enhanced\lua\"`)
  - Path separators are normalized (`/` converted to `\`)
  - Trailing backslash is optional (automatically added if missing)

**Returns:**
- `table` - 1-indexed array of directory name strings
  - Returns a Lua table with directory names (not full paths, just directory names)
  - Table is 1-indexed (Lua array style)
  - Each element is a string containing a directory name
  - Returns empty table (`{}`) if directory doesn't exist, is empty, or on error
  - Only includes directories (files are excluded)
  - Skips "." and ".." entries

**Notes:**
- Uses Win32 API (`FindFirstFileA`, `FindNextFileA`, `FindClose`) for directory enumeration
- Only returns directory names, not full paths or files
- Directories are returned in the order found by the filesystem (not sorted)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Relative paths are resolved relative to `game:\`
- Directory existence: Returns empty table if directory doesn't exist (no error thrown)
- Directory filtering: Only includes directories (excludes files and special entries)
- Useful for:
  - Discovering available subdirectories
  - Building directory browsers
  - Navigating directory structures
  - Checking what directories are available
  - Iterating through subdirectories
- Returns empty table on any error (directory not found, access denied, etc.)
- Does not return files - only directories in the specified directory
- Works like `listfiles()` but for directories instead of files

**Example: Basic Usage:**
```lua
-- List directories in default directory (game:\)
local dirs = listdir()
print(string.format("Found %d directories", #dirs))

for i = 1, #dirs do
    print(string.format("  %d. %s", i, dirs[i]))
end
```

**Example: List Directories in Specific Directory:**
```lua
-- List directories in game:\
local subdirs = listdir("game:\\")
print(string.format("Found %d subdirectories", #subdirs))

for i = 1, #subdirs do
    print(string.format("  - %s", subdirs[i]))
end
```

**Example: With Absolute Path:**
```lua
-- List directories using absolute path
local dirs = listdir("hdd1:\\fce360-enhanced\\")
print(string.format("Directories in hdd1: directory: %d", #dirs))

for i = 1, #dirs do
    print(dirs[i])
end
```

**Example: Directory Navigation:**
```lua
function onload()
    -- Discover available directories
    local topLevelDirs = listdir("game:\\")
    
    print(string.format("Found %d top-level directories:", #topLevelDirs))
    for i = 1, #topLevelDirs do
        local dirname = topLevelDirs[i]
        print(string.format("  - %s", dirname))
        
        -- List subdirectories
        local subdirs = listdir("game:\\" .. dirname)
        if #subdirs > 0 then
            print(string.format("    Subdirectories: %d", #subdirs))
        end
    end
end
```

**Example: Directory Browser:**
```lua
function script()
    local dirs = listdir()
    local y = 4
    
    drawtext(4, y, string.format("Directories: %d", #dirs), 0x27)
    y = y + 12
    
    -- Display first 10 directories
    for i = 1, math.min(#dirs, 10) do
        local dirname = dirs[i]
        -- Truncate long directory names
        if string.len(dirname) > 30 then
            dirname = string.sub(dirname, 1, 27) .. "..."
        end
        drawtext(4, y, string.format("%d. %s", i, dirname), 0x37)
        y = y + 10
    end
    
    if #dirs > 10 then
        drawtext(4, y, string.format("... and %d more", #dirs - 10), 0x29)
    end
end
```

**Example: Check if Directory Has Subdirectories:**
```lua
-- Check if a directory contains any subdirectories
local subdirs = listdir("game:\\lua")
if #subdirs > 0 then
    print(string.format("Found %d subdirectories:", #subdirs))
    for i = 1, #subdirs do
        print(string.format("  - %s", subdirs[i]))
    end
else
    print("No subdirectories found")
end
```

**Example: Recursive Directory Discovery:**
```lua
-- Discover all directories recursively (simplified example)
local function listAllDirs(basePath, depth)
    depth = depth or 0
    if depth > 3 then return end  -- Limit recursion depth
    
    local dirs = listdir(basePath)
    for i = 1, #dirs do
        local indent = string.rep("  ", depth)
        print(string.format("%s%s", indent, dirs[i]))
        
        -- Recursively list subdirectories
        local subPath = basePath .. dirs[i] .. "\\"
        listAllDirs(subPath, depth + 1)
    end
end

listAllDirs("game:\\")
```

**Example: Compare listdir vs listfiles:**
```lua
-- List both directories and files
local dirs = listdir("game:\\")
local files = listfiles("game:\\")

print(string.format("Directories: %d", #dirs))
print(string.format("Files: %d", #files))

-- Show directories
if #dirs > 0 then
    print("Directories:")
    for i = 1, #dirs do
        print(string.format("  [DIR] %s", dirs[i]))
    end
end

-- Show files
if #files > 0 then
    print("Files:")
    for i = 1, #files do
        print(string.format("  [FILE] %s", files[i]))
    end
end
```

**Example: Error Handling:**
```lua
-- Safely list directories, handling errors
local dirs = listdir("some_directory")
if dirs then
    if #dirs > 0 then
        print(string.format("Found %d directories", #dirs))
        -- Process directories
    else
        print("Directory is empty or doesn't exist")
    end
else
    print("Error listing directories")
end
```

**Example: Building Directory Paths:**
```lua
-- List directories and build full paths
local basePath = "game:\\"
local dirs = listdir(basePath)

for i = 1, #dirs do
    local fullPath = basePath .. dirs[i] .. "\\"
    print(string.format("Directory: %s", fullPath))
    
    -- List files in each directory
    local files = listfiles(fullPath)
    print(string.format("  Contains %d files", #files))
end
```

## Directory Management Functions

### `mkdir`

**Signature:** `mkdir(path)`
Creates a directory. This function creates the specified directory and all parent directories as needed, making it useful for setting up directory structures for data storage.

**Parameters:**
- `path` (string, required): Directory path to create
  - Can be relative to game directory (e.g., `"mydir"` becomes `"game:\mydir"`)
  - Can be absolute path (e.g., `"game:\mydir"` or `"hdd1:\fce360-enhanced\lua\mydir"`)
  - Path separators are normalized (`/` converted to `\`)
  - Trailing backslash is optional (automatically removed if present)

**Returns:**
- `boolean` - `true` if directory was created successfully or already exists, `false` on error
  - Returns `true` if directory is created successfully
  - Returns `true` if directory already exists (idempotent)
  - Returns `false` if directory cannot be created (permission denied, invalid path, etc.)
  - Returns `false` for empty path

**Notes:**
- Uses Win32 API (`CreateDirectoryA`) for directory creation
- Automatically creates all parent directories recursively if they don't exist
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Relative paths are resolved relative to `game:\`
- Idempotent: Returns `true` if directory already exists (safe to call multiple times)
- Parent directory creation: All parent directories in the path are created automatically
- Useful for:
  - Creating directories for data storage
  - Setting up directory structures
  - Ensuring directories exist before writing files
  - Organizing file storage
- Returns `false` on any error (permission denied, invalid path, etc.)
- Directory must be empty to be deleted (use `rmdir()` to remove)

**Example: Basic Usage:**
```lua
-- Create a simple directory
local success = mkdir("mydir")
if success then
    print("Directory created successfully")
else
    print("Failed to create directory")
end
```

**Example: Create Nested Directories:**
```lua
-- Create nested directory structure (parent dirs created automatically)
local success = mkdir("data\\saves\\slot1")
if success then
    print("Directory structure created")
    -- Now you can write files to this directory
    writefile("data\\saves\\slot1\\save.dat", "save data")
end
```

**Example: With Absolute Path:**
```lua
-- Create directory using absolute path
local success = mkdir("hdd1:\\fce360-enhanced\\lua\\mydata")
if success then
    print("Directory created in hdd1:")
end
```

**Example: Ensure Directory Exists Before Writing:**
```lua
-- Create directory if it doesn't exist, then write file
local dataDir = "game_data"
if mkdir(dataDir) then
    local success = writefile(dataDir .. "\\data.txt", "some data")
    if success then
        print("File written to new directory")
    end
end
```

**Example: Idempotent Behavior:**
```lua
-- Safe to call multiple times
mkdir("mydir")  -- Creates directory
mkdir("mydir")  -- Returns true (already exists)
mkdir("mydir")  -- Returns true (already exists)
```

**Example: Create Multiple Directories:**
```lua
-- Create multiple directories
local dirs = {"saves", "logs", "config"}
for i = 1, #dirs do
    if mkdir(dirs[i]) then
        print(string.format("Created directory: %s", dirs[i]))
    end
end
```

**Example: Error Handling:**
```lua
local dirPath = "important_data"
if mkdir(dirPath) then
    print("Directory created successfully")
    -- Use the directory
else
    print("Failed to create directory")
    print("Check permissions and path validity")
end
```

### `rmdir`

**Signature:** `rmdir(path)`
Deletes a directory. This function removes an empty directory from the filesystem.

**Parameters:**
- `path` (string, required): Directory path to delete
  - Can be relative to game directory (e.g., `"mydir"` becomes `"game:\mydir"`)
  - Can be absolute path (e.g., `"game:\mydir"` or `"hdd1:\fce360-enhanced\lua\mydir"`)
  - Path separators are normalized (`/` converted to `\`)
  - Trailing backslash is optional (automatically removed if present)

**Returns:**
- `boolean` - `true` if directory was deleted successfully or doesn't exist, `false` on error
  - Returns `true` if directory is deleted successfully
  - Returns `true` if directory doesn't exist (idempotent)
  - Returns `false` if directory cannot be deleted (not empty, in use, permission denied, etc.)
  - Returns `false` for empty path

**Notes:**
- Uses Win32 API (`RemoveDirectoryA`) for directory deletion
- Only deletes empty directories (directory must not contain files or subdirectories)
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Relative paths are resolved relative to `game:\`
- Idempotent: Returns `true` if directory doesn't exist (safe to call multiple times)
- Directory must be empty: Use `rmfile()` to delete files first, then delete subdirectories recursively
- Useful for:
  - Cleaning up temporary directories
  - Removing unused directories
  - Directory cleanup operations
- Returns `false` on any error (directory not empty, in use, permission denied, etc.)
- To delete non-empty directories, delete all contents first (files and subdirectories)

**Example: Basic Usage:**
```lua
-- Delete an empty directory
local success = rmdir("mydir")
if success then
    print("Directory deleted successfully")
else
    print("Failed to delete directory (may not be empty)")
end
```

**Example: Delete Directory After Cleaning:**
```lua
-- Delete all files in directory first, then delete directory
local dirPath = "temp_data"
local files = listfiles(dirPath)

-- Delete all files
for i = 1, #files do
    rmfile(dirPath .. "\\" .. files[i])
end

-- Delete subdirectories
local dirs = listdir(dirPath)
for i = 1, #dirs do
    rmdir(dirPath .. "\\" .. dirs[i])
end

-- Now delete the directory
if rmdir(dirPath) then
    print("Directory deleted")
end
```

**Example: With Absolute Path:**
```lua
-- Delete directory using absolute path
local success = rmdir("hdd1:\\fce360-enhanced\\lua\\temp")
if success then
    print("Directory deleted from hdd1:")
end
```

**Example: Idempotent Behavior:**
```lua
-- Safe to call multiple times
rmdir("mydir")  -- Deletes directory
rmdir("mydir")  -- Returns true (doesn't exist)
rmdir("mydir")  -- Returns true (doesn't exist)
```

**Example: Error Handling:**
```lua
local dirPath = "my_directory"
if rmdir(dirPath) then
    print("Directory deleted successfully")
else
    print("Failed to delete directory")
    print("Directory may not be empty or may be in use")
    
    -- Check if directory still exists
    local dirs = listdir()
    for i = 1, #dirs do
        if dirs[i] == dirPath then
            print("Directory still exists - may not be empty")
            break
        end
    end
end
```

**Example: Cleanup Temporary Directory:**
```lua
-- Create, use, then delete temporary directory
local tempDir = "temp_work"
if mkdir(tempDir) then
    -- Use the directory
    writefile(tempDir .. "\\temp.txt", "temporary data")
    
    -- Cleanup: delete file first
    rmfile(tempDir .. "\\temp.txt")
    
    -- Then delete directory
    if rmdir(tempDir) then
        print("Temporary directory cleaned up")
    end
end
```

### `rmfile`

**Signature:** `rmfile(filename)`
Deletes a file. This function removes a file from the filesystem.

**Parameters:**
- `filename` (string, required): File path to delete
  - Can be relative to game directory (e.g., `"data.txt"` becomes `"game:\data.txt"`)
  - Can be absolute path (e.g., `"game:\data.txt"` or `"hdd1:\fce360-enhanced\lua\data.txt"`)
  - Path separators are normalized (`/` converted to `\`)

**Returns:**
- `boolean` - `true` if file was deleted successfully or doesn't exist, `false` on error
  - Returns `true` if file is deleted successfully
  - Returns `true` if file doesn't exist (idempotent)
  - Returns `false` if file cannot be deleted (in use, permission denied, etc.)
  - Returns `false` for empty filename

**Notes:**
- Uses Win32 API (`DeleteFileA`) for file deletion
- Path normalization: Forward slashes (`/`) are converted to backslashes (`\`)
- Absolute paths (containing `:` or starting with `\` or `/`) are used as-is
- Relative paths are resolved relative to `game:\`
- Idempotent: Returns `true` if file doesn't exist (safe to call multiple times)
- Useful for:
  - Cleaning up temporary files
  - Removing old files
  - File cleanup operations
  - Deleting files before recreating them
- Returns `false` on any error (file in use, permission denied, etc.)
- Cannot delete directories (use `rmdir()` for directories)

**Example: Basic Usage:**
```lua
-- Delete a file
local success = rmfile("old_data.txt")
if success then
    print("File deleted successfully")
else
    print("Failed to delete file")
end
```

**Example: Delete File After Reading:**
```lua
-- Read file, process it, then delete it
local data = readfile("temp_data.txt")
if data then
    -- Process data
    print(string.format("Processed %d bytes", string.len(data)))
    
    -- Delete the file
    if rmfile("temp_data.txt") then
        print("Temporary file cleaned up")
    end
end
```

**Example: With Absolute Path:**
```lua
-- Delete file using absolute path
local success = rmfile("hdd1:\\fce360-enhanced\\lua\\old_file.txt")
if success then
    print("File deleted from hdd1:")
end
```

**Example: Idempotent Behavior:**
```lua
-- Safe to call multiple times
rmfile("myfile.txt")  -- Deletes file
rmfile("myfile.txt")  -- Returns true (doesn't exist)
rmfile("myfile.txt")  -- Returns true (doesn't exist)
```

**Example: Delete Multiple Files:**
```lua
-- Delete multiple files
local files = {"file1.txt", "file2.txt", "file3.txt"}
local deleted = 0

for i = 1, #files do
    if rmfile(files[i]) then
        deleted = deleted + 1
    end
end

print(string.format("Deleted %d/%d files", deleted, #files))
```

**Example: Cleanup Old Files:**
```lua
-- Delete all files matching a pattern
local allFiles = listfiles()
local deleted = 0

for i = 1, #allFiles do
    local filename = allFiles[i]
    -- Check if it's a temporary file
    if string.find(filename, "^temp_") then
        if rmfile(filename) then
            deleted = deleted + 1
        end
    end
end

print(string.format("Deleted %d temporary files", deleted))
```

**Example: Error Handling:**
```lua
local filename = "important_file.txt"
if rmfile(filename) then
    print("File deleted successfully")
else
    print("Failed to delete file")
    print("File may be in use or permission denied")
    
    -- Check if file still exists
    if fileexists(filename) then
        print("File still exists - may be in use")
    end
end
```

**Example: Delete Before Recreating:**
```lua
-- Delete old file before creating new one
local filename = "data.txt"
rmfile(filename)  -- Safe even if file doesn't exist

-- Create new file
if writefile(filename, "new data") then
    print("File recreated successfully")
end
```

## See Also

- **[Drawing Functions](Drawing-Functions)** - Functions for drawing on the screen
- **[Memory Functions](Memory-Functions)** - Functions for reading and writing memory
- **[State Management Functions](State-Management-Functions)** - Functions for save/load states
- **[Monitoring Functions](Monitoring-Functions)** - Functions for performance and timing
- **[Home](Home)** - Return to the main wiki page