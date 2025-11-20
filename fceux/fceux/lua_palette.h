#pragma once

#ifdef USE_LUA

#include "lua.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all palette-related Lua bindings
void Lua_RegisterPalette(lua_State* L);

// Lifecycle hooks (if needed)
void Lua_PaletteReset(void);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

