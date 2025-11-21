#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_movie.h"
#include "lua_helpers.h"
#include "lua_fileio.h"

#include "fceulua.h"
#include "fceu.h"
#include "types.h"
#include "state.h"
#include "file.h"

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

// --- Input recording and playback state ---
static bool s_inputRecording = false;
static std::vector<uint8> s_recordedInput[4];
static bool s_inputPlayback = false;
static std::vector<uint8> s_playbackInput[4];
static int s_playbackFrame = 0;
static double s_playbackPosition = 0.0;
static double s_playbackSpeed = 1.0;
static std::map<std::string, int> s_recordingMarkers;

// MovieCreateParentDirectories removed - use Lua_FileIOCreateParentDirectories from lua_fileio.h

void Lua_MovieReset(void)
{
	s_inputRecording = false;
	s_inputPlayback = false;
	s_playbackFrame = 0;
	s_playbackPosition = 0.0;
	s_playbackSpeed = 1.0;
	for (int p = 0; p < 4; ++p) {
		s_recordedInput[p].clear();
		s_playbackInput[p].clear();
	}
	s_recordingMarkers.clear();
}

bool Lua_MovieIsPlaybackActive(void)
{
	return s_inputPlayback;
}

uint32_t Lua_MovieApplyPowerpad(uint32_t powerpadbuf)
{
	if (!s_inputPlayback) {
		return powerpadbuf;
	}

	s_playbackFrame = (int)s_playbackPosition;

	if (s_playbackFrame < (int)s_playbackInput[0].size()) {
		uint8 pad0 = s_playbackInput[0][s_playbackFrame];
		powerpadbuf = (powerpadbuf & 0xFFFFFF00) | pad0;
	}

	if (s_playbackFrame < (int)s_playbackInput[1].size()) {
		uint8 pad1 = s_playbackInput[1][s_playbackFrame];
		powerpadbuf = (powerpadbuf & 0xFFFF00FF) | ((uint32_t)pad1 << 8);
	}

	return powerpadbuf;
}

uint8_t Lua_MovieProcessJoypad(int player, uint8_t finalButtons)
{
	if (player >= 0 && player < 4) {
		if (s_inputPlayback) {
			s_playbackFrame = (int)s_playbackPosition;
			if (s_playbackFrame < (int)s_playbackInput[player].size()) {
				finalButtons = s_playbackInput[player][s_playbackFrame];
			}
		}
		if (s_inputRecording) {
			s_recordedInput[player].push_back(finalButtons);
		}
	}
	return finalButtons;
}

void Lua_MovieAdvancePlayback(void)
{
	if (!s_inputPlayback) {
		return;
	}

	if (s_playbackSpeed >= 1.0) {
		s_playbackFrame = (int)s_playbackPosition;
		s_playbackPosition += 1.0;
	} else {
		s_playbackPosition += s_playbackSpeed;
		s_playbackFrame = (int)floor(s_playbackPosition);
	}

	int maxFrames = 0;
	for (int p = 0; p < 4; ++p) {
		if ((int)s_playbackInput[p].size() > maxFrames) {
			maxFrames = (int)s_playbackInput[p].size();
		}
	}
	if (s_playbackFrame >= maxFrames && maxFrames > 0) {
		s_playbackFrame = maxFrames - 1;
	}

	bool allFinished = true;
	for (int p = 0; p < 4; ++p) {
		if (s_playbackFrame < (int)s_playbackInput[p].size()) {
			allFinished = false;
			break;
		}
	}
	if (allFinished) {
		s_inputPlayback = false;
		s_playbackFrame = 0;
		s_playbackPosition = 0.0;
	}
}

// Lua bindings -------------------------------------------------------------

static int lua_startinputrecording(lua_State* L)
{
	(void)L;
	if (s_inputRecording) {
		lua_pushboolean(L, 0);
		return 1;
	}
	for (int p = 0; p < 4; ++p) {
		s_recordedInput[p].clear();
	}
	s_recordingMarkers.clear();
	s_inputRecording = true;
	lua_pushboolean(L, 1);
	return 1;
}

static int lua_stopinputrecording(lua_State* L)
{
	if (!s_inputRecording) {
		return luaL_error(L, "stopinputrecording: not currently recording");
	}

	s_inputRecording = false;
	lua_createtable(L, 0, 4);

	for (int p = 0; p < 4; ++p) {
		lua_createtable(L, (int)s_recordedInput[p].size(), 0);
		for (size_t i = 0; i < s_recordedInput[p].size(); ++i) {
			lua_pushinteger(L, (int)s_recordedInput[p][i]);
			lua_rawseti(L, -2, (int)(i + 1));
		}
		char key[16];
		snprintf(key, sizeof(key), "player%d", p);
		lua_setfield(L, -2, key);
	}

	return 1;
}

static int lua_playinputrecording(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "playinputrecording", 1, 1, n);
	}
	LuaCheckTable(L, 1, "playinputrecording");

	s_inputPlayback = false;
	s_playbackFrame = 0;
	s_playbackPosition = 0.0;

	for (int p = 0; p < 4; ++p) {
		s_playbackInput[p].clear();
	}

	for (int p = 0; p < 4; ++p) {
		char key[16];
		snprintf(key, sizeof(key), "player%d", p);
		lua_getfield(L, 1, key);
		if (lua_istable(L, -1)) {
			int i = 1;
			while (true) {
				lua_rawgeti(L, -1, i);
				if (!lua_isnumber(L, -1)) {
					lua_pop(L, 1);
					break;
				}
				int value = (int)luaL_checkinteger(L, -1);
				lua_pop(L, 1);
				if (value < 0) value = 0;
				if (value > 0xFF) value = 0xFF;
				s_playbackInput[p].push_back((uint8)(value & 0xFF));
				++i;
			}
		}
		lua_pop(L, 1);
	}

	s_inputPlayback = true;
	s_playbackFrame = 0;
	s_playbackPosition = 0.0;
	return 0;
}

static int lua_saveinputrecording(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "saveinputrecording", 1, 1, n);
	}

	const char* filename = LuaCheckPath(L, 1, "saveinputrecording");
	if (!filename || strlen(filename) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	bool hasData = false;
	size_t maxFrames = 0;
	for (int p = 0; p < 4; ++p) {
		if (!s_recordedInput[p].empty()) {
			hasData = true;
			if (s_recordedInput[p].size() > maxFrames) {
				maxFrames = s_recordedInput[p].size();
			}
		}
	}
	if (!hasData) {
		lua_pushboolean(L, 0);
		return 1;
	}

	char fullpath[512];
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		const char* baseDir = "hdd1:\\fce360-enhanced\\lua\\recordings\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	}

	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}

	Lua_FileIOCreateParentDirectories(fullpath);

	HANDLE hFile = CreateFileA(fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (!strchr(filename, ':') && filename[0] != '\\' && filename[0] != '/') {
			const char* gameDir = "game:\\lua\\recordings\\";
			snprintf(fullpath, sizeof(fullpath), "%s%s", gameDir, filename);
			for (int i = 0; fullpath[i] != '\0'; i++) {
				if (fullpath[i] == '/') {
					fullpath[i] = '\\';
				}
			}
			Lua_FileIOCreateParentDirectories(fullpath);
			hFile = CreateFileA(fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		if (hFile == INVALID_HANDLE_VALUE) {
			lua_pushboolean(L, 0);
			return 1;
		}
	}

	bool writeSuccess = true;
	for (size_t frame = 0; frame < maxFrames; ++frame) {
		char line[64];
		int len = 0;
		for (int p = 0; p < 4; ++p) {
			uint8 buttonMask = 0;
			if (frame < s_recordedInput[p].size()) {
				buttonMask = s_recordedInput[p][frame];
			}
			if (p > 0) {
				line[len++] = ',';
			}
			len += snprintf(line + len, sizeof(line) - len, "%d", (int)buttonMask);
		}
		line[len++] = '\n';
		line[len] = '\0';
		DWORD bytesWritten = 0;
		BOOL result = WriteFile(hFile, line, (DWORD)len, &bytesWritten, NULL);
		if (!result || bytesWritten != (DWORD)len) {
			writeSuccess = false;
			break;
		}
	}

	CloseHandle(hFile);
	lua_pushboolean(L, writeSuccess ? 1 : 0);
	return 1;
}

static int lua_loadinputrecording(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "loadinputrecording", 1, 1, n);
	}

	const char* filename = LuaCheckPath(L, 1, "loadinputrecording");
	if (!filename || strlen(filename) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	char fullpath[512];
	if (strchr(filename, ':') || filename[0] == '\\' || filename[0] == '/') {
		strncpy(fullpath, filename, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	} else {
		const char* baseDir = "hdd1:\\fce360-enhanced\\lua\\recordings\\";
		snprintf(fullpath, sizeof(fullpath), "%s%s", baseDir, filename);
	}

	for (int i = 0; fullpath[i] != '\0'; i++) {
		if (fullpath[i] == '/') {
			fullpath[i] = '\\';
		}
	}

	FILE* file = fopen(fullpath, "rb");
	if (!file) {
		const char* altPaths[] = {
			"game:\\lua\\recordings\\%s",
			"hdd1:\\fce360-enhanced\\lua\\recordings\\%s",
			"game:\\lua\\%s",
			"hdd1:\\fce360-enhanced\\lua\\%s",
			"game:\\%s"
		};
		bool found = false;
		for (int i = 0; i < (int)(sizeof(altPaths) / sizeof(altPaths[0])); i++) {
			char altPath[512];
			snprintf(altPath, sizeof(altPath), altPaths[i], filename);
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
			lua_pushboolean(L, 0);
			return 1;
		}
	}

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fileSize <= 0) {
		fclose(file);
		lua_pushboolean(L, 0);
		return 1;
	}

	char* buffer = (char*)malloc(fileSize + 1);
	if (!buffer) {
		fclose(file);
		lua_pushboolean(L, 0);
		return 1;
	}
	size_t bytesRead = fread(buffer, 1, fileSize, file);
	fclose(file);
	if (bytesRead != (size_t)fileSize) {
		free(buffer);
		lua_pushboolean(L, 0);
		return 1;
	}
	buffer[fileSize] = '\0';

	s_inputPlayback = false;
	s_playbackFrame = 0;
	s_playbackPosition = 0.0;
	for (int p = 0; p < 4; ++p) {
		s_playbackInput[p].clear();
	}

	char* lineStart = buffer;
	while (lineStart < buffer + fileSize) {
		char* lineEnd = strchr(lineStart, '\n');
		if (!lineEnd) {
			lineEnd = buffer + fileSize;
		}
		char savedChar = *lineEnd;
		*lineEnd = '\0';

		if (lineStart == lineEnd || (*lineStart == '\r' && lineStart + 1 == lineEnd)) {
			*lineEnd = savedChar;
			lineStart = lineEnd + 1;
			continue;
		}

		int values[4] = {0, 0, 0, 0};
		int valueIndex = 0;
		char* token = lineStart;
		while (token < lineEnd && valueIndex < 4) {
			while (*token == ' ' || *token == '\t' || *token == '\r') {
				token++;
			}
			if (token >= lineEnd) break;

			char* comma = strchr(token, ',');
			if (!comma || comma > lineEnd) {
				comma = lineEnd;
			}
			char savedComma = *comma;
			*comma = '\0';

			int value = atoi(token);
			if (value < 0) value = 0;
			if (value > 0xFF) value = 0xFF;
			values[valueIndex] = value;

			*comma = savedComma;
			valueIndex++;
			token = comma + 1;
		}

		*lineEnd = savedChar;
		for (int p = 0; p < 4; ++p) {
			s_playbackInput[p].push_back((uint8)(values[p] & 0xFF));
		}

		lineStart = lineEnd + 1;
	}

	free(buffer);

	bool hasData = false;
	for (int p = 0; p < 4; ++p) {
		if (!s_playbackInput[p].empty()) {
			hasData = true;
			break;
		}
	}

	if (!hasData) {
		lua_pushboolean(L, 0);
		return 1;
	}

	s_inputPlayback = true;
	s_playbackFrame = 0;
	s_playbackPosition = 0.0;
	lua_pushboolean(L, 1);
	return 1;
}

static int lua_setrecordingmarker(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "setrecordingmarker", 1, 1, n);
	}
	
	const char* name = LuaCheckString(L, 1, "setrecordingmarker");
	if (!name || strlen(name) == 0) {
		return 0;
	}
	if (!s_inputRecording) {
		return 0;
	}

	int currentFrame = (int)s_recordedInput[0].size();
	s_recordingMarkers[std::string(name)] = currentFrame;
	return 0;
}

static int lua_jumptorecordingmarker(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "jumptorecordingmarker", 1, 1, n);
	}
	
	const char* name = LuaCheckString(L, 1, "jumptorecordingmarker");
	if (!name || strlen(name) == 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!s_inputPlayback) {
		lua_pushboolean(L, 0);
		return 1;
	}

	std::map<std::string, int>::iterator it = s_recordingMarkers.find(std::string(name));
	if (it == s_recordingMarkers.end()) {
		lua_pushboolean(L, 0);
		return 1;
	}

	int markerFrame = it->second;
	int maxFrames = 0;
	for (int p = 0; p < 4; ++p) {
		if ((int)s_playbackInput[p].size() > maxFrames) {
			maxFrames = (int)s_playbackInput[p].size();
		}
	}

	if (markerFrame < 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	if (markerFrame >= maxFrames) {
		if (maxFrames > 0) {
			markerFrame = maxFrames - 1;
		} else {
			lua_pushboolean(L, 0);
			return 1;
		}
	}

	s_playbackFrame = markerFrame;
	s_playbackPosition = (double)markerFrame;
	lua_pushboolean(L, 1);
	return 1;
}

static int lua_setplaybackspeed(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "setplaybackspeed", 1, 1, n);
	}
	double mult = LuaCheckNumber(L, 1, "setplaybackspeed");
	if (mult < 0.1) mult = 0.1;
	if (mult > 10.0) mult = 10.0;
	s_playbackSpeed = mult;
	return 0;
}

static int lua_trimrecording(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "trimrecording", 2, 2, n);
	}
	
	int startFrame = LuaCheckNonNegative(L, 1, "trimrecording", "startFrame");
	int endFrame = LuaCheckNonNegative(L, 2, "trimrecording", "endFrame");

	if (!s_inputRecording) {
		lua_pushboolean(L, 0);
		return 1;
	}
	if (startFrame < 0 || endFrame < 0 || startFrame > endFrame) {
		lua_pushboolean(L, 0);
		return 1;
	}

	int maxFrames = 0;
	for (int p = 0; p < 4; ++p) {
		if ((int)s_recordedInput[p].size() > maxFrames) {
			maxFrames = (int)s_recordedInput[p].size();
		}
	}
	if (startFrame >= maxFrames || endFrame >= maxFrames) {
		lua_pushboolean(L, 0);
		return 1;
	}

	for (int p = 0; p < 4; ++p) {
		if (!s_recordedInput[p].empty()) {
			std::vector<uint8> trimmed;
			trimmed.reserve(endFrame - startFrame + 1);
			for (int i = startFrame; i <= endFrame; ++i) {
				if (i < (int)s_recordedInput[p].size()) {
					trimmed.push_back(s_recordedInput[p][i]);
				} else {
					trimmed.push_back(0);
				}
			}
			s_recordedInput[p] = trimmed;
		}
	}

	std::map<std::string, int> newMarkers;
	for (std::map<std::string, int>::iterator it = s_recordingMarkers.begin();
	     it != s_recordingMarkers.end(); ++it) {
		int markerFrame = it->second;
		if (markerFrame >= startFrame && markerFrame <= endFrame) {
			newMarkers[it->first] = markerFrame - startFrame;
		}
	}
	s_recordingMarkers = newMarkers;

	lua_pushboolean(L, 1);
	return 1;
}

static int lua_savestate(lua_State *L)
{
	int slot = 0;
	if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
		slot = LuaCheckRange(L, 1, 0, 9, "savestate", "slot");
	}

	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);

	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "savestate(slot) failed: no game loaded");
	}

	size_t lastSlash = stateFilename.find_last_of("\\/");
	if (lastSlash != std::string::npos) {
		std::string stateDir = stateFilename.substr(0, lastSlash);
		for (size_t i = 0; i < stateDir.length(); i++) {
			if (stateDir[i] == '/') {
				stateDir[i] = '\\';
			}
		}
		if (!stateDir.empty() &&
		    (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
			stateDir = stateDir.substr(0, stateDir.length() - 1);
		}
		CreateDirectoryA(stateDir.c_str(), NULL);
	}

	extern void FCEUSS_Save(const char *fname);
	FCEUSS_Save(stateFilename.c_str());

	extern bool file_exists(const char * filename);
	bool success = false;
	if (file_exists(stateFilename.c_str())) {
		FILE *fp = fopen(stateFilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			fclose(fp);
			success = (size > 100);
			if (!success) {
				printf("savestate() failed: file exists but is too small (%ld bytes) at '%s'\n", size, stateFilename.c_str());
			}
		} else {
			printf("savestate() failed: cannot open file for verification at '%s'\n", stateFilename.c_str());
		}
	} else {
		printf("savestate() failed: file not created at '%s'\n", stateFilename.c_str());
	}

	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

static int lua_loadstate(lua_State *L)
{
	int slot = 0;
	if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
		slot = LuaCheckRange(L, 1, 0, 9, "loadstate", "slot");
	}

	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);

	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "loadstate(slot) failed: no game loaded");
	}

	extern bool file_exists(const char * filename);
	if (!file_exists(stateFilename.c_str())) {
		lua_pushboolean(L, 0);
		return 1;
	}

	extern bool FCEUSS_Load(const char *fname);
	bool success = FCEUSS_Load(stateFilename.c_str());
	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

static int lua_hasstate(lua_State *L)
{
	int slot = 0;
	if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
		slot = LuaCheckRange(L, 1, 0, 9, "hasstate", "slot");
	}

	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);

	extern bool file_exists(const char * filename);
	bool exists = file_exists(stateFilename.c_str());
	lua_pushboolean(L, exists ? 1 : 0);
	return 1;
}

static int lua_savestatefile(lua_State *L)
{
	if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
		return luaL_error(L, "savestatefile(filename) failed: filename is required");
	}

	const char* customFilename = LuaCheckPath(L, 1, "savestatefile");
	if (!customFilename || strlen(customFilename) == 0) {
		return luaL_error(L, "savestatefile(filename) failed: filename cannot be empty");
	}

	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "savestatefile(filename) failed: no game loaded");
	}

	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string tempStatePath = FCEU_MakeFName(1, 0, 0);
	size_t lastSlash = tempStatePath.find_last_of("\\/");
	std::string stateDir;
	if (lastSlash != std::string::npos) {
		stateDir = tempStatePath.substr(0, lastSlash);
	} else {
		stateDir = "game:\\states";
	}
	for (size_t i = 0; i < stateDir.length(); i++) {
		if (stateDir[i] == '/') {
			stateDir[i] = '\\';
		}
	}
	if (!stateDir.empty() &&
	    (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
		stateDir = stateDir.substr(0, stateDir.length() - 1);
	}
	CreateDirectoryA(stateDir.c_str(), NULL);

	std::string baseFilename = customFilename;
	size_t lastDot = baseFilename.find_last_of(".");
	bool hasExtension = (lastDot != std::string::npos && lastDot < baseFilename.length() - 1);
	if (!hasExtension) {
		baseFilename += ".fc0";
	}

	std::string fullPath = stateDir;
	if (!fullPath.empty() &&
	    fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
		fullPath += "\\";
	}
	fullPath += baseFilename;
	for (size_t i = 0; i < fullPath.length(); i++) {
		if (fullPath[i] == '/') {
			fullPath[i] = '\\';
		}
	}

	extern void FCEUSS_Save(const char *fname);
	FCEUSS_Save(fullPath.c_str());

	extern bool file_exists(const char * filename);
	bool success = false;
	if (file_exists(fullPath.c_str())) {
		FILE *fp = fopen(fullPath.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			fclose(fp);
			success = (size > 100);
			if (!success) {
				printf("savestatefile() failed: file exists but is too small (%ld bytes) at '%s'\n", size, fullPath.c_str());
			}
		} else {
			printf("savestatefile() failed: cannot open file for verification at '%s'\n", fullPath.c_str());
		}
	} else {
		printf("savestatefile() failed: file not created at '%s'\n", fullPath.c_str());
	}

	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

static int lua_loadstatefile(lua_State *L)
{
	if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
		return luaL_error(L, "loadstatefile(filename) failed: filename is required");
	}

	const char* customFilename = LuaCheckPath(L, 1, "loadstatefile");
	if (!customFilename || strlen(customFilename) == 0) {
		return luaL_error(L, "loadstatefile(filename) failed: filename cannot be empty");
	}

	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "loadstatefile(filename) failed: no game loaded");
	}

	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string tempStatePath = FCEU_MakeFName(1, 0, 0);
	size_t lastSlash = tempStatePath.find_last_of("\\/");
	std::string stateDir;
	if (lastSlash != std::string::npos) {
		stateDir = tempStatePath.substr(0, lastSlash);
	} else {
		stateDir = "game:\\states";
	}
	for (size_t i = 0; i < stateDir.length(); i++) {
		if (stateDir[i] == '/') {
			stateDir[i] = '\\';
		}
	}
	if (!stateDir.empty() &&
	    (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
		stateDir = stateDir.substr(0, stateDir.length() - 1);
	}

	std::string baseFilename = customFilename;
	size_t lastDot = baseFilename.find_last_of(".");
	bool hasExtension = (lastDot != std::string::npos && lastDot < baseFilename.length() - 1);
	if (!hasExtension) {
		baseFilename += ".fc0";
	}

	std::string fullPath = stateDir;
	if (!fullPath.empty() &&
	    fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
		fullPath += "\\";
	}
	fullPath += baseFilename;
	for (size_t i = 0; i < fullPath.length(); i++) {
		if (fullPath[i] == '/') {
			fullPath[i] = '\\';
		}
	}

	extern bool file_exists(const char * filename);
	if (!file_exists(fullPath.c_str())) {
		lua_pushboolean(L, 0);
		return 1;
	}

	extern bool FCEUSS_Load(const char *fname);
	bool success = FCEUSS_Load(fullPath.c_str());
	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

static const luaL_Reg kMovieFuncs[] = {
	{"startinputrecording", lua_startinputrecording},
	{"stopinputrecording", lua_stopinputrecording},
	{"playinputrecording", lua_playinputrecording},
	{"saveinputrecording", lua_saveinputrecording},
	{"loadinputrecording", lua_loadinputrecording},
	{"setrecordingmarker", lua_setrecordingmarker},
	{"jumptorecordingmarker", lua_jumptorecordingmarker},
	{"setplaybackspeed", lua_setplaybackspeed},
	{"trimrecording", lua_trimrecording},
	{"savestate", lua_savestate},
	{"loadstate", lua_loadstate},
	{"hasstate", lua_hasstate},
	{"savestatefile", lua_savestatefile},
	{"loadstatefile", lua_loadstatefile},
	{NULL, NULL}
};

void Lua_RegisterMovie(lua_State* L)
{
	if (!L) {
		return;
	}

	// Manually register each function (luaL_register with NULL has issues)
	for (const luaL_Reg* reg = kMovieFuncs; reg->name != NULL; reg++) {
		lua_register(L, reg->name, reg->func);
	}
}

#endif // USE_LUA


