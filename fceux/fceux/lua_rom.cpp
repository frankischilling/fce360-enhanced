#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_rom.h"
#include "fceu.h"
#include "cart.h"
#include "file.h"
#include "driver.h"  // For FCEUD_UTF8fopen
#include "ines.h"
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <string>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// ==================== ROM Information Functions ====================

// getromname() -> string
// Gets the current ROM filename (without path)
// Returns the filename with extension (.nes, .fds, etc.)
// Works for both NES and FDS games
static int lua_getromname(lua_State* L)
{
	extern FCEUGI *GameInfo;
	
	// Check if a game is loaded
	if (!GameInfo || !GameInfo->filename) {
		lua_pushstring(L, "");
		return 1;
	}
	
	// Get the full filename/path
	const char* fullPath = GameInfo->filename;
	if (!fullPath || !fullPath[0]) {
		lua_pushstring(L, "");
		return 1;
	}
	
	// Handle zip archive format: "path.zip|internal.nes" or "path.zip|internal.fds"
	std::string filename;
	const char* pipePos = strchr(fullPath, '|');
	if (pipePos) {
		// Extract filename after the pipe (internal file in archive)
		filename = pipePos + 1;
	} else {
		// Not in archive, use the full path
		filename = fullPath;
	}
	
	// Extract just the filename without path
	size_t lastSlash = filename.find_last_of("\\/");
	if (lastSlash != std::string::npos) {
		filename = filename.substr(lastSlash + 1);
	}
	
	// Return the filename with extension (e.g., "Super Mario Bros.nes" or "game.fds")
	lua_pushstring(L, filename.c_str());
	return 1;
}

// getrompath() -> string
// Gets the full ROM file path (or archive entry)
// Use case: File operations relative to ROM location
static int lua_getrompath(lua_State* L)
{
	extern FCEUGI *GameInfo;
	
	if (!GameInfo || !GameInfo->filename) {
		lua_pushstring(L, "");
		return 1;
	}
	
	const char* fullPath = GameInfo->filename;
	if (!fullPath || !fullPath[0]) {
		lua_pushstring(L, "");
		return 1;
	}
	
	lua_pushstring(L, fullPath);
	return 1;
}

// getsavepath() -> string
// Gets the battery save path for the current ROM
// Use case: Save file management
static int lua_getsavepath(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	
	if (!GameInfo) {
		lua_pushstring(L, "");
		return 1;
	}
	
	// Canonical per-ROM battery save path (single file holding all in-game slots)
	std::string savePath = FCEU_MakeFName(FCEUMKF_SAV, 0, "sav");

	// If canonical is missing but a legacy game.sav exists (older builds),
	// surface that path so scripts can migrate/inspect it.
	FILE* fp = FCEUD_UTF8fopen(savePath.c_str(), "rb");
	if(!fp)
	{
		// Build legacy path in same directory named "game.sav"
		std::string legacyPath;
		size_t sep = savePath.find_last_of("/\\");
		if(sep == std::string::npos)
			legacyPath = "game.sav";
		else
			legacyPath = savePath.substr(0, sep + 1) + "game.sav";

		fp = FCEUD_UTF8fopen(legacyPath.c_str(), "rb");
		if(fp)
			savePath = legacyPath;
	}

	// If we still have no file, return empty string to indicate absence.
	if(!fp)
	{
		lua_pushstring(L, "");
		return 1;
	}

	fclose(fp);
	lua_pushstring(L, savePath.c_str());
	return 1;
}

// getromhash(algorithm) -> string
// Gets ROM hash using specified algorithm
// Parameters: algorithm (string: "crc32", "crc", "md5", "sum", "sum16", "xor", etc.)
// Returns: hash value as hexadecimal string, or empty string if no ROM is loaded
// Throws error if algorithm is invalid or unsupported (e.g., "sha1")
static int lua_getromhash(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern uint32 iNESGameCRC32;
	extern uint8 *PRGptr[32];
	extern uint8 *CHRptr[32];
	extern uint32 ROM_size;
	extern uint32 VROM_size;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushstring(L, "");
		return 1;
	}
	
	// Get algorithm parameter
	const char* algorithm = luaL_checkstring(L, 1);
	if (!algorithm || !algorithm[0]) {
		return luaL_error(L, "getromhash: algorithm cannot be empty");
	}
	
	// Convert algorithm to lowercase for case-insensitive comparison
	char lowerAlg[16];
	int i = 0;
	for (; algorithm[i] && i < 15; ++i) {
		char c = algorithm[i];
		if (c >= 'A' && c <= 'Z') {
			lowerAlg[i] = c - 'A' + 'a';
		} else {
			lowerAlg[i] = c;
		}
	}
	lowerAlg[i] = '\0';
	
	// Handle different algorithms
	if (strcmp(lowerAlg, "crc32") == 0 || strcmp(lowerAlg, "crc") == 0) {
		// Return CRC32 as 8-character hex string
		char hexStr[9];
		sprintf(hexStr, "%08x", iNESGameCRC32);
		lua_pushstring(L, hexStr);
		return 1;
	} else if (strcmp(lowerAlg, "md5") == 0) {
		// Return MD5 as 32-character hex string
		char hexStr[33];
		char* ptr = hexStr;
		for (int i = 0; i < 16; ++i) {
			sprintf(ptr, "%02x", GameInfo->MD5[i]);
			ptr += 2;
		}
		hexStr[32] = '\0';
		lua_pushstring(L, hexStr);
		return 1;
	} else if (strcmp(lowerAlg, "sum") == 0 || strcmp(lowerAlg, "checksum") == 0) {
		// Calculate simple 8-bit sum of all ROM bytes
		uint32 sum = 0;
		uint32 prgSize = ROM_size << 14;
		uint32 chrSize = VROM_size << 13;
		
		// Sum PRG-ROM
		if (PRGptr[0]) {
			for (uint32 i = 0; i < prgSize && i < (32 << 14); ++i) {
				sum += PRGptr[0][i];
			}
		}
		
		// Sum CHR-ROM
		if (CHRptr[0]) {
			for (uint32 i = 0; i < chrSize && i < (32 << 13); ++i) {
				sum += CHRptr[0][i];
			}
		}
		
		// Return as 2-character hex string (8-bit sum)
		char hexStr[3];
		sprintf(hexStr, "%02x", sum & 0xFF);
		lua_pushstring(L, hexStr);
		return 1;
	} else if (strcmp(lowerAlg, "sum16") == 0) {
		// Calculate 16-bit sum of all ROM bytes
		uint32 sum = 0;
		uint32 prgSize = ROM_size << 14;
		uint32 chrSize = VROM_size << 13;
		
		// Sum PRG-ROM
		if (PRGptr[0]) {
			for (uint32 i = 0; i < prgSize && i < (32 << 14); ++i) {
				sum += PRGptr[0][i];
			}
		}
		
		// Sum CHR-ROM
		if (CHRptr[0]) {
			for (uint32 i = 0; i < chrSize && i < (32 << 13); ++i) {
				sum += CHRptr[0][i];
			}
		}
		
		// Return as 4-character hex string (16-bit sum)
		char hexStr[5];
		sprintf(hexStr, "%04x", sum & 0xFFFF);
		lua_pushstring(L, hexStr);
		return 1;
	} else if (strcmp(lowerAlg, "xor") == 0) {
		// Calculate XOR checksum of all ROM bytes
		uint8 xorSum = 0;
		uint32 prgSize = ROM_size << 14;
		uint32 chrSize = VROM_size << 13;
		
		// XOR PRG-ROM
		if (PRGptr[0]) {
			for (uint32 i = 0; i < prgSize && i < (32 << 14); ++i) {
				xorSum ^= PRGptr[0][i];
			}
		}
		
		// XOR CHR-ROM
		if (CHRptr[0]) {
			for (uint32 i = 0; i < chrSize && i < (32 << 13); ++i) {
				xorSum ^= CHRptr[0][i];
			}
		}
		
		// Return as 2-character hex string
		char hexStr[3];
		sprintf(hexStr, "%02x", xorSum);
		lua_pushstring(L, hexStr);
		return 1;
	} else if (strcmp(lowerAlg, "sha1") == 0 || strcmp(lowerAlg, "sha256") == 0 || strcmp(lowerAlg, "sha512") == 0) {
		// SHA algorithms are not supported in FCEUX
		return luaL_error(L, "getromhash: %s is not supported in FCEUX", algorithm);
	} else {
		// Invalid algorithm
		return luaL_error(L, "getromhash: invalid algorithm '%s'. Valid algorithms: 'crc32', 'crc', 'md5', 'sum', 'sum16', 'xor'", algorithm);
	}
}

// getinesheader() -> table
// Gets full iNES header dump
// Returns: Table with mapper, mirroring, flags, and header data
// Use case: ROM analysis, mapper detection
static int lua_getinesheader(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern struct iNES_HEADER head;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushnil(L);
		return 1;
	}
	
	// Create table to return
	lua_createtable(L, 0, 20);
	
	// Header identification
	char idStr[5];
	idStr[0] = head.ID[0];
	idStr[1] = head.ID[1];
	idStr[2] = head.ID[2];
	idStr[3] = head.ID[3];
	idStr[4] = '\0';
	lua_pushstring(L, "id");
	lua_pushstring(L, idStr);
	lua_settable(L, -3);
	
	// ROM sizes (raw header values)
	lua_pushstring(L, "rom_size");
	lua_pushinteger(L, head.ROM_size);
	lua_settable(L, -3);
	
	lua_pushstring(L, "vrom_size");
	lua_pushinteger(L, head.VROM_size);
	lua_settable(L, -3);
	
	// ROM type flags
	lua_pushstring(L, "rom_type");
	lua_pushinteger(L, head.ROM_type);
	lua_settable(L, -3);
	
	lua_pushstring(L, "rom_type2");
	lua_pushinteger(L, head.ROM_type2);
	lua_settable(L, -3);
	
	// Mapper number (calculated from header)
	int mapper = (head.ROM_type >> 4);
	mapper |= (head.ROM_type2 & 0xF0);
	lua_pushstring(L, "mapper");
	lua_pushinteger(L, mapper);
	lua_settable(L, -3);
	
	// Mirroring (0=horizontal, 1=vertical, 2=four-screen)
	int mirroring = (head.ROM_type & 1);
	if (head.ROM_type & 8) {
		mirroring = 2;  // Four-screen VRAM
	}
	lua_pushstring(L, "mirroring");
	lua_pushinteger(L, mirroring);
	lua_settable(L, -3);
	
	// Mirroring as string
	const char* mirroringStr = "horizontal";
	if (mirroring == 1) {
		mirroringStr = "vertical";
	} else if (mirroring == 2) {
		mirroringStr = "four-screen";
	}
	lua_pushstring(L, "mirroring_string");
	lua_pushstring(L, mirroringStr);
	lua_settable(L, -3);
	
	// Flags
	lua_pushstring(L, "has_battery");
	lua_pushboolean(L, (head.ROM_type & 2) != 0);
	lua_settable(L, -3);
	
	lua_pushstring(L, "has_trainer");
	lua_pushboolean(L, (head.ROM_type & 4) != 0);
	lua_settable(L, -3);
	
	lua_pushstring(L, "four_screen");
	lua_pushboolean(L, (head.ROM_type & 8) != 0);
	lua_settable(L, -3);
	
	lua_pushstring(L, "vs_system");
	lua_pushboolean(L, (head.ROM_type2 & 1) != 0);
	lua_settable(L, -3);
	
	lua_pushstring(L, "playchoice10");
	lua_pushboolean(L, (head.ROM_type2 & 2) != 0);
	lua_settable(L, -3);
	
	lua_pushstring(L, "nes2_format");
	lua_pushboolean(L, (head.ROM_type2 & 8) != 0);
	lua_settable(L, -3);
	
	// Raw header bytes (for advanced analysis)
	lua_pushstring(L, "raw_header");
	lua_createtable(L, 16, 0);
	for (int i = 0; i < 16; ++i) {
		lua_pushinteger(L, i + 1);  // 1-indexed
		lua_pushinteger(L, ((uint8*)&head)[i]);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	// Reserve bytes
	lua_pushstring(L, "reserve");
	lua_createtable(L, 8, 0);
	for (int i = 0; i < 8; ++i) {
		lua_pushinteger(L, i + 1);  // 1-indexed
		lua_pushinteger(L, head.reserve[i]);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	return 1;
}

// getregion() -> string
// Gets ROM region ("NTSC", "PAL", "Dendy")
// Use case: Region-specific behavior
static int lua_getregion(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern FCEUS FSettings;
	extern uint8 PAL;
	extern struct iNES_HEADER head;
	
	int regionCode = 0; // 0 = NTSC, 1 = PAL, 2 = Dendy
	
	if (GameInfo) {
		bool isNES20 = ((head.ROM_type2 & 0x0C) == 0x08);
		if (isNES20 && head.reserve[4]) {
			int tvBits = head.reserve[4] & 0x03; // NES 2.0 timing bits
			if (tvBits == 1) regionCode = 1;
			else if (tvBits == 3) regionCode = 2;
			else regionCode = 0;
		} else {
			if ((head.reserve[1] & 0x01) != 0)
				regionCode = 1;
		}
		
		if (GameInfo->vidsys == GIV_PAL) {
			regionCode = 1;
		} else if (GameInfo->vidsys == GIV_USER) {
			if (FSettings.PAL == 2) regionCode = 2;
			else if (FSettings.PAL) regionCode = 1;
		}
	} else {
		if (FSettings.PAL == 2) regionCode = 2;
		else regionCode = FSettings.PAL ? 1 : 0;
	}

	if (regionCode != 2 && PAL)
		regionCode = 1;
	
	const char* regionStr = "NTSC";
	if (regionCode >= 2) {
		regionStr = "Dendy";
	} else if (regionCode == 1) {
		regionStr = "PAL";
	}
	
	lua_pushstring(L, regionStr);
	return 1;
}

// getromsize() -> integer
// Gets ROM size in bytes (PRG-ROM + CHR-ROM)
// Returns total ROM size in bytes, or 0 if no ROM is loaded
static int lua_getromsize(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern uint32 ROM_size;
	extern uint32 VROM_size;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Calculate total ROM size
	// ROM_size is in 16KB units (0x4000 bytes), VROM_size is in 8KB units (0x2000 bytes)
	uint32 totalSize = (ROM_size << 14) + (VROM_size << 13);
	
	lua_pushinteger(L, (int)totalSize);
	return 1;
}

// getprgsize() -> integer
// Gets PRG-ROM size in bytes
// Returns PRG-ROM size in bytes, or 0 if no ROM is loaded
static int lua_getprgsize(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern uint32 ROM_size;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Calculate PRG-ROM size
	// ROM_size is in 16KB units (0x4000 bytes)
	uint32 prgSize = ROM_size << 14;
	
	lua_pushinteger(L, (int)prgSize);
	return 1;
}

// getchrsize() -> integer
// Gets CHR-ROM size in bytes
// Returns CHR-ROM size in bytes, or 0 if no ROM is loaded
static int lua_getchrsize(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern uint32 VROM_size;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Calculate CHR-ROM size
	// VROM_size is in 8KB units (0x2000 bytes)
	uint32 chrSize = VROM_size << 13;
	
	lua_pushinteger(L, (int)chrSize);
	return 1;
}

// hasbattery() -> boolean
// Checks if ROM has battery-backed save RAM
// Returns true if ROM has battery, false otherwise
static int lua_hasbattery(lua_State* L)
{
	extern FCEUGI *GameInfo;
	extern CartInfo iNESCart;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Return battery status (non-zero = has battery)
	lua_pushboolean(L, iNESCart.battery != 0);
	return 1;
}

// getmapper() -> integer
// Gets NES mapper number (0-255)
// Returns mapper number, or 0 if no ROM is loaded
static int lua_getmapper(lua_State* L)
{
	extern FCEUGI *GameInfo;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Return mapper number (0-255)
	lua_pushinteger(L, GameInfo->mappernum);
	return 1;
}

// getmapperstring() -> string
// Gets mapper name as string (e.g., "NROM", "MMC1", "MMC3")
// Returns mapper name, or formatted "Mapper X" if mapper is not recognized
static int lua_getmapperstring(lua_State* L)
{
	extern FCEUGI *GameInfo;
	
	// Check if a game is loaded
	if (!GameInfo) {
		lua_pushstring(L, "");
		return 1;
	}
	
	int mapper = GameInfo->mappernum;
	
	// Mapper name lookup table (common mappers)
	const char* mapperName = NULL;
	
	switch (mapper) {
		case 0:  mapperName = "NROM"; break;
		case 1:  mapperName = "MMC1"; break;
		case 2:  mapperName = "UNROM"; break;
		case 3:  mapperName = "CNROM"; break;
		case 4:  mapperName = "MMC3"; break;
		case 5:  mapperName = "MMC5"; break;
		case 7:  mapperName = "AOROM"; break;
		case 9:  mapperName = "MMC2"; break;
		case 10: mapperName = "MMC4"; break;
		case 11: mapperName = "Color Dreams"; break;
		case 13: mapperName = "CPROM"; break;
		case 15: mapperName = "100-in1"; break;
		case 16: mapperName = "Bandai"; break;
		case 19: mapperName = "Namco 163"; break;
		case 21: mapperName = "VRC4"; break;
		case 22: mapperName = "VRC2"; break;
		case 23: mapperName = "VRC2"; break;
		case 24: mapperName = "VRC6"; break;
		case 25: mapperName = "VRC4"; break;
		case 26: mapperName = "VRC6"; break;
		case 34: mapperName = "BNROM"; break;
		case 66: mapperName = "GNROM"; break;
		case 68: mapperName = "Sunsoft Mapper #4"; break;
		case 69: mapperName = "FME-7"; break;
		case 71: mapperName = "Camerica"; break;
		case 78: mapperName = "Irem"; break;
		case 85: mapperName = "VRC7"; break;
		case 93: mapperName = "Sunsoft UNROM"; break;
		case 94: mapperName = "UN1ROM"; break;
		case 118: mapperName = "TLSROM"; break;
		case 119: mapperName = "TQROM"; break;
		case 159: mapperName = "Bandai"; break;
		case 232: mapperName = "Camerica"; break;
		default:
			// For unknown mappers, return formatted as "Mapper X"
			if (mapper >= 0 && mapper <= 255) {
				static char unknownMapper[32];
				snprintf(unknownMapper, sizeof(unknownMapper), "Mapper %d", mapper);
				lua_pushstring(L, unknownMapper);
				return 1;
			} else {
				lua_pushstring(L, "Unknown");
				return 1;
			}
	}
	
	lua_pushstring(L, mapperName);
	return 1;
}

// ==================== Registrar Function ====================

void Lua_RegisterRom(lua_State* L) {
	if (!L) {
		return;
	}

	lua_register(L, "getromname", lua_getromname);
	lua_register(L, "getrompath", lua_getrompath);
	lua_register(L, "getsavepath", lua_getsavepath);
	lua_register(L, "getromhash", lua_getromhash);
	lua_register(L, "getinesheader", lua_getinesheader);
	lua_register(L, "getregion", lua_getregion);
	lua_register(L, "getromsize", lua_getromsize);
	lua_register(L, "getprgsize", lua_getprgsize);
	lua_register(L, "getchrsize", lua_getchrsize);
	lua_register(L, "hasbattery", lua_hasbattery);
	lua_register(L, "getmapper", lua_getmapper);
	lua_register(L, "getmapperstring", lua_getmapperstring);
}

#endif // USE_LUA

