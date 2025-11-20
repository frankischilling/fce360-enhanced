#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_runtime.h"
#include "lua_helpers.h"
#include "lua_video.h" // For LuaConsolePushLine
#include "types.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// Script interval (default 33ms = ~30 FPS)
static DWORD s_scriptIntervalMs = 33;

// ==================== Runtime Management Functions ====================

// getluamem() -> table
// Returns Lua allocator stats (similar to collectgarbage("count")), plus bytes.
static int lua_getluamem(lua_State* L)
{
	int kb = lua_gc(L, LUA_GCCOUNT, 0);
	int remainder = lua_gc(L, LUA_GCCOUNTB, 0);
	double bytes = (double)kb * 1024.0 + (double)remainder;

	lua_newtable(L);

	lua_pushstring(L, "kilobytes");
	lua_pushnumber(L, (lua_Number)kb + (lua_Number)remainder / 1024.0);
	lua_settable(L, -3);

	lua_pushstring(L, "bytes");
	lua_pushnumber(L, bytes);
	lua_settable(L, -3);

	lua_pushstring(L, "rounded_bytes");
	lua_pushinteger(L, (lua_Integer)(kb * 1024 + remainder));
	lua_settable(L, -3);

	return 1;
}

// collectgarbage_now() -> nil
// Forces a full garbage collection cycle immediately.
static int lua_collectgarbage_now(lua_State* L)
{
	lua_gc(L, LUA_GCCOLLECT, 0);
	return 0;
}

// setscriptinterval(ms) -- clamp 16..1000 ms
// Sets the script execution interval in milliseconds
// The script() callback will be called at most once per interval
static int lua_setscriptinterval(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "setscriptinterval", 1, 1, n);
	}
	int ms = LuaCheckPositive(L, 1, "setscriptinterval", "ms");
	if (ms < 16) ms = 16;
	if (ms > 1000) ms = 1000;
	s_scriptIntervalMs = (DWORD)ms;
	return 0;
}

// getscriptinterval() -> ms
// Gets the current script execution interval in milliseconds
static int lua_getscriptinterval(lua_State* L) {
	lua_pushinteger(L, (int)s_scriptIntervalMs);
	return 1;
}

// print(...) -> nil
// Redirects Lua's print() function to the console overlay
// Concatenates all arguments with tabs and pushes to console
static int lua_print_redirect(lua_State* L) {
	int n = lua_gettop(L);
	if (n == 0) { 
		LuaConsolePushLine(""); 
		return 0; 
	}
	char buffer[512]; 
	buffer[0] = '\0';
	for (int i = 1; i <= n; ++i) {
		size_t slen = 0;
		const char* s = lua_tolstring(L, i, &slen);
		if (!s) continue; // only append string arguments
		if (i > 1 && buffer[0] != '\0') {
			strncat(buffer, "\t", sizeof(buffer) - strlen(buffer) - 1);
		}
		strncat(buffer, s, sizeof(buffer) - strlen(buffer) - 1);
	}
	LuaConsolePushLine(buffer);
	return 0;
}

// log(...) -> nil
// Logging function that redirects to console (same as print)
static int lua_log(lua_State* L) { 
	return lua_print_redirect(L); 
}

// ==================== Accessor Functions ====================

// Get script interval (used by FCEU_LuaGui)
DWORD Lua_RuntimeGetScriptInterval(void) {
	return s_scriptIntervalMs;
}

// Set script interval (used internally)
void Lua_RuntimeSetScriptInterval(DWORD ms) {
	if (ms < 16) ms = 16;
	if (ms > 1000) ms = 1000;
	s_scriptIntervalMs = ms;
}

// ==================== Registrar Function ====================

void Lua_RegisterRuntime(lua_State* L) {
	if (!L) {
		return;
	}

	lua_register(L, "getluamem", lua_getluamem);
	lua_register(L, "collectgarbage_now", lua_collectgarbage_now);
	lua_register(L, "setscriptinterval", lua_setscriptinterval);
	lua_register(L, "getscriptinterval", lua_getscriptinterval);
	lua_register(L, "log", lua_log);
	// Override Lua's built-in print() function
	lua_pushcfunction(L, lua_print_redirect);
	lua_setglobal(L, "print");
}

#endif // USE_LUA

