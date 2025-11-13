/* FCE Ultra Lua Integration for Xbox 360
 * Basic Lua support for FPS display and other overlays
 * 
 * Enhanced for fce360-enhanced
 * GitHub: https://github.com/frankischilling/fce360-enhanced
 * 
 * Contributors:
 * @frankischilling
 * Ced2911 (original Xbox 360 port)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
void FCEU_LuaCheckAudioEvents(void);  // Check and trigger audio event callbacks

#ifdef __cplusplus
extern "C" {
#endif
void FCEU_LuaJoypadApply(void);
void FCEU_LuaJoypadClear(int player);
#ifdef __cplusplus
}
#endif

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
 int lua_drawtextscaled(lua_State *L);
 int lua_drawpixel(lua_State *L);
 int lua_drawline(lua_State *L);
 int lua_drawthickline(lua_State *L);
 int lua_drawpolygon(lua_State *L);
 int lua_drawpolyline(lua_State *L);
 int lua_fillpolygon(lua_State *L);
 int lua_drawimage(lua_State *L);
 int lua_drawimageindexed(lua_State *L);
 int lua_drawimageex(lua_State *L);
 int lua_drawtile(lua_State *L);
 int lua_drawchrtile(lua_State *L);
 int lua_setdrawmode(lua_State *L);
 int lua_setclipregion(lua_State *L);
 int lua_clearclipregion(lua_State *L);
 int lua_setdrawcolor(lua_State *L);
 int lua_pushdrawstate(lua_State *L);
 int lua_popdrawstate(lua_State *L);
 int lua_settransform(lua_State *L);
 int lua_resettransform(lua_State *L);
 int lua_beginbatch(lua_State *L);
 int lua_endbatch(lua_State *L);
 int lua_setimagescale(lua_State *L);
 int lua_getimagescale(lua_State *L);
 int lua_createcanvas(lua_State *L);
 int lua_setrendertarget(lua_State *L);
 int lua_blit(lua_State *L);
 int lua_lineargradient(lua_State *L);
 int lua_fillrectgradient(lua_State *L);
 int lua_radialgradient(lua_State *L);
 int lua_textstyle(lua_State *L);
 int lua_measuretextblock(lua_State *L);
 int lua_screenshotregion(lua_State *L);
 // Lua memory functions
 int lua_getfps(lua_State *L);
 int lua_readbyte(lua_State *L);
 int lua_readword(lua_State *L);
 int lua_readbytes(lua_State *L);
 int lua_scanbyte(lua_State *L);
 int lua_scanword(lua_State *L);
 int lua_scanbytes(lua_State *L);
 int lua_findpattern(lua_State *L);
 int lua_scanchanged(lua_State *L);
 int lua_watchbyte(lua_State *L);
 int lua_unwatchbyte(lua_State *L);
 int lua_getmemorysnapshot(lua_State *L);
 int lua_setbit(lua_State *L);
 int lua_clearbit(lua_State *L);
 int lua_togglebit(lua_State *L);
 int lua_testbit(lua_State *L);
 int lua_writebyte(lua_State *L);
 int lua_writeword(lua_State *L);
 int lua_writebytes(lua_State *L);
 
 // Lua console controls
 void FCEU_SetLuaConsoleVisible(int visible);
 int  FCEU_IsLuaConsoleVisible(void);
 void FCEU_ToggleLuaConsole(void);
 void FCEU_LuaLogAppend(const char* msg);
 
 #endif // USE_LUA
 
 #endif // FCEULUA_H
 