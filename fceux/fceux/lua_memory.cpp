#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_memory.h"

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
		return luaL_error(L, "readbyte(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "readbyte: address must be in range 0x0000-0xFFFF");
	}

	uint8 value = ARead[address](address);

	lua_pushinteger(L, value);
	return 1;
}

int lua_readword(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "readword(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "readword: address must be in range 0x0000-0xFFFF");
	}

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
	if (n < 1) {
		return luaL_error(L, "readbytes(address, count) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "readbytes: address must be in range 0x0000-0xFFFF");
	}

	if (count < 1) {
		return luaL_error(L, "readbytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "readbytes: count cannot exceed 256");
	}

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
		return luaL_error(L, "readram(startAddr, count) requires 2 arguments");
	}

	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);

	if (startAddr > 0x1FFF) {
		return luaL_error(L, "readram: startAddr must be in RAM range 0x0000-0x1FFF");
	}

	if (count < 1) {
		return luaL_error(L, "readram: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "readram: count cannot exceed 256");
	}

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
		return luaL_error(L, "scanbyte(value, startAddr, endAddr) requires 3 arguments");
	}
	int value = (int)luaL_checkinteger(L, 1);
	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 2);
	unsigned int endAddr = (unsigned int)luaL_checkinteger(L, 3);
	if (value < 0 || value > 255) {
		return luaL_error(L, "scanbyte: value must be in range 0-255");
	}
	if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
		return luaL_error(L, "scanbyte: addresses must be in range 0x0000-0xFFFF");
	}
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
		return luaL_error(L, "scanword(value, startAddr, endAddr) requires 3 arguments");
	}
	int value = (int)luaL_checkinteger(L, 1);
	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 2);
	unsigned int endAddr = (unsigned int)luaL_checkinteger(L, 3);
	if (value < 0 || value > 65535) {
		return luaL_error(L, "scanword: value must be in range 0-65535");
	}
	if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
		return luaL_error(L, "scanword: addresses must be in range 0x0000-0xFFFF");
	}
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
		return luaL_error(L, "scanbytes requires either (table, startAddr, endAddr) or (b1, b2, ..., startAddr, endAddr)");
	}

	unsigned int startAddr = 0;
	unsigned int endAddr = 0;
	std::vector<uint8> pattern;
	pattern.reserve(64);

	if (lua_istable(L, 1)) {
		if (n < 3) {
			return luaL_error(L, "scanbytes(table, startAddr, endAddr) requires 3 arguments");
		}
		startAddr = (unsigned int)luaL_checkinteger(L, 2);
		endAddr = (unsigned int)luaL_checkinteger(L, 3);

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
			return luaL_error(L, "scanbytes(b1, b2, ..., startAddr, endAddr) requires at least 3 arguments");
		}
		startAddr = (unsigned int)luaL_checkinteger(L, n - 1);
		endAddr = (unsigned int)luaL_checkinteger(L, n);
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
		return luaL_error(L, "findpattern requires (pattern, startAddr, endAddr, [mask])");
	}

	if (!lua_istable(L, 1)) {
		return luaL_error(L, "findpattern: first argument must be a table (pattern)");
	}

	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 2);
	unsigned int endAddr = (unsigned int)luaL_checkinteger(L, 3);
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
		return luaL_error(L, "scanchanged requires (oldSnapshot, newSnapshot, startAddr)");
	}

	if (!lua_istable(L, 1)) {
		return luaL_error(L, "scanchanged: oldSnapshot (1st argument) must be a table");
	}
	if (!lua_istable(L, 2)) {
		return luaL_error(L, "scanchanged: newSnapshot (2nd argument) must be a table");
	}

	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 3);

	if (startAddr > 0xFFFF) {
		return luaL_error(L, "scanchanged: startAddr must be in range 0x0000-0xFFFF");
	}

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
		return luaL_error(L, "watchbyte(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "watchbyte: address must be in range 0x0000-0xFFFF");
	}

	uint8 currentValue = ARead[address](address);
	s_watchedAddresses[address] = currentValue;

	return 0;
}

int lua_unwatchbyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "unwatchbyte(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "unwatchbyte: address must be in range 0x0000-0xFFFF");
	}

	s_watchedAddresses.erase(address);

	return 0;
}

int lua_getmemorysnapshot(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "getmemorysnapshot requires (startAddr, endAddr)");
	}

	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int endAddr = (unsigned int)luaL_checkinteger(L, 2);

	if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
		return luaL_error(L, "getmemorysnapshot: addresses must be in range 0x0000-0xFFFF");
	}

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
		return luaL_error(L, "setbit(address, bit) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int bit = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "setbit: address must be in range 0x0000-0xFFFF");
	}

	if (bit < 0 || bit > 7) {
		return luaL_error(L, "setbit: bit must be in range 0-7");
	}

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue | (1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_clearbit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "clearbit(address, bit) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int bit = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "clearbit: address must be in range 0x0000-0xFFFF");
	}

	if (bit < 0 || bit > 7) {
		return luaL_error(L, "clearbit: bit must be in range 0-7");
	}

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue & ~(1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_togglebit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "togglebit(address, bit) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int bit = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "togglebit: address must be in range 0x0000-0xFFFF");
	}

	if (bit < 0 || bit > 7) {
		return luaL_error(L, "togglebit: bit must be in range 0-7");
	}

	uint8 currentValue = ARead[address](address);
	uint8 newValue = currentValue ^ (1 << bit);
	BWrite[address](address, newValue);

	return 0;
}

int lua_testbit(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "testbit(address, bit) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int bit = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "testbit: address must be in range 0x0000-0xFFFF");
	}
	if (bit < 0 || bit > 7) {
		return luaL_error(L, "testbit: bit must be in range 0-7");
	}

	uint8 value = ARead[address](address);
	int mask = (1 << bit);
	int isSet = ((value & mask) != 0) ? 1 : 0;
	lua_pushboolean(L, isSet);
	return 1;
}

int lua_writebyte(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "writebyte(address, value) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int value = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "writebyte: address must be in range 0x0000-0xFFFF");
	}

	if (value < 0 || value > 255) {
		return luaL_error(L, "writebyte: value must be in range 0-255");
	}

	uint8 byteValue = (uint8)(value & 0xFF);
	BWrite[address](address, byteValue);

	return 0;
}

int lua_writeword(lua_State* L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "writeword(address, value) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int value = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "writeword: address must be in range 0x0000-0xFFFF");
	}

	if (value < 0 || value > 65535) {
		return luaL_error(L, "writeword: value must be in range 0-65535");
	}

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
		return luaL_error(L, "writebytes(address, value1, value2, ...) requires at least 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "writebytes: address must be in range 0x0000-0xFFFF");
	}

	int count = n - 1;

	for (int i = 0; i < count; ++i) {
		int value = (int)luaL_checkinteger(L, i + 2);

		if (value < 0 || value > 255) {
			return luaL_error(L, "writebytes: value %d must be in range 0-255", i + 1);
		}

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
		return luaL_error(L, "writeprg(address, value) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int value = (int)luaL_checkinteger(L, 2);

	if (address < 0x8000 || address > 0xFFFF) {
		return luaL_error(L, "writeprg: address must be in program ROM range 0x8000-0xFFFF");
	}

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
		return luaL_error(L, "fillbytes(address, count, value) requires 3 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);
	int value = (int)luaL_checkinteger(L, 3);

	if (address > 0xFFFF) {
		return luaL_error(L, "fillbytes: address must be in range 0x0000-0xFFFF");
	}

	if (count < 1) {
		return luaL_error(L, "fillbytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "fillbytes: count cannot exceed 256");
	}

	if (value < 0 || value > 255) {
		return luaL_error(L, "fillbytes: value must be in range 0-255");
	}

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
		return luaL_error(L, "copybytes(sourceAddr, destAddr, count) requires 3 arguments");
	}

	unsigned int sourceAddr = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int destAddr = (unsigned int)luaL_checkinteger(L, 2);
	int count = (int)luaL_checkinteger(L, 3);

	if (sourceAddr > 0xFFFF) {
		return luaL_error(L, "copybytes: sourceAddr must be in range 0x0000-0xFFFF");
	}
	if (destAddr > 0xFFFF) {
		return luaL_error(L, "copybytes: destAddr must be in range 0x0000-0xFFFF");
	}

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
		return luaL_error(L, "comparebytes(addr1, addr2, count) requires 3 arguments");
	}

	unsigned int addr1 = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int addr2 = (unsigned int)luaL_checkinteger(L, 2);
	int count = (int)luaL_checkinteger(L, 3);

	if (addr1 > 0xFFFF) {
		return luaL_error(L, "comparebytes: addr1 must be in range 0x0000-0xFFFF");
	}
	if (addr2 > 0xFFFF) {
		return luaL_error(L, "comparebytes: addr2 must be in range 0x0000-0xFFFF");
	}

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
		return luaL_error(L, "backupbytes(address, count) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);

	if (address > 0xFFFF) {
		return luaL_error(L, "backupbytes: address must be in range 0x0000-0xFFFF");
	}

	if (count < 1) {
		return luaL_error(L, "backupbytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "backupbytes: count cannot exceed 256");
	}

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
		return luaL_error(L, "restorebytes(address, data) requires 2 arguments");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "restorebytes: address must be in range 0x0000-0xFFFF");
	}

	if (!lua_istable(L, 2)) {
		return luaL_error(L, "restorebytes: data must be a table (from backupbytes)");
	}

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
		return luaL_error(L, "getmemorytype(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "getmemorytype: address must be in range 0x0000-0xFFFF");
	}

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
		return luaL_error(L, "ismemorywritable(address) requires 1 argument");
	}

	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

	if (address > 0xFFFF) {
		return luaL_error(L, "ismemorywritable: address must be in range 0x0000-0xFFFF");
	}

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

void Lua_RegisterMemory(lua_State* L) {
	if (!L) {
		return;
	}

	lua_register(L, "readbyte", lua_readbyte);
	lua_register(L, "readword", lua_readword);
	lua_register(L, "readbytes", lua_readbytes);
	lua_register(L, "readram", lua_readram);
	lua_register(L, "scanbyte", lua_scanbyte);
	lua_register(L, "scanword", lua_scanword);
	lua_register(L, "scanbytes", lua_scanbytes);
	lua_register(L, "findpattern", lua_findpattern);
	lua_register(L, "scanchanged", lua_scanchanged);
	lua_register(L, "watchbyte", lua_watchbyte);
	lua_register(L, "unwatchbyte", lua_unwatchbyte);
	lua_register(L, "getmemorysnapshot", lua_getmemorysnapshot);
	lua_register(L, "setbit", lua_setbit);
	lua_register(L, "clearbit", lua_clearbit);
	lua_register(L, "togglebit", lua_togglebit);
	lua_register(L, "testbit", lua_testbit);
	lua_register(L, "writebyte", lua_writebyte);
	lua_register(L, "writeword", lua_writeword);
	lua_register(L, "writebytes", lua_writebytes);
	lua_register(L, "writeprg", lua_writeprg);
	lua_register(L, "fillbytes", lua_fillbytes);
	lua_register(L, "copybytes", lua_copybytes);
	lua_register(L, "comparebytes", lua_comparebytes);
	lua_register(L, "backupbytes", lua_backupbytes);
	lua_register(L, "restorebytes", lua_restorebytes);
	lua_register(L, "getmemorytype", lua_getmemorytype);
	lua_register(L, "ismemorywritable", lua_ismemorywritable);
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


