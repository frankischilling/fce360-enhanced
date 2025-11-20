#pragma once

#ifdef USE_LUA

#include "lua.h"
#include "types.h" // For uint8, DWORD

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all Lua runtime management bindings
void Lua_RegisterRuntime(lua_State* L);

// Accessor for script interval (used by FCEU_LuaGui)
DWORD Lua_RuntimeGetScriptInterval(void);
void Lua_RuntimeSetScriptInterval(DWORD ms);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

