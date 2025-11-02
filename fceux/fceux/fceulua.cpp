/* FCE Ultra Lua Integration for Xbox 360
 * Implementation
 */

#include "../stdafx.h"

#ifdef USE_LUA

#include "fceulua.h"
#include "fceu.h"
#include "drawing.h"
#include "video.h"
#include "driver.h"
#include <stdio.h>
#include <string.h>

// On-screen status message for debugging (shows last load attempt)
static char g_luaStatusMsg[128] = "Lua: (not loaded)";
// stdafx.h already includes xtl.h which provides GetTickCount() and DWORD

// Double-buffered overlay for Lua-drawn content (updated at 30Hz, composited at 60Hz to prevent flicker)
// Front buffer: currently displayed (what we composite)
// Back buffer: where Lua draws next frame (only published on success)
static uint8* s_overlay_front = NULL; // currently displayed
static uint8* s_overlay_back  = NULL; // where Lua draws next

static void EnsureOverlay() {
    if (!s_overlay_front) {
        s_overlay_front = (uint8*)malloc(256 * 240);
        if (s_overlay_front) {
            memset(s_overlay_front, 0, 256 * 240);
        }
    }
    if (!s_overlay_back) {
        s_overlay_back = (uint8*)malloc(256 * 240);
        if (s_overlay_back) {
            memset(s_overlay_back, 0, 256 * 240);
        }
    }
}

static inline void CompositeOverlay(uint8* XBuf) {
    // Blit the front overlay onto the NES frame every frame (prevents flicker when Lua runs at 30Hz)
    if (!s_overlay_front || !XBuf) return;
    
    const int N = 256 * 240;
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

// Helper: Clear a rectangle in the overlay buffer with bounds checking
static inline void clear_rect(uint8* buf, int x, int y, int w, int h) {
    if (!buf) return;
    
    // Clamp to valid bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 256) w = 256 - x;
    if (y + h > 240) h = 240 - y;
    if (w <= 0 || h <= 0) return;
    
    // Clear each row of the rectangle
    for (int dy = 0; dy < h; ++dy) {
        memset(buf + (y + dy) * 256 + x, 0, w);
    }
}

// Utility: Check if overlay actually changed (fast path with stripes, then full compare)
static inline bool overlay_has_changes(const uint8* a, const uint8* b) {
    if (!a || !b) return true;  // If either is NULL, consider it changed
    // Scan a few stripes first (fast path), then fall back to full compare
    const int pitch = 256, h = 240;
    for (int y = 0; y < h; y += 16) {
        if (memcmp(a + y*pitch, b + y*pitch, pitch) != 0) return true;
    }
    return memcmp(a, b, 256*240) != 0;
}

// Dirty flag: tracks if anything was actually drawn to the overlay
// Only publish new overlay if Lua succeeded AND drew something
static bool g_overlayDirty = false;

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

// FPS tracking
static DWORD lastFPSUpdate = 0;
static int frameCount = 0;
static double currentFPS = 0.0;
static DWORD lastFrameTime = 0;

// Forward declaration
static uint8* currentXBuf = NULL;

// Lua drawing function - allows scripts to draw text
int lua_drawtext(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return luaL_error(L, "drawtext(x, y, text [, color]) requires at least 3 arguments");
	}
	
	int x = (int)luaL_checkinteger(L, 1);
	int y = (int)luaL_checkinteger(L, 2);
	const char* text = luaL_checkstring(L, 3);
	int color = 0x20; // default color
	if (n >= 4) {
		color = (int)luaL_optinteger(L, 4, 0x20);
	}
	
	// Draw text on the current frame buffer (set by FCEU_LuaGui)
	if (currentXBuf && x >= 0 && y >= 0 && x < 256 && y < 240) {
		uint8 *dest = currentXBuf + y * 256 + x;
		DrawTextTrans(dest, 256, (uint8*)text, color + 0x80);
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Get current FPS
int lua_getfps(lua_State *L) {
	lua_pushnumber(L, currentFPS);
	return 1;
}

// Initialize Lua
static void InitLua() {
	if (luaState != NULL) {
		lua_close(luaState);
	}
	
	luaState = lua_open();
	if (luaState == NULL) {
		return;
	}
	
	luaL_openlibs(luaState);
	
	// Register FCEU functions
	lua_register(luaState, "drawtext", lua_drawtext);
	lua_register(luaState, "getfps", lua_getfps);
	
	luaInitialized = true;
}

// Load and run Lua script
int FCEU_LoadLuaScript(const char* filename) {
	if (!luaInitialized) {
		InitLua();
		if (!luaInitialized) {
			return 0;
		}
	}
	
	if (luaState == NULL) {
		return 0;
	}
	
	// Try to load script from game:/lua/ folder (expanded paths for RGH/retail)
	char fullpath[512];
	const char* workingPath = NULL;
	
	// Try different path formats - prioritize user-writable locations first
	// Note: Xbox file paths are case-sensitive and need correct separators
	// hdd1: locations work even when game: is read-only (XZP/STFS packages)
	const char* paths[] = {
		"hdd1:\\fce360-enhanced\\lua\\%s",  // Primary: user-writable location
		"hdd1:\\fce360-enhanced\\Lua\\%s",  // Uppercase variant
		"game:\\lua\\%s",                   // Fallback: game folder (may be read-only)
		"game:/lua/%s",
		"game:lua/%s",
		"game:\\Lua\\%s",                  // Try uppercase
		"game:/Lua/%s",
		"hdd1:\\fce360-enhanced\\lua\\%s",          // Legacy paths for compatibility
		"hdd1:\\fce360-enhanced\\Lua\\%s",
		"hdd1:\\lua\\%s",
		"hdd1:\\Lua\\%s",
		"usb0:\\lua\\%s",
		"usb0:\\Lua\\%s",
		"game:\\fce360-enhanced\\lua\\%s",
		"game:\\fce360-enhanced\\Lua\\%s",
		"game:\\%s",                        // Try directly in game: folder
		"game:/%s",
		"hdd1:\\_Emus\\fce360-enhanced\\lua\\%s"  // Alternative emulator organization path
	};
	
	const int numPaths = sizeof(paths) / sizeof(paths[0]);
	
	for (int i = 0; i < numPaths; i++) {
		snprintf(fullpath, sizeof(fullpath), paths[i], filename);
		
		// Check if file exists - try both read and binary mode
		FILE* f = fopen(fullpath, "rb");
		if (f != NULL) {
			// File exists - get size to verify it's valid
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			fclose(f);
			
				if (size > 0) {
				workingPath = fullpath;
				break;
			}
		}
	}
	
	if (workingPath == NULL) {
		snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: %s NOT FOUND", filename);
		return 0;
	}
	
	// Update status message
	snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loading %s", workingPath);
	g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
	
	// Load and execute script
	int result = luaL_dofile(luaState, workingPath);
	if (result != 0) {
		// Script failed to load - show error on screen
		const char* err = lua_tostring(luaState, -1);
		if (err) {
			snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: ERROR - %s", err);
		} else {
			snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Load failed");
		}
		g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
		lua_pop(luaState, 1);
		return 0;
	}
	
	snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loaded %s", workingPath);
	g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
	// Verify gui() function exists
	lua_getglobal(luaState, "gui");
	if (lua_isfunction(luaState, -1)) {
		lua_pop(luaState, 1);
	} else {
		lua_pop(luaState, 1);
	}
	
	return 1;
}

// Auto-load all .lua scripts from lua directories
void FCEU_AutoLoadLuaScripts(void) {
	// Try multiple directories in priority order (user-writable first)
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
	int totalLoaded = 0;
	
	for (int dirIdx = 0; dirIdx < numDirs; dirIdx++) {
		char searchPattern[512];
		snprintf(searchPattern, sizeof(searchPattern), "%s\\*.lua", searchDirs[dirIdx]);
		
		WIN32_FIND_DATAA ffd;
		HANDLE h = FindFirstFileA(searchPattern, &ffd);
		
		if (h != INVALID_HANDLE_VALUE) {
			do {
				if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					// Found a .lua file - load it
					if (FCEU_LoadLuaScript(ffd.cFileName)) {
						totalLoaded++;
					}
				}
			} while (FindNextFileA(h, &ffd));
			FindClose(h);
		}
		
		// Also try lowercase pattern
		snprintf(searchPattern, sizeof(searchPattern), "%s\\*.LUA", searchDirs[dirIdx]);
		h = FindFirstFileA(searchPattern, &ffd);
		if (h != INVALID_HANDLE_VALUE) {
			do {
				if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					if (FCEU_LoadLuaScript(ffd.cFileName)) {
						totalLoaded++;
					}
				}
			} while (FindNextFileA(h, &ffd));
			FindClose(h);
		}
	}
	
}

// Frame boundary callback
void FCEU_LuaFrameBoundary() {
	if (!luaInitialized || luaState == NULL) {
		return;
	}
	
	// Update FPS
	DWORD currentTime = GetTickCount();
	frameCount++;
	
	if (lastFPSUpdate == 0) {
		lastFPSUpdate = currentTime;
	}
	
	if (currentTime - lastFPSUpdate >= 1000) {
		currentFPS = (double)frameCount * 1000.0 / (double)(currentTime - lastFPSUpdate);
		frameCount = 0;
		lastFPSUpdate = currentTime;
	}
	
	lastFrameTime = currentTime;
}

// GUI drawing callback - called from video.cpp
// Update Lua at 30Hz, but composite the last overlay every frame to prevent flicker
// Double-buffered: only publish new overlay if Lua succeeds (fail-safe)
void FCEU_LuaGui(uint8 *XBuf) {
	// Safety check: ensure overlay is initialized before proceeding
	EnsureOverlay();
	if (!s_overlay_back || !s_overlay_front) {
		// Overlay buffers not initialized - skip this frame
		return;
	}
	
	static DWORD lastGuiTime = 0;
	DWORD now = GetTickCount();
	const DWORD step = 33; // ~30Hz Lua updates (33ms between calls)
	
	// Update overlay contents at ~30Hz (only when Lua needs to run)
	if (now - lastGuiTime >= step) {
		lastGuiTime = now;
		
		// Seed the back buffer from the front buffer (makes swaps safe - never publish all zeros)
		// This ensures even if script doesn't draw, we keep the last good HUD
		// BUT: If we just cleared the status, don't re-seed the cleared area (would bring back ghost pixels)
		static bool justClearedStatus = false;
		if (s_overlay_back) {
			if (s_overlay_front) {
				if (justClearedStatus) {
					// After clearing status, copy the front buffer but preserve the cleared area
					// Copy everything EXCEPT the cleared region (y=17 to y=31)
					memcpy(s_overlay_back, s_overlay_front, 256 * 240);  // Full copy first
					// Then re-clear the status area to prevent ghost pixels from re-appearing
					const int sx = 0, sy = 17, clearW = 256, clearH = 14;
					clear_rect(s_overlay_back, sx, sy, clearW, clearH);
					justClearedStatus = false;  // Reset after handling
				} else {
					memcpy(s_overlay_back, s_overlay_front, 256 * 240);  // Normal seed with last good HUD
				}
			} else {
				memset(s_overlay_back, 0, 256 * 240);
			}
		}
		
		// Optional status message for a few seconds - clears automatically after timeout
		// Status banner (top-left)
		static int statusTicks = 0;
		static bool statusShown = false;
		static char lastMsg[128] = {0};
		bool needToClearStatus = false;
		
		// If message text changed since last frame, restart TTL
		if (strncmp(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1) != 0) {
			strncpy(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1);
			lastMsg[sizeof(lastMsg) - 1] = '\0';
			statusTicks = 0;
			statusShown = false;
		}
		
		if (s_overlay_back) {
			// Draw area params (over-clear generously to eliminate ghosting)
			// Status message is drawn at y=20 (below "LUA ON" at y=4), x=4
			// "LUA ON" occupies y=4 to y=12 (8px tall), so status starts lower at y=20
			const int statusY = 20;   // Position lower on screen, well below "LUA ON" text
			const int sx = 0;         // clear full width to be safe
			const int sy = 17;        // 3 pixels above status text baseline (y=20)
			const int clearW = 256;   // wipe whole row region; avoids width misestimates
			const int clearH = 14;    // 8px glyph + 6px padding (3 above, 3 below) - more generous
			
			if (statusTicks < 180) {  // ~6s @ 30Hz
				// Draw status message below "LUA ON" (which is at 4*256 + 4)
				// Status is drawn at y=20, so offset is 20*256+4 = 5124, buffer is 256*240=61440 bytes - safe
				if (s_overlay_back && statusY >= 0 && statusY < 232 && g_luaStatusMsg[0] != '\0') {
					DrawTextTrans(s_overlay_back + statusY*256 + 4, 256, (uint8*)g_luaStatusMsg, 0x2E | 0x80);
				}
				statusTicks++;
				statusShown = true;
				// (Intentionally DO NOT set g_overlayDirty here; let Lua work trigger swaps)
			} else if (statusShown) {
				// Clear both back and front buffers so nothing persists
				// Over-clear a larger rectangle to catch any edge pixels and prevent ghosting
				if (s_overlay_back) {
					clear_rect(s_overlay_back, sx, sy, clearW, clearH);
				}
				if (s_overlay_front) {
					clear_rect(s_overlay_front, sx, sy, clearW, clearH);
				}
				
				statusShown = false;
				justClearedStatus = true;   // Mark that we just cleared (prevent re-seeding)
				g_overlayDirty = true;      // Ensure overlay considered changed
				needToClearStatus = true;   // Force publish of cleared version
			}
		}
		
		bool ok = false;  // Track if Lua succeeded
		// Don't reset g_overlayDirty if we just cleared status (it's already set for the clear)
		if (!needToClearStatus) {
			g_overlayDirty = false;  // Reset before calling Lua
		}
		
		if (luaInitialized && luaState != NULL) {
			// Point Lua draw calls at the back buffer, not the front buffer
			currentXBuf = s_overlay_back;
			
			// Call gui() function if it exists
			lua_getglobal(luaState, "gui");
			if (lua_isfunction(luaState, -1)) {
				if (lua_pcall(luaState, 0, 0, 0) == 0) {
					ok = true;  // Script executed successfully
				} else {
					// Lua error - draw a visible error marker
					const char* err = lua_tostring(luaState, -1);
					// Draw error indicator on screen so failures are visible
					if (s_overlay_back) {
						DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"LUA ERR", 0x0F | 0x80);
						g_overlayDirty = true;  // Error marker counts as drawing
					}
					lua_pop(luaState, 1);
					// Leave g_overlayDirty as-is (true if error marker drawn)
				}
			} else {
				// gui() function doesn't exist
				lua_pop(luaState, 1);
				if (s_overlay_back) {
					DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"NO gui()", 0x0F | 0x80);
					g_overlayDirty = true;  // Error indicator counts as drawing
				}
			}
			
			currentXBuf = NULL;
		} else {
			// Lua not initialized
			if (s_overlay_back) {
				DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"LUA OFF", 0x0F | 0x80);
				g_overlayDirty = true;  // Status indicator counts as drawing
			}
		}
		
		// Only publish the new overlay if Lua succeeded AND actually drew something
		// OR if we cleared the status message this frame (to make it disappear)
		// If script early-returns without drawing, we keep showing the last good overlay
		// The memcpy seed above ensures we never swap in all zeros
		if ((ok && g_overlayDirty) || needToClearStatus) {
			// Only publish if the back buffer actually differs from the front
			// This prevents unnecessary swaps when content hasn't changed
			// Note: needToClearStatus ensures we publish even if Lua didn't draw (status clear is important)
			if (!s_overlay_front || overlay_has_changes(s_overlay_back, s_overlay_front)) {
				SwapOverlays();
			}
		}
		// Otherwise keep showing last good overlay (prevents blank HUD on no-draw frames)
	}
	
	// ALWAYS composite the last published overlay every frame (prevents flicker when Lua runs at 30Hz)
	// This ensures smooth 60Hz display even though Lua only updates at 30Hz
	if (XBuf && s_overlay_front) {
		CompositeOverlay(XBuf);
	}
}

// Stop Lua
void FCEU_LuaStop() {
	if (luaState != NULL) {
		lua_close(luaState);
		luaState = NULL;
	}
	luaInitialized = false;
}

// Call registered Lua functions
void CallRegisteredLuaFunctions(LUACALL callID) {
	// Not implemented yet - can be extended for more callbacks
	(void)callID;
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

#endif // USE_LUA
