#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_helpers.h"
#include <string.h>
#include <algorithm>
#include <cmath>

// Screen dimensions constants (matching lua_video.cpp)
enum { OVL_W = 256, OVL_H = 240 };

// ============================================================================
// Argument Extraction and Validation
// ============================================================================

int LuaCheckInt(lua_State* L, int arg, const char* funcName) {
	if (!lua_isnumber(L, arg)) {
		return LuaArgTypeError(L, funcName, arg, "integer");
	}
	return (int)luaL_checkinteger(L, arg);
}

int LuaCheckIntOpt(lua_State* L, int arg, int defaultValue, const char* funcName) {
	if (lua_isnoneornil(L, arg)) {
		return defaultValue;
	}
	return LuaCheckInt(L, arg, funcName);
}

double LuaCheckNumber(lua_State* L, int arg, const char* funcName) {
	if (!lua_isnumber(L, arg)) {
		return LuaArgTypeError(L, funcName, arg, "number");
	}
	return luaL_checknumber(L, arg);
}

double LuaCheckNumberOpt(lua_State* L, int arg, double defaultValue, const char* funcName) {
	if (lua_isnoneornil(L, arg)) {
		return defaultValue;
	}
	return LuaCheckNumber(L, arg, funcName);
}

const char* LuaCheckString(lua_State* L, int arg, const char* funcName) {
	if (!lua_isstring(L, arg)) {
		LuaArgTypeError(L, funcName, arg, "string");
		return NULL; // Won't reach here, but satisfies compiler
	}
	return luaL_checkstring(L, arg);
}

const char* LuaCheckStringOpt(lua_State* L, int arg, const char* defaultValue, const char* funcName) {
	if (lua_isnoneornil(L, arg)) {
		return defaultValue;
	}
	return LuaCheckString(L, arg, funcName);
}

int LuaCheckBool(lua_State* L, int arg, const char* funcName) {
	if (!lua_isboolean(L, arg)) {
		return LuaArgTypeError(L, funcName, arg, "boolean");
	}
	return lua_toboolean(L, arg) ? 1 : 0;
}

int LuaCheckBoolOpt(lua_State* L, int arg, int defaultValue, const char* funcName) {
	if (lua_isnoneornil(L, arg)) {
		return defaultValue;
	}
	return LuaCheckBool(L, arg, funcName);
}

void LuaCheckTable(lua_State* L, int arg, const char* funcName) {
	if (!lua_istable(L, arg)) {
		LuaArgTypeError(L, funcName, arg, "table");
	}
}

// ============================================================================
// Range Validation
// ============================================================================

int LuaCheckRange(lua_State* L, int arg, int min, int max, const char* funcName, const char* paramName) {
	int value = LuaCheckInt(L, arg, funcName);
	if (value < min || value > max) {
		return LuaBoundsError(L, funcName, paramName, value, min, max);
	}
	return value;
}

int LuaCheckPositive(lua_State* L, int arg, const char* funcName, const char* paramName) {
	int value = LuaCheckInt(L, arg, funcName);
	if (value <= 0) {
		return luaL_error(L, "%s: %s must be positive (got %d)", funcName, paramName, value);
	}
	return value;
}

int LuaCheckNonNegative(lua_State* L, int arg, const char* funcName, const char* paramName) {
	int value = LuaCheckInt(L, arg, funcName);
	if (value < 0) {
		return luaL_error(L, "%s: %s must be non-negative (got %d)", funcName, paramName, value);
	}
	return value;
}

int LuaCheckScreenX(lua_State* L, int arg, const char* funcName) {
	return LuaCheckRange(L, arg, 0, OVL_W - 1, funcName, "x coordinate");
}

int LuaCheckScreenY(lua_State* L, int arg, const char* funcName) {
	return LuaCheckRange(L, arg, 0, OVL_H - 1, funcName, "y coordinate");
}

// ============================================================================
// NES-Specific Validations
// ============================================================================

int LuaCheckNESColor(lua_State* L, int arg, const char* funcName, int strict) {
	int color = LuaCheckInt(L, arg, funcName);
	if (color < 0 || color > 0x3F) {
		if (strict) {
			return luaL_error(L, "%s: color must be in range 0x00-0x3F (got 0x%02X)", funcName, color);
		}
		// Non-strict: clamp to valid range
		return LuaClampNESColor(color);
	}
	return color;
}

uint8 LuaClampNESColor(int color) {
	if (color < 0) return 0;
	if (color > 0x3F) return 0x3F;
	return (uint8)(color & 0xFF);
}

int LuaCheckPaletteIndex(lua_State* L, int arg, int maxIndex, const char* funcName) {
	int index = LuaCheckInt(L, arg, funcName);
	if (index < 0 || index > maxIndex) {
		return luaL_error(L, "%s: palette index must be in range 0-%d (got %d)", funcName, maxIndex, index);
	}
	return index;
}

// ============================================================================
// Error Reporting
// ============================================================================

int LuaArgCountError(lua_State* L, const char* funcName, int expectedMin, int expectedMax, int actual) {
	if (expectedMin == expectedMax) {
		return luaL_error(L, "%s requires %d argument%s (got %d)", funcName, expectedMin, 
			(expectedMin == 1) ? "" : "s", actual);
	} else {
		return luaL_error(L, "%s requires %d-%d arguments (got %d)", funcName, expectedMin, expectedMax, actual);
	}
}

int LuaArgTypeError(lua_State* L, const char* funcName, int arg, const char* expectedType) {
	const char* actualType = lua_typename(L, lua_type(L, arg));
	return luaL_error(L, "%s: argument %d must be %s (got %s)", funcName, arg, expectedType, actualType);
}

int LuaBoundsError(lua_State* L, const char* funcName, const char* paramName, int value, int min, int max) {
	return luaL_error(L, "%s: %s must be in range %d-%d (got %d)", funcName, paramName, min, max, value);
}

double LuaCheckNumberRange(lua_State* L, int arg, double min, double max, const char* funcName, const char* paramName) {
	double value = LuaCheckNumber(L, arg, funcName);
	if (value < min || value > max) {
		LuaBoundsErrorDouble(L, funcName, paramName, value, min, max);
		return 0.0; // Won't reach here, but satisfies compiler
	}
	return value;
}

int LuaBoundsErrorDouble(lua_State* L, const char* funcName, const char* paramName, double value, double min, double max) {
	return luaL_error(L, "%s: %s must be in range %.2f-%.2f (got %.2f)", funcName, paramName, min, max, value);
}

int LuaArgError(lua_State* L, const char* funcName, int arg, const char* message) {
	return luaL_error(L, "%s: argument %d: %s", funcName, arg, message);
}

// ============================================================================
// Table Conversions
// ============================================================================

int LuaTableToIntVector(lua_State* L, int tableIndex, std::vector<int>& out, const char* funcName) {
	LuaCheckTable(L, tableIndex, funcName);
	
	out.clear();
	
	// Lua tables are 1-indexed, iterate from 1 until we hit nil
	int count = 0;
	for (int i = 1; ; ++i) {
		lua_rawgeti(L, tableIndex, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			return luaL_error(L, "%s: table element %d must be a number", funcName, i);
		}
		int value = (int)luaL_checkinteger(L, -1);
		out.push_back(value);
		lua_pop(L, 1);
		count++;
	}
	
	return count;
}

int LuaTableToByteVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName) {
	LuaCheckTable(L, tableIndex, funcName);
	
	out.clear();
	
	// Lua tables are 1-indexed, iterate from 1 until we hit nil
	int count = 0;
	for (int i = 1; ; ++i) {
		lua_rawgeti(L, tableIndex, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			return luaL_error(L, "%s: table element %d must be a number", funcName, i);
		}
		int value = (int)luaL_checkinteger(L, -1);
		// Clamp to byte range
		if (value < 0) value = 0;
		if (value > 255) value = 255;
		out.push_back((uint8)value);
		lua_pop(L, 1);
		count++;
	}
	
	return count;
}

int LuaTableToNESColorVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName) {
	LuaCheckTable(L, tableIndex, funcName);
	
	out.clear();
	
	// Lua tables are 1-indexed, iterate from 1 until we hit nil
	int count = 0;
	for (int i = 1; ; ++i) {
		lua_rawgeti(L, tableIndex, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			return luaL_error(L, "%s: table element %d must be a number", funcName, i);
		}
		int colorValue = (int)luaL_checkinteger(L, -1);
		// Clamp to NES color range (0x00-0x3F)
		uint8 clampedColor = LuaClampNESColor(colorValue);
		out.push_back(clampedColor);
		lua_pop(L, 1);
		count++;
	}
	
	return count;
}

void LuaPushIntVector(lua_State* L, const std::vector<int>& vec) {
	lua_createtable(L, (int)vec.size(), 0);
	for (size_t i = 0; i < vec.size(); ++i) {
		lua_pushinteger(L, vec[i]);
		lua_rawseti(L, -2, (int)(i + 1)); // Lua is 1-indexed
	}
}

void LuaPushByteVector(lua_State* L, const std::vector<uint8>& vec) {
	lua_createtable(L, (int)vec.size(), 0);
	for (size_t i = 0; i < vec.size(); ++i) {
		lua_pushinteger(L, vec[i]);
		lua_rawseti(L, -2, (int)(i + 1)); // Lua is 1-indexed
	}
}

int LuaGetTableSize(lua_State* L, int tableIndex) {
	LuaCheckTable(L, tableIndex, "LuaGetTableSize");
	
	int size = 0;
	for (int i = 1; ; ++i) {
		lua_rawgeti(L, tableIndex, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		lua_pop(L, 1);
		size++;
	}
	
	return size;
}

// ============================================================================
// Path Normalization
// ============================================================================

std::string LuaNormalizePath(const char* path) {
	if (!path || !path[0]) {
		return std::string();
	}
	
	std::string normalized;
	normalized.reserve(strlen(path) + 1);
	
	// Convert backslashes to forward slashes and collapse multiple separators
	bool lastWasSep = false;
	for (const char* p = path; *p; ++p) {
		char c = *p;
		if (c == '\\' || c == '/') {
			if (!lastWasSep) {
				normalized += '/';
				lastWasSep = true;
			}
		} else {
			normalized += c;
			lastWasSep = false;
		}
	}
	
	return normalized;
}

const char* LuaCheckPath(lua_State* L, int arg, const char* funcName) {
	const char* path = LuaCheckString(L, arg, funcName);
	if (!path || !path[0]) {
		luaL_error(L, "%s: path cannot be empty", funcName);
		// Never returns, but satisfy compiler
		return NULL;
	}
	return path;
}

// ============================================================================
// Common Conversions
// ============================================================================

uint8 NESColorToOverlay(uint8 nesColor) {
	// Map 0x00-0x3F to overlay-coded 0x80-0xBF (never dim)
	return (nesColor & 0x3F) | 0x80;
}

uint8 OverlayToNESColor(uint8 overlayColor) {
	// Extract NES color from overlay format
	return overlayColor & 0x3F;
}

void LuaClampScreenCoords(int& x, int& y) {
	if (x < 0) x = 0;
	if (x >= OVL_W) x = OVL_W - 1;
	if (y < 0) y = 0;
	if (y >= OVL_H) y = OVL_H - 1;
}

void LuaClampScreenRect(int& x, int& y, int& w, int& h) {
	// Clamp position
	if (x < 0) {
		w += x; // Reduce width
		x = 0;
	}
	if (y < 0) {
		h += y; // Reduce height
		y = 0;
	}
	
	// Clamp size to screen bounds
	if (x + w > OVL_W) {
		w = OVL_W - x;
	}
	if (y + h > OVL_H) {
		h = OVL_H - y;
	}
	
	// Ensure non-negative dimensions
	if (w < 0) w = 0;
	if (h < 0) h = 0;
}

#endif // USE_LUA

