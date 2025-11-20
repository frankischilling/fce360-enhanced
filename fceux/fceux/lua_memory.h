#pragma once

#ifdef USE_LUA

struct lua_State;

void Lua_RegisterMemory(lua_State* L);
void Lua_MemoryOnFrame(lua_State* L);
void Lua_MemoryResetWatchpoints(void);

#endif // USE_LUA


