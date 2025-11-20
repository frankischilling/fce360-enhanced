#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_profiler.h"
#include "lua_helpers.h"
#include "lua_video.h" // For LuaConsolePushLine
#include "fceu.h" // For FCEU_printf
#include "types.h"

#include <map>
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

// Constants for frame timing
static const double kNTSCFrameRate = 60.0988118623484;
static const double kIdealFrameMs = 1000.0 / kNTSCFrameRate;

// Profile start times map
static std::map<std::string, DWORD> s_luaProfileStartTimes;

// ==================== Timing Functions ====================

// gettimedelta() -> float
// Gets time since last frame in seconds
// Returns: Float (time in seconds since last frame)
// Use case: Delta time calculations, physics, frame-independent movement
// Note: Returns 0.0 on first call, then the actual delta time on subsequent calls
static int lua_gettimedelta(lua_State* L)
{
	DWORD currentTime = GetTickCount();
	
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

// getframetime_ms() -> float
// Gets time since last frame in milliseconds
// Returns: Float milliseconds since last frame (0 on first call)
// Use case: Frame pacing metrics
static int lua_getframetime_ms(lua_State* L)
{
	DWORD currentTime = GetTickCount();

	lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	lua_gettable(L, LUA_REGISTRYINDEX);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
		lua_pushinteger(L, (lua_Integer)currentTime);
		lua_settable(L, LUA_REGISTRYINDEX);
		lua_pushnumber(L, 0.0);
		return 1;
	}

	lua_Integer lastFrameTime = lua_tointeger(L, -1);
	lua_pop(L, 1);

	DWORD deltaMs = currentTime - (DWORD)lastFrameTime;

	lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	lua_pushinteger(L, (lua_Integer)currentTime);
	lua_settable(L, LUA_REGISTRYINDEX);

	lua_pushnumber(L, (lua_Number)deltaMs);
	return 1;
}

// getjitter_ms() -> float
// Returns absolute deviation from the ideal 60 Hz frame time.
static int lua_getjitter_ms(lua_State* L)
{
	DWORD currentTime = GetTickCount();

	lua_pushstring(L, "FCEU_LAST_FRAME_TIME_JITTER");
	lua_gettable(L, LUA_REGISTRYINDEX);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "FCEU_LAST_FRAME_TIME_JITTER");
		lua_pushinteger(L, (lua_Integer)currentTime);
		lua_settable(L, LUA_REGISTRYINDEX);
		lua_pushnumber(L, 0.0);
		return 1;
	}

	lua_Integer lastFrameTime = lua_tointeger(L, -1);
	lua_pop(L, 1);

	DWORD deltaMs = currentTime - (DWORD)lastFrameTime;

	lua_pushstring(L, "FCEU_LAST_FRAME_TIME_JITTER");
	lua_pushinteger(L, (lua_Integer)currentTime);
	lua_settable(L, LUA_REGISTRYINDEX);

	double jitter = fabs((double)deltaMs - kIdealFrameMs);
	lua_pushnumber(L, jitter);
	return 1;
}

// ==================== Profiling Functions ====================

// beginprofile(tag) -> nil
// Marks the beginning of a profiling section identified by tag.
// Parameters: tag (string) - identifier for the profile block
// Returns: Nothing
static int lua_beginprofile(lua_State* L)
{
	const char* rawTag = LuaCheckString(L, 1, "beginprofile");
	if (!rawTag || !rawTag[0])
		return LuaArgError(L, "beginprofile", 1, "requires a non-empty tag string");

	std::string tag(rawTag);
	s_luaProfileStartTimes[tag] = GetTickCount();
	return 0;
}

// endprofile(tag) -> nil
// Marks the end of a profiling section and logs elapsed time to the Lua console.
// Parameters: tag (string) - identifier used in beginprofile
// Returns: Nothing
static int lua_endprofile(lua_State* L)
{
	const char* rawTag = LuaCheckString(L, 1, "endprofile");
	if (!rawTag || !rawTag[0])
		return LuaArgError(L, "endprofile", 1, "requires a non-empty tag string");

	std::string tag(rawTag);
	std::map<std::string, DWORD>::iterator it = s_luaProfileStartTimes.find(tag);
	char buffer[160];

	if (it == s_luaProfileStartTimes.end())
	{
		snprintf(buffer, sizeof(buffer), "[PROFILE] %s: beginprofile() missing", tag.c_str());
		LuaConsolePushLine(buffer);
		return 0;
	}

	DWORD start = it->second;
	s_luaProfileStartTimes.erase(it);

	DWORD elapsedMs = GetTickCount() - start;
	snprintf(buffer, sizeof(buffer), "[PROFILE] %s: %u ms", tag.c_str(), (unsigned)elapsedMs);
	LuaConsolePushLine(buffer);
	FCEU_printf("%s\n", buffer);
	return 0;
}

// ==================== Cadence Management Functions ====================

// sleepframes(frames) -> nil
// Delays script execution for N frames
// Parameters: frames (integer) - number of frames to sleep
// Returns: Nothing
// Use case: Frame-accurate delays
// Note: Pauses emulation during sleep, freezing the game for the specified number of frames
static int lua_sleepframes(lua_State* L)
{
	// Get number of frames to sleep
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "sleepframes", 1, 1, n);
	}
	
	int frames = LuaCheckNonNegative(L, 1, "sleepframes", "frames");
	
	// Calculate sleep duration in milliseconds using standard NTSC timing
	DWORD sleepDurationMs = (DWORD)(frames * kIdealFrameMs);
	DWORD sleepStartTime = GetTickCount();
	
	// Store sleep start time and duration in Lua registry
	lua_pushstring(L, "FCEU_SLEEP_START_TIME");
	lua_pushinteger(L, (lua_Integer)sleepStartTime);
	lua_settable(L, LUA_REGISTRYINDEX);
	
	lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
	lua_pushinteger(L, (lua_Integer)sleepDurationMs);
	lua_settable(L, LUA_REGISTRYINDEX);
	
	// Store original pause state so we can restore it
	extern int FCEUI_EmulationPaused(void);
	int wasPaused = FCEUI_EmulationPaused();
	lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
	lua_pushboolean(L, wasPaused != 0);
	lua_settable(L, LUA_REGISTRYINDEX);
	
	// Pause emulation to freeze the game during sleep
	extern void FCEUI_SetEmulationPaused(int val);
	FCEUI_SetEmulationPaused(1);
	
	return 0; // Returns nothing
}

// Helper function to check if script is currently sleeping
// Returns true if script should skip execution, false otherwise
// Also handles unpausing emulation when sleep completes
bool Lua_IsSleeping(lua_State* L)
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

// ==================== Registrar and Lifecycle ====================

void Lua_RegisterProfiler(lua_State* L)
{
	if (!L) {
		return;
	}

	// Timing functions
	lua_register(L, "gettimedelta", lua_gettimedelta);
	lua_register(L, "getframetime_ms", lua_getframetime_ms);
	lua_register(L, "getjitter_ms", lua_getjitter_ms);

	// Profiling functions
	lua_register(L, "beginprofile", lua_beginprofile);
	lua_register(L, "endprofile", lua_endprofile);

	// Cadence management functions
	lua_register(L, "sleepframes", lua_sleepframes);
}

void Lua_ProfilerReset(void)
{
	// Clear profile start times
	s_luaProfileStartTimes.clear();
}

#endif // USE_LUA

