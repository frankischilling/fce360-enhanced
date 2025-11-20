#pragma once

#ifdef USE_LUA

#include "lua.h"
#include "types.h" // For uint8, uint16, int32

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all profiling, timing, and cadence management Lua bindings
void Lua_RegisterProfiler(lua_State* L);

// Lifecycle hook
void Lua_ProfilerReset(void);

// Helper function to check if script is currently sleeping
// Returns true if script should skip execution, false otherwise
// Also handles unpausing emulation when sleep completes
bool Lua_IsSleeping(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

