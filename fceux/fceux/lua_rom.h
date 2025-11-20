#pragma once

#ifdef USE_LUA

struct lua_State;
#include "types.h" // For uint8, uint16, int32

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all ROM and cartridge information Lua bindings
void Lua_RegisterRom(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

