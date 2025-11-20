#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_gamegenie.h"
#include "lua_helpers.h"
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <string>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// ==================== Game Genie Functions ====================

// getgamegeniecode(address, value, compare) -> string
// Generates Game Genie code from address, value, and optional compare
// Returns 6-character code (no compare) or 8-character code (with compare)
static int lua_getgamegeniecode(lua_State* L)
{
	// Get parameters
	int n = lua_gettop(L);
	if (n < 2 || n > 3) {
		return LuaArgCountError(L, "getgamegeniecode", 2, 3, n);
	}
	
	// Validate and get address (must be 0x8000-0xFFFF)
	int address = LuaCheckRange(L, 1, 0x8000, 0xFFFF, "getgamegeniecode", "address");
	
	// Validate and get value (0-255)
	int value = LuaCheckRange(L, 2, 0, 255, "getgamegeniecode", "value");
	
	// Optional compare value
	bool hasCompare = (n >= 3 && !lua_isnil(L, 3));
	int compare = 0;
	if (hasCompare) {
		compare = LuaCheckRange(L, 3, 0, 255, "getgamegeniecode", "compare");
	}
	
	// Game Genie character mapping
	const char* gg_chars = "APZLGITYEOXUKSVN";
	
	// Mask address to 15 bits (NES Game Genie uses 15-bit addresses)
	address &= 0x7FFF;
	
	// Extract address parts
	int addr_high = (address >> 8) & 0x7F;  // 7 bits
	int addr_low = address & 0xFF;          // 8 bits
	
	// Extract value nibbles
	int val_high = (value >> 4) & 0x0F;
	int val_low = value & 0x0F;
	
	// Build code array (6 characters for no compare, 8 for compare)
	int code[8];
	code[0] = val_low;
	code[1] = val_high;
	code[2] = addr_low & 0x0F;
	code[3] = (addr_low >> 4) & 0x0F;
	code[4] = addr_high & 0x0F;
	code[5] = (addr_high >> 4) & 0x07;
	
	// Add compare value if provided
	if (hasCompare) {
		int comp_high = (compare >> 4) & 0x0F;
		int comp_low = compare & 0x0F;
		code[6] = comp_low;
		code[7] = comp_high;
	}
	
	// Convert to Game Genie string
	std::string gg_code;
	int codeLen = hasCompare ? 8 : 6;
	for (int i = 0; i < codeLen; i++) {
		if (code[i] < 0 || code[i] > 15) {
			return luaL_error(L, "Internal error: invalid code value");
		}
		gg_code += gg_chars[code[i]];
	}
	
	// Return the Game Genie code string
	lua_pushstring(L, gg_code.c_str());
	return 1;
}

// decodegamegenie(code) -> table {address, value, compare}
// Decodes a Game Genie code string back into address, value, and optional compare
// Returns a table with address, value, and compare (if present)
static int lua_decodegamegenie(lua_State* L)
{
	// Get code string parameter
	int n = lua_gettop(L);
	const char* codeStr = LuaCheckString(L, 1, "decodegamegenie");
	if (!codeStr) {
		return LuaArgCountError(L, "decodegamegenie", 1, 1, n);
	}
	
	std::string code(codeStr);
	
	// Validate code length (must be 6 or 8 characters)
	if (code.length() != 6 && code.length() != 8) {
		return luaL_error(L, "decodegamegenie() code must be 6 or 8 characters");
	}
	
	// Game Genie character mapping
	const char* gg_chars = "APZLGITYEOXUKSVN";
	
	// Build reverse lookup map for character to index
	int charMap[256];
	for (int i = 0; i < 256; i++) {
		charMap[i] = -1;
	}
	for (int i = 0; i < 16; i++) {
		charMap[(unsigned char)gg_chars[i]] = i;
	}
	
	// Decode characters to indices
	int codeIndices[8];
	for (size_t i = 0; i < code.length(); i++) {
		unsigned char c = (unsigned char)code[i];
		int idx = charMap[c];
		if (idx < 0 || idx > 15) {
			return luaL_error(L, "decodegamegenie() invalid character in code: '%c'", c);
		}
		codeIndices[i] = idx;
	}
	
	// Decode value
	// code[0] = val_low, code[1] = val_high
	int val_low = codeIndices[0];
	int val_high = codeIndices[1];
	int value = (val_high << 4) | val_low;
	
	// Decode address
	// code[2] = addr_low & 0x0F, code[3] = (addr_low >> 4) & 0x0F
	// code[4] = addr_high & 0x0F, code[5] = (addr_high >> 4) & 0x07
	int addr_low_low = codeIndices[2];
	int addr_low_high = codeIndices[3];
	int addr_low = (addr_low_high << 4) | addr_low_low;
	
	int addr_high_low = codeIndices[4];
	int addr_high_high = codeIndices[5];
	int addr_high = (addr_high_high << 4) | addr_high_low;
	
	// Reconstruct 15-bit address, then add 0x8000 base
	int address = 0x8000 | (addr_high << 8) | addr_low;
	
	// Decode compare value if present (8-character code)
	bool hasCompare = (code.length() == 8);
	int compare = -1;
	if (hasCompare) {
		// code[6] = comp_low, code[7] = comp_high
		int comp_low = codeIndices[6];
		int comp_high = codeIndices[7];
		compare = (comp_high << 4) | comp_low;
	}
	
	// Create and return Lua table
	lua_newtable(L);
	
	// Push address
	lua_pushstring(L, "address");
	lua_pushinteger(L, address);
	lua_settable(L, -3);
	
	// Push value
	lua_pushstring(L, "value");
	lua_pushinteger(L, value);
	lua_settable(L, -3);
	
	// Push compare (only if present)
	if (hasCompare) {
		lua_pushstring(L, "compare");
		lua_pushinteger(L, compare);
		lua_settable(L, -3);
	}
	
	return 1;
}

// ==================== Registrar Function ====================

void Lua_RegisterGameGenie(lua_State* L) {
	if (!L) {
		return;
	}

	lua_register(L, "getgamegeniecode", lua_getgamegeniecode);
	lua_register(L, "decodegamegenie", lua_decodegamegenie);
}

#endif // USE_LUA

