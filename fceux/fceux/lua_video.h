#pragma once

#ifdef USE_LUA

#include "lua.h"
#include "types.h" // For uint8

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all video/drawing Lua bindings
void Lua_RegisterVideo(lua_State* L);

// Lifecycle hooks
void Lua_VideoReset(void);
void Lua_VideoOnFrame(lua_State* L);

// Main GUI callback - called every frame to composite overlay
void FCEU_LuaGui(uint8* XBuf);

// Console functions
void LuaConsolePushLine(const char* msg);
void FCEU_SetLuaConsoleVisible(int visible);
int  FCEU_IsLuaConsoleVisible(void);
void FCEU_ToggleLuaConsole(void);
void FCEU_SetLuaConsoleLineGap(int px);
int  FCEU_GetLuaConsoleLineGap(void);

// Internal overlay management (used by FCEU_LuaGui)
void Lua_VideoSetRenderTarget(uint8* buffer);
uint8* Lua_VideoGetOverlayBack(void);
uint8* Lua_VideoGetOverlayFront(void);
void EnsureOverlay(void);
void CompositeOverlay(uint8* XBuf);
void SwapOverlays(void);
void ClearOverlaysIfAny(void);
void DrawLuaConsole(uint8* buf);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

