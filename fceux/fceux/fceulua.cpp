/* FCE Ultra Lua Integration for Xbox 360
 * Implementation
 * 
 * Enhanced for fce360-enhanced
 * GitHub: https://github.com/frankischilling/fce360-enhanced
 * 
 * Contributors:
 * @frankischilling
 * Ced2911 (original Xbox 360 port)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

 #include "../stdafx.h"

 #ifdef USE_LUA
 
#include "fceulua.h"
#include "lua_bindings.h"
#include "fceu.h"
#include "sound.h"
#include "drawing.h"
#include "video.h"
#include "driver.h"
#include "state.h"
#include "ppu.h"
#include "git.h"
#include "cart.h"
#include "file.h"  // BaseDirectory and path helpers
#include "ines.h"
#include "movie.h"
#include "x6502.h"
#include "../xbox/Cemulator.h"
#include "../xbox/input.h"
#ifdef _XBOX
#	include <xtl.h>
#else
#	include <windows.h>
#	include <XInput.h>
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <vector>
#include <map>
#include <string>

// kNTSCFrameRate moved to lua_emulator.cpp

// Extern PPU data for tile rendering
extern uint8 PALRAM[0x20];
extern uint8 PPU[4];
extern uint8 *VPage[8];
extern uint8 *CHRptr[32];

// Extern font data from drawing.cpp
extern uint8 Font6x7[792];
extern int FixJoedChar(uint8 ch);
extern int JoedCharWidth(uint8 ch);

// Extern joypad state from input.cpp
extern uint8 joy[4];

// Extern powerpadbuf from Cemulator.cpp (Xbox input buffer)
extern uint32 powerpadbuf;

// --- Overlay geometry and font metrics ---
enum { OVL_W = 256, OVL_H = 240, GLYPH_H = 8 };

static int s_consoleLineGap = 2; // pixels of extra leading between lines
static inline int CON_LINE_ADV(void) { return GLYPH_H + s_consoleLineGap; }

// Minimal Lua API forward declarations (avoid changing symbol mappings)
extern "C" {
struct lua_State;
const char* lua_tolstring(lua_State* L, int idx, size_t* len);
const char* lua_tostring(lua_State* L, int idx);
int lua_gettop(lua_State* L);
void lua_settop(lua_State* L, int idx);
void lua_pushcfunction(lua_State* L, int (*fn)(lua_State*));
void lua_setglobal(lua_State* L, const char* name);
int luaL_error(lua_State* L, const char* fmt, ...);
int luaL_checkinteger(lua_State* L, int idx);
double luaL_checknumber(lua_State* L, int idx);
const char* luaL_checkstring(lua_State* L, int idx);
void lua_pushinteger(lua_State* L, int n);
}
 
// On-screen status message for debugging (shows last load attempt)
static char g_luaStatusMsg[128] = "Lua: disabled";
// stdafx.h already includes xtl.h which provides GetTickCount() and DWORD
// Script callback interval (ms); default ~33ms (~30 Hz)
// Script interval moved to lua_runtime.cpp

// Mode constants (matching Cemulator::Settings::LuaAutoloadMode enum)
enum { LUA_AUTO_ALL = 0, LUA_AUTO_ONE = 1, LUA_AUTO_NONE = 2 };

// ---- Master kill switch: single owner in this TU only ----
// PRIVATE static - no other translation unit can access this directly
// This is the hard gate that every load/boot path must respect
static volatile int s_luaDisabled = 1;  // Start disabled; UI will enable explicitly
static int s_luaMode = LUA_AUTO_NONE;
static char s_luaOneScript[256] = {0};

// ---- pending selection (UI -> core) ----
static int s_hasPending = 0;
static int s_pendingMode = LUA_AUTO_NONE;
static char s_pendingScript[256] = {0};

// Legacy global for compatibility (maps to s_pendingMode)
int g_pendingLuaMode = -1;
char g_pendingLuaScript[256] = {0};

// ---- Memory watchpoint system ----
// Frame counter moved to lua_emulator.cpp

// Public accessor
extern "C" int FCEU_LuaIsDisabled(void) { 
	return s_luaDisabled; 
}

// Public setter - nukes any running state when disabling
extern "C" void FCEU_LuaSetDisabled(int disabled) {
	s_luaDisabled = disabled ? 1 : 0;
	if (s_luaDisabled) {
		FCEU_LuaKillAll(); // make it a real kill-switch
		strncpy(g_luaStatusMsg, "Lua: disabled", sizeof(g_luaStatusMsg)-1);
		g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
	} else {
		strncpy(g_luaStatusMsg, "Lua: enabled", sizeof(g_luaStatusMsg)-1);
		g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
	}
	printf("FCEU_LuaSetDisabled: %s\n", s_luaDisabled ? "disabled" : "enabled");
}

// Kill all Lua scripts (stops and clears everything)
extern "C" void FCEU_LuaKillAll(void) {
	FCEU_LuaStop();  // This already stops Lua and clears overlays
	Lua_EmulatorReset();  // Reset frame counter and FPS tracking
	Lua_MemoryResetWatchpoints();  // Clear all watchpoints
	Lua_MovieReset();  // Reset movie/input systems
	Lua_InputReset();  // Reset input systems
}

// Status message accessor
extern "C" const char* FCEU_LuaGetStatusMsg(void) {
	return g_luaStatusMsg;
}

// Cemulator UI calls this right before launching a game
extern "C" void FCEU_SetPendingLua(int mode, const char* scriptUtf8OrNull)
{
	s_hasPending = 1;
	s_pendingMode = mode;
	
	// Keep legacy global in sync for compatibility
	g_pendingLuaMode = mode;
	
	if (scriptUtf8OrNull) {
		strncpy(s_pendingScript, scriptUtf8OrNull, sizeof(s_pendingScript)-1);
		s_pendingScript[sizeof(s_pendingScript)-1] = '\0';
		strncpy(g_pendingLuaScript, scriptUtf8OrNull, sizeof(g_pendingLuaScript)-1);
		g_pendingLuaScript[sizeof(g_pendingLuaScript)-1] = '\0';
	} else {
		s_pendingScript[0] = '\0';
		g_pendingLuaScript[0] = '\0';
	}
	printf("FCEU_SetPendingLua: mode=%d, script='%s'\n", mode, scriptUtf8OrNull ? scriptUtf8OrNull : "(null)");
}
 
 // Double-buffered overlay for Lua-drawn content (updated at 30Hz, composited at 60Hz to prevent flicker)
 // Front buffer: currently displayed (what we composite)
 // Back buffer: where Lua draws next frame (only published on success)
 static uint8* s_overlay_front = NULL; // currently displayed
 static uint8* s_overlay_back  = NULL; // where Lua draws next
 
 static void EnsureOverlay() {
	 if (!s_overlay_front) {
		 s_overlay_front = (uint8*)malloc(OVL_W * OVL_H);
		 if (s_overlay_front) {
			 memset(s_overlay_front, 0, OVL_W * OVL_H);
		 }
	 }
	 if (!s_overlay_back) {
		 s_overlay_back = (uint8*)malloc(OVL_W * OVL_H);
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, OVL_W * OVL_H);
		 }
	 }
 }
 
 static inline void CompositeOverlay(uint8* XBuf) {
	 // Blit the front overlay onto the NES frame every frame (prevents flicker when Lua runs at 30Hz)
	 if (!s_overlay_front || !XBuf) return;
	 
	 const int N = OVL_W * OVL_H;
	 const uint8* src = s_overlay_front;
	 for (int i = 0; i < N; ++i) {
		 uint8 v = src[i];
		 if (v) XBuf[i] = v;  // Only overwrite non-zero overlay pixels
	 }
 }
 
 static inline void SwapOverlays() {
	 // Swap back and front buffers (publish the new overlay only on success)
	 uint8* t = s_overlay_front;
	 s_overlay_front = s_overlay_back;
	 s_overlay_back = t;
 }
 
 static void ClearOverlaysIfAny() {
	 EnsureOverlay();
	 if (s_overlay_back)  memset(s_overlay_back,  0, OVL_W * OVL_H);
	 if (s_overlay_front) memset(s_overlay_front, 0, OVL_W * OVL_H);
 }
 
// ---------------- Lua Console (logs + on-screen panel) ----------------
static bool s_consoleVisible = false;
static const int LUA_CONSOLE_X       = 0;
static const int LUA_CONSOLE_Y       = 8;      // wherever you draw the console
static const int LUA_CONSOLE_W_PX    = 256;    // use full width of the overlay
static const int LUA_CONSOLE_MAX_LINES = 256; // Increased to support more console lines
static const int LUA_CONSOLE_LINE_CHARS = 256; // storage capacity per line (chars)
static char s_luaConsoleLines[LUA_CONSOLE_MAX_LINES][LUA_CONSOLE_LINE_CHARS];
static int s_luaConsoleCount = 0;
static int s_consoleScrollOffset = 0; // Scroll offset for console (0 = show most recent)
static int s_consoleScrollOffsetH = 0; // Horizontal scroll offset for console (in pixels, 0 = leftmost)
static bool s_consoleDpadUpLast = false;
static bool s_consoleDpadDownLast = false;
static bool s_consoleDpadLeftLast = false;
static bool s_consoleDpadRightLast = false;
static int s_consoleScrollHoldFrames = 0; // Frame counter for continuous scrolling
static int s_consoleScrollHoldFramesH = 0; // Frame counter for continuous horizontal scrolling

void LuaConsolePushLine(const char* msg) {
     if (!msg || !msg[0]) return;
     if (s_luaConsoleCount < LUA_CONSOLE_MAX_LINES) {
         strncpy(s_luaConsoleLines[s_luaConsoleCount], msg, LUA_CONSOLE_LINE_CHARS - 1);
         s_luaConsoleLines[s_luaConsoleCount][LUA_CONSOLE_LINE_CHARS - 1] = '\0';
         s_luaConsoleCount++;
     } else {
         for (int i = 1; i < LUA_CONSOLE_MAX_LINES; ++i) {
             memcpy(s_luaConsoleLines[i - 1], s_luaConsoleLines[i], LUA_CONSOLE_LINE_CHARS);
         }
         strncpy(s_luaConsoleLines[LUA_CONSOLE_MAX_LINES - 1], msg, LUA_CONSOLE_LINE_CHARS - 1);
         s_luaConsoleLines[LUA_CONSOLE_MAX_LINES - 1][LUA_CONSOLE_LINE_CHARS - 1] = '\0';
     }
}

void FCEU_SetLuaConsoleVisible(int visible) { 
	s_consoleVisible = (visible != 0);
	if (!s_consoleVisible) {
		s_consoleScrollOffset = 0; // Reset scroll when console is hidden
		s_consoleScrollOffsetH = 0; // Reset horizontal scroll when console is hidden
	}
}
int  FCEU_IsLuaConsoleVisible(void) { return s_consoleVisible ? 1 : 0; }

extern "C" void FCEU_SetLuaConsoleLineGap(int px) {
	if (px < 0) px = 0;
	if (px > 8) px = 8;
	s_consoleLineGap = px;
}
extern "C" int FCEU_GetLuaConsoleLineGap(void) { return s_consoleLineGap; }

void FCEU_ToggleLuaConsole(void) { 
	s_consoleVisible = !s_consoleVisible;
	if (!s_consoleVisible) {
		s_consoleScrollOffset = 0; // Reset scroll when console is hidden
		s_consoleScrollOffsetH = 0; // Reset horizontal scroll when console is hidden
	}
}

// Runtime management functions moved to lua_runtime.cpp

 // Performance: Disable printf spam in retail builds
 #if !defined(DEBUG) && !defined(_DEBUG)
 #undef printf
 #define printf(...) ((void)0)
 #endif
 
 // Log budget system to prevent excessive debug output
 static int g_log_budget = 200; // print at most 200 lines total per run
 #define LOGF(...) do { if (g_log_budget > 0) { --g_log_budget; printf(__VA_ARGS__); } } while(0)
 
 // Debug logging helper
 static void dbg(const char* s) { 
 #ifdef _XBOX
	 OutputDebugStringA(s); 
 #else
	 printf("%s", s);
 #endif
 }
 
 #define printf(...) do { \
	 char b[512]; \
	 _snprintf(b, 511, __VA_ARGS__); \
	 b[511] = 0; \
	 dbg(b); \
 } while(0)
 
 // Lua headers
 extern "C" {
 #include "../xbox/lua/src/lua.h"
 #include "../xbox/lua/src/lauxlib.h"
 #include "../xbox/lua/src/lualib.h"
 }
 
 lua_State* luaState = NULL;
 static bool luaInitialized = false;
 
// FPS tracking moved to lua_emulator.cpp
 
 // Forward declaration
 static uint8* currentXBuf = NULL;
 // Store the actual frame buffer passed to FCEU_LuaGui (not the overlay)
 static uint8* s_frameXBuf = NULL;
 // Pre-initialize screenshot directory path to avoid lag on first screenshot
 static bool s_screenshotDirInitialized = false;

// playinputrecording(data) -> nothing
 // Plays back recorded input from a table
 // data: table from stopinputrecording() with keys "player0", "player1", "player2", "player3"
 static int lua_playinputrecording(lua_State* L)
 {
	 if (lua_gettop(L) < 1) {
		 return luaL_error(L, "playinputrecording(data) requires 1 argument");
	 }
	 
	 if (!lua_istable(L, 1)) {
		 return luaL_error(L, "playinputrecording: data must be a table");
	 }
	 
	 // Stop any current playback
	 s_inputPlayback = false;
	 s_playbackFrame = 0;
	 
	 // Clear playback data
	 for (int p = 0; p < 4; ++p) {
		 s_playbackInput[p].clear();
	 }
	 
	 // Read data from Lua table
	 for (int p = 0; p < 4; ++p) {
		 char key[16];
		 snprintf(key, sizeof(key), "player%d", p);
		 
		 // Get player's table
		 lua_getfield(L, 1, key);
		 if (lua_istable(L, -1)) {
			 // Read all entries from the table (1-indexed)
			 int i = 1;
			 while (true) {
				 lua_rawgeti(L, -1, i);
				 if (!lua_isnumber(L, -1)) {
					 lua_pop(L, 1);
					 break;  // End of table
				 }
				 int value = (int)luaL_checkinteger(L, -1);
				 lua_pop(L, 1);
				 
				 // Clamp to valid range
				 if (value < 0) value = 0;
				 if (value > 0xFF) value = 0xFF;
				 
				 s_playbackInput[p].push_back((uint8)(value & 0xFF));
				 ++i;
			 }
		 }
		 lua_pop(L, 1);  // Pop player table
	 }
	 
	 // Start playback
	 s_inputPlayback = true;
	 s_playbackFrame = 0;
	 s_playbackPosition = 0.0;
	 
	 return 0;
 }


 // saveinputrecording(path) -> boolean
 // Saves input recording to file
 // Parameters: path (file path string)
 // Returns: Boolean (success)
 // Use case: Save TAS inputs
 static int lua_saveinputrecording(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "saveinputrecording(path) requires 1 argument");
	 }
	 
	 const char* filename = luaL_checkstring(L, 1);
	 if (!filename || strlen(filename) == 0) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 // Check if there's any recorded data
	 bool hasData = false;
	 size_t maxFrames = 0;
	 for (int p = 0; p < 4; ++p) {
		 if (s_recordedInput[p].size() > 0) {
			 hasData = true;
			 if (s_recordedInput[p].size() > maxFrames) {
				 maxFrames = s_recordedInput[p].size();
			 }
		 }
	 }
	 
	 if (!hasData) {
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
		 // Relative to writable directory (try hdd1: first as it's always writable)
		 // Save in "recordings" subfolder within lua directory
		 const char* baseDir = "hdd1:\\fce360-enhanced\\lua\\recordings\\";
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
			 // Save in "recordings" subfolder within lua directory
			 const char* gameDir = "game:\\lua\\recordings\\";
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
	 // Format: one frame per line, comma-separated button masks for each player
	 // Example: "0,0,0,0\n" for frame 0 (all players no buttons)
	 bool writeSuccess = true;
	 DWORD bytesWritten = 0;
	 
	 for (size_t frame = 0; frame < maxFrames; ++frame) {
		 char line[64];
		 int len = 0;
		 
		 // Write button masks for each player (comma-separated)
		 for (int p = 0; p < 4; ++p) {
			 uint8 buttonMask = 0;
			 if (frame < s_recordedInput[p].size()) {
				 buttonMask = s_recordedInput[p][frame];
			 }
			 
			 if (p > 0) {
				 line[len++] = ',';
			 }
			 
			 // Write button mask as decimal number
			 len += snprintf(line + len, sizeof(line) - len, "%d", (int)buttonMask);
		 }
		 
		 line[len++] = '\n';
		 line[len] = '\0';
		 
		 BOOL result = WriteFile(hFile, line, (DWORD)len, &bytesWritten, NULL);
		 if (!result || bytesWritten != (DWORD)len) {
			 writeSuccess = false;
			 break;
		 }
	 }
	 
	 CloseHandle(hFile);
	 
	 lua_pushboolean(L, writeSuccess ? 1 : 0);
	 return 1;
 }

 // loadinputrecording(path) -> boolean
 // Loads input recording from file and starts playback
 // Parameters: path (file path string)
 // Returns: Boolean (success)
 // Use case: Playback TAS inputs
 static int lua_loadinputrecording(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "loadinputrecording(path) requires 1 argument");
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
		 // Try recordings directory first (where saveinputrecording saves)
		 const char* baseDir = "hdd1:\\fce360-enhanced\\lua\\recordings\\";
		 snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	 }
	 
	 // Normalize path separators (convert / to \)
	 for (int i = 0; fullpath[i] != '\0'; i++) {
		 if (fullpath[i] == '/') {
			 fullpath[i] = '\\';
		 }
	 }
	 
	 // Try to open file
	 FILE* file = fopen(fullpath, "rb");
	 if (!file) {
		 // Try alternative paths if initial path fails
		 const char* altPaths[] = {
			 "game:\\lua\\recordings\\%s",
			 "hdd1:\\fce360-enhanced\\lua\\recordings\\%s",
			 "game:\\lua\\%s",
			 "hdd1:\\fce360-enhanced\\lua\\%s",
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
			 lua_pushboolean(L, 0);
			 return 1;
		 }
	 }
	 
	 // Get file size
	 fseek(file, 0, SEEK_END);
	 long fileSize = ftell(file);
	 fseek(file, 0, SEEK_SET);
	 
	 if (fileSize < 0) {
		 fclose(file);
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 if (fileSize == 0) {
		 // Empty file
		 fclose(file);
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 // Allocate buffer for file contents
	 char* buffer = (char*)malloc(fileSize + 1);
	 if (!buffer) {
		 fclose(file);
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 // Read file
	 size_t bytesRead = fread(buffer, 1, fileSize, file);
	 fclose(file);
	 
	 if (bytesRead != (size_t)fileSize) {
		 free(buffer);
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 // Null-terminate
	 buffer[fileSize] = '\0';
	 
	 // Stop any current playback
	 s_inputPlayback = false;
	 s_playbackFrame = 0;
	 s_playbackPosition = 0.0;
	 
	 // Clear playback data
	 for (int p = 0; p < 4; ++p) {
		 s_playbackInput[p].clear();
	 }

	 // Parse file contents
	 // Format: one frame per line, comma-separated button masks for each player
	 // Example: "0,0,0,0\n" for frame 0 (all players no buttons)
	 char* lineStart = buffer;
	 
	 while (lineStart < buffer + fileSize) {
		 // Find end of line
		 char* lineEnd = strchr(lineStart, '\n');
		 if (!lineEnd) {
			 lineEnd = buffer + fileSize;
		 }
		 
		 // Null-terminate line for parsing
		 char savedChar = *lineEnd;
		 *lineEnd = '\0';
		 
		 // Skip empty lines
		 if (lineStart == lineEnd || (*lineStart == '\r' && lineStart + 1 == lineEnd)) {
			 *lineEnd = savedChar;
			 lineStart = lineEnd + 1;
			 continue;
		 }
		 
		 // Parse comma-separated values for 4 players
		 int values[4] = {0, 0, 0, 0};
		 int valueIndex = 0;
		 char* token = lineStart;
		 
		 while (token < lineEnd && valueIndex < 4) {
			 // Skip whitespace
			 while (*token == ' ' || *token == '\t' || *token == '\r') {
				 token++;
			 }
			 
			 if (token >= lineEnd) break;
			 
			 // Find comma or end of line
			 char* comma = strchr(token, ',');
			 if (!comma || comma > lineEnd) {
				 comma = lineEnd;
			 }
			 
			 // Parse number
			 char savedComma = *comma;
			 *comma = '\0';
			 
			 int value = atoi(token);
			 if (value < 0) value = 0;
			 if (value > 0xFF) value = 0xFF;
			 values[valueIndex] = value;
			 
			 *comma = savedComma;
			 
			 valueIndex++;
			 token = comma + 1;
		 }
		 
		 // Restore line end character
		 *lineEnd = savedChar;
		 
		 // Add frame data for each player
		 for (int p = 0; p < 4; ++p) {
			 s_playbackInput[p].push_back((uint8)(values[p] & 0xFF));
		 }
		 
		 // Move to next line
		 lineStart = lineEnd + 1;
	 }
	 
	 free(buffer);
	 
	 // Check if we loaded any data
	 bool hasData = false;
	 for (int p = 0; p < 4; ++p) {
		 if (s_playbackInput[p].size() > 0) {
			 hasData = true;
			 break;
		 }
	 }
	 
	 if (!hasData) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 
	 // Start playback
	 s_inputPlayback = true;
	 s_playbackFrame = 0;
	 
	 lua_pushboolean(L, 1);
	 return 1;
 }

 // setrecordingmarker(name) -> nothing
 // Sets a marker at the current frame in the recording
 // Parameters: name (marker name string)
 // Returns: Nothing
 // Use case: Bookmark positions in recording
 static int lua_setrecordingmarker(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "setrecordingmarker(name) requires 1 argument");
	 }

	 const char* name = luaL_checkstring(L, 1);
	 if (!name || strlen(name) == 0) {
		 return 0;  // Empty name, do nothing
	 }

	 // Check if recording is active
	 if (!s_inputRecording) {
		 return 0;  // Not recording, do nothing
	 }

	 // Get current frame number (use player 0's size as frame count)
	 // The size represents the number of frames recorded so far
	 // Since setrecordingmarker() is called from beforeframe() callback,
	 // the current frame hasn't been recorded yet - it will be recorded at the end of the frame
	 // So we mark the frame at index = size (the next frame to be recorded)
	 // Example: If 10 frames are recorded (indices 0-9), size=10, we mark frame 10
	 int currentFrame = (int)s_recordedInput[0].size();

	 // Store marker
	 s_recordingMarkers[std::string(name)] = currentFrame;

	 return 0;
 }

 // jumptorecordingmarker(name) -> boolean
 // Jumps to marker in recording during playback
 // Parameters: name (marker name string)
 // Returns: Boolean (success)
 // Use case: Navigate recording
 static int lua_jumptorecordingmarker(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "jumptorecordingmarker(name) requires 1 argument");
	 }

	 const char* name = luaL_checkstring(L, 1);
	 if (!name || strlen(name) == 0) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 // Check if playback is active
	 if (!s_inputPlayback) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 // Look up marker
	 std::map<std::string, int>::iterator it = s_recordingMarkers.find(std::string(name));
	 if (it == s_recordingMarkers.end()) {
		 // Marker not found
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 int markerFrame = it->second;

	 // Check if marker frame is within bounds of playback data
	 // Find the maximum frame count across all players
	 int maxFrames = 0;
	 for (int p = 0; p < 4; ++p) {
		 if ((int)s_playbackInput[p].size() > maxFrames) {
			 maxFrames = (int)s_playbackInput[p].size();
		 }
	 }

	 // Validate marker frame is within bounds
	 // Note: markerFrame is 0-indexed, so valid range is 0 to maxFrames-1
	 // However, if markerFrame equals maxFrames (one past the end), clamp it to maxFrames-1
	 if (markerFrame < 0) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }
	 if (markerFrame >= maxFrames) {
		 // Clamp to last valid frame if marker is out of bounds
		 if (maxFrames > 0) {
			 markerFrame = maxFrames - 1;
		 } else {
			 lua_pushboolean(L, 0);
			 return 1;
		 }
	 }

	 // Jump to marker frame
	 s_playbackFrame = markerFrame;
	 s_playbackPosition = (double)markerFrame;

	 lua_pushboolean(L, 1);
	 return 1;
 }

 // setplaybackspeed(mult) -> nothing
 // Sets playback speed multiplier
 // Parameters: mult (number: 0.5, 1.0, 2.0, etc.)
 // Returns: Nothing
 // Use case: Slow/fast motion playback
 static int lua_setplaybackspeed(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "setplaybackspeed(mult) requires 1 argument");
	 }

	 double mult = luaL_checknumber(L, 1);
	 
	 // Clamp speed to reasonable range (0.1 to 10.0)
	 if (mult < 0.1) mult = 0.1;
	 if (mult > 10.0) mult = 10.0;

	 // Set playback speed
	 s_playbackSpeed = mult;

	 return 0;
 }

 // trimrecording(startFrame, endFrame) -> boolean
 // Trims recording to frame range
 // Parameters: startFrame, endFrame (frame numbers)
 // Returns: Boolean (success)
 // Use case: Edit recordings
static int lua_trimrecording(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "trimrecording(startFrame, endFrame) requires 2 arguments");
	 }

	 int startFrame = (int)luaL_checkinteger(L, 1);
	 int endFrame = (int)luaL_checkinteger(L, 2);

	 // Check if recording is active
	 if (!s_inputRecording) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 // Validate frame range
	 if (startFrame < 0 || endFrame < 0) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 if (startFrame > endFrame) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 // Find the maximum frame count across all players
	 int maxFrames = 0;
	 for (int p = 0; p < 4; ++p) {
		 if ((int)s_recordedInput[p].size() > maxFrames) {
			 maxFrames = (int)s_recordedInput[p].size();
		 }
	 }

	 // Check if frame range is valid
	 if (startFrame >= maxFrames || endFrame >= maxFrames) {
		 lua_pushboolean(L, 0);
		 return 1;
	 }

	 // Trim all player vectors to the specified range
	 // Keep frames from startFrame to endFrame (inclusive)
	 for (int p = 0; p < 4; ++p) {
		 if ((int)s_recordedInput[p].size() > 0) {
			 // Create new vector with trimmed range
			 std::vector<uint8> trimmed;
			 trimmed.reserve(endFrame - startFrame + 1);
			 
			 for (int i = startFrame; i <= endFrame; ++i) {
				 if (i < (int)s_recordedInput[p].size()) {
					 trimmed.push_back(s_recordedInput[p][i]);
				 } else {
					 // If this player has fewer frames, pad with 0
					 trimmed.push_back(0);
				 }
			 }
			 
			 // Replace original with trimmed version
			 s_recordedInput[p] = trimmed;
		 }
	 }

	 // Update markers: remove markers outside the trimmed range, adjust others
	 std::map<std::string, int> newMarkers;
	 for (std::map<std::string, int>::iterator it = s_recordingMarkers.begin(); it != s_recordingMarkers.end(); ++it) {
		 int markerFrame = it->second;
		 if (markerFrame >= startFrame && markerFrame <= endFrame) {
			 // Marker is within range, adjust its position (subtract startFrame)
			 newMarkers[it->first] = markerFrame - startFrame;
		 }
		 // Markers outside the range are discarded
	 }
	 s_recordingMarkers = newMarkers;

	 lua_pushboolean(L, 1);
	 return 1;
 }

#endif // movie functions migrated

// ROM functions moved to lua_rom.cpp

// Emulation state functions moved to lua_emulator.cpp

// Game Genie functions moved to lua_gamegenie.cpp

// Emulation state and timing functions moved to lua_emulator.cpp

// Audio functions moved to lua_audio.cpp
// Profiler, timing, and cadence management functions moved to lua_profiler.cpp

 // File I/O functions moved to lua_fileio.cpp

 // Palette functions moved to lua_palette.cpp

 // File I/O functions moved to lua_fileio.cpp

 // Palette functions moved to lua_palette.cpp
 // Audio functions moved to lua_audio.cpp
 // Profiler, timing, and cadence management functions moved to lua_profiler.cpp

// Runtime management functions moved to lua_runtime.cpp

// Mapper functions moved to lua_rom.cpp

 // getbuttonmask(buttonName) -> integer bitmask
	 
	 // Get last frame time from Lua registry
	 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 
	 if (lua_isnil(L, -1)) {
		 // First call - no previous frame time, return 0.0
		 lua_pop(L, 1);
		 
		 // Store current time for next call
		 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
		 lua_pushinteger(L, (lua_Integer)currentTime);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushnumber(L, 0.0);
		 return 1;
	 }
	 
	 // Get last frame time
	 lua_Integer lastFrameTime = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 // Calculate delta in milliseconds
	 DWORD deltaMs = currentTime - (DWORD)lastFrameTime;
	 
	 // Convert to seconds (float)
	 double deltaSeconds = (double)deltaMs / 1000.0;
	 
	 // Update last frame time for next call
	 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	 lua_pushinteger(L, (lua_Integer)currentTime);
	 lua_settable(L, LUA_REGISTRYINDEX);
	 
	 lua_pushnumber(L, deltaSeconds);
	 return 1;
 }

// Runtime management functions moved to lua_runtime.cpp

 // getmapper() -> integer
 {
	 // Check if sleep is active
	 lua_pushstring(L, "FCEU_SLEEP_START_TIME");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 
	 if (lua_isnil(L, -1)) {
		 // No sleep state - not sleeping
		 lua_pop(L, 1);
		 return false;
	 }
	 
	 // Get sleep start time and duration
	 lua_Integer sleepStartTime = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 lua_Integer sleepDurationMs = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 // Get original pause state
	 lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 int wasPaused = lua_toboolean(L, -1);
	 lua_pop(L, 1);
	 
	 // Check if sleep duration has elapsed
	 DWORD currentTime = GetTickCount();
	 DWORD elapsed = currentTime - (DWORD)sleepStartTime;
	 
	 if (elapsed >= (DWORD)sleepDurationMs) {
		 // Sleep complete - clear sleep state and restore pause state
		 lua_pushstring(L, "FCEU_SLEEP_START_TIME");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 // Restore original pause state
		 extern void FCEUI_SetEmulationPaused(int val);
		 FCEUI_SetEmulationPaused(wasPaused ? 1 : 0);
		 
		 return false; // Sleep complete
	 }
	 
	 // Still sleeping - ensure emulation is paused
	 extern void FCEUI_SetEmulationPaused(int val);
	 FCEUI_SetEmulationPaused(1);
	 
	 return true; // Still sleeping
 }

// Mapper functions moved to lua_rom.cpp

 // getbuttonmask(buttonName) -> integer bitmask
 // Converts button name to bitmask
 // Button names: "A", "B", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT" (case-insensitive)
 static int lua_getbuttonmask(lua_State* L)
 {
	 const char* buttonName = luaL_checkstring(L, 1);
	 
	 if (!buttonName || !buttonName[0]) {
		 return luaL_error(L, "getbuttonmask: button name cannot be empty");
	 }
	 
	 // Map button name to bitmask (case-insensitive)
	 // FCEUX bit order: A=0x01, B=0x02, Select=0x04, Start=0x08, Up=0x10, Down=0x20, Left=0x40, Right=0x80
	 uint8 buttonMask = 0;
	 
	 // Convert to uppercase for case-insensitive comparison
	 char upperButton[16];
	 int i = 0;
	 for (; buttonName[i] && i < 15; ++i) {
		 char c = buttonName[i];
		 if (c >= 'a' && c <= 'z') {
			 upperButton[i] = c - 'a' + 'A';
		 } else {
			 upperButton[i] = c;
		 }
	 }
	 upperButton[i] = '\0';
	 
	 // Map button name to bitmask
	 if (strcmp(upperButton, "A") == 0) {
		 buttonMask = 0x01;
	 } else if (strcmp(upperButton, "B") == 0) {
		 buttonMask = 0x02;
	 } else if (strcmp(upperButton, "SELECT") == 0) {
		 buttonMask = 0x04;
	 } else if (strcmp(upperButton, "START") == 0) {
		 buttonMask = 0x08;
	 } else if (strcmp(upperButton, "UP") == 0) {
		 buttonMask = 0x10;
	 } else if (strcmp(upperButton, "DOWN") == 0) {
		 buttonMask = 0x20;
	 } else if (strcmp(upperButton, "LEFT") == 0) {
		 buttonMask = 0x40;
	 } else if (strcmp(upperButton, "RIGHT") == 0) {
		 buttonMask = 0x80;
	 } else {
		 return luaL_error(L, "getbuttonmask: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	 }
	 
	 // Return the bitmask
	 lua_pushinteger(L, (int)buttonMask);
	 return 1;
 }

 // drawtextbox(x, y, width, height, text, color, bgColor, borderColor)
 // Draws text in a bordered box with background
 // bgColor and borderColor are optional (nil or -1 for no background/border)
 static int lua_drawtextbox(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return luaL_error(L, "drawtextbox(x, y, width, height, text, color [, bgColor, borderColor]) requires at least 6 arguments");
	 }

	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int width = (int)luaL_checkinteger(L, 3);
	 int height = (int)luaL_checkinteger(L, 4);
	 const char* text = luaL_checkstring(L, 5);
	 int color = (int)luaL_checkinteger(L, 6);
	 
	 // Optional parameters
	 int bgColor = -1;
	 int borderColor = -1;
	 if (n >= 7 && !lua_isnil(L, 7)) {
		 bgColor = (int)luaL_checkinteger(L, 7);
	 }
	 if (n >= 8 && !lua_isnil(L, 8)) {
		 borderColor = (int)luaL_checkinteger(L, 8);
	 }

	 if (!currentXBuf || !text) return 0;

	 // Clamp dimensions
	 if (width <= 0 || height <= 0) return 0;
	 if (x < 0) x = 0;
	 if (y < 0) y = 0;
	 if (x + width > OVL_W) width = OVL_W - x;
	 if (y + height > OVL_H) height = OVL_H - y;
	 if (width <= 0 || height <= 0) return 0;

	 // Border thickness (3 pixels as requested for testing)
	 const int borderThickness = 3;

	 // Calculate inner area (accounting for border)
	 int innerX = x;
	 int innerY = y;
	 int innerW = width;
	 int innerH = height;
	 
	 if (borderColor >= 0) {
		 innerX += borderThickness;
		 innerY += borderThickness;
		 innerW -= borderThickness * 2;
		 innerH -= borderThickness * 2;
		 
		 // Ensure inner area is valid
		 if (innerW <= 0 || innerH <= 0) {
			 // Box too small for border, just draw border
			 innerW = 0;
			 innerH = 0;
		 }
	 }

	 // Draw background (if specified)
	 if (bgColor >= 0 && innerW > 0 && innerH > 0) {
		 uint8 mappedBg = map_overlay_color(bgColor);
		 for (int py = innerY; py < innerY + innerH && py < OVL_H; ++py) {
			 if (py < 0) continue;
			 for (int px = innerX; px < innerX + innerW && px < OVL_W; ++px) {
				 if (px < 0) continue;
				 if (!is_point_clipped(px, py)) {
					 uint8* dest = currentXBuf + py * OVL_W + px;
					 *dest = apply_blend_mode(*dest, mappedBg);
				 }
			 }
		 }
	 }

	 // Draw border (if specified) - 3 pixel thick border
	 if (borderColor >= 0) {
		 uint8 mappedBorder = map_overlay_color(borderColor);
		 
		 // Draw border as filled rectangles on each side
		 // Top border
		 for (int by = 0; by < borderThickness && by < height; ++by) {
			 int py = y + by;
			 if (py >= 0 && py < OVL_H) {
				 for (int px = x; px < x + width && px < OVL_W; ++px) {
					 if (px >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Bottom border
		 for (int by = height - borderThickness; by < height; ++by) {
			 int py = y + by;
			 if (py >= 0 && py < OVL_H) {
				 for (int px = x; px < x + width && px < OVL_W; ++px) {
					 if (px >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Left border
		 for (int bx = 0; bx < borderThickness && bx < width; ++bx) {
			 int px = x + bx;
			 if (px >= 0 && px < OVL_W) {
				 for (int py = y; py < y + height && py < OVL_H; ++py) {
					 if (py >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Right border
		 for (int bx = width - borderThickness; bx < width; ++bx) {
			 int px = x + bx;
			 if (px >= 0 && px < OVL_W) {
				 for (int py = y; py < y + height && py < OVL_H; ++py) {
					 if (py >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
	 }

	 // Draw text inside the box (with padding)
	 if (innerW > 0 && innerH > 0 && text && *text) {
		 int textX = innerX + 2;  // 2 pixel padding
		 int textY = innerY + 2;  // 2 pixel padding
		 int textW = innerW - 4;  // Account for padding on both sides
		 int textH = innerH - 4;  // Account for padding on both sides
		 
		 if (textW > 0 && textH > 0 && textX >= 0 && textY >= 0 && textX < OVL_W && textY < OVL_H) {
			 uint8 mapped = map_overlay_color(color);
			 uint8* dest = currentXBuf + textY * OVL_W + textX;
			 
			 // Use DrawTextTransWH with border=0 (no text border, we have box border)
			 DrawTextTransWH(dest, OVL_W, (uint8*)text, mapped, textW, textH, 0);
		 }
	 }

	 g_overlayDirty = true;
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a single pixel
 int lua_drawpixel(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 3) {
		 return luaL_error(L, "drawpixel(x, y, color) requires 3 arguments");
	 }
	 
	 float x = (float)luaL_checknumber(L, 1);
	 float y = (float)luaL_checknumber(L, 2);
	 int color = (int)luaL_checkinteger(L, 3);
	 
	 if (!currentXBuf) return 0;
	 
	 // Apply transform
	 float tx, ty;
	 transform_point(x, y, tx, ty);
	 
	 // Convert to integer coordinates
	 int ix = (int)(tx + 0.5f);  // Round to nearest
	 int iy = (int)(ty + 0.5f);
	 
	 // Early return if completely off-screen (better performance and safety)
	 if (ix < 0 || ix >= OVL_W || iy < 0 || iy >= OVL_H) return 0;
	 
	 // Draw pixel on the current frame buffer (set by FCEU_LuaGui)
	 // Additional defensive check on buffer pointer
	 if (iy * OVL_W + ix < 0 || iy * OVL_W + ix >= OVL_W * OVL_H) return 0;
	 
	 // Check clipping
	 if (is_point_clipped(ix, iy)) return 0;
	 
	 uint8 *dest = currentXBuf + iy * OVL_W + ix;
	 uint8 srcColor = map_overlay_color(color);
	 *dest = apply_blend_mode(*dest, srcColor);
	 g_overlayDirty = true;  // Mark that something was drawn
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a line between two points
 int lua_drawline(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return luaL_error(L, "drawline(x1, y1, x2, y2, color) requires 5 arguments");
	 }
	 
	 int x1 = (int)luaL_checkinteger(L, 1);
	 int y1 = (int)luaL_checkinteger(L, 2);
	 int x2 = (int)luaL_checkinteger(L, 3);
	 int y2 = (int)luaL_checkinteger(L, 4);
	 int color = (int)luaL_checkinteger(L, 5);
	 
	 if (!currentXBuf) return 0;
	 
	 // Early return if both points are completely off-screen
	 if ((x1 < 0 || x1 >= OVL_W || y1 < 0 || y1 >= OVL_H) &&
	     (x2 < 0 || x2 >= OVL_W || y2 < 0 || y2 >= OVL_H)) {
		 return 0; // Both points off-screen, skip drawing
	 }
	 
	 // Clamp coordinates to safe bounds (for line algorithm, we'll check bounds in the loop)
	 // But we still need to clamp to prevent integer overflow in calculations
	 if (x1 < -OVL_W) x1 = -OVL_W;
	 if (x1 > OVL_W * 2) x1 = OVL_W * 2;
	 if (x2 < -OVL_W) x2 = -OVL_W;
	 if (x2 > OVL_W * 2) x2 = OVL_W * 2;
	 if (y1 < -OVL_H) y1 = -OVL_H;
	 if (y1 > OVL_H * 2) y1 = OVL_H * 2;
	 if (y2 < -OVL_H) y2 = -OVL_H;
	 if (y2 > OVL_H * 2) y2 = OVL_H * 2;
	 
	 // Use Bresenham's line algorithm to draw the line
	 int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	 int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	 int sx = (x1 < x2) ? 1 : -1;
	 int sy = (y1 < y2) ? 1 : -1;
	 int err = dx - dy;
	 
	 int x = x1;
	 int y = y1;
	 bool drewSomething = false;
	 
	 // Safety limit to prevent infinite loops (max pixels we could draw)
	 int maxIterations = (OVL_W + OVL_H) * 2;
	 int iterations = 0;
	 
	 while (iterations < maxIterations) {
		 iterations++;
		 
		 // Check bounds and draw pixel
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 // Additional defensive check on buffer pointer
				 if (y * OVL_W + x >= 0 && y * OVL_W + x < OVL_W * OVL_H) {
					 // Check clipping
					 if (!is_point_clipped(x, y)) {
						 uint8 *dest = currentXBuf + y * OVL_W + x;
						 uint8 srcColor = map_overlay_color(color);
						 *dest = apply_blend_mode(*dest, srcColor);
						 drewSomething = true;
					 }
				 }
			 }
		 
		 // Check if we've reached the end point
		 if (x == x2 && y == y2) break;
		 
		 int e2 = 2 * err;
		 
		 if (e2 > -dy) {
			 err -= dy;
			 x += sx;
		 }
		 
		 if (e2 < dx) {
			 err += dx;
			 y += sy;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
 }


// FPS function moved to lua_emulator.cpp

// Initialize Lua (original version - checks disabled state)
 static void InitLua() {
	 // CRITICAL: Check disabled state BEFORE initializing Lua
	 if (s_luaDisabled) {
		 printf("InitLua: BLOCKED - Lua is disabled, refusing to initialize\n");
		 return;
	 }
	 
	 if (luaState != NULL) {
		 printf("InitLua: Closing existing Lua state\n");
		 lua_close(luaState);
	 }
	 
	 printf("InitLua: Opening new Lua state (disabled=%d)\n", FCEU_LuaIsDisabled());
	 luaState = lua_open();
	 if (luaState == NULL) {
		 printf("InitLua: FAILED - lua_open() returned NULL\n");
		 return;
	 }
	 
	 printf("InitLua: Loading Lua libraries\n");
	 luaL_openlibs(luaState);
	 
	 // Register FCEU functions
	 Lua_RegisterVideo(luaState);
	 Lua_RegisterInput(luaState);
	 Lua_RegisterMovie(luaState);
	 Lua_RegisterRom(luaState);
	 Lua_RegisterEmulator(luaState);
	 Lua_RegisterRuntime(luaState);
	 Lua_RegisterAudio(luaState);
	 Lua_RegisterFileIO(luaState);
	 Lua_RegisterPalette(luaState);
	 Lua_RegisterProfiler(luaState);
	 Lua_RegisterGameGenie(luaState);
	 Lua_RegisterMemory(luaState);
	 Lua_RegisterMovie(luaState);
	 
	 luaInitialized = true;
 }

// Simple init helper - checks disabled state first
static void EnsureLuaInit() {
	if (s_luaDisabled) {
		// Do not even allocate a Lua state when disabled
		return;
	}
	if (luaInitialized && luaState) return;
	if (luaState) { lua_close(luaState); luaState = NULL; }
	luaState = lua_open();
	if (!luaState) {
		snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: alloc fail");
		return;
	}
	luaL_openlibs(luaState);
	#define REG(n,f) lua_register(luaState, n, f)
	Lua_RegisterVideo(luaState);
	Lua_RegisterInput(luaState);
	Lua_RegisterMovie(luaState);
	Lua_RegisterRom(luaState);
	Lua_RegisterEmulator(luaState);
	Lua_RegisterRuntime(luaState);
	Lua_RegisterAudio(luaState);
	Lua_RegisterFileIO(luaState);
	Lua_RegisterPalette(luaState);
	Lua_RegisterMemory(luaState);
	Lua_RegisterMovie(luaState);
	#undef REG
	luaInitialized = true;
	snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: init OK");
}
 
 // Load and run Lua script
 int FCEU_LoadLuaScript(const char* filename) {
	 printf("FCEU_LoadLuaScript: ENTERING with filename='%s'\n", filename ? filename : "(null)");
	 if (s_luaDisabled) { 
		 strncpy(g_luaStatusMsg, "Lua: disabled", sizeof(g_luaStatusMsg)-1);
		 g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		 printf("FCEU_LoadLuaScript: BLOCKED - Lua is disabled\n");
		 return 0; 
	 }

	 if (!luaInitialized) { InitLua(); if (!luaInitialized) { printf("FCEU_LoadLuaScript: Failed to initialize Lua\n"); return 0; } }
	 if (luaState == NULL) { printf("FCEU_LoadLuaScript: luaState is NULL\n"); return 0; }

	 char fullpath[512]; const char* workingPath = NULL;

	 // Try direct path if it looks like one
	 if (filename && (strchr(filename, ':') || strchr(filename, '\\') || strchr(filename, '/'))) {
		 printf("FCEU_LoadLuaScript: Detected direct path, trying: %s\n", filename);
		 FILE* f = fopen(filename, "rb");
		 if (f) {
			 fseek(f, 0, SEEK_END);
			 long sz = ftell(f);
			 fclose(f);
			 if (sz > 0) {
				 workingPath = filename;
				 printf("FCEU_LoadLuaScript: Direct path found and valid (size=%ld): %s\n", sz, filename);
			 } else {
				 printf("FCEU_LoadLuaScript: Direct path exists but file is empty: %s\n", filename);
			 }
		 } else {
			 printf("FCEU_LoadLuaScript: Direct path not accessible: %s\n", filename);
		 }
	 }

	 // If not a direct path or missing, search known places
	 if (!workingPath) {
		 printf("FCEU_LoadLuaScript: Searching known directories for: %s\n", filename ? filename : "(null)");
		 const char* paths[] = {
			 "hdd1:\\fce360-enhanced\\lua\\%s",
			 "hdd1:\\fce360-enhanced\\Lua\\%s",
			 "game:\\lua\\%s", "game:/lua/%s", "game:\\Lua\\%s", "game:/Lua/%s",
			 "hdd1:\\lua\\%s", "hdd1:\\Lua\\%s", "usb0:\\lua\\%s", "usb0:\\Lua\\%s",
			 "game:\\%s", "game:/%s",
			 "hdd1:\\_Emus\\fce360-enhanced\\lua\\%s"
		 };
		 for (int i=0;i<(int)(sizeof(paths)/sizeof(paths[0]));++i) {
			 _snprintf(fullpath, sizeof(fullpath), paths[i], filename ? filename : "");
			 FILE* f = fopen(fullpath, "rb");
			 if (f) { 
				 fseek(f,0,SEEK_END); 
				 long sz=ftell(f); 
				 fclose(f); 
				 if (sz>0) { 
					 workingPath = fullpath; 
					 printf("FCEU_LoadLuaScript: Found in search path %d (size=%ld): %s\n", i, sz, fullpath);
					 break; 
				 }
			 }
		 }
	 }

	 if (!workingPath) { 
		 snprintf(g_luaStatusMsg,sizeof(g_luaStatusMsg),"Lua: %s NOT FOUND", filename?filename:"(null)"); 
		 printf("FCEU_LoadLuaScript: FAILED - script not found: %s\n", filename ? filename : "(null)");
		 return 0; 
	 }

	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loading %s", workingPath);

	 // Final disabled check
	 if (s_luaDisabled) return 0;

	 printf("FCEU_LoadLuaScript: Loading %s\n", workingPath);
	 int rc = luaL_dofile(luaState, workingPath);
	 if (rc != 0) {
		 const char* err = lua_tostring(luaState, -1);
		 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: ERROR - %s", err ? err : "load failed");
		 printf("LUA ERROR (load): %s\n", err ? err : "load failed");
		 if (err && err[0]) LuaConsolePushLine(err);
		 if (err) lua_pop(luaState, 1);
		 return 0;
	 }

	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loaded %s", workingPath);
	 // Check for script() function, fallback to gui() for backward compatibility
	 lua_getglobal(luaState, "script");
	 if (!lua_isfunction(luaState, -1)) {
		 lua_pop(luaState, 1);
	 lua_getglobal(luaState, "gui");
	 }
	 if (!lua_isfunction(luaState, -1)) { lua_pop(luaState, 1); }
	 else { lua_pop(luaState, 1); }
	 return 1;
 }
 
// Ensure MAX_PATH is defined (standard Windows value is 260)
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Ensure INVALID_FILE_ATTRIBUTES is defined (for Xbox compatibility)
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// Helper: Check if file exists (using GetFileAttributesA for Xbox compatibility)
static bool file_existsA(const char* path) {
	DWORD a = GetFileAttributesA(path);
	return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int lua_togglebit(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "togglebit(address, bit) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int bit = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "togglebit: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate bit range (must be 0-7)
	 if (bit < 0 || bit > 7) {
		 return luaL_error(L, "togglebit: bit must be in range 0-7");
	 }
	 
	 // Read current byte value
	 uint8 currentValue = ARead[address](address);
	 
	 // Toggle the specified bit using XOR
	 uint8 newValue = currentValue ^ (1 << bit);
	 
	 // Write back using BWrite which handles all memory mapping correctly
	 BWrite[address](address, newValue);
	 
	 return 0;
}

int lua_testbit(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "testbit(address, bit) requires 2 arguments");
	 }

	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int bit = (int)luaL_checkinteger(L, 2);

	 if (address > 0xFFFF) {
		 return luaL_error(L, "testbit: address must be in range 0x0000-0xFFFF");
	 }
	 if (bit < 0 || bit > 7) {
		 return luaL_error(L, "testbit: bit must be in range 0-7");
	 }

	 uint8 value = ARead[address](address);
	 int mask = (1 << bit);
	 int isSet = ((value & mask) != 0) ? 1 : 0;
	 lua_pushboolean(L, isSet);
	 return 1;
}

// Helper: Try to run a script in a specific directory
static int try_run_in(const char* dir, const char* file) {
	// CRITICAL: Check disabled state FIRST - this is the master switch
	if (s_luaDisabled) return 0;
	
	char full[MAX_PATH];
	full[0] = 0;
	_snprintf(full, MAX_PATH-1, "%s\\%s", dir, file);
	full[MAX_PATH-1] = 0;
		if (file_existsA(full)) {
			// File exists at this path - use the full path directly since we know it exists
			// FCEU_LoadLuaScript will try direct paths first, then search if needed
			if (!s_luaDisabled) {
			// Try full path first (direct path mode in FCEU_LoadLuaScript)
			int result = FCEU_LoadLuaScript(full);
			if (result) {
				printf("try_run_in: Successfully loaded %s from %s\n", file, dir);
				return result;
			}
			// Fallback: try just the filename (let FCEU_LoadLuaScript search)
			const char* filename = strrchr(file, '\\');
			if (!filename) filename = strrchr(file, '/');
			if (filename) filename++; // skip the slash
			else filename = file; // no path separator, use as-is
			
			result = FCEU_LoadLuaScript(filename);
			if (result) {
				printf("try_run_in: Successfully loaded %s (searched) from %s\n", filename, dir);
			} else {
				printf("try_run_in: FCEU_LoadLuaScript failed for %s (file exists at %s)\n", filename, full);
			}
			return result;
		}
	}
	return 0;
}

// Forward declarations for helper functions
static bool LoadOneScriptByName(const char* nameUtf8);
static void LoadAllFromFolder(const char* folder);

// Wrapper functions that respect the master disabled gate
static void Lua_LoadAllInKnownFolders(void) {
	if (s_luaDisabled) { 
		printf("Lua: blocked load-all (disabled)\n"); 
		return; 
	}
	
	const char* dirs[] = {
		"game:\\lua",
		"game:\\media\\lua",
		"hdd1:\\fce360-enhanced\\lua"
	};
	
	for (int i = 0; i < (int)(sizeof(dirs)/sizeof(dirs[0])); ++i) {
		if (s_luaDisabled) break; // Check again in case it was disabled during loop
		LoadAllFromFolder(dirs[i]);
	}
}

static void Lua_LoadSingleScript(const char* utf8Path) {
	if (s_luaDisabled || !utf8Path || !utf8Path[0]) {
		printf("Lua: blocked load-one (disabled or empty)\n"); 
		return;
	}
	LoadOneScriptByName(utf8Path);
}

static void Lua_OnNewRomBoot(void) {
	// This is the critical autoload hook that was ignoring the gate
	if (s_luaDisabled) { 
		printf("Lua: new ROM -> disabled; no autoload\n"); 
		return; 
	}

	if (s_luaMode == LUA_AUTO_ALL) {
		Lua_LoadAllInKnownFolders();
	} else if (s_luaMode == LUA_AUTO_ONE && s_luaOneScript[0]) {
		Lua_LoadSingleScript(s_luaOneScript);
	} else {
		printf("Lua: new ROM -> mode=NONE or no script; nothing to load\n");
	}
}

// Helper: Run all .lua files in a directory
static void run_all_in(const char* dir) {
	// CRITICAL: Check disabled state FIRST - this is the master switch
	if (s_luaDisabled) { 
		printf("run_all_in blocked (disabled), dir=%s\n", dir); 
		return; 
	}
	
	char pat[MAX_PATH];
	pat[0] = 0;
	_snprintf(pat, MAX_PATH-1, "%s\\*.lua", dir);
	pat[MAX_PATH-1] = 0;

	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(pat, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		// FCEU_LoadLuaScript searches paths, so pass just the filename
		// FCEU_LoadLuaScript will also check disabled state
		if (!s_luaDisabled) {
			FCEU_LoadLuaScript(fd.cFileName);
		}
		}
	} while (FindNextFileA(h, &fd));
	
	FindClose(h);
}

// Load one script by name - uses FCEU_LoadLuaScript which respects disabled flag
static bool LoadOneScriptByName(const char* nameUtf8) {
	if (s_luaDisabled || !nameUtf8 || !*nameUtf8) {
		printf("LoadOneScriptByName: blocked (disabled or empty)\n");
		return false;
	}
	EnsureLuaInit();
	return FCEU_LoadLuaScript(nameUtf8) != 0;
}

// Load all .lua files from a folder
static void LoadAllFromFolder(const char* folder) {
	if (s_luaDisabled || !folder || !*folder) {
		printf("LoadAllFromFolder skipped (disabled or empty)\n");
		return;
	}
	EnsureLuaInit();
	WIN32_FIND_DATAA fd;
	memset(&fd, 0, sizeof(fd));
	char pattern[256];
	_snprintf(pattern, sizeof(pattern)-1, "%s\\*.lua", folder);
	pattern[sizeof(pattern)-1] = 0;
	HANDLE h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Use the safe loader that respects the disabled flag and search paths
			char path[512];
			_snprintf(path, sizeof(path)-1, "%s\\%s", folder, fd.cFileName);
			path[sizeof(path)-1] = 0;
			FCEU_LoadLuaScript(path);
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

// Helper: Check if selection means "none" (NULL, empty, whitespace, or keywords)
static bool is_none_selection(const char* s) {
	if (!s) return true;  // NULL => NONE (not ALL)
	
	// Skip leading whitespace
	const char* p = s;
	while (*p && isspace((unsigned char)*p)) ++p;
	if (!*p) return true;  // "" or whitespace => NONE
	
	// Case-insensitive checks for common labels
#ifdef _MSC_VER
	return *s == 0 || _stricmp(p, "none") == 0 || _stricmp(p, "(none)") == 0
		|| _stricmp(p, "off") == 0 || _stricmp(p, "disabled") == 0;
#else
	return *s == 0 || strcasecmp(p, "none") == 0 || strcasecmp(p, "(none)") == 0
		|| strcasecmp(p, "off") == 0 || strcasecmp(p, "disabled") == 0;
#endif
}

// Helper: Check if selection explicitly means "all"
static bool is_all_selection(const char* s) {
	if (!s) return false;
#ifdef _MSC_VER
	return (_stricmp(s, "all") == 0 || _stricmp(s, "*") == 0);
#else
	return (strcasecmp(s, "all") == 0 || strcmp(s, "*") == 0);
#endif
}

// Centralized application of the menu setting.
// Call this *after* a ROM is loaded and anytime the user changes the selection.
extern "C" void FCEU_ApplyLuaMode(int mode, const char* scriptUtf8OrNull)
{
	// Always start clean
	FCEU_LuaKillAll();

	s_luaMode = mode;

	if (mode == LUA_AUTO_NONE) {
		FCEU_LuaSetDisabled(1);
		printf("Lua: Apply -> NONE (master OFF)\n");
		return;
	}

	FCEU_LuaSetDisabled(0);

	if (mode == LUA_AUTO_ALL) {
		s_luaOneScript[0] = '\0';
		printf("Lua: Apply -> ALL\n");
		EnsureLuaInit(); // ensure Lua is initialized before loading
		Lua_LoadAllInKnownFolders();
		return;
	}

	if (mode == LUA_AUTO_ONE) {
		if (scriptUtf8OrNull && scriptUtf8OrNull[0]) {
			strncpy(s_luaOneScript, scriptUtf8OrNull, sizeof(s_luaOneScript)-1);
			s_luaOneScript[sizeof(s_luaOneScript)-1] = '\0';
			printf("Lua: Apply -> ONE (%s)\n", s_luaOneScript);
			EnsureLuaInit(); // ensure Lua is initialized before loading
			Lua_LoadSingleScript(s_luaOneScript);
			char msg[256];
			_snprintf(msg, sizeof(msg)-1, "Lua: ONE %s", s_luaOneScript);
			msg[sizeof(msg)-1] = '\0';
			strncpy(g_luaStatusMsg, msg, sizeof(g_luaStatusMsg)-1);
			g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		} else {
			// No script provided → safest is OFF
			printf("Lua: Apply -> ONE but no script; forcing NONE\n");
			s_luaMode = LUA_AUTO_NONE;
			FCEU_LuaSetDisabled(1);
			strncpy(g_luaStatusMsg, "Lua: NONE", sizeof(g_luaStatusMsg)-1);
			g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		}
		return;
	}

	// Unknown → OFF
	printf("Lua: Apply -> unknown mode %d; forcing NONE\n", mode);
	s_luaMode = LUA_AUTO_NONE;
	FCEU_LuaSetDisabled(1);
	strncpy(g_luaStatusMsg, "Lua: NONE", sizeof(g_luaStatusMsg)-1);
	g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
}

// Call this immediately AFTER the ROM is loaded and the core is powered.
extern "C" void FCEU_ApplyPendingLuaForNewGame(void) {
	// Consume pending at the one place core calls "ROM is ready"
	if (s_hasPending) {
		FCEU_ApplyLuaMode(s_pendingMode,
		                  (s_pendingMode == LUA_AUTO_ONE) ? s_pendingScript : NULL);
		s_hasPending = 0;
		s_pendingScript[0] = '\0';
		// Clear legacy globals too
		g_pendingLuaMode = -1;
		g_pendingLuaScript[0] = '\0';
	} else {
		// No UI change pending → apply current mode (still respects NONE)
		Lua_OnNewRomBoot();
	}
}


// Load Lua scripts based on user selection
// NULL/empty/"none" = load nothing (NONE)
// "all"/"*" = load all scripts from known directories
// Otherwise = load ONE specific script by name
void FCEU_AutoLoadLuaScripts(const char* selectedScript) {
	// Master kill-switch first
	if (s_luaDisabled) { 
		printf("FCEU_AutoLoadLuaScripts: disabled -> noop\n"); 
		return; 
	}
	
	// Treat NULL, "", "none", "(none)", "off", "disabled" as NO-OP
	if (is_none_selection(selectedScript)) {
		printf("FCEU_AutoLoadLuaScripts: NONE selection -> no scripts loaded\n");
		return;
	}
	
	// If the UI explicitly asked for ALL, then load from known dirs
	if (is_all_selection(selectedScript)) {
		const char* D1 = "hdd1:\\fce360-enhanced\\lua";
		const char* D2 = "game:\\lua";
		const char* D3 = "game:\\Lua";
		printf("FCEU_AutoLoadLuaScripts: ALL\n");
		run_all_in(D1);
		run_all_in(D2);
		run_all_in(D3);
		return;
	}
	
	// Otherwise: load ONE specific script by name
	if (selectedScript && selectedScript[0] != '\0') {
		if (!FCEU_LoadLuaScript(selectedScript)) {
			printf("FCEU_AutoLoadLuaScripts: failed to load '%s'\n", selectedScript);
		}
		return;
	}
	
	// Defensive default: if we got here, do nothing
	printf("FCEU_AutoLoadLuaScripts: no valid selection -> noop\n");
}
 
 // Get list of available Lua scripts
 int FCEU_GetLuaScriptList(char names[][256], int maxNames) {
	 const char* searchDirs[] = {
		 "hdd1:\\fce360-enhanced\\lua",
		 "hdd1:\\fce360-enhanced\\Lua",
		 "game:\\lua",
		 "game:/lua",
		 "game:\\Lua",
		 "game:/Lua",
		 "hdd1:\\lua",
		 "hdd1:\\Lua",
		 "usb0:\\lua",
		 "usb0:\\Lua"
	 };
	 
	 const int numDirs = sizeof(searchDirs) / sizeof(searchDirs[0]);
	 int count = 0;
	 
	 // Use a simple set to avoid duplicates
	 for (int dirIdx = 0; dirIdx < numDirs && count < maxNames; dirIdx++) {
		 char searchPattern[512];
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.lua", searchDirs[dirIdx]);
		 
		 WIN32_FIND_DATAA ffd;
		 HANDLE h = FindFirstFileA(searchPattern, &ffd);
		 
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 // Check if we already have this script
					 bool duplicate = false;
					 for (int i = 0; i < count; i++) {
						 if (strcmp(names[i], ffd.cFileName) == 0) {
							 duplicate = true;
							 break;
						 }
					 }
					 
					 if (!duplicate && count < maxNames) {
						 strncpy(names[count], ffd.cFileName, 255);
						 names[count][255] = '\0';
						 count++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd) && count < maxNames);
			 FindClose(h);
		 }
		 
		 // Also try uppercase pattern
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.LUA", searchDirs[dirIdx]);
		 h = FindFirstFileA(searchPattern, &ffd);
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 bool duplicate = false;
					 for (int i = 0; i < count; i++) {
						 if (strcmp(names[i], ffd.cFileName) == 0) {
							 duplicate = true;
							 break;
						 }
					 }
					 
					 if (!duplicate && count < maxNames) {
						 strncpy(names[count], ffd.cFileName, 255);
						 names[count][255] = '\0';
						 count++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd) && count < maxNames);
			 FindClose(h);
		 }
	 }
	 
	return count;
}

// Stub implementations for missing Lua functions (not yet implemented in Xbox port)
#ifdef USE_LUA
// Lua save/load data structure implementation
void LuaSaveData::ExportRecords(void* f) { 
	(void)f; 
	// Not implemented yet
}

void LuaSaveData::ImportRecords(void* f) { 
	(void)f; 
	// Not implemented yet
}

// Stub functions for save/load state integration
void CallRegisteredLuaSaveFunctions(void* state, LuaSaveData& saveData) {
	(void)state;
	(void)saveData;
	// Not implemented yet
}

void CallRegisteredLuaLoadFunctions(void* state, LuaSaveData& saveData) {
	(void)state;
	(void)saveData;
	// Not implemented yet
}

// Stub function for palette updates
void FCEU_LuaUpdatePalette(void) {
	// Not implemented yet - palette is handled in video path
}

// Stub function for reloading Lua code
void FCEU_ReloadLuaCode(void) {
	// Not implemented yet - would require full script reload
}
#endif

 // Frame boundary callback
 void FCEU_LuaFrameBoundary() {
	 if (s_luaDisabled) {
		 // Paranoid: make sure nothing is running and no overlay remains
		 FCEU_LuaStop();
		 return;
	 }
	 
	 if (!luaInitialized || luaState == NULL) {
		 return;
	 }
	 
	 // Process input callbacks and hold states
	 Lua_InputOnFrame(luaState);
	 
	 // Check for audio events and trigger callbacks
	 FCEU_LuaCheckAudioEvents();
	 
	 // Update frame counter and FPS tracking
	 Lua_EmulatorUpdateFrame();
	 
	 // Check watched addresses for changes
	 Lua_MemoryOnFrame(luaState);
	 
	 // Update rumble state - check if rumble duration has expired
	 for (int player = 0; player < 4; ++player) {
		 if (s_rumbleState[player].active) {
			 DWORD elapsed = currentTime - s_rumbleState[player].startTime;
			 if (elapsed >= s_rumbleState[player].duration) {
				 // Rumble duration expired - stop rumble
				 s_rumbleState[player].active = false;
				 if (Gamepads[player].bConnected) {
					 XINPUT_VIBRATION vibration;
					 vibration.wLeftMotorSpeed = 0;
					 vibration.wRightMotorSpeed = 0;
					 XInputSetState(player, &vibration);
				 }
			 }
		 }
	 }
	 
	 // Call "beforeframe" function if it exists - this runs BEFORE input polling
	 // This allows scripts to set joypad state before FCEU_UpdateInput() is called
	 // Check if script is sleeping - if so, skip callback execution
	 if (!Lua_IsSleeping(luaState)) {
		 lua_getglobal(luaState, "beforeframe");
		 if (lua_isfunction(luaState, -1)) {
			 if (lua_pcall(luaState, 0, 0, 0) != 0) {
				 // Error occurred - pop error message
				 lua_pop(luaState, 1);
			 }
		 } else {
			 lua_pop(luaState, 1);
		 }
	 }
	 
	 // Note: powerpadbuf override is now handled in Cemulator::UpdateInput()
	 // right after hardware input is read, ensuring it happens before UpdateGP() reads it
 }
 
 // GUI drawing callback - called from video.cpp
 // Update Lua at 30Hz, but composite the last overlay every frame to prevent flicker
 // Double-buffered: only publish new overlay if Lua succeeds (fail-safe)
 void FCEU_LuaGui(uint8 *XBuf) {
	 // Store the frame buffer for screenshot() function to use
	 // This MUST be set before any early returns, so screenshot() always has valid data
	 s_frameXBuf = XBuf;
	 
	 // Pre-initialize screenshot directory path on first call to avoid lag
	 if (!s_screenshotDirInitialized) {
		 extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
		 std::string tempPath = FCEU_MakeFName(2, 0, "png");  // 2 = FCEUMKF_SNAP
		 size_t lastSlash = tempPath.find_last_of("\\/");
		 if (lastSlash != std::string::npos) {
			 std::string dirPath = tempPath.substr(0, lastSlash);
			 // Normalize path separators
			 for (size_t i = 0; i < dirPath.length(); i++) {
				 if (dirPath[i] == '/') {
					 dirPath[i] = '\\';
				 }
			 }
			 // Ensure directory exists (fast if already exists)
			 CreateDirectoryA(dirPath.c_str(), NULL);
		 }
		 s_screenshotDirInitialized = true;
	 }
	 
	 // Safety check: ensure overlay is initialized before proceeding
	 EnsureOverlay();
	 if (!s_overlay_back || !s_overlay_front) {
		 // Overlay buffers not initialized - skip this frame
		 // But keep s_frameXBuf set so screenshots can still work
		 return;
	 }
	 
	 // If console is visible, we need to draw it even when Lua is disabled
	 // Otherwise, if Lua is disabled, clear overlays and return early
	 if (s_luaDisabled && !s_consoleVisible) {
		 ClearOverlaysIfAny();
		 // Keep s_frameXBuf set even when Lua is disabled, so screenshots can work
		 return; // no composite, no "LUA OFF" banner, truly silent
	 }
	 
     static DWORD lastGuiTime = 0;
     static bool prevConsoleVisible = false;
     DWORD now = GetTickCount();
     DWORD step = Lua_RuntimeGetScriptInterval(); // script()-cadence (default 33ms)
	 
	 // Check if console visibility changed - if so, force an update immediately
	 bool consoleVisibilityChanged = (s_consoleVisible != prevConsoleVisible);
	 
	 // Update overlay contents at ~30Hz (only when Lua needs to run)
	 // Also update immediately if console visibility changed
	 if (now - lastGuiTime >= step || consoleVisibilityChanged) {
		 lastGuiTime = now;
		 
		 // Always start fresh to avoid "ghost" rectangles from prior frames
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, OVL_W * OVL_H);
		 }
		 
		 // Optional status message for a few seconds - clears automatically after timeout
		 // Status banner (top-left)
		 static int statusTicks = 0;
		 static bool statusShown = false;
		 static char lastMsg[128] = {0};
		 
		 // If message text changed since last frame, restart TTL
		 if (strncmp(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1) != 0) {
			 strncpy(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1);
			 lastMsg[sizeof(lastMsg) - 1] = '\0';
			 statusTicks = 0;
			 statusShown = false;
		 }
		 
         // Suppress status banner while console is visible to avoid duplicate text
         if (s_overlay_back && !s_consoleVisible) {
			 // Status message is drawn at y=20 (below "LUA ON" at y=4), x=4
			 const int statusY = 20;
			 
			 if (statusTicks < 180) {  // ~6s @ 30Hz
				 // Draw status message below "LUA ON"
				 if (statusY >= 0 && statusY < OVL_H && g_luaStatusMsg[0] != '\0') {
					 DrawTextTrans(s_overlay_back + statusY*OVL_W + 4, OVL_W, (uint8*)g_luaStatusMsg, 0x2E | 0x80);
				 }
				 statusTicks++;
				 statusShown = true;
			 } else {
				 statusShown = false;
				 // Next frame will start blank, so status won't appear
			 }
		 }
		 
		 bool ok = false;  // Track if Lua succeeded
		 g_overlayDirty = false;  // Reset before calling Lua
		 
		 // Only run Lua scripts if Lua is enabled and initialized
		 if (!s_luaDisabled && luaInitialized && luaState != NULL) {
			 // Check if script is sleeping - if so, skip callback execution
			 if (!Lua_IsSleeping(luaState)) {
				 // Always reset render target to screen at start of each frame
				 // This ensures we don't leave currentXBuf pointing to a canvas from previous frame
				 s_currentRenderTarget = NULL;
				 s_renderTargetWidth = OVL_W;
				 s_renderTargetHeight = OVL_H;
				 
				 // Point Lua draw calls at the back buffer, not the front buffer
				 currentXBuf = s_overlay_back;
				 
			 // Call script() function if it exists (also support legacy gui() for backward compatibility)
			 lua_getglobal(luaState, "script");
			 if (!lua_isfunction(luaState, -1)) {
				 lua_pop(luaState, 1);
				 // Try legacy gui() function for backward compatibility
				 lua_getglobal(luaState, "gui");
			 }
				 if (lua_isfunction(luaState, -1)) {
				 if (lua_pcall(luaState, 0, 0, 0) == 0) {
					 ok = true;  // Script executed successfully
				 } else {
					 // Lua error - draw a visible error marker
					 const char* err = lua_tostring(luaState, -1);
					 printf("LUA ERROR (runtime): %s\n", err ? err : "unknown error");
					 if (err && err[0]) LuaConsolePushLine(err);
					 // Draw error indicator on screen so failures are visible
					 if (s_overlay_back) {
						 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"LUA ERR", 0x0F | 0x80);
						 g_overlayDirty = true;  // Error marker counts as drawing
					 }
					 lua_pop(luaState, 1);
					 // Leave g_overlayDirty as-is (true if error marker drawn)
				 }
			 } else {
			 // script() function doesn't exist
				 lua_pop(luaState, 1);
				 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"NO script()", 0x0F | 0x80);
					 g_overlayDirty = true;  // Error indicator counts as drawing
				 }
			 }
			 
			 currentXBuf = NULL;
			 } // End of sleep check block
		 } else {
			 // Lua not initialized
			 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"LUA OFF", 0x0F | 0x80);
				 g_overlayDirty = true;  // Status indicator counts as drawing
			 }
		 }
		 
         // If console visible, draw it now onto back buffer and mark dirty
		 if (s_consoleVisible && s_overlay_back) {
			 // Handle D-pad scrolling (vertical)
			 bool dpadUp = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
			 bool dpadDown = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
			 
			 // Scroll on button press (immediate) or hold (throttled)
			 if (dpadUp) {
				 if (!s_consoleDpadUpLast) {
					 // First press - scroll immediately
					 s_consoleScrollOffset++;
					 s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFrames++;
					 if (s_consoleScrollHoldFrames >= 3) {
						 s_consoleScrollOffset++;
						 s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 if (dpadDown) {
				 if (!s_consoleDpadDownLast) {
					 // First press - scroll immediately
				 if (s_consoleScrollOffset > 0) {
					 s_consoleScrollOffset--;
				 }
					 s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFrames++;
					 if (s_consoleScrollHoldFrames >= 3) {
						 if (s_consoleScrollOffset > 0) {
							 s_consoleScrollOffset--;
						 }
						 s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadUp && !dpadDown) {
				 s_consoleScrollHoldFrames = 0;
			 }
			 
			 s_consoleDpadUpLast = dpadUp;
			 s_consoleDpadDownLast = dpadDown;
			 
			 // Handle D-pad scrolling (horizontal)
			 bool dpadLeft = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
			 bool dpadRight = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
			 
			 // Scroll horizontally on button press (immediate) or hold (throttled)
			 if (dpadLeft) {
				 if (!s_consoleDpadLeftLast) {
					 // First press - scroll immediately (scroll left = decrease offset)
					 if (s_consoleScrollOffsetH > 0) {
						 s_consoleScrollOffsetH -= 8; // Scroll by ~1 character width
						 if (s_consoleScrollOffsetH < 0) s_consoleScrollOffsetH = 0;
					 }
					 s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFramesH++;
					 if (s_consoleScrollHoldFramesH >= 3) {
						 if (s_consoleScrollOffsetH > 0) {
							 s_consoleScrollOffsetH -= 8;
							 if (s_consoleScrollOffsetH < 0) s_consoleScrollOffsetH = 0;
						 }
						 s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 if (dpadRight) {
				 if (!s_consoleDpadRightLast) {
					 // First press - scroll immediately (scroll right = increase offset)
					 s_consoleScrollOffsetH += 8; // Scroll by ~1 character width
					 s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFramesH++;
					 if (s_consoleScrollHoldFramesH >= 3) {
						 s_consoleScrollOffsetH += 8;
						 s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadLeft && !dpadRight) {
				 s_consoleScrollHoldFramesH = 0;
			 }
			 
			 s_consoleDpadLeftLast = dpadLeft;
			 s_consoleDpadRightLast = dpadRight;
			 
			 const int cx = 4, cy = 40;
			 int maxX = 4 + 248 - 1; if (maxX > OVL_W - 1) maxX = OVL_W - 1;
			 int maxY = 40 + 180 - 1; if (maxY > OVL_H - 1) maxY = OVL_H - 1;
			 const uint8 bg = 0x80 | 0x10;
			 const uint8 bd = 0x80 | 0x2E;
			 for (int py = cy; py <= maxY; ++py) {
				 for (int px = cx; px <= maxX; ++px) s_overlay_back[py*OVL_W + px] = bg;
			 }
			 for (int px = cx; px <= maxX; ++px) { s_overlay_back[cy*OVL_W + px] = bd; s_overlay_back[maxY*OVL_W + px] = bd; }
			 DrawTextTrans(s_overlay_back + cy*OVL_W + (cx + 6), OVL_W, (uint8*)"Lua Console (LS+RS)", 0x2E | 0x80);
			 
			 // Clamp scroll offset (using new line advance) - DrawLuaConsole will handle the actual drawing
			 const int lineStartY = cy + 12;
			 const int adv = CON_LINE_ADV();
			 const int availableHeight = maxY - lineStartY + 1;
			 int maxLines = availableHeight / adv;
			 if (maxLines < 1) maxLines = 1;
			 // Allow scrolling to see all lines, including the oldest (line 0)
			 // maxScroll should allow viewing from line 0 to line (count - maxLines)
			 int maxScroll = (s_luaConsoleCount > maxLines) ? (s_luaConsoleCount - maxLines) : 0;
			 if (s_consoleScrollOffset > maxScroll) s_consoleScrollOffset = maxScroll;
			 if (s_consoleScrollOffset < 0) s_consoleScrollOffset = 0;
			 
			 g_overlayDirty = true;
			 ok = true; // force publish when console drew
         } else if (!s_consoleVisible && prevConsoleVisible && s_overlay_back) {
             // Console just turned off: push a cleared overlay once to remove it immediately
             memset(s_overlay_back, 0, 256 * 240);
             g_overlayDirty = true;
             ok = true;
		 }
		 
		 // Draw console using the new drawer with proper line spacing
		 DrawLuaConsole(s_overlay_back);
		 
		 // Only publish the new overlay if something was drawn
         if (g_overlayDirty) {
			 // Only publish if the back buffer actually differs from the front
			 // This prevents unnecessary swaps when content hasn't changed
			 if (!s_overlay_front || overlay_has_changes(s_overlay_back, s_overlay_front)) {
				 SwapOverlays();
			 }
		 }
         prevConsoleVisible = (s_consoleVisible != 0);
	 }
	 
	 // ALWAYS composite the last published overlay every frame (prevents flicker when Lua runs at 30Hz)
	 // This ensures smooth 60Hz display even though Lua only updates at 30Hz
	 if (XBuf && s_overlay_front) {
		 CompositeOverlay(XBuf);
	 }
 }
 
 // Audio event callback system - static variables
 static uint8 lastEnabledChannels = 0;
 
 // Stop Lua
 void FCEU_LuaStop() {
	 // Clean up input module Lua refs before closing Lua state
	 Lua_InputCleanup(luaState);
	
	 if (luaState != NULL) {
		 lua_close(luaState);
		 luaState = NULL;
	 }
	 luaInitialized = false;
	 ClearOverlaysIfAny();
	 Lua_MemoryResetWatchpoints();  // Clear all watchpoints
	 Lua_MovieReset();  // Reset movie/input systems
	 Lua_InputReset();  // Reset input systems
	 Lua_PaletteReset();
	 Lua_AudioReset();  // Reset audio filter states
	 Lua_ProfilerReset();  // Reset profiler state
	 lastEnabledChannels = 0;  // Reset audio event tracking
	 printf("FCEU_LuaStop: Lua state closed and overlays cleared\n");
 }
 
 // Call registered Lua functions
 void CallRegisteredLuaFunctions(LUACALL callID) {
	 // Not implemented yet - can be extended for more callbacks
	 (void)callID;
 }
 
 // Call audio event callbacks from Lua
 void FCEU_LuaAudioEvent(const char* eventName, int channel, bool enabled) {
	 if (!luaInitialized || luaState == NULL) {
		 return;
	 }
	 
	 // Call the callback function if it exists
	 lua_getglobal(luaState, eventName);
	 if (lua_isfunction(luaState, -1)) {
		 lua_pushinteger(luaState, channel);
		 lua_pushboolean(luaState, enabled ? 1 : 0);
		 if (lua_pcall(luaState, 2, 0, 0) != 0) {
			 // Error occurred - log it but don't crash
			 const char* err = lua_tostring(luaState, -1);
			 if (err) {
				 printf("LUA AUDIO EVENT ERROR: %s\n", err);
			 }
			 lua_pop(luaState, 1);
		 }
	 } else {
		 lua_pop(luaState, 1);
	 }
 }
 
 // Check for audio channel state changes and trigger callbacks
void FCEU_LuaCheckAudioEvents(void) {
	 if (!luaInitialized || luaState == NULL || FSettings.SndRate == 0) {
		 return;
	 }
	 
	 // Check for channel enable/disable changes
	 uint8 currentChannels = EnabledChannels;
	 if (currentChannels != lastEnabledChannels) {
		 // Check each channel for changes
		 for (int channel = 0; channel < 5; channel++) {
			 bool wasEnabled = false;
			 bool isEnabled = false;
			 
			 if (channel < 4) {
				 wasEnabled = (lastEnabledChannels & (1 << channel)) != 0;
				 isEnabled = (currentChannels & (1 << channel)) != 0;
			 } else {
				 // DMC channel (bit 4)
				 wasEnabled = (lastEnabledChannels & 0x10) != 0;
				 isEnabled = (currentChannels & 0x10) != 0;
			 }
			 
			 // If state changed, trigger callback
			 if (wasEnabled != isEnabled) {
				 FCEU_LuaAudioEvent("onaudiochannelchange", channel, isEnabled);
			 }
		 }
		 
		 lastEnabledChannels = currentChannels;
	 }
}

 // Memory hook callback
 void CallRegisteredLuaMemHook(unsigned int address, int size, uint8 value, LUAMEMHOOK hookType) {
	 // Not implemented yet - can be extended for memory hooks
	 (void)address;
	 (void)size;
	 (void)value;
	 (void)hookType;
 }
 
 // Joypad read callback
 uint32 FCEU_LuaReadJoypad(int n, uint32 ret) {
	 if (!luaInitialized || luaState == NULL) {
		 return ret;
	 }
	 
	 // Call joypad() function if it exists
	 lua_getglobal(luaState, "joypad");
	 if (lua_isfunction(luaState, -1)) {
		 lua_pushinteger(luaState, n);
		 lua_pushinteger(luaState, ret);
		 if (lua_pcall(luaState, 2, 1, 0) == 0) {
			 if (lua_isnumber(luaState, -1)) {
				 ret = (uint32)lua_tointeger(luaState, -1);
			 }
			 lua_pop(luaState, 1);
		 } else {
			 lua_pop(luaState, 1);
		 }
	 } else {
		 lua_pop(luaState, 1);
	 }
	 
	 return ret;
 }

// Apply Lua joypad overrides after input polling
// Call this each frame at the END of FCEU_UpdateInput() to ensure Lua's override wins over hardware
// This merges Lua overrides into the final pad state the NES core reads
extern "C" void FCEU_LuaJoypadApply(void)
{
	// Store hardware input BEFORE any overrides (for gethardwarejoypad() function)
	// powerpadbuf format: pad[0] in low byte, pad[1] in next byte
	Lua_InputSetHardwareJoypad(0, (uint8_t)(powerpadbuf & 0xFF));
	Lua_InputSetHardwareJoypad(1, (uint8_t)((powerpadbuf >> 8) & 0xFF));
	Lua_InputSetHardwareJoypad(2, (uint8_t)((powerpadbuf >> 16) & 0xFF));
	Lua_InputSetHardwareJoypad(3, (uint8_t)((powerpadbuf >> 24) & 0xFF));
	
	// Apply movie playback to powerpadbuf first (if active)
	uint32 newPowerpadbuf = powerpadbuf;  // Start with current hardware input
	bool moviePlayback = Lua_MovieIsPlaybackActive();
	newPowerpadbuf = Lua_MovieApplyPowerpad(newPowerpadbuf);
	
	// Temporarily update powerpadbuf so Lua_InputProcessJoypad can work with movie playback result
	powerpadbuf = newPowerpadbuf;
	
	// Process input overrides (this will update powerpadbuf and joy[] arrays)
	// Note: Lua_InputProcessJoypad expects powerpadbuf to already have movie playback applied
	Lua_InputProcessJoypad();
	
	// Now apply movie processing to joy[] array (after input overrides)
	for (int p = 0; p < 4; ++p) {
		joy[p] = Lua_MovieProcessJoypad(p, joy[p]);
	}
	
	// Advance movie playback
	Lua_MovieAdvancePlayback();
}

// Clear Lua joypad overrides
// player: -1 = all players, 0-3 = specific player
extern "C" void FCEU_LuaJoypadClear(int player)
{
	if (player == -1) {
		for (int p = 0; p < 4; ++p) {
			Lua_InputSetLuaJoypadMask(p, 0);
			Lua_InputSetLuaJoypadLatched(p, 0);
		}
	} else if (player >= 0 && player < 4) {
		Lua_InputSetLuaJoypadMask(player, 0);
		Lua_InputSetLuaJoypadLatched(player, 0);
	}
	// leave s_luaJoypadValue as-is; it won't be applied while latched==0
 }
  
 #endif // USE_LUA	