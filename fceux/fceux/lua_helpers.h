#pragma once

#ifdef USE_LUA

// Lua headers must be included with C linkage
#ifdef __cplusplus
extern "C" {
#endif
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#ifdef __cplusplus
}
#endif

#include "types.h"
#include <vector>
#include <string>

// C++ functions (returning std::string, std::vector, etc.) must be outside extern "C"
#ifdef __cplusplus
// Forward declarations for C++ functions
std::string LuaNormalizePath(const char* path);
int LuaTableToIntVector(lua_State* L, int tableIndex, std::vector<int>& out, const char* funcName);
int LuaTableToByteVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName);
int LuaTableToNESColorVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName);
void LuaPushIntVector(lua_State* L, const std::vector<int>& vec);
void LuaPushByteVector(lua_State* L, const std::vector<uint8>& vec);
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Argument Extraction and Validation
// ============================================================================

// Check and extract integer argument
// Returns the integer value, or throws Lua error if invalid
int LuaCheckInt(lua_State* L, int arg, const char* funcName);

// Check and extract integer argument with optional default
int LuaCheckIntOpt(lua_State* L, int arg, int defaultValue, const char* funcName);

// Check and extract number (double) argument
double LuaCheckNumber(lua_State* L, int arg, const char* funcName);

// Check and extract number with optional default
double LuaCheckNumberOpt(lua_State* L, int arg, double defaultValue, const char* funcName);

// Check and extract string argument
const char* LuaCheckString(lua_State* L, int arg, const char* funcName);

// Check and extract string with optional default (returns NULL if not provided)
const char* LuaCheckStringOpt(lua_State* L, int arg, const char* defaultValue, const char* funcName);

// Check and extract boolean argument
int LuaCheckBool(lua_State* L, int arg, const char* funcName);

// Check and extract boolean with optional default
int LuaCheckBoolOpt(lua_State* L, int arg, int defaultValue, const char* funcName);

// Check that argument is a table
void LuaCheckTable(lua_State* L, int arg, const char* funcName);

// ============================================================================
// Range Validation
// ============================================================================

// Check integer is within range [min, max] (inclusive)
// Returns the value if valid, throws error otherwise
int LuaCheckRange(lua_State* L, int arg, int min, int max, const char* funcName, const char* paramName);

// Check integer is positive (> 0)
int LuaCheckPositive(lua_State* L, int arg, const char* funcName, const char* paramName);

// Check integer is non-negative (>= 0)
int LuaCheckNonNegative(lua_State* L, int arg, const char* funcName, const char* paramName);

// Check integer is within screen bounds (0 to width-1, 0 to height-1)
int LuaCheckScreenX(lua_State* L, int arg, const char* funcName);
int LuaCheckScreenY(lua_State* L, int arg, const char* funcName);

// ============================================================================
// NES-Specific Validations
// ============================================================================

// Check NES color index (0x00-0x3F, 64 colors)
// Returns clamped value (0-63) if out of range, or throws error if strict=true
int LuaCheckNESColor(lua_State* L, int arg, const char* funcName, int strict);

// Clamp NES color to valid range (0x00-0x3F)
// Returns the clamped value (does not throw)
uint8 LuaClampNESColor(int color);

// Check palette index (0-31 for PPU palette, 0-63 for NES color palette)
int LuaCheckPaletteIndex(lua_State* L, int arg, int maxIndex, const char* funcName);

// ============================================================================
// Error Reporting
// ============================================================================

// Report argument count error
int LuaArgCountError(lua_State* L, const char* funcName, int expectedMin, int expectedMax, int actual);

// Report argument type error
int LuaArgTypeError(lua_State* L, const char* funcName, int arg, const char* expectedType);

// Report argument range/bounds error
int LuaBoundsError(lua_State* L, const char* funcName, const char* paramName, int value, int min, int max);

// Check number (double) is within range [min, max] (inclusive)
// Returns the value if valid, throws error otherwise
double LuaCheckNumberRange(lua_State* L, int arg, double min, double max, const char* funcName, const char* paramName);

// Bounds error for double values
int LuaBoundsErrorDouble(lua_State* L, const char* funcName, const char* paramName, double value, double min, double max);

// Report generic argument error with custom message
int LuaArgError(lua_State* L, const char* funcName, int arg, const char* message);

// Get table size (count of numeric indices starting from 1)
int LuaGetTableSize(lua_State* L, int tableIndex);

// Validate and normalize file path
const char* LuaCheckPath(lua_State* L, int arg, const char* funcName);

// ============================================================================
// Common Conversions
// ============================================================================

// Convert NES color index to overlay format (0x00-0x3F -> 0x80-0xBF)
uint8 NESColorToOverlay(uint8 nesColor);

// Convert overlay format to NES color index (0x80-0xBF -> 0x00-0x3F)
uint8 OverlayToNESColor(uint8 overlayColor);

// Clamp coordinates to screen bounds (0 to OVL_W-1, 0 to OVL_H-1)
void LuaClampScreenCoords(int& x, int& y);

// Clamp rectangle to screen bounds
void LuaClampScreenRect(int& x, int& y, int& w, int& h);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++-Only Functions (must be outside extern "C")
// ============================================================================

#ifdef __cplusplus

// ============================================================================
// Table Conversions
// ============================================================================

// Convert Lua table to vector of integers (1-indexed Lua table -> 0-indexed C++ vector)
// Returns number of elements read
int LuaTableToIntVector(lua_State* L, int tableIndex, std::vector<int>& out, const char* funcName);

// Convert Lua table to vector of bytes (1-indexed Lua table -> 0-indexed C++ vector)
// Clamps values to 0-255 range
int LuaTableToByteVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName);

// Convert Lua table to vector of bytes with NES color validation (0x00-0x3F)
int LuaTableToNESColorVector(lua_State* L, int tableIndex, std::vector<uint8>& out, const char* funcName);

// Convert C++ vector to Lua table (0-indexed C++ vector -> 1-indexed Lua table)
void LuaPushIntVector(lua_State* L, const std::vector<int>& vec);
void LuaPushByteVector(lua_State* L, const std::vector<uint8>& vec);

// ============================================================================
// Path Normalization
// ============================================================================

// Normalize path for cross-platform safety
// Converts backslashes to forward slashes, removes redundant separators
std::string LuaNormalizePath(const char* path);

#endif // __cplusplus

#endif // USE_LUA

