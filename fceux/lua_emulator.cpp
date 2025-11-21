#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_emulator.h"
#include "lua_shared_state.h"
#include "lua_helpers.h"
#include "fceu.h"
#include "x6502.h"
#include "types.h"
#include "drawing.h"
#include "../xbox/Cemulator.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// Use timing constants from shared header
using namespace LuaProfilerState;

// Frame counter and FPS tracking
static int s_totalFrameCount = 0;
static DWORD lastFPSUpdate = 0;
static int frameCount = 0;  // FPS calculation counter (resets every second)
static double currentFPS = 0.0;
static DWORD lastFrameTime = 0;

// Extern overlay dimensions
enum { OVL_W = 256, OVL_H = 240 };

// ==================== Emulation State Functions ====================

// getframecount() -> integer
// Gets total frame count since game start
// Returns the number of frames that have been emulated since the game started
static int lua_getframecount(lua_State* L)
{
	// Use our own frame counter that increments every frame
	// This is more reliable than FCEUMOV_GetFrame() which only works during movie playback
	lua_pushinteger(L, s_totalFrameCount);
	return 1;
}

// getframecycles() -> integer
// Gets cycles executed in the current frame
// Returns the number of CPU cycles accumulated since the start of the current frame
static int lua_getframecycles(lua_State* L)
{
	// Get cycles from x6502.cpp - this is the timestamp variable
	// which accumulates cycles during the frame and resets at frame end
	uint32 cycles = FCEU_GetFrameCycles();
	
	// If we're after frame end (timestamp already reset to 0), use the latched value
	// This happens when called from gui() callback which runs after the frame completes
	if (cycles == 0) {
		cycles = FCEU_GetLastFrameCycles();
	}
	
	lua_pushinteger(L, (lua_Integer)cycles);
	return 1;
}

// getppucycles() -> integer
// Gets the number of PPU cycles executed in the current frame
// Derived from CPU frame cycles (3 PPU cycles per CPU cycle)
static int lua_getppucycles(lua_State* L)
{
	uint32 cycles = FCEU_GetPPUCycles();

	// If frame already finished, use the latched value
	if (cycles == 0) {
		cycles = FCEU_GetLastPPUCycles();
	}

	lua_pushinteger(L, (lua_Integer)cycles);
	return 1;
}

// getapucycles() -> integer
// Gets the APU cycle count (matches CPU cycles)
static int lua_getapucycles(lua_State* L)
{
	uint32 cycles = FCEU_GetAPUCycles();

	if (cycles == 0) {
		cycles = FCEU_GetLastAPUCycles();
	}

	lua_pushinteger(L, (lua_Integer)cycles);
	return 1;
}

// getelapsedtime() -> float
// Gets elapsed time since game start in seconds
// Returns the elapsed time as a floating-point number
static int lua_getelapsedtime(lua_State* L)
{
	// Use our frame counter and divide by NTSC frame rate
	double elapsedTime = (double)s_totalFrameCount / LuaProfilerState::kNTSCFrameRate;
	
	lua_pushnumber(L, elapsedTime);
	return 1;
}

// getelapsedframes() -> integer
// Gets elapsed frames since game start
// Returns the total number of frames that have elapsed since the ROM was loaded
static int lua_getelapsedframes(lua_State* L)
{
	// Return the total frame count since game start
	// This is the same as getframecount() but with a name that pairs with getelapsedtime()
	lua_pushinteger(L, s_totalFrameCount);
	return 1;
}

// gettime() -> integer
// Gets current system time in milliseconds since system boot
// Returns: Integer (milliseconds since system boot)
// Use case: Time-based logic, timestamps, relative time measurements
// Note: This returns milliseconds since system boot, not since epoch. Use for relative timing.
static int lua_gettime(lua_State* L)
{
	// GetTickCount() returns milliseconds since system boot
	// This is useful for relative time measurements and timestamps
	DWORD currentTime = GetTickCount();
	lua_pushinteger(L, (lua_Integer)currentTime);
	return 1;
}

// getfps() -> float
// Gets current frames-per-second
// Returns: Float (current FPS)
// Use case: Performance monitoring, frame rate display
static int lua_getfps(lua_State* L)
{
	lua_pushnumber(L, currentFPS);
	return 1;
}

// isframeadvancing() -> boolean
// Checks if emulation is advancing frames
// Returns true if frames are advancing, false if paused
static int lua_isframeadvancing(lua_State* L)
{
	extern int EmulationPaused;
	
	// EmulationPaused bit 0 indicates if paused
	// If bit 0 is set, emulation is paused (frames NOT advancing)
	// If bit 0 is clear, emulation is running (frames ARE advancing)
	lua_pushboolean(L, (EmulationPaused & 1) == 0);
	return 1;
}

// isrewinding() -> boolean
// Checks if currently rewinding
// Returns true if rewinding, false otherwise
static int lua_isrewinding(lua_State* L)
{
	// Access the global Cemulator instance
	extern Cemulator emul;
	
	// Return rewind state
	lua_pushboolean(L, emul.IsRewinding() ? 1 : 0);
	return 1;
}

// isfastforwarding() -> boolean
// Checks if currently fast-forwarding
// Returns true if fast-forwarding, false otherwise
static int lua_isfastforwarding(lua_State* L)
{
	// Access the global Cemulator instance
	extern Cemulator emul;
	
	// Return fast-forward state
	lua_pushboolean(L, emul.IsFastForwarding() ? 1 : 0);
	return 1;
}

// getscreenwidth() -> integer
// Gets screen width in pixels
// Returns: Integer (256 for NES)
// Use case: Dynamic positioning, centering
static int lua_getscreenwidth(lua_State* L)
{
	// NES screen width is always 256 pixels
	lua_pushinteger(L, OVL_W);  // OVL_W is 256
	return 1;
}

// getscreenheight() -> integer
// Gets screen height in pixels
// Returns: Integer (240 for NES)
// Use case: Dynamic positioning
static int lua_getscreenheight(lua_State* L)
{
	// NES screen height is always 240 pixels
	lua_pushinteger(L, OVL_H);  // OVL_H is 240
	return 1;
}

// getscreensize() -> table
// Gets screen dimensions
// Returns: Table {width, height}
// Use case: Screen size queries
static int lua_getscreensize(lua_State* L)
{
	// Create a table with width and height
	lua_newtable(L);
	
	// Push width
	lua_pushstring(L, "width");
	lua_pushinteger(L, OVL_W);  // 256
	lua_settable(L, -3);
	
	// Push height
	lua_pushstring(L, "height");
	lua_pushinteger(L, OVL_H);  // 240
	lua_settable(L, -3);
	
	// Also push as array indices for convenience
	lua_pushinteger(L, 1);
	lua_pushinteger(L, OVL_W);
	lua_settable(L, -3);
	
	lua_pushinteger(L, 2);
	lua_pushinteger(L, OVL_H);
	lua_settable(L, -3);
	
	return 1;  // Return the table
}

// ==================== Lifecycle Functions ====================

// Update frame counter and FPS tracking
// Call this from FCEU_LuaGui() every frame
void Lua_EmulatorUpdateFrame(void)
{
	DWORD currentTime = GetTickCount();
	frameCount++;
	s_totalFrameCount++;  // Increment total frame counter
	
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

// Reset emulator state (frame counter, FPS tracking)
// Call this when game is closed or Lua is stopped
void Lua_EmulatorReset(void)
{
	s_totalFrameCount = 0;
	lastFPSUpdate = 0;
	frameCount = 0;
	currentFPS = 0.0;
	lastFrameTime = 0;
}

// ==================== Registrar Function ====================

static const luaL_Reg kEmulatorFuncs[] = {
	{"getframecount", lua_getframecount},
	{"getframecycles", lua_getframecycles},
	{"getppucycles", lua_getppucycles},
	{"getapucycles", lua_getapucycles},
	{"getelapsedtime", lua_getelapsedtime},
	{"getelapsedframes", lua_getelapsedframes},
	{"gettime", lua_gettime},
	{"getfps", lua_getfps},
	{"isframeadvancing", lua_isframeadvancing},
	{"isrewinding", lua_isrewinding},
	{"isfastforwarding", lua_isfastforwarding},
	{"getscreenwidth", lua_getscreenwidth},
	{"getscreenheight", lua_getscreenheight},
	{"getscreensize", lua_getscreensize},
	{NULL, NULL}
};

void Lua_RegisterEmulator(lua_State* L) {
	if (!L) {
		return;
	}

	// Manually register each function (luaL_register with NULL has issues)
	for (const luaL_Reg* reg = kEmulatorFuncs; reg->name != NULL; reg++) {
		lua_register(L, reg->name, reg->func);
	}
}

#endif // USE_LUA

