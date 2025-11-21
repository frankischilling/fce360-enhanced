#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_memory.h"
#include "lua_helpers.h"
#include "types.h"  // Must include types.h before fceu.h
#include "fceu.h"  // For ARead and BWrite
#include "fceulua.h"
#include "x6502.h"

#include <map>
#include <vector>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

static std::map<unsigned int, uint8> s_watchedAddresses;  // address -> previous value

// Forward declarations for helpers
static void CheckWatchedAddresses(lua_State* L);

int lua_readbyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "readbyte", 1, 1, n);
	}
	
	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "readbyte", "address");

	uint8 value = ARead[address](address);

	lua_pushinteger(L, value);
	return 1;
}

int lua_readword(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "readword", 1, 1, n);
	}
	
	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "readword", "address");

	uint8 lowByte = ARead[address](address);
	uint8 highByte = 0;

	if (address + 1 <= 0xFFFF) {
		highByte = ARead[address + 1](address + 1);
	}

	uint16 value = lowByte + (highByte * 256);

	lua_pushinteger(L, value);
	return 1;
}

int lua_readbytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "readbytes", 2, 2, n);
	}
	
	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "readbytes", "address");
	int count = LuaCheckRange(L, 2, 1, 256, "readbytes", "count");

	if (address + count > 0x10000) {
		count = 0x10000 - address;
	}

	lua_createtable(L, count, 0);

	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) break;

		uint8 value = ARead[currentAddr](currentAddr);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

int lua_readram(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "readram", 2, 2, n);
	}
	
	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 1, 0, 0x1FFF, "readram", "startAddr");
	int count = LuaCheckRange(L, 2, 1, 256, "readram", "count");

	if (startAddr + count > 0x2000) {
		count = 0x2000 - startAddr;
	}

	lua_createtable(L, count, 0);

	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = startAddr + i;
		if (currentAddr > 0x1FFF) break;

		uint8 value = ARead[currentAddr](currentAddr);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

int lua_scanbyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "scanbyte", 3, 3, n);
	}
	int value = LuaCheckRange(L, 1, 0, 255, "scanbyte", "value");
	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "scanbyte", "startAddr");
	unsigned int endAddr = (unsigned int)LuaCheckRange(L, 3, 0, 0xFFFF, "scanbyte", "endAddr");
	if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }
	lua_createtable(L, 0, 0);
	int resultIndex = 1;
	uint8 target = (uint8)(value & 0xFF);
	for (unsigned int addr = startAddr; addr <= endAddr; ++addr) {
		uint8 b = ARead[addr](addr);
		if (b == target) {
			lua_pushinteger(L, addr);
			lua_rawseti(L, -2, resultIndex++);
		}
		if (addr == 0xFFFF) break;
	}
	return 1;
}

int lua_scanword(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "scanword", 3, 3, n);
	}
	int value = LuaCheckRange(L, 1, 0, 65535, "scanword", "value");
	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "scanword", "startAddr");
	unsigned int endAddr = (unsigned int)LuaCheckRange(L, 3, 0, 0xFFFF, "scanword", "endAddr");
	if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }
	lua_createtable(L, 0, 0);
	int outIndex = 1;
	unsigned int target = (unsigned int)(value & 0xFFFF);
	for (unsigned int addr = startAddr; addr <= endAddr; ++addr) {
		uint8 low = ARead[addr](addr);
		uint8 high = 0;
		if (addr < 0xFFFF) {
			high = ARead[addr + 1](addr + 1);
		}
		unsigned int w = (unsigned int)low | ((unsigned int)high << 8);
		if (w == target) {
			lua_pushinteger(L, addr);
			lua_rawseti(L, -2, outIndex++);
		}
		if (addr == 0xFFFF) break;
	}
	return 1;
}

int lua_scanbytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "scanbytes", 3, 3, n);
	}

	unsigned int startAddr = 0;
	unsigned int endAddr = 0;
	std::vector<uint8> pattern;
	pattern.reserve(64);

	if (lua_istable(L, 1)) {
		if (n < 3) {
			return LuaArgCountError(L, "scanbytes", 3, 3, n);
		}
		startAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "scanbytes", "startAddr");
		endAddr = (unsigned int)LuaCheckRange(L, 3, 0, 0xFFFF, "scanbytes", "endAddr");

		int count = 0;
		for (int i = 1; i <= 256; ++i) {
			lua_rawgeti(L, 1, i);
			if (!lua_isnumber(L, -1)) { lua_pop(L, 1); break; }
			int v = (int)luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			if (v < 0 || v > 255) {
				return luaL_error(L, "scanbytes: pattern values must be 0-255");
			}
			pattern.push_back((uint8)(v & 0xFF));
			++count;
		}
		if (count <= 0) {
			return luaL_error(L, "scanbytes: pattern table must contain at least one byte");
		}
	} else {
		if (n < 3) {
			return LuaArgCountError(L, "scanbytes", 3, 3, n);
		}
		startAddr = (unsigned int)LuaCheckRange(L, n - 1, 0, 0xFFFF, "scanbytes", "startAddr");
		endAddr = (unsigned int)LuaCheckRange(L, n, 0, 0xFFFF, "scanbytes", "endAddr");
		int patCount = n - 2;
		if (patCount <= 0) {
			return luaL_error(L, "scanbytes: must provide at least one byte in the pattern");
		}
		if (patCount > 256) {
			return luaL_error(L, "scanbytes: pattern length cannot exceed 256 bytes");
		}
		for (int i = 1; i <= patCount; ++i) {
			int v = (int)luaL_checkinteger(L, i);
			if (v < 0 || v > 255) {
				return luaL_error(L, "scanbytes: pattern values must be 0-255");
			}
			pattern.push_back((uint8)(v & 0xFF));
		}
	}

	if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
		return luaL_error(L, "scanbytes: addresses must be in range 0x0000-0xFFFF");
	}
	if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }

	if (pattern.empty()) {
		return luaL_error(L, "scanbytes: pattern cannot be empty");
	}

	lua_createtable(L, 0, 0);
	int outIndex = 1;

	unsigned int maxStart;
	if (pattern.size() - 1 > (size_t)0xFFFF) {
		maxStart = 0;
	}
	if (endAddr < (unsigned int)(pattern.size() - 1)) {
		maxStart = 0;
	} else {
		maxStart = endAddr - (unsigned int)(pattern.size() - 1);
	}

	for (unsigned int addr = startAddr; addr <= endAddr && addr <= maxStart; ++addr) {
		bool match = true;
		for (size_t i = 0; i < pattern.size(); ++i) {
			unsigned int cur = addr + (unsigned int)i;
			uint8 b = ARead[cur](cur);
			if (b != pattern[i]) { match = false; break; }
		}
		if (match) {
			lua_pushinteger(L, addr);
			lua_rawseti(L, -2, outIndex++);
		}
		if (addr == 0xFFFF) break;
	}

	return 1;
}

int lua_findpattern(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "findpattern", 3, 4, n);
	}

	LuaCheckTable(L, 1, "findpattern");

	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "findpattern", "startAddr");
	unsigned int endAddr = (unsigned int)LuaCheckRange(L, 3, 0, 0xFFFF, "findpattern", "endAddr");
	std::vector<uint8> pattern;
	std::vector<uint8> mask;
	pattern.reserve(64);
	mask.reserve(64);

	int patternCount = 0;
	for (int i = 1; i <= 256; ++i) {
		lua_rawgeti(L, 1, i);
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); break; }
		int v = (int)luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		if (v < 0 || v > 255) {
			return luaL_error(L, "findpattern: pattern values must be 0-255");
		}
		pattern.push_back((uint8)(v & 0xFF));
		++patternCount;
	}
	if (patternCount <= 0) {
		return luaL_error(L, "findpattern: pattern table must contain at least one byte");
	}

	bool hasMask = false;
	if (n >= 4 && !lua_isnil(L, 4)) {
		if (!lua_istable(L, 4)) {
			return luaL_error(L, "findpattern: mask (4th argument) must be a table or nil");
		}
		hasMask = true;
		int maskCount = 0;
		for (int i = 1; i <= 256; ++i) {
			lua_rawgeti(L, 4, i);
			if (!lua_isnumber(L, -1)) { lua_pop(L, 1); break; }
			int v = (int)luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			mask.push_back((v != 0) ? 1 : 0);
			++maskCount;
		}
		if (maskCount != patternCount) {
			return luaL_error(L, "findpattern: mask table length must match pattern table length");
		}
	}

	if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
		return luaL_error(L, "findpattern: addresses must be in range 0x0000-0xFFFF");
	}
	if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }

	if (pattern.empty()) {
		return luaL_error(L, "findpattern: pattern cannot be empty");
	}

	lua_createtable(L, 0, 0);
	int outIndex = 1;

	unsigned int maxStart;
	if (pattern.size() - 1 > (size_t)0xFFFF) {
		maxStart = 0;
	}
	if (endAddr < (unsigned int)(pattern.size() - 1)) {
		maxStart = 0;
	} else {
		maxStart = endAddr - (unsigned int)(pattern.size() - 1);
	}

	for (unsigned int addr = startAddr; addr <= endAddr && addr <= maxStart; ++addr) {
		bool match = true;
		for (size_t i = 0; i < pattern.size(); ++i) {
			if (hasMask && mask[i] == 0) {
				continue;
			}

			unsigned int cur = addr + (unsigned int)i;
			uint8 b = ARead[cur](cur);
			if (b != pattern[i]) {
				match = false;
				break;
			}
		}
		if (match) {
			lua_pushinteger(L, addr);
			lua_rawseti(L, -2, outIndex++);
		}
		if (addr == 0xFFFF) break;
	}

	return 1;
}

int lua_scanchanged(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "scanchanged", 3, 3, n);
	}

	LuaCheckTable(L, 1, "scanchanged");
	LuaCheckTable(L, 2, "scanchanged");

	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 3, 0, 0xFFFF, "scanchanged", "startAddr");

	std::vector<uint8> oldSnapshot;
	std::vector<uint8> newSnapshot;
	oldSnapshot.reserve(256);
	newSnapshot.reserve(256);

	int oldCount = 0;
	for (int i = 1; i <= 256; ++i) {
		lua_rawgeti(L, 1, i);
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		int v = (int)luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		if (v < 0 || v > 255) {
			return luaL_error(L, "scanchanged: oldSnapshot value at index %d must be in range 0-255", i);
		}
		oldSnapshot.push_back((uint8)(v & 0xFF));
		++oldCount;
	}

	int newCount = 0;
	for (int i = 1; i <= 256; ++i) {
		lua_rawgeti(L, 2, i);
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		int v = (int)luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		if (v < 0 || v > 255) {
			return luaL_error(L, "scanchanged: newSnapshot value at index %d must be in range 0-255", i);
		}
		newSnapshot.push_back((uint8)(v & 0xFF));
		++newCount;
	}

	if (oldCount != newCount) {
		return luaL_error(L, "scanchanged: oldSnapshot and newSnapshot must have the same length");
	}

	if (oldCount <= 0) {
		return luaL_error(L, "scanchanged: snapshots must contain at least one byte");
	}

	lua_createtable(L, 0, 0);

	for (int i = 0; i < oldCount; ++i) {
		if (oldSnapshot[i] != newSnapshot[i]) {
			unsigned int addr = startAddr + (unsigned int)i;
			if (addr > 0xFFFF) break;

			lua_pushinteger(L, addr);
			lua_pushinteger(L, newSnapshot[i]);
			lua_rawset(L, -3);
		}
	}

	return 1;
}

int lua_watchbyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "watchbyte", 1, 1, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "watchbyte", "address");

	uint8 currentValue = ARead[address](address);
	s_watchedAddresses[address] = currentValue;

	return 0;
}

int lua_unwatchbyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "unwatchbyte", 1, 1, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "unwatchbyte", "address");

	s_watchedAddresses.erase(address);

	return 0;
}

int lua_getmemorysnapshot(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "getmemorysnapshot", 2, 2, n);
	}

	unsigned int startAddr = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "getmemorysnapshot", "startAddr");
	unsigned int endAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "getmemorysnapshot", "endAddr");

	if (startAddr > endAddr) {
		unsigned int t = startAddr;
		startAddr = endAddr;
		endAddr = t;
	}

	if (endAddr - startAddr > 0xFFFF) {
		return luaL_error(L, "getmemorysnapshot: range cannot exceed 65536 bytes");
	}

	lua_createtable(L, 0, 0);

	for (unsigned int addr = startAddr; addr <= endAddr; ++addr) {
		uint8 value = ARead[addr](addr);
		lua_pushinteger(L, addr);
		lua_pushinteger(L, value);
		lua_rawset(L, -3);
	}

	return 1;
}

int lua_setbit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "setbit", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "setbit", "address");
	int bit = LuaCheckRange(L, 2, 0, 7, "setbit", "bit");

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue | (1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_clearbit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "clearbit", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "clearbit", "address");
	int bit = LuaCheckRange(L, 2, 0, 7, "clearbit", "bit");

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue & ~(1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_togglebit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "togglebit", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "togglebit", "address");
	int bit = LuaCheckRange(L, 2, 0, 7, "togglebit", "bit");

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue ^ (1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_testbit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "testbit", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "testbit", "address");
	int bit = LuaCheckRange(L, 2, 0, 7, "testbit", "bit");

	uint8 value = ARead[address](address);
	int mask = (1 << bit);
	int isSet = ((value & mask) != 0) ? 1 : 0;
	lua_pushboolean(L, isSet);
	return 1;
}

int lua_writebyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "writebyte", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "writebyte", "address");
	int value = LuaCheckRange(L, 2, 0, 255, "writebyte", "value");

	uint8 byteValue = (uint8)(value & 0xFF);
	BWrite[address](address, byteValue);

	return 0;
}

int lua_writeword(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "writeword", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "writeword", "address");
	int value = LuaCheckRange(L, 2, 0, 65535, "writeword", "value");

	uint8 lowByte = (uint8)(value & 0xFF);
	uint8 highByte = (uint8)((value >> 8) & 0xFF);

	if (address <= 0xFFFF) {
		BWrite[address](address, lowByte);
	}
	if (address + 1 <= 0xFFFF) {
		BWrite[address + 1](address + 1, highByte);
	}

	return 0;
}

int lua_writebytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "writebytes", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "writebytes", "address");

	int count = n - 1;

	for (int i = 0; i < count; ++i) {
		int value = LuaCheckRange(L, i + 2, 0, 255, "writebytes", "value");

		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) {
			break;
		}

		uint8 byteValue = (uint8)(value & 0xFF);
		BWrite[currentAddr](currentAddr, byteValue);
	}

	return 0;
}

int lua_writeprg(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "writeprg", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0x8000, 0xFFFF, "writeprg", "address");
	int value = LuaCheckRange(L, 2, 0, 255, "writeprg", "value");

	if (value < 0 || value > 255) {
		return luaL_error(L, "writeprg: value must be in range 0-255");
	}

	uint8 byteValue = (uint8)(value & 0xFF);
	BWrite[address](address, byteValue);

	return 0;
}

int lua_fillbytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "fillbytes", 3, 3, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "fillbytes", "address");
	int count = LuaCheckRange(L, 2, 1, 256, "fillbytes", "count");
	int value = LuaCheckRange(L, 3, 0, 255, "fillbytes", "value");

	uint8 byteValue = (uint8)(value & 0xFF);

	if (address + count > 0x10000) {
		count = 0x10000 - address;
	}

	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) break;

		BWrite[currentAddr](currentAddr, byteValue);
	}

	return 0;
}

int lua_copybytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "copybytes", 3, 3, n);
	}

	unsigned int sourceAddr = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "copybytes", "sourceAddr");
	unsigned int destAddr = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "copybytes", "destAddr");
	int count = LuaCheckPositive(L, 3, "copybytes", "count");

	if (count < 1) {
		return luaL_error(L, "copybytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "copybytes: count cannot exceed 256");
	}

	if (sourceAddr + count > 0x10000) {
		count = 0x10000 - sourceAddr;
	}
	if (destAddr + count > 0x10000) {
		count = 0x10000 - destAddr;
	}

	if (destAddr > sourceAddr && destAddr < sourceAddr + count) {
		for (int i = count - 1; i >= 0; --i) {
			unsigned int srcAddr = sourceAddr + i;
			unsigned int dstAddr = destAddr + i;
			if (srcAddr > 0xFFFF || dstAddr > 0xFFFF) break;

			uint8 value = ARead[srcAddr](srcAddr);
			BWrite[dstAddr](dstAddr, value);
		}
	} else {
		for (int i = 0; i < count; ++i) {
			unsigned int srcAddr = sourceAddr + i;
			unsigned int dstAddr = destAddr + i;
			if (srcAddr > 0xFFFF || dstAddr > 0xFFFF) break;

			uint8 value = ARead[srcAddr](srcAddr);
			BWrite[dstAddr](dstAddr, value);
		}
	}

	return 0;
}

int lua_comparebytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return LuaArgCountError(L, "comparebytes", 3, 3, n);
	}

	unsigned int addr1 = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "comparebytes", "addr1");
	unsigned int addr2 = (unsigned int)LuaCheckRange(L, 2, 0, 0xFFFF, "comparebytes", "addr2");
	int count = LuaCheckPositive(L, 3, "comparebytes", "count");

	if (count < 1) {
		return luaL_error(L, "comparebytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "comparebytes: count cannot exceed 256");
	}

	if (addr1 + count > 0x10000) {
		count = 0x10000 - addr1;
	}
	if (addr2 + count > 0x10000) {
		int count2 = 0x10000 - addr2;
		if (count2 < count) {
			count = count2;
		}
	}

	for (int i = 0; i < count; ++i) {
		unsigned int addr1_current = addr1 + i;
		unsigned int addr2_current = addr2 + i;
		if (addr1_current > 0xFFFF || addr2_current > 0xFFFF) break;

		uint8 value1 = ARead[addr1_current](addr1_current);
		uint8 value2 = ARead[addr2_current](addr2_current);

		if (value1 != value2) {
			lua_pushboolean(L, 0);
			return 1;
		}
	}

	lua_pushboolean(L, 1);
	return 1;
}

int lua_backupbytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "backupbytes", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "backupbytes", "address");
	int count = LuaCheckRange(L, 2, 1, 256, "backupbytes", "count");

	if (address + count > 0x10000) {
		count = 0x10000 - address;
	}

	lua_createtable(L, count, 0);

	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) break;

		uint8 value = ARead[currentAddr](currentAddr);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

int lua_restorebytes(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "restorebytes", 2, 2, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "restorebytes", "address");

	LuaCheckTable(L, 2, "restorebytes");

	int count = 0;
	for (int i = 1; i <= 256; ++i) {
		lua_rawgeti(L, 2, i);
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		int value = (int)luaL_checkinteger(L, -1);
		lua_pop(L, 1);

		if (value < 0 || value > 255) {
			return luaL_error(L, "restorebytes: value at index %d must be in range 0-255", i);
		}

		unsigned int currentAddr = address + count;
		if (currentAddr > 0xFFFF) {
			break;
		}

		uint8 byteValue = (uint8)(value & 0xFF);
		BWrite[currentAddr](currentAddr, byteValue);
		++count;
	}

	if (count <= 0) {
		return luaL_error(L, "restorebytes: data table must contain at least one byte");
	}

	return 0;
}

int lua_getmemorytype(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getmemorytype", 1, 1, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "getmemorytype", "address");

	const char* memType;
	if (address <= 0x1FFF) {
		memType = "RAM";
	} else if (address >= 0x2000 && address <= 0x3FFF) {
		memType = "PPU";
	} else if (address >= 0x4000 && address <= 0x401F) {
		memType = "APU";
	} else if (address >= 0x8000 && address <= 0xFFFF) {
		memType = "ROM";
	} else {
		memType = "UNKNOWN";
	}

	lua_pushstring(L, memType);
	return 1;
}

int lua_ismemorywritable(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "ismemorywritable", 1, 1, n);
	}

	unsigned int address = (unsigned int)LuaCheckRange(L, 1, 0, 0xFFFF, "ismemorywritable", "address");

	int isWritable;
	if (address <= 0x1FFF) {
		isWritable = 1;
	} else if (address >= 0x2000 && address <= 0x3FFF) {
		isWritable = 1;
	} else if (address >= 0x4000 && address <= 0x401F) {
		isWritable = 1;
	} else {
		isWritable = 0;
	}

	lua_pushboolean(L, isWritable);
	return 1;
}

static void CheckWatchedAddresses(lua_State* L) {
	if (!L) {
		return;
	}

	if (s_watchedAddresses.empty()) {
		return;
	}

	lua_getglobal(L, "onwatch");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	for (std::map<unsigned int, uint8>::iterator it = s_watchedAddresses.begin();
		 it != s_watchedAddresses.end(); ++it) {
		unsigned int addr = it->first;
		uint8 oldValue = it->second;
		uint8 newValue = ARead[addr](addr);

		if (oldValue != newValue) {
			it->second = newValue;

			lua_pushvalue(L, -1);
			lua_pushinteger(L, addr);
			lua_pushinteger(L, oldValue);
			lua_pushinteger(L, newValue);
			if (lua_pcall(L, 3, 0, 0) != 0) {
				const char* err = lua_tostring(L, -1);
				printf("LUA ERROR (onwatch callback): %s\n", err ? err : "unknown error");
				if (err && err[0]) LuaConsolePushLine(err);
				lua_pop(L, 1);
			}
		}
	}

	lua_pop(L, 1);
}

static const luaL_Reg kMemoryFuncs[] = {
	{"readbyte", lua_readbyte},
	{"readword", lua_readword},
	{"readbytes", lua_readbytes},
	{"readram", lua_readram},
	{"scanbyte", lua_scanbyte},
	{"scanword", lua_scanword},
	{"scanbytes", lua_scanbytes},
	{"findpattern", lua_findpattern},
	{"scanchanged", lua_scanchanged},
	{"watchbyte", lua_watchbyte},
	{"unwatchbyte", lua_unwatchbyte},
	{"getmemorysnapshot", lua_getmemorysnapshot},
	{"setbit", lua_setbit},
	{"clearbit", lua_clearbit},
	{"togglebit", lua_togglebit},
	{"testbit", lua_testbit},
	{"writebyte", lua_writebyte},
	{"writeword", lua_writeword},
	{"writebytes", lua_writebytes},
	{"writeprg", lua_writeprg},
	{"fillbytes", lua_fillbytes},
	{"copybytes", lua_copybytes},
	{"comparebytes", lua_comparebytes},
	{"backupbytes", lua_backupbytes},
	{"restorebytes", lua_restorebytes},
	{"getmemorytype", lua_getmemorytype},
	{"ismemorywritable", lua_ismemorywritable},
	{NULL, NULL}
};

void Lua_RegisterMemory(lua_State* L) {
	if (!L) {
		return;
	}

	// Manually register each function (luaL_register with NULL has issues)
	for (const luaL_Reg* reg = kMemoryFuncs; reg->name != NULL; reg++) {
		lua_register(L, reg->name, reg->func);
	}
}

void Lua_MemoryOnFrame(lua_State* L) {
	if (!L || s_watchedAddresses.empty()) {
		return;
	}
	CheckWatchedAddresses(L);
}

void Lua_MemoryResetWatchpoints(void) {
	s_watchedAddresses.clear();
}

#endif // USE_LUA


