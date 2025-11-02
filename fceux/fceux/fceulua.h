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

// Auto-load Lua scripts (if selectedScript is NULL or empty, load all; otherwise load only selectedScript)
void FCEU_AutoLoadLuaScripts(const char* selectedScript);

// Keep these in a shared header to avoid mismatches
// Note: These match Cemulator::Settings::LuaAutoloadMode enum values
// For C code: use macros
#ifndef __cplusplus
#define LUA_AUTO_ALL  0
#define LUA_AUTO_ONE  1
#define LUA_AUTO_NONE 2
#endif
// For C++: use the enum from Cemulator::Settings::LuaAutoloadMode, or cast integers
// These values are: LUA_AUTO_ALL=0, LUA_AUTO_ONE=1, LUA_AUTO_NONE=2

#ifdef __cplusplus
extern "C" {
#endif

// Master switch
void FCEU_LuaSetDisabled(int disabled);
int  FCEU_LuaIsDisabled(void);

// Lifecycle helpers
void FCEU_LuaKillAll(void);

// Mode application & pending selection
void FCEU_ApplyLuaMode(int mode, const char* scriptUtf8OrNull);
void FCEU_SetPendingLua(int mode, const char* scriptUtf8OrNull);

// Optional (status/debug)
const char* FCEU_LuaGetStatusMsg(void);

// Call this immediately AFTER the ROM is loaded and the core is powered.
// Applies any pending Lua mode that was set via FCEU_SetPendingLua().
void FCEU_ApplyPendingLuaForNewGame(void);

#ifdef __cplusplus
}
#endif

// Get list of available Lua scripts (returns count, fills names array, maxNames is array size)
// Returns number of scripts found
int FCEU_GetLuaScriptList(char names[][256], int maxNames);

// Additional Lua functions (stubs for Xbox port)
void FCEU_LuaUpdatePalette(void);
void FCEU_ReloadLuaCode(void);

// Lua save/load state integration (forward declarations)
struct LuaSaveData {
	void* recordList;
	void ExportRecords(void* f);
	void ImportRecords(void* f);
};
void CallRegisteredLuaSaveFunctions(void* state, LuaSaveData& saveData);
void CallRegisteredLuaLoadFunctions(void* state, LuaSaveData& saveData);
 
 // Lua drawing functions exposed to scripts
 int lua_drawtext(lua_State *L);
 int lua_drawpixel(lua_State *L);
 int lua_drawline(lua_State *L);
 int lua_drawthickline(lua_State *L);
 int lua_drawpolygon(lua_State *L);
 int lua_drawpolyline(lua_State *L);
 int lua_fillpolygon(lua_State *L);
 int lua_getfps(lua_State *L);
 
 #endif // USE_LUA
 
 #endif // FCEULUA_H
 