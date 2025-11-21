#pragma once

#ifdef USE_LUA

struct lua_State;

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all Game Genie encoding/decoding Lua bindings
void Lua_RegisterGameGenie(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

