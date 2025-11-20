#pragma once

#ifdef USE_LUA

#include "lua.h"
#include "types.h" // For uint8, uint32_t

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all input-related Lua bindings
void Lua_RegisterInput(lua_State* L);

// Lifecycle hooks
void Lua_InputReset(void);
void Lua_InputOnFrame(lua_State* L);
void Lua_InputCleanup(lua_State* L);  // Cleanup Lua refs before lua_close

// Input processing (called from FCEU_LuaJoypadApply)
void Lua_InputProcessJoypad(void);

// Getter functions for input state (used by FCEU_LuaJoypadApply)
uint8_t Lua_InputGetHardwareJoypad(int player);
uint8_t Lua_InputGetLuaJoypadValue(int player);
uint8_t Lua_InputGetLuaJoypadMask(int player);
uint8_t Lua_InputGetLuaJoypadLatched(int player);
uint8_t Lua_InputGetOneFramePress(int player);
uint8_t Lua_InputGetOneFrameRelease(int player);

// Setter functions for input state
void Lua_InputSetHardwareJoypad(int player, uint8_t value);
void Lua_InputSetLuaJoypadValue(int player, uint8_t value);
void Lua_InputSetLuaJoypadMask(int player, uint8_t value);
void Lua_InputSetLuaJoypadLatched(int player, uint8_t value);
void Lua_InputSetOneFramePress(int player, uint8_t value);
void Lua_InputSetOneFrameRelease(int player, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

