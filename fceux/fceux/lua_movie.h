#pragma once

#ifdef USE_LUA

#include <stdint.h>

struct lua_State;

void Lua_RegisterMovie(lua_State* L);
void Lua_MovieReset(void);
bool Lua_MovieIsPlaybackActive(void);
uint32_t Lua_MovieApplyPowerpad(uint32_t powerpadbuf);
uint8_t Lua_MovieProcessJoypad(int player, uint8_t finalButtons);
void Lua_MovieAdvancePlayback(void);

#endif // USE_LUA


