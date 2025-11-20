#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_fileio.h"
#include "fceulua.h"
#include "fceu.h"
#include "types.h"

#include <map>
#include <vector>
#include <string>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// ============================================================================
// Helper Functions
// ============================================================================

// Helper function to create all parent directories in a path
void Lua_FileIOCreateParentDirectories(const char* filepath) {
	char path[512];
	strncpy(path, filepath, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	
	// Find the last backslash (directory separator)
	const char* lastSlash = strrchr(path, '\\');
	if (!lastSlash) {
		return;  // No directory path to create
	}
	
	// Extract directory path
	size_t dirLen = lastSlash - path;
	if (dirLen == 0) {
		return;  // Root path, nothing to create
	}
	
	char dirPath[512];
	strncpy(dirPath, path, dirLen);
	dirPath[dirLen] = '\0';
	
	// Create directories by walking through each level
	// For path like "hdd1:\\fce360-enhanced\\lua", we need to create:
	// 1. "hdd1:\\fce360-enhanced"
	// 2. "hdd1:\\fce360-enhanced\\lua"
	for (size_t i = 0; i < dirLen; i++) {
		if (dirPath[i] == '\\' && i > 0) {
			// Create directory up to this point
			char tempPath[512];
			if (i < sizeof(tempPath)) {
				strncpy(tempPath, dirPath, i);
				tempPath[i] = '\0';
				
				// Skip if it's just a drive letter (e.g., "hdd1:")
				if (strlen(tempPath) > 0 && tempPath[strlen(tempPath) - 1] != ':') {
					CreateDirectoryA(tempPath, NULL);
				}
			}
		}
	}
	
	// Create the final directory path
	CreateDirectoryA(dirPath, NULL);
}

// ============================================================================
// Lua File I/O Functions
// ============================================================================

// readfile(filename) -> string or nil
static int lua_readfile(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "readfile(filename) requires 1 argument");
	}
	
	const char* filename = luaL_checkstring(L, 1);
	if (!filename || strlen(filename) == 0) {
		lua_pushnil(L);
		return 1;
	}
	
	// Build full path - try game: directory first
	char fullpath[512];
	
	// If filename already contains a drive/path, use it as-is
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		// Normalize path separators
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
		
		// Normalize path separators (convert / to \)
		for (int i = 0; fullpath[i] != '\0'; i++) {
			if (fullpath[i] == '/') {
				fullpath[i] = '\\';
			}
		}
	}
	
	// Try to open file
	FILE* file = fopen(fullpath, "rb");  // Open in binary mode to read any file type
	if (!file) {
		// Try alternative paths if initial path fails
		const char* altPaths[] = {
			"game:\\lua\\%s",
			"game:\\Lua\\%s",
			"hdd1:\\fce360-enhanced\\lua\\%s",
			"hdd1:\\fce360-enhanced\\Lua\\%s",
			"game:\\%s"
		};
		
		bool found = false;
		for (int i = 0; i < (int)(sizeof(altPaths) / sizeof(altPaths[0])); i++) {
			char altPath[512];
			snprintf(altPath, sizeof(altPath), altPaths[i], filename);
			
			// Normalize path separators
			for (int j = 0; altPath[j] != '\0'; j++) {
				if (altPath[j] == '/') {
					altPath[j] = '\\';
				}
			}
			
			file = fopen(altPath, "rb");
			if (file) {
				strncpy(fullpath, altPath, sizeof(fullpath) - 1);
				fullpath[sizeof(fullpath) - 1] = '\0';
				found = true;
				break;
			}
		}
		
		if (!found) {
			// File not found
			lua_pushnil(L);
			return 1;
		}
	}
	
	// Get file size
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	if (fileSize < 0) {
		fclose(file);
		lua_pushnil(L);
		return 1;
	}
	
	// Read file contents
	if (fileSize == 0) {
		// Empty file - return empty string
		fclose(file);
		lua_pushstring(L, "");
		return 1;
	}
	
	// Allocate buffer for file contents
	char* buffer = (char*)malloc(fileSize + 1);
	if (!buffer) {
		fclose(file);
		lua_pushnil(L);
		return 1;
	}
	
	// Read file
	size_t bytesRead = fread(buffer, 1, fileSize, file);
	fclose(file);
	
	if (bytesRead != (size_t)fileSize) {
		free(buffer);
		lua_pushnil(L);
		return 1;
	}
	
	// Null-terminate (for text files, though we support binary too)
	buffer[fileSize] = '\0';
	
	// Push as Lua string (Lua strings can contain binary data)
	lua_pushlstring(L, buffer, fileSize);
	
	// Free buffer
	free(buffer);
	
	return 1;
}

// fileexists(filename) -> boolean
static int lua_fileexists(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "fileexists(filename) requires 1 argument");
	}
	
	const char* filename = luaL_checkstring(L, 1);
	if (!filename || strlen(filename) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Build full path - try game: directory first
	char fullpath[512];
	
	// If filename already contains a drive/path, use it as-is
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Use GetFileAttributesA to check if file exists (more efficient than fopen)
	// GetFileAttributesA is available via stdafx.h on Xbox
	DWORD fileAttributes = GetFileAttributesA(fullpath);
	if (fileAttributes != 0xFFFFFFFF && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		// File exists and is not a directory
		lua_pushboolean(L, 1);
		return 1;
	}
	
	// Try alternative paths if initial path fails
	const char* altPaths[] = {
		"game:\\lua\\%s",
		"game:\\Lua\\%s",
		"hdd1:\\fce360-enhanced\\lua\\%s",
		"hdd1:\\fce360-enhanced\\Lua\\%s",
		"game:\\%s"
	};
	
	for (int i = 0; i < (int)(sizeof(altPaths) / sizeof(altPaths[0])); i++) {
		char altPath[512];
		snprintf(altPath, sizeof(altPath), altPaths[i], filename);
		
		// Normalize path separators
		for (int j = 0; altPath[j] != '\0'; j++) {
			if (altPath[j] == '/') {
				altPath[j] = '\\';
			}
		}
		
		fileAttributes = GetFileAttributesA(altPath);
		if (fileAttributes != 0xFFFFFFFF && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// File exists and is not a directory
			lua_pushboolean(L, 1);
			return 1;
		}
	}
	
	// File not found
	lua_pushboolean(L, 0);
	return 1;
}

// listfiles(path) -> table
static int lua_listfiles(lua_State* L)
{
	int n = lua_gettop(L);
	const char* path = NULL;
	
	// Get path parameter (optional, defaults to "game:\")
	if (n >= 1) {
		path = luaL_checkstring(L, 1);
	}
	
	// Build full path
	char fullpath[512];
	
	if (!path || strlen(path) == 0) {
		// Default to game: directory
		strncpy(fullpath, "game:\\", sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else if (strchr(path, ':') || path[0] == '\\' || path[0] == '/') {
		// Absolute path
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, path);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Ensure path ends with backslash for directory listing
	size_t pathLen = strlen(fullpath);
	if (pathLen > 0 && fullpath[pathLen - 1] != '\\') {
		if (pathLen < sizeof(fullpath) - 1) {
			fullpath[pathLen] = '\\';
			fullpath[pathLen + 1] = '\0';
		}
	}
	
	// Build search pattern (directory path + wildcard)
	char searchPattern[512];
	snprintf(searchPattern, sizeof(searchPattern), "%s*", fullpath);
	
	// Use Win32 API to find files
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPattern, &findData);
	
	if (hFind == INVALID_HANDLE_VALUE) {
		// Directory not found or error - return empty table
		lua_newtable(L);
		return 1;
	}
	
	// Create Lua table for results
	lua_newtable(L);
	int tableIndex = 1;
	
	// Iterate through files
	do {
		// Skip "." and ".." entries
		if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
			continue;
		}
		
		// Only include files (not directories)
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Add filename to table
			lua_pushstring(L, findData.cFileName);
			lua_rawseti(L, -2, tableIndex);
			tableIndex++;
		}
	} while (FindNextFileA(hFind, &findData) != 0);
	
	// Close the search handle
	FindClose(hFind);
	
	return 1;  // Return the table
}

// listdir(path) -> table
static int lua_listdir(lua_State* L)
{
	int n = lua_gettop(L);
	const char* path = NULL;
	
	// Get path parameter (optional, defaults to "game:\")
	if (n >= 1) {
		path = luaL_checkstring(L, 1);
	}
	
	// Build full path
	char fullpath[512];
	
	if (!path || strlen(path) == 0) {
		// Default to game: directory
		strncpy(fullpath, "game:\\", sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else if (strchr(path, ':') || path[0] == '\\' || path[0] == '/') {
		// Absolute path
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, path);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Ensure path ends with backslash for directory listing
	size_t pathLen = strlen(fullpath);
	if (pathLen > 0 && fullpath[pathLen - 1] != '\\') {
		if (pathLen < sizeof(fullpath) - 1) {
			fullpath[pathLen] = '\\';
			fullpath[pathLen + 1] = '\0';
		}
	}
	
	// Build search pattern (directory path + wildcard)
	char searchPattern[512];
	snprintf(searchPattern, sizeof(searchPattern), "%s*", fullpath);
	
	// Use Win32 API to find directories
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPattern, &findData);
	
	if (hFind == INVALID_HANDLE_VALUE) {
		// Directory not found or error - return empty table
		lua_newtable(L);
		return 1;
	}
	
	// Create Lua table for results
	lua_newtable(L);
	int tableIndex = 1;
	
	// Iterate through directories
	do {
		// Skip "." and ".." entries
		if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
			continue;
		}
		
		// Only include directories (not files)
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Add directory name to table
			lua_pushstring(L, findData.cFileName);
			lua_rawseti(L, -2, tableIndex);
			tableIndex++;
		}
	} while (FindNextFileA(hFind, &findData) != 0);
	
	// Close the search handle
	FindClose(hFind);
	
	return 1;  // Return the table
}

// mkdir(path) -> boolean
static int lua_mkdir(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "mkdir(path) requires 1 argument");
	}
	
	const char* path = luaL_checkstring(L, 1);
	if (!path || strlen(path) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Build full path
	char fullpath[512];
	
	// If path already contains a drive/path, use it as-is
	if (strchr(path, ':') || path[0] == '\\' || path[0] == '/') {
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, path);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Remove trailing backslash if present (for directory creation)
	size_t pathLen = strlen(fullpath);
	if (pathLen > 0 && fullpath[pathLen - 1] == '\\') {
		fullpath[pathLen - 1] = '\0';
		pathLen--;
	}
	
	if (pathLen == 0) {
		// Empty path after normalization
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Create parent directories recursively
	// Walk through each level and create directories
	for (size_t i = 0; i < pathLen; i++) {
		if (fullpath[i] == '\\' && i > 0) {
			// Create directory up to this point
			char tempPath[512];
			if (i < sizeof(tempPath)) {
				strncpy(tempPath, fullpath, i);
				tempPath[i] = '\0';
				
				// Skip if it's just a drive letter (e.g., "hdd1:")
				if (strlen(tempPath) > 0 && tempPath[strlen(tempPath) - 1] != ':') {
					CreateDirectoryA(tempPath, NULL);
				}
			}
		}
	}
	
	// Create the final directory
	BOOL success = CreateDirectoryA(fullpath, NULL);
	
	// Check if directory was created or already exists
	if (success) {
		// Directory created successfully
		lua_pushboolean(L, 1);
		return 1;
	} else {
		// Check if directory already exists
		DWORD error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS) {
			// Directory already exists - check if it's actually a directory
			DWORD attrs = GetFileAttributesA(fullpath);
			if (attrs != 0xFFFFFFFF && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
				// It's a directory, so success
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		
		// Failed to create directory
		lua_pushboolean(L, 0);
		return 1;
	}
}

// rmfile(filename) -> boolean
static int lua_rmfile(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "rmfile(filename) requires 1 argument");
	}
	
	const char* filename = luaL_checkstring(L, 1);
	if (!filename || strlen(filename) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Build full path
	char fullpath[512];
	
	// If filename already contains a drive/path, use it as-is
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Use Win32 API to delete file
	BOOL success = DeleteFileA(fullpath);
	
	if (success) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		// Check if file doesn't exist (not an error for deletion)
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
			// File doesn't exist - return true (idempotent)
			lua_pushboolean(L, 1);
			return 1;
		}
		
		// Failed to delete file
		lua_pushboolean(L, 0);
		return 1;
	}
}

// rmdir(path) -> boolean
static int lua_rmdir(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "rmdir(path) requires 1 argument");
	}
	
	const char* path = luaL_checkstring(L, 1);
	if (!path || strlen(path) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Build full path
	char fullpath[512];
	
	// If path already contains a drive/path, use it as-is
	if (strchr(path, ':') || path[0] == '\\' || path[0] == '/') {
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, path);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Remove trailing backslash if present (for directory deletion)
	size_t pathLen = strlen(fullpath);
	if (pathLen > 0 && fullpath[pathLen - 1] == '\\') {
		fullpath[pathLen - 1] = '\0';
		pathLen--;
	}
	
	if (pathLen == 0) {
		// Empty path after normalization
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Use Win32 API to delete directory
	BOOL success = RemoveDirectoryA(fullpath);
	
	if (success) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		// Check if directory doesn't exist (not an error for deletion)
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
			// Directory doesn't exist - return true (idempotent)
			lua_pushboolean(L, 1);
			return 1;
		}
		
		// Failed to delete directory (may be non-empty or in use)
		lua_pushboolean(L, 0);
		return 1;
	}
}

// writefile(filename, data) -> boolean
static int lua_writefile(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "writefile(filename, data) requires 2 arguments");
	}
	
	const char* filename = luaL_checkstring(L, 1);
	const char* data = luaL_checkstring(L, 2);
	
	if (!filename || strlen(filename) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	if (!data) {
		// Allow empty data (write empty file)
		data = "";
	}
	
	// Build full path
	char fullpath[512];
	
	// If filename already contains a drive/path, use it as-is
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to writable directory (try hdd1: first as it's always writable)
		// Prefer hdd1: for writing as it's always writable
		const char* baseDir = "hdd1:\\fce360-enhanced\\lua\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Create all parent directories recursively
	Lua_FileIOCreateParentDirectories(fullpath);
	
	// Use Win32 API for file writing (better compatibility with Xbox 360 paths like hdd1:)
	HANDLE hFile = CreateFileA(fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		// If hdd1: failed and path was relative, try game: directory
		if (!strchr(filename, ':') && filename[0] != '\\' && filename[0] != '/') {
			const char* gameDir = "game:\\";
			snprintf(fullpath, sizeof(fullpath), "%s%s", gameDir, filename);
			
			// Normalize path separators
			for (int i = 0; fullpath[i] != '\0'; i++) {
				if (fullpath[i] == '/') {
					fullpath[i] = '\\';
				}
			}
			
			// Create all parent directories recursively
			Lua_FileIOCreateParentDirectories(fullpath);
			
			hFile = CreateFileA(fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		
		if (hFile == INVALID_HANDLE_VALUE) {
			// Failed to open file for writing
			lua_pushboolean(L, 0);
			return 1;
		}
	}
	
	// Write data to file
	size_t dataLen = strlen(data);
	DWORD bytesWritten = 0;
	BOOL writeSuccess = FALSE;
	
	if (dataLen > 0) {
		writeSuccess = WriteFile(hFile, data, (DWORD)dataLen, &bytesWritten, NULL);
	} else {
		// Empty file - write succeeded (file created)
		writeSuccess = TRUE;
		bytesWritten = 0;
	}
	
	// Close file
	CloseHandle(hFile);
	
	// Check if write was successful
	if (writeSuccess && (dataLen == 0 || bytesWritten == dataLen)) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		// Write failed (partial write or error)
		lua_pushboolean(L, 0);
		return 1;
	}
}

// ============================================================================
// Module Registrar and Lifecycle Hooks
// ============================================================================

void Lua_RegisterFileIO(lua_State* L)
{
	if (!L) {
		return;
	}

	lua_register(L, "readfile", lua_readfile);
	lua_register(L, "writefile", lua_writefile);
	lua_register(L, "fileexists", lua_fileexists);
	lua_register(L, "listfiles", lua_listfiles);
	lua_register(L, "listdir", lua_listdir);
	lua_register(L, "mkdir", lua_mkdir);
	lua_register(L, "rmfile", lua_rmfile);
	lua_register(L, "rmdir", lua_rmdir);
}

void Lua_FileIOReset(void)
{
	// File I/O state is stateless, so no reset needed
	// This function exists for consistency with other modules
}

#endif // USE_LUA

