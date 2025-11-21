#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_palette.h"
#include "lua_helpers.h"
#include "fceulua.h"
#include "fceu.h"
#include "types.h"
#include "driver.h"
#include "ppu.h"

#include <map>
#include <vector>
#include <string>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// Extern PPU data for palette access
extern uint8 PALRAM[0x20];

// ============================================================================
// Lua Palette Functions
// ============================================================================

// getcolorrgb(paletteIndex) -> table
static int lua_getcolorrgb(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getcolorrgb", 1, 1, n);
	}
	
	int paletteIndex = LuaCheckPaletteIndex(L, 1, 63, "getcolorrgb");
	
	// Get RGB values from palette
	// Palette colors are stored at indices 128-191 (0x80-0xBF), not 0-63
	uint8 r, g, b;
	FCEUD_GetPalette((uint8)(128 + paletteIndex), &r, &g, &b);
	
	// Create table with 3 elements
	lua_createtable(L, 3, 0);
	
	// Push RGB values as array (1-indexed for Lua)
	lua_pushinteger(L, r);
	lua_rawseti(L, -2, 1);
	
	lua_pushinteger(L, g);
	lua_rawseti(L, -2, 2);
	
	lua_pushinteger(L, b);
	lua_rawseti(L, -2, 3);
	
	return 1;  // Return the table
}

// getpalettecolor(index) -> integer
static int lua_getpalettecolor(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getpalettecolor", 1, 1, n);
	}
	
	int index = LuaCheckPaletteIndex(L, 1, 31, "getpalettecolor");
	
	// Read palette color from PALRAM
	uint8 colorValue = PALRAM[index] & 0x3F;  // Mask to 6 bits (0-63)
	
	lua_pushinteger(L, colorValue);
	return 1;  // Return the color value
}

// getpalette() -> table
static int lua_getpalette(lua_State* L)
{
	int n = lua_gettop(L);
	if (n > 0) {
		return luaL_error(L, "getpalette() takes no arguments");
	}
	
	// Create a table with 32 entries (0-31)
	lua_createtable(L, 32, 0);
	
	// Read all palette entries from PALRAM and populate the table
	for (int i = 0; i < 32; ++i) {
		uint8 colorValue = PALRAM[i] & 0x3F;  // Mask to 6 bits (0-63)
		
		// Use 0-indexed keys (Lua tables can have 0-indexed keys)
		lua_pushinteger(L, i);
		lua_pushinteger(L, colorValue);
		lua_rawset(L, -3);  // Set table[i] = colorValue
	}
	
	return 1;  // Return the table
}

// setpalettecolor(index, color) -> void
static int lua_setpalettecolor(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "setpalettecolor", 2, 2, n);
	}
	
	int index = LuaCheckPaletteIndex(L, 1, 31, "setpalettecolor");
	int validatedColor = LuaCheckNESColor(L, 2, "setpalettecolor", 1); // strict validation
	uint8 colorValue = (uint8)(validatedColor & 0x3F);
	
	// Write to PALRAM
	PALRAM[index] = colorValue;
	
	// Handle universal color mirroring (NES behavior)
	// Universal background color (0x00) is mirrored to 0x04, 0x08, 0x0C
	if (index == 0x00) {
		PALRAM[0x04] = colorValue;
		PALRAM[0x08] = colorValue;
		PALRAM[0x0C] = colorValue;
	}
	// Universal sprite color (0x10) is mirrored to 0x14, 0x18, 0x1C
	else if (index == 0x10) {
		PALRAM[0x14] = colorValue;
		PALRAM[0x18] = colorValue;
		PALRAM[0x1C] = colorValue;
	}
	
	return 0;  // Return nothing
}

// setpalette(paletteTable) -> void
static int lua_setpalette(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "setpalette", 1, 1, n);
	}
	
	// Check if paletteTable is a table
	LuaCheckTable(L, 1, "setpalette");
	
	// Track which indices we've set to handle universal color mirroring
	bool setUniversalBg = false;
	bool setUniversalSpr = false;
	uint8 universalBgColor = 0;
	uint8 universalSprColor = 0;
	
	// Iterate through the table
	// Support both sequential array (1-indexed) and key-value pairs (0-31 indexed)
	lua_pushnil(L);  // First key for iteration
	while (lua_next(L, 1) != 0) {
		// Key is at index -2, value is at index -1
		if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
			int key = (int)luaL_checkinteger(L, -2); // Key from lua_next, can't use helper
			int color = (int)luaL_checkinteger(L, -1); // Value from lua_next, can't use helper
			
			// Convert 1-indexed array to 0-indexed palette index
			int index = key;
			if (key >= 1 && key <= 32) {
				// Sequential array: [1] = PALRAM[0], [2] = PALRAM[1], etc.
				index = key - 1;
			}
			
			// Validate palette index range (0-31)
			if (index < 0 || index > 31) {
				lua_pop(L, 2);  // Remove value and key
				return luaL_error(L, "setpalette: palette index must be in range 0-31 (got %d)", index);
			}
			
			// Validate color range (0-63)
			if (color < 0 || color > 63) {
				lua_pop(L, 2);  // Remove value and key
				return luaL_error(L, "setpalette: color value must be in range 0-63 (got %d)", color);
			}
			
			// Mask color to 6 bits (0x3F)
			uint8 colorValue = (uint8)(color & 0x3F);
			
			// Write to PALRAM
			PALRAM[index] = colorValue;
			
			// Track universal colors for mirroring
			if (index == 0x00) {
				setUniversalBg = true;
				universalBgColor = colorValue;
			} else if (index == 0x10) {
				setUniversalSpr = true;
				universalSprColor = colorValue;
			}
		}
		
		lua_pop(L, 1);  // Remove value, keep key for next iteration
	}
	
	// Handle universal color mirroring (NES behavior)
	// Universal background color (0x00) is mirrored to 0x04, 0x08, 0x0C
	if (setUniversalBg) {
		PALRAM[0x04] = universalBgColor;
		PALRAM[0x08] = universalBgColor;
		PALRAM[0x0C] = universalBgColor;
	}
	// Universal sprite color (0x10) is mirrored to 0x14, 0x18, 0x1C
	if (setUniversalSpr) {
		PALRAM[0x14] = universalSprColor;
		PALRAM[0x18] = universalSprColor;
		PALRAM[0x1C] = universalSprColor;
	}
	
	return 0;  // Return nothing
}

// loadpalette(path) -> boolean
static int lua_loadpalette(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "loadpalette", 1, 1, n);
	}
	
	const char* path = LuaCheckPath(L, 1, "loadpalette");
	if (!path || strlen(path) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Build full path - try game: directory first
	char fullpath[512];
	
	// If path already contains a drive/path, use it as-is
	if (strchr(path, ':') || path[0] == '\\' || path[0] == '/') {
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		// Relative to game: directory
		const char* baseDir = "game:\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, path);
	}
	
	// Normalize path separators (convert / to \)
	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}
	
	// Try to open file
	FILE* file = fopen(fullpath, "rb");
	if (!file) {
		// Try alternative paths if initial path fails
		const char* altPaths[] = {
			"game:\\lua\\%s",
			"game:\\Lua\\%s",
			"hdd1:\\fce360-enhanced\\lua\\%s",
			"hdd1:\\fce360-enhanced\\Lua\\%s",
			"game:\\%s"
		};
		
		bool found = false;
		for (int i = 0; i < (int)(sizeof(altPaths) / sizeof(altPaths[0])); i++) {
			char altPath[512];
			snprintf(altPath, sizeof(altPath), altPaths[i], path);
			
			// Normalize path separators
			for (int j = 0; altPath[j] != '\0'; j++) {
				if (altPath[j] == '/') {
					altPath[j] = '\\';
				}
			}
			
			file = fopen(altPath, "rb");
			if (file) {
				strncpy(fullpath, altPath, sizeof(fullpath) - 1);
				fullpath[sizeof(fullpath) - 1] = '\0';
				found = true;
				break;
			}
		}
		
		if (!found) {
			// File not found
			lua_pushboolean(L, 0);
			return 1;
		}
	}
	
	// .pal file format: 192 bytes (64 colors * 3 bytes RGB)
	// Format: RGBRGBRGB... for 64 colors
	uint8 paletteData[192];
	
	// Read palette data
	size_t bytesRead = fread(paletteData, 1, 192, file);
	fclose(file);
	
	if (bytesRead != 192) {
		// File is too small or read error
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Apply the palette using FCEUI_SetPaletteArray
	// This function expects RGBRGBRGB format (192 bytes for 64 colors)
	FCEUI_SetPaletteArray(paletteData);
	
	lua_pushboolean(L, 1);
	return 1;  // Return success
}

// getnescolor(index) -> integer
static int lua_getnescolor(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getnescolor", 1, 1, n);
	}
	
	int index = LuaCheckPaletteIndex(L, 1, 63, "getnescolor");
	
	// Get RGB values from palette
	// Palette colors are stored at indices 128-191 (0x80-0xBF), not 0-63
	uint8 r, g, b;
	FCEUD_GetPalette((uint8)(128 + index), &r, &g, &b);
	
	// Pack RGB into single integer: 0xRRGGBB format
	uint32 rgbValue = ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b;
	
	lua_pushinteger(L, rgbValue);
	return 1;  // Return the packed RGB value
}

// blendcolors(color1, color2, ratio) -> integer
static int lua_blendcolors(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "blendcolors", 3, 3, n);
	}
	
	int color1 = LuaCheckPaletteIndex(L, 1, 63, "blendcolors");
	int color2 = LuaCheckPaletteIndex(L, 2, 63, "blendcolors");
	double ratio = LuaCheckNumberRange(L, 3, 0.0, 1.0, "blendcolors", "ratio");
	
	// Get RGB values for both colors
	uint8 r1, g1, b1, r2, g2, b2;
	FCEUD_GetPalette((uint8)(128 + color1), &r1, &g1, &b1);
	FCEUD_GetPalette((uint8)(128 + color2), &r2, &g2, &b2);
	
	// Blend RGB components: result = color1 * (1 - ratio) + color2 * ratio
	int blendedR = (int)(r1 * (1.0 - ratio) + r2 * ratio + 0.5);  // Round to nearest
	int blendedG = (int)(g1 * (1.0 - ratio) + g2 * ratio + 0.5);
	int blendedB = (int)(b1 * (1.0 - ratio) + b2 * ratio + 0.5);
	
	// Clamp to valid range
	if (blendedR > 255) blendedR = 255;
	if (blendedG > 255) blendedG = 255;
	if (blendedB > 255) blendedB = 255;
	if (blendedR < 0) blendedR = 0;
	if (blendedG < 0) blendedG = 0;
	if (blendedB < 0) blendedB = 0;
	
	// Find the closest matching palette color index
	int bestIndex = 0;
	double minDistance = 999999.0;
	
	for (int i = 0; i < 64; i++) {
		uint8 pr, pg, pb;
		FCEUD_GetPalette((uint8)(128 + i), &pr, &pg, &pb);
		
		// Calculate Euclidean distance in RGB space
		double dr = (double)blendedR - (double)pr;
		double dg = (double)blendedG - (double)pg;
		double db = (double)blendedB - (double)pb;
		double distance = dr * dr + dg * dg + db * db;
		
		if (distance < minDistance) {
			minDistance = distance;
			bestIndex = i;
		}
	}
	
	lua_pushinteger(L, bestIndex);
	return 1;  // Return the closest matching color index
}

// ============================================================================
// Module Registrar and Lifecycle Hooks
// ============================================================================

static const luaL_Reg kPaletteFuncs[] = {
	{"getcolorrgb", lua_getcolorrgb},
	{"getpalettecolor", lua_getpalettecolor},
	{"getpalette", lua_getpalette},
	{"setpalettecolor", lua_setpalettecolor},
	{"setpalette", lua_setpalette},
	{"loadpalette", lua_loadpalette},
	{"getnescolor", lua_getnescolor},
	{"blendcolors", lua_blendcolors},
	{NULL, NULL}
};

void Lua_RegisterPalette(lua_State* L)
{
	if (!L) {
		return;
	}

	// Manually register each function (luaL_register with NULL has issues)
	for (const luaL_Reg* reg = kPaletteFuncs; reg->name != NULL; reg++) {
		lua_register(L, reg->name, reg->func);
	}
}

void Lua_PaletteReset(void)
{
	// Palette state is managed by the NES core, so no reset needed
	// This function exists for consistency with other modules
}

#endif // USE_LUA

