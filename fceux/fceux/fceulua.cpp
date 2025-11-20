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

// Extern Gamepads from input system
extern GAMEPAD Gamepads[];

// --- Overlay geometry and font metrics ---
enum { OVL_W = 256, OVL_H = 240, GLYPH_H = 8 };

// Console line gap is managed in lua_video.cpp - use accessor function
static inline int CON_LINE_ADV(void) { return GLYPH_H + FCEU_GetLuaConsoleLineGap(); }

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
// Console state is now managed in lua_video.cpp - use accessor functions to access it
// Console functions moved to lua_video.cpp (they're used by DrawLuaConsole)

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
bool luaInitialized = false;  // Non-static so other modules can access it
 
// FPS tracking moved to lua_emulator.cpp
 
 // Forward declaration
 static uint8* currentXBuf = NULL;
 // Store the actual frame buffer passed to FCEU_LuaGui (not the overlay)
 static uint8* s_frameXBuf = NULL;
 // Pre-initialize screenshot directory path to avoid lag on first screenshot
 static bool s_screenshotDirInitialized = false;
 
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
	 DWORD currentTime = GetTickCount();
	 Lua_InputUpdateRumble(currentTime);
	 
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
	 if (s_luaDisabled && !FCEU_IsLuaConsoleVisible()) {
		 ClearOverlaysIfAny();
		 // Keep s_frameXBuf set even when Lua is disabled, so screenshots can work
		 return; // no composite, no "LUA OFF" banner, truly silent
	 }
	 
     static DWORD lastGuiTime = 0;
     static bool prevConsoleVisible = false;
     DWORD now = GetTickCount();
     DWORD step = Lua_RuntimeGetScriptInterval(); // script()-cadence (default 33ms)
	 
	 // Check if console visibility changed - if so, force an update immediately
	 bool consoleVisibilityChanged = (FCEU_IsLuaConsoleVisible() != prevConsoleVisible);
	 
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
         if (s_overlay_back && !FCEU_IsLuaConsoleVisible()) {
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
		 Lua_VideoSetOverlayDirty(false);  // Reset before calling Lua
		 
		 // Only run Lua scripts if Lua is enabled and initialized
		 if (!s_luaDisabled && luaInitialized && luaState != NULL) {
			 // Check if script is sleeping - if so, skip callback execution
			 if (!Lua_IsSleeping(luaState)) {
				 // Always reset render target to screen at start of each frame
				 // This ensures we don't leave currentXBuf pointing to a canvas from previous frame
				 Lua_VideoResetRenderTarget();
				 
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
						 Lua_VideoSetOverlayDirty(true);  // Error marker counts as drawing
					 }
					 lua_pop(luaState, 1);
					 // Leave g_overlayDirty as-is (true if error marker drawn)
				 }
			 } else {
			 // script() function doesn't exist
				 lua_pop(luaState, 1);
				 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"NO script()", 0x0F | 0x80);
					 Lua_VideoSetOverlayDirty(true);  // Error indicator counts as drawing
				 }
			 }
			 
			 currentXBuf = NULL;
			 } // End of sleep check block
		 } else {
			 // Lua not initialized
			 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"LUA OFF", 0x0F | 0x80);
				 Lua_VideoSetOverlayDirty(true);  // Status indicator counts as drawing
			 }
		 }
		 
         // If console visible, draw it now onto back buffer and mark dirty
		 if (FCEU_IsLuaConsoleVisible() && s_overlay_back) {
			 // Handle D-pad scrolling (vertical)
			 bool dpadUp = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
			 bool dpadDown = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
			 
			 // Get pointers to scroll state variables
			 bool* s_consoleDpadUpLast = FCEU_GetLuaConsoleDpadUpLast();
			 bool* s_consoleDpadDownLast = FCEU_GetLuaConsoleDpadDownLast();
			 int* s_consoleScrollHoldFrames = FCEU_GetLuaConsoleScrollHoldFrames();
			 
			 // Scroll on button press (immediate) or hold (throttled)
			 if (dpadUp) {
				 if (!*s_consoleDpadUpLast) {
					 // First press - scroll immediately
					 FCEU_SetLuaConsoleScrollOffset(FCEU_GetLuaConsoleScrollOffset() + 1);
					 *s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 (*s_consoleScrollHoldFrames)++;
					 if (*s_consoleScrollHoldFrames >= 3) {
						 FCEU_SetLuaConsoleScrollOffset(FCEU_GetLuaConsoleScrollOffset() + 1);
						 *s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 if (dpadDown) {
				 if (!*s_consoleDpadDownLast) {
					 // First press - scroll immediately
				 int offset = FCEU_GetLuaConsoleScrollOffset();
				 if (offset > 0) {
					 FCEU_SetLuaConsoleScrollOffset(offset - 1);
				 }
					 *s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 (*s_consoleScrollHoldFrames)++;
					 if (*s_consoleScrollHoldFrames >= 3) {
						 int offset = FCEU_GetLuaConsoleScrollOffset();
						 if (offset > 0) {
							 FCEU_SetLuaConsoleScrollOffset(offset - 1);
						 }
						 *s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadUp && !dpadDown) {
				 *s_consoleScrollHoldFrames = 0;
			 }
			 
			 *s_consoleDpadUpLast = dpadUp;
			 *s_consoleDpadDownLast = dpadDown;
			 
			 // Handle D-pad scrolling (horizontal)
			 bool dpadLeft = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
			 bool dpadRight = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
			 
			 // Get pointers to horizontal scroll state variables
			 bool* s_consoleDpadLeftLast = FCEU_GetLuaConsoleDpadLeftLast();
			 bool* s_consoleDpadRightLast = FCEU_GetLuaConsoleDpadRightLast();
			 int* s_consoleScrollHoldFramesH = FCEU_GetLuaConsoleScrollHoldFramesH();
			 
			 // Scroll horizontally on button press (immediate) or hold (throttled)
			 if (dpadLeft) {
				 if (!*s_consoleDpadLeftLast) {
					 // First press - scroll immediately (scroll left = decrease offset)
					 int offsetH = FCEU_GetLuaConsoleScrollOffsetH();
					 if (offsetH > 0) {
						 offsetH -= 8; // Scroll by ~1 character width
						 if (offsetH < 0) offsetH = 0;
						 FCEU_SetLuaConsoleScrollOffsetH(offsetH);
					 }
					 *s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 (*s_consoleScrollHoldFramesH)++;
					 if (*s_consoleScrollHoldFramesH >= 3) {
						 int offsetH = FCEU_GetLuaConsoleScrollOffsetH();
						 if (offsetH > 0) {
							 offsetH -= 8;
							 if (offsetH < 0) offsetH = 0;
							 FCEU_SetLuaConsoleScrollOffsetH(offsetH);
						 }
						 *s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 if (dpadRight) {
				 if (!*s_consoleDpadRightLast) {
					 // First press - scroll immediately (scroll right = increase offset)
					 FCEU_SetLuaConsoleScrollOffsetH(FCEU_GetLuaConsoleScrollOffsetH() + 8); // Scroll by ~1 character width
					 *s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 (*s_consoleScrollHoldFramesH)++;
					 if (*s_consoleScrollHoldFramesH >= 3) {
						 FCEU_SetLuaConsoleScrollOffsetH(FCEU_GetLuaConsoleScrollOffsetH() + 8);
						 *s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadLeft && !dpadRight) {
				 *s_consoleScrollHoldFramesH = 0;
			 }
			 
			 *s_consoleDpadLeftLast = dpadLeft;
			 *s_consoleDpadRightLast = dpadRight;
			 
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
			 int consoleCount = FCEU_GetLuaConsoleCount();
			 int maxScroll = (consoleCount > maxLines) ? (consoleCount - maxLines) : 0;
			 int scrollOffset = FCEU_GetLuaConsoleScrollOffset();
			 if (scrollOffset > maxScroll) FCEU_SetLuaConsoleScrollOffset(maxScroll);
			 if (scrollOffset < 0) FCEU_SetLuaConsoleScrollOffset(0);
			 
			 Lua_VideoSetOverlayDirty(true);
			 ok = true; // force publish when console drew
         } else if (!FCEU_IsLuaConsoleVisible() && prevConsoleVisible && s_overlay_back) {
             // Console just turned off: push a cleared overlay once to remove it immediately
             memset(s_overlay_back, 0, 256 * 240);
             Lua_VideoSetOverlayDirty(true);
             ok = true;
		 }
		 
		 // Draw console using the new drawer with proper line spacing
		 DrawLuaConsole(s_overlay_back);
		 
		 // Only publish the new overlay if something was drawn
         if (Lua_VideoGetOverlayDirty()) {
			 // Only publish if the back buffer actually differs from the front
			 // This prevents unnecessary swaps when content hasn't changed
			 if (!s_overlay_front || overlay_has_changes(s_overlay_back, s_overlay_front)) {
				 SwapOverlays();
			 }
		 }
         prevConsoleVisible = (FCEU_IsLuaConsoleVisible() != 0);
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