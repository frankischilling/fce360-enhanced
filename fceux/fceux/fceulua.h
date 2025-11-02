/* FCE Ultra Lua Integration for Xbox 360
 * Basic Lua support for FPS display and other overlays
 */

 #ifndef FCEULUA_H
 #define FCEULUA_H
 
 #ifdef USE_LUA
 
 #include "types.h"
 #include "x6502.h"
 #include "input.h"
 
 // Lua state - forward declare
 struct lua_State;
 extern struct lua_State* luaState;
 
 // Callback types
 enum LUACALL {
	 LUACALL_BEFOREEMULATION = 0,
	 LUACALL_AFTEREMULATION = 1,
	 LUACALL_BEFOREEXIT = 2
 };
 
 enum LUAMEMHOOK {
	 LUAMEMHOOK_EXEC = 0,
	 LUAMEMHOOK_READ = 1,
	 LUAMEMHOOK_WRITE = 2
 };
 
 // Lua API functions
 void FCEU_LuaFrameBoundary(void);
 void FCEU_LuaGui(uint8 *XBuf);
 void FCEU_LuaStop(void);
 void CallRegisteredLuaFunctions(LUACALL callID);
 void CallRegisteredLuaMemHook(unsigned int address, int size, uint8 value, LUAMEMHOOK hookType);
 uint32 FCEU_LuaReadJoypad(int n, uint32 ret);
 
 // Load and run Lua script from file
 int FCEU_LoadLuaScript(const char* filename);
 
 // Lua drawing functions exposed to scripts
 int lua_drawtext(lua_State *L);
 int lua_drawpixel(lua_State *L);
 int lua_drawline(lua_State *L);
 int lua_getfps(lua_State *L);
 
 #endif // USE_LUA
 
 #endif // FCEULUA_H
 