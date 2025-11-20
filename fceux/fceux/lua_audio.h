#pragma once

#ifdef USE_LUA

#include "lua.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all audio-related Lua bindings
void Lua_RegisterAudio(lua_State* L);

// Lifecycle hooks
void Lua_AudioReset(void);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

