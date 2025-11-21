#pragma once

#ifdef USE_LUA

struct lua_State;
#include "types.h" // For uint8

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
uint8 Lua_InputGetHardwareJoypad(int player);
uint8 Lua_InputGetLuaJoypadValue(int player);
uint8 Lua_InputGetLuaJoypadMask(int player);
uint8 Lua_InputGetLuaJoypadLatched(int player);
uint8 Lua_InputGetOneFramePress(int player);
uint8 Lua_InputGetOneFrameRelease(int player);

// Setter functions for input state
void Lua_InputSetHardwareJoypad(int player, uint8 value);
void Lua_InputSetLuaJoypadValue(int player, uint8 value);
void Lua_InputSetLuaJoypadMask(int player, uint8 value);
void Lua_InputSetLuaJoypadLatched(int player, uint8 value);
void Lua_InputSetOneFramePress(int player, uint8 value);
void Lua_InputSetOneFrameRelease(int player, uint8 value);

// Rumble state management (used by FCEU_LuaGui)
void Lua_InputUpdateRumble(DWORD currentTime);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

