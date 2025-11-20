#pragma once

#ifdef USE_LUA

struct lua_State;
#include "types.h" // For uint8

// Console functions (declared in fceulua.h, defined in lua_video.cpp)
// Forward declaration for modules that need LuaConsolePushLine
void LuaConsolePushLine(const char* msg);

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all video/drawing Lua bindings
void Lua_RegisterVideo(lua_State* L);

// Lifecycle hooks
void Lua_VideoReset(void);
void Lua_VideoOnFrame(lua_State* L);

// Internal overlay management (used by FCEU_LuaGui)
void Lua_VideoSetRenderTarget(uint8* buffer);
uint8* Lua_VideoGetOverlayBack(void);
uint8* Lua_VideoGetOverlayFront(void);
void EnsureOverlay(void);
void CompositeOverlay(uint8* XBuf);
void SwapOverlays(void);
void ClearOverlaysIfAny(void);
void DrawLuaConsole(uint8* buf);

// Accessor for overlay dirty flag (used by FCEU_LuaGui)
bool Lua_VideoGetOverlayDirty(void);
void Lua_VideoSetOverlayDirty(bool dirty);

// Check if overlay has changes (used by FCEU_LuaGui)
bool overlay_has_changes(const uint8* a, const uint8* b);

// Accessors for render target state (used by FCEU_LuaGui)
void Lua_VideoResetRenderTarget(void);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

