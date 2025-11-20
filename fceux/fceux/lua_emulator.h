#pragma once

#ifdef USE_LUA

struct lua_State;
#include "types.h" // For uint8, uint16, int32

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all emulation state and timing Lua bindings
void Lua_RegisterEmulator(lua_State* L);

// Lifecycle hooks for frame counter and FPS tracking
// Call these from FCEU_LuaGui() to update frame counter and FPS
void Lua_EmulatorUpdateFrame(void);
void Lua_EmulatorReset(void);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

