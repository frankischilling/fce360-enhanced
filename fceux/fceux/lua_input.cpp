#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_input.h"
#include "fceulua.h"
#include "fceu.h"
#include "types.h"

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

// Extern joypad state from input.cpp
extern uint8 joy[4];

// Extern powerpadbuf from Cemulator.cpp (Xbox input buffer)
extern uint32 powerpadbuf;

// Forward declaration for gamepad input (defined in Cemulator.cpp)
extern GAMEPAD Gamepads[];

// Xbox 360 controller button definitions
#define XINPUT_GAMEPAD_DPAD_UP    0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN  0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT  0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_A          0x1000
#define XINPUT_GAMEPAD_B          0x2000
#define XINPUT_GAMEPAD_X          0x4000
#define XINPUT_GAMEPAD_Y          0x8000
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_LEFT_THUMB     0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB    0x0080
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 30

// --- Lua-forced joypad state (per player) ---
static uint8 s_luaJoypadValue[4]  = {0,0,0,0};   // full 8-bit NES mask
static uint8 s_luaJoypadMask[4]   = {0,0,0,0};   // which bits to force (0xFF = all)
static uint8 s_luaJoypadLatched[4]= {0,0,0,0};   // 1 if override active
static uint8 s_oneFramePress[4]   = {0,0,0,0};   // one-frame button presses (cleared after each frame)
static uint8 s_oneFrameRelease[4] = {0,0,0,0};   // one-frame button releases (cleared after each frame)
static uint8 s_hardwareJoypad[4] = {0,0,0,0};   // hardware input before override

struct ButtonCallbackInfo {
	WORD mask;
	std::string canonicalName;
	int luaRef;

	ButtonCallbackInfo() : mask(0), canonicalName(), luaRef(-1) {}
};

enum {
	BUTTON_INDEX_A = 0,
	BUTTON_INDEX_B,
	BUTTON_INDEX_X,
	BUTTON_INDEX_Y,
	BUTTON_INDEX_START,
	BUTTON_INDEX_BACK,
	BUTTON_INDEX_LEFT_SHOULDER,
	BUTTON_INDEX_RIGHT_SHOULDER,
	BUTTON_INDEX_LEFT_THUMB,
	BUTTON_INDEX_RIGHT_THUMB,
	BUTTON_INDEX_DPAD_UP,
	BUTTON_INDEX_DPAD_DOWN,
	BUTTON_INDEX_DPAD_LEFT,
	BUTTON_INDEX_DPAD_RIGHT,
	BUTTON_INDEX_COUNT,

	ANALOG_INDEX_LS_UP = BUTTON_INDEX_COUNT,
	ANALOG_INDEX_LS_DOWN,
	ANALOG_INDEX_LS_LEFT,
	ANALOG_INDEX_LS_RIGHT,
	ANALOG_INDEX_RS_UP,
	ANALOG_INDEX_RS_DOWN,
	ANALOG_INDEX_RS_LEFT,
	ANALOG_INDEX_RS_RIGHT,
	TRIGGER_INDEX_LT,
	TRIGGER_INDEX_RT,
	ANALOG_INDEX_COUNT,

	TOTAL_HOLD_INDEX_COUNT = ANALOG_INDEX_COUNT
};

static std::map<WORD, ButtonCallbackInfo> s_buttonPressCallbacks;
static std::map<WORD, ButtonCallbackInfo> s_buttonReleaseCallbacks;
static WORD s_prevXboxButtonState[4] = {0};
static DWORD s_buttonHoldStart[4][TOTAL_HOLD_INDEX_COUNT] = {0};
static bool s_buttonWasHeld[4][TOTAL_HOLD_INDEX_COUNT] = {false};
static const uint8 s_nesButtonMask[8] = {
	0x01, // A
	0x02, // B
	0x04, // Select
	0x08, // Start
	0x10, // Up
	0x20, // Down
	0x40, // Left
	0x80  // Right
};
static DWORD s_nesButtonHoldStart[4][8] = {0};
static bool s_nesButtonWasHeld[4][8] = {false};
static std::map<std::string, WORD> s_buttonNameToMask;
static const WORD s_buttonIndexToMask[BUTTON_INDEX_COUNT] = {
	XINPUT_GAMEPAD_A,
	XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X,
	XINPUT_GAMEPAD_Y,
	XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_BACK,
	XINPUT_GAMEPAD_LEFT_SHOULDER,
	XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_LEFT_THUMB,
	XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_RIGHT
};
static const float ANALOG_HOLD_THRESHOLD = 0.4f;

// Rumble state tracking
struct RumbleState {
	DWORD startTime;      // When rumble started (GetTickCount())
	DWORD duration;       // Duration in milliseconds
	float intensity;      // Intensity (0.0-1.0)
	bool active;          // Whether rumble is currently active
};
static RumbleState s_rumbleState[4] = {0};  // One per player (0-3)

// Virtual input mapping - per-script input remapping
static std::map<lua_State*, std::map<std::string, std::string> > s_virtualInputMappings;

// Helper functions
static int GetButtonIndexFromMask(WORD mask)
{
	for (int i = 0; i < BUTTON_INDEX_COUNT; ++i) {
		if (s_buttonIndexToMask[i] == mask) {
			return i;
		}
	}
	return -1;
}

static void ToUpperButtonName(const char* src, char* dest, size_t destSize)
{
	size_t i = 0;
	for (; src[i] && i < destSize - 1; ++i) {
		char c = src[i];
		if (c >= 'a' && c <= 'z') {
			c = c - 'a' + 'A';
		} else if (c == '-' || c == ' ') {
			c = '_';
		}
		dest[i] = c;
	}
	dest[i] = '\0';
}

static int GetAnalogDirectionIndex(const char* upperName)
{
	if (strcmp(upperName, "LS_UP") == 0 || strcmp(upperName, "LEFT_STICK_UP") == 0) return ANALOG_INDEX_LS_UP;
	if (strcmp(upperName, "LS_DOWN") == 0 || strcmp(upperName, "LEFT_STICK_DOWN") == 0) return ANALOG_INDEX_LS_DOWN;
	if (strcmp(upperName, "LS_LEFT") == 0 || strcmp(upperName, "LEFT_STICK_LEFT") == 0) return ANALOG_INDEX_LS_LEFT;
	if (strcmp(upperName, "LS_RIGHT") == 0 || strcmp(upperName, "LEFT_STICK_RIGHT") == 0) return ANALOG_INDEX_LS_RIGHT;
	if (strcmp(upperName, "RS_UP") == 0 || strcmp(upperName, "RIGHT_STICK_UP") == 0) return ANALOG_INDEX_RS_UP;
	if (strcmp(upperName, "RS_DOWN") == 0 || strcmp(upperName, "RIGHT_STICK_DOWN") == 0) return ANALOG_INDEX_RS_DOWN;
	if (strcmp(upperName, "RS_LEFT") == 0 || strcmp(upperName, "RIGHT_STICK_LEFT") == 0) return ANALOG_INDEX_RS_LEFT;
	if (strcmp(upperName, "RS_RIGHT") == 0 || strcmp(upperName, "RIGHT_STICK_RIGHT") == 0) return ANALOG_INDEX_RS_RIGHT;
	if (strcmp(upperName, "LT") == 0 || strcmp(upperName, "LEFT_TRIGGER") == 0) return TRIGGER_INDEX_LT;
	if (strcmp(upperName, "RT") == 0 || strcmp(upperName, "RIGHT_TRIGGER") == 0) return TRIGGER_INDEX_RT;
	return -1;
}

static int GetNESButtonIndex(const char* upperName)
{
	if (strcmp(upperName, "NES_A") == 0 || strcmp(upperName, "NES-BUTTON_A") == 0) return 0;
	if (strcmp(upperName, "NES_B") == 0 || strcmp(upperName, "NES-BUTTON_B") == 0) return 1;
	if (strcmp(upperName, "NES_SELECT") == 0) return 2;
	if (strcmp(upperName, "NES_START") == 0) return 3;
	if (strcmp(upperName, "NES_UP") == 0) return 4;
	if (strcmp(upperName, "NES_DOWN") == 0) return 5;
	if (strcmp(upperName, "NES_LEFT") == 0) return 6;
	if (strcmp(upperName, "NES_RIGHT") == 0) return 7;
	return -1;
}

static bool IsAnalogDirectionActive(int player, int analogIndex)
{
	if (player < 0 || player >= 4) return false;
	switch (analogIndex) {
		case ANALOG_INDEX_LS_UP:    return Gamepads[player].fY1 >  ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_LS_DOWN:  return Gamepads[player].fY1 < -ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_LS_LEFT:  return Gamepads[player].fX1 < -ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_LS_RIGHT: return Gamepads[player].fX1 >  ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_RS_UP:    return Gamepads[player].fY2 >  ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_RS_DOWN:  return Gamepads[player].fY2 < -ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_RS_LEFT:  return Gamepads[player].fX2 < -ANALOG_HOLD_THRESHOLD;
		case ANALOG_INDEX_RS_RIGHT: return Gamepads[player].fX2 >  ANALOG_HOLD_THRESHOLD;
		case TRIGGER_INDEX_LT:      return Gamepads[player].bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		case TRIGGER_INDEX_RT:      return Gamepads[player].bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		default: return false;
	}
}

static void ResetHoldStatesForPlayer(int player)
{
	for (int idx = 0; idx < TOTAL_HOLD_INDEX_COUNT; ++idx) {
		s_buttonWasHeld[player][idx] = false;
		s_buttonHoldStart[player][idx] = 0;
	}
	for (int idx = 0; idx < 8; ++idx) {
		s_nesButtonWasHeld[player][idx] = false;
		s_nesButtonHoldStart[player][idx] = 0;
	}
}

static void UpdateHoldStatesForPlayer(int player, WORD currentButtons, DWORD now)
{
	for (int idx = 0; idx < BUTTON_INDEX_COUNT; ++idx) {
		WORD mask = s_buttonIndexToMask[idx];
		bool pressedNow = (currentButtons & mask) != 0;
		if (pressedNow) {
			if (!s_buttonWasHeld[player][idx]) {
				s_buttonWasHeld[player][idx] = true;
				s_buttonHoldStart[player][idx] = now;
			}
		} else {
			s_buttonWasHeld[player][idx] = false;
			s_buttonHoldStart[player][idx] = 0;
		}
	}

	for (int idx = BUTTON_INDEX_COUNT; idx < TOTAL_HOLD_INDEX_COUNT; ++idx) {
		bool active = IsAnalogDirectionActive(player, idx);
		if (active) {
			if (!s_buttonWasHeld[player][idx]) {
				s_buttonWasHeld[player][idx] = true;
				s_buttonHoldStart[player][idx] = now;
			}
		} else {
			s_buttonWasHeld[player][idx] = false;
			s_buttonHoldStart[player][idx] = 0;
		}
	}

	uint8 nesHardware = s_hardwareJoypad[player];
	for (int idx = 0; idx < 8; ++idx) {
		bool pressed = (nesHardware & s_nesButtonMask[idx]) != 0;
		if (pressed) {
			if (!s_nesButtonWasHeld[player][idx]) {
				s_nesButtonWasHeld[player][idx] = true;
				s_nesButtonHoldStart[player][idx] = now;
			}
		} else {
			s_nesButtonWasHeld[player][idx] = false;
			s_nesButtonHoldStart[player][idx] = 0;
		}
	}
}

static void EnsureButtonNameMap()
{
	if (!s_buttonNameToMask.empty()) return;
	s_buttonNameToMask["A"] = XINPUT_GAMEPAD_A;
	s_buttonNameToMask["B"] = XINPUT_GAMEPAD_B;
	s_buttonNameToMask["X"] = XINPUT_GAMEPAD_X;
	s_buttonNameToMask["Y"] = XINPUT_GAMEPAD_Y;
	s_buttonNameToMask["START"] = XINPUT_GAMEPAD_START;
	s_buttonNameToMask["BACK"] = XINPUT_GAMEPAD_BACK;
	s_buttonNameToMask["LEFT_SHOULDER"] = XINPUT_GAMEPAD_LEFT_SHOULDER;
	s_buttonNameToMask["LEFTSHOULDER"] = XINPUT_GAMEPAD_LEFT_SHOULDER;
	s_buttonNameToMask["LB"] = XINPUT_GAMEPAD_LEFT_SHOULDER;
	s_buttonNameToMask["RIGHT_SHOULDER"] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	s_buttonNameToMask["RIGHTSHOULDER"] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	s_buttonNameToMask["RB"] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	s_buttonNameToMask["LEFT_THUMB"] = XINPUT_GAMEPAD_LEFT_THUMB;
	s_buttonNameToMask["LEFTTHUMB"] = XINPUT_GAMEPAD_LEFT_THUMB;
	s_buttonNameToMask["LS"] = XINPUT_GAMEPAD_LEFT_THUMB;
	s_buttonNameToMask["RIGHT_THUMB"] = XINPUT_GAMEPAD_RIGHT_THUMB;
	s_buttonNameToMask["RIGHTTHUMB"] = XINPUT_GAMEPAD_RIGHT_THUMB;
	s_buttonNameToMask["RS"] = XINPUT_GAMEPAD_RIGHT_THUMB;
	s_buttonNameToMask["DPAD_UP"] = XINPUT_GAMEPAD_DPAD_UP;
	s_buttonNameToMask["UP"] = XINPUT_GAMEPAD_DPAD_UP;
	s_buttonNameToMask["DPAD_DOWN"] = XINPUT_GAMEPAD_DPAD_DOWN;
	s_buttonNameToMask["DOWN"] = XINPUT_GAMEPAD_DPAD_DOWN;
	s_buttonNameToMask["DPAD_LEFT"] = XINPUT_GAMEPAD_DPAD_LEFT;
	s_buttonNameToMask["LEFT"] = XINPUT_GAMEPAD_DPAD_LEFT;
	s_buttonNameToMask["DPAD_RIGHT"] = XINPUT_GAMEPAD_DPAD_RIGHT;
	s_buttonNameToMask["RIGHT"] = XINPUT_GAMEPAD_DPAD_RIGHT;
}

static bool MapXboxButtonName(const char* buttonName, WORD& buttonMask, const char*& canonicalName)
{
	if (!buttonName || !buttonName[0]) {
		return false;
	}

	EnsureButtonNameMap();

	char upperButton[32];
	ToUpperButtonName(buttonName, upperButton, sizeof(upperButton));

	std::map<std::string, WORD>::const_iterator it = s_buttonNameToMask.find(upperButton);
	if (it == s_buttonNameToMask.end()) {
		return false;
	}

	buttonMask = it->second;
	switch (buttonMask) {
		case XINPUT_GAMEPAD_A: canonicalName = "A"; break;
		case XINPUT_GAMEPAD_B: canonicalName = "B"; break;
		case XINPUT_GAMEPAD_X: canonicalName = "X"; break;
		case XINPUT_GAMEPAD_Y: canonicalName = "Y"; break;
		case XINPUT_GAMEPAD_START: canonicalName = "START"; break;
		case XINPUT_GAMEPAD_BACK: canonicalName = "BACK"; break;
		case XINPUT_GAMEPAD_LEFT_SHOULDER: canonicalName = "LEFT_SHOULDER"; break;
		case XINPUT_GAMEPAD_RIGHT_SHOULDER: canonicalName = "RIGHT_SHOULDER"; break;
		case XINPUT_GAMEPAD_LEFT_THUMB: canonicalName = "LEFT_THUMB"; break;
		case XINPUT_GAMEPAD_RIGHT_THUMB: canonicalName = "RIGHT_THUMB"; break;
		case XINPUT_GAMEPAD_DPAD_UP: canonicalName = "DPAD_UP"; break;
		case XINPUT_GAMEPAD_DPAD_DOWN: canonicalName = "DPAD_DOWN"; break;
		case XINPUT_GAMEPAD_DPAD_LEFT: canonicalName = "DPAD_LEFT"; break;
		case XINPUT_GAMEPAD_DPAD_RIGHT: canonicalName = "DPAD_RIGHT"; break;
		default:
			canonicalName = upperButton;
			break;
	}

	return true;
}

static void TriggerButtonCallback(const ButtonCallbackInfo& info, int player)
{
	extern lua_State* luaState;
	extern int s_luaDisabled;
	extern int luaInitialized;
	extern void LuaConsolePushLine(const char* msg);

	if (info.luaRef < 0) {
		return;
	}

	if (s_luaDisabled || !luaInitialized || luaState == NULL) {
		return;
	}

	lua_rawgeti(luaState, LUA_REGISTRYINDEX, info.luaRef);
	lua_pushinteger(luaState, player);
	lua_pushstring(luaState, info.canonicalName.c_str());
	if (lua_pcall(luaState, 2, 0, 0) != 0) {
		const char* err = lua_tostring(luaState, -1);
		printf("LUA ERROR (button callback): %s\n", err ? err : "unknown error");
		if (err && err[0]) {
			LuaConsolePushLine(err);
		}
		lua_pop(luaState, 1);
	}
}

// Helper function to resolve virtual button name to physical spec
static const char* ResolveVirtualButton(lua_State* L, const char* virtualBtn)
{
	if (!L || !virtualBtn || !virtualBtn[0]) {
		return NULL;
	}
	
	// Check if this Lua state has virtual mappings
	std::map<lua_State*, std::map<std::string, std::string> >::iterator stateIt = s_virtualInputMappings.find(L);
	if (stateIt == s_virtualInputMappings.end()) {
		return NULL;
	}
	
	// Convert virtual button name to uppercase for case-insensitive lookup
	char upperVirtual[64];
	int i = 0;
	for (; virtualBtn[i] && i < 63; ++i) {
		char c = virtualBtn[i];
		if (c >= 'a' && c <= 'z') {
			upperVirtual[i] = c - 'a' + 'A';
		} else {
			upperVirtual[i] = c;
		}
	}
	upperVirtual[i] = '\0';
	
	// Look up the mapping
	std::map<std::string, std::string>& mappings = stateIt->second;
	std::map<std::string, std::string>::const_iterator it = mappings.find(upperVirtual);
	if (it != mappings.end()) {
		return it->second.c_str();
	}
	
	return NULL;
}

// ============================================================================
// Lua Input Functions
// ============================================================================

// getjoypad(player) -> integer bitmask
static int lua_getjoypad(lua_State* L)
{
	int player = (int)luaL_checkinteger(L, 1);
	
	// Validate player number (0-3)
	if (player < 0 || player > 3) {
		return luaL_error(L, "getjoypad: player must be 0-3 (0=Player 1, 1=Player 2, 2=Player 3, 3=Player 4)");
	}
	
	// Return current button state for the specified player
	lua_pushinteger(L, (int)joy[player]);
	return 1;
}

// gethardwarejoypad(player) -> integer bitmask
static int lua_gethardwarejoypad(lua_State* L)
{
	int player = (int)luaL_checkinteger(L, 1);
	
	// Validate player number (0-3)
	if (player < 0 || player > 3) {
		return luaL_error(L, "gethardwarejoypad: player must be 0-3 (0=Player 1, 1=Player 2, 2=Player 3, 3=Player 4)");
	}
	
	// Return hardware button state (before Lua override)
	lua_pushinteger(L, (int)s_hardwareJoypad[player]);
	return 1;
}

// isxboxbuttonpressed(player, button) -> boolean
static int lua_isxboxbuttonpressed(lua_State* L)
{
	int player = (int)luaL_checkinteger(L, 1);
	const char* buttonName = luaL_checkstring(L, 2);
	
	// Validate player number (0-3)
	if (player < 0 || player > 3) {
		return luaL_error(L, "isxboxbuttonpressed: player must be 0-3 (0=Player 1, 1=Player 2, 2=Player 3, 3=Player 4)");
	}
	
	if (!buttonName || !buttonName[0]) {
		return luaL_error(L, "isxboxbuttonpressed: button name cannot be empty");
	}
	
	// Get current Xbox 360 controller button state
	WORD buttons = Gamepads[player].wButtons;
	
	WORD buttonMask = 0;
	const char* canonicalName = NULL;
	if (!MapXboxButtonName(buttonName, buttonMask, canonicalName)) {
		return luaL_error(L, "isxboxbuttonpressed: invalid button name '%s'. Valid buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", buttonName);
	}
	
	// Check if button is pressed (bit is set)
	bool isPressed = (buttons & buttonMask) != 0;
	lua_pushboolean(L, isPressed ? 1 : 0);
	return 1;
}

static int lua_onbuttonpress(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "onbuttonpress(btn, cb) requires 2 arguments");
	}

	const char* buttonName = luaL_checkstring(L, 1);
	WORD buttonMask = 0;
	const char* canonicalName = NULL;
	if (!MapXboxButtonName(buttonName, buttonMask, canonicalName)) {
		return luaL_error(L, "onbuttonpress: invalid button name '%s'. Valid buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", buttonName);
	}

	if (lua_isnil(L, 2)) {
		std::map<WORD, ButtonCallbackInfo>::iterator it = s_buttonPressCallbacks.find(buttonMask);
		if (it != s_buttonPressCallbacks.end()) {
			if (it->second.luaRef >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, it->second.luaRef);
			}
			s_buttonPressCallbacks.erase(it);
		}
		return 0;
	}

	if (!lua_isfunction(L, 2)) {
		return luaL_error(L, "onbuttonpress: callback must be a function or nil");
	}

	ButtonCallbackInfo& info = s_buttonPressCallbacks[buttonMask];
	if (info.luaRef >= 0) {
		luaL_unref(L, LUA_REGISTRYINDEX, info.luaRef);
	}

	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	info.mask = buttonMask;
	info.canonicalName = canonicalName;
	info.luaRef = ref;

	// Initialize previous state to avoid false triggers from currently held buttons
	for (int p = 0; p < 4; ++p) {
		s_prevXboxButtonState[p] = Gamepads[p].wButtons;
	}

	return 0;
}

static int lua_onbuttonrelease(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "onbuttonrelease(btn, cb) requires 2 arguments");
	}

	const char* buttonName = luaL_checkstring(L, 1);
	WORD buttonMask = 0;
	const char* canonicalName = NULL;
	if (!MapXboxButtonName(buttonName, buttonMask, canonicalName)) {
		return luaL_error(L, "onbuttonrelease: invalid button name '%s'. Valid buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", buttonName);
	}

	if (lua_isnil(L, 2)) {
		std::map<WORD, ButtonCallbackInfo>::iterator it = s_buttonReleaseCallbacks.find(buttonMask);
		if (it != s_buttonReleaseCallbacks.end()) {
			if (it->second.luaRef >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, it->second.luaRef);
			}
			s_buttonReleaseCallbacks.erase(it);
		}
		return 0;
	}

	if (!lua_isfunction(L, 2)) {
		return luaL_error(L, "onbuttonrelease: callback must be a function or nil");
	}

	ButtonCallbackInfo& info = s_buttonReleaseCallbacks[buttonMask];
	if (info.luaRef >= 0) {
		luaL_unref(L, LUA_REGISTRYINDEX, info.luaRef);
	}

	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	info.mask = buttonMask;
	info.canonicalName = canonicalName;
	info.luaRef = ref;

	// Initialize previous state to avoid false triggers from currently held buttons
	for (int p = 0; p < 4; ++p) {
		s_prevXboxButtonState[p] = Gamepads[p].wButtons;
	}

	return 0;
}

static int lua_getbuttonheldms(lua_State* L)
{
	if (lua_gettop(L) < 1) {
		return luaL_error(L, "getbuttonheldms(btn) requires 1 argument");
	}

	const char* buttonName = luaL_checkstring(L, 1);
	WORD buttonMask = 0;
	const char* canonicalName = NULL;
	char upperButton[32];
	ToUpperButtonName(buttonName, upperButton, sizeof(upperButton));

	int analogIdx = GetAnalogDirectionIndex(upperButton);
	int nesIdx = GetNESButtonIndex(upperButton);
	int idx = -1;
	DWORD currentTime = GetTickCount();
	DWORD heldMs = 0;

	if (nesIdx >= 0) {
		for (int p = 0; p < 4; ++p) {
			if (s_nesButtonWasHeld[p][nesIdx] && s_nesButtonHoldStart[p][nesIdx] != 0) {
				DWORD start = s_nesButtonHoldStart[p][nesIdx];
				DWORD duration = (start <= currentTime) ? (currentTime - start) : 0;
				if (duration > heldMs) {
					heldMs = duration;
				}
			}
		}
		lua_pushnumber(L, (lua_Number)heldMs);
		return 1;
	}

	if (analogIdx >= 0) {
		idx = analogIdx;
	} else {
		if (!MapXboxButtonName(buttonName, buttonMask, canonicalName)) {
			return luaL_error(L, "getbuttonheldms: invalid button name '%s'. Valid buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT, LS/RS directions, LT/RT triggers, NES_A/B/SELECT/START/UP/DOWN/LEFT/RIGHT", buttonName);
		}
		idx = GetButtonIndexFromMask(buttonMask);
		if (idx < 0) {
			lua_pushnumber(L, 0);
			return 1;
		}
	}

	for (int p = 0; p < 4; ++p) {
		if (s_buttonWasHeld[p][idx] && s_buttonHoldStart[p][idx] != 0) {
			DWORD start = s_buttonHoldStart[p][idx];
			if (start <= currentTime) {
				DWORD duration = currentTime - start;
				if (duration > heldMs) {
					heldMs = duration;
				}
			}
		}
	}

	lua_pushnumber(L, (lua_Number)heldMs);
	return 1;
}

static int lua_isbuttonpressed(lua_State* L)
{
	int player = (int)luaL_checkinteger(L, 1);
	const char* buttonName = luaL_checkstring(L, 2);
	
	// Validate player number (0-3)
	if (player < 0 || player > 3) {
		return luaL_error(L, "isbuttonpressed: player must be 0-3 (0=Player 1, 1=Player 2, 2=Player 3, 3=Player 4)");
	}
	
	if (!buttonName || !buttonName[0]) {
		return luaL_error(L, "isbuttonpressed: button name cannot be empty");
	}
	
	// Check if this is a virtual button name (mapped via mapinput)
	const char* physicalSpec = ResolveVirtualButton(L, buttonName);
	if (physicalSpec) {
		// Resolve virtual button to physical spec and check that instead
		buttonName = physicalSpec;
	}
	
	// Check if this is an Xbox button (check Xbox button mapping first)
	EnsureButtonNameMap();
	WORD xboxMask = 0;
	const char* dummyCanonical = NULL;
	bool isXboxButton = MapXboxButtonName(buttonName, xboxMask, dummyCanonical);
	
	if (isXboxButton) {
		// Check Xbox 360 controller button state
		WORD buttons = Gamepads[player].wButtons;
		bool isPressed = (buttons & xboxMask) != 0;
		lua_pushboolean(L, isPressed ? 1 : 0);
		return 1;
	}
	
	// Otherwise, check NES button state
	uint8 buttons = joy[player];
	
	// Map button name to bitmask (case-insensitive)
	uint8 buttonMask = 0;
	
	// Convert to uppercase for case-insensitive comparison
	char upperButton[16];
	int i = 0;
	for (; buttonName[i] && i < 15; ++i) {
		char c = buttonName[i];
		if (c >= 'a' && c <= 'z') {
			upperButton[i] = c - 'a' + 'A';
		} else {
			upperButton[i] = c;
		}
	}
	upperButton[i] = '\0';
	
	// Map button name to bitmask
	if (strcmp(upperButton, "A") == 0) {
		buttonMask = 0x01;
	} else if (strcmp(upperButton, "B") == 0) {
		buttonMask = 0x02;
	} else if (strcmp(upperButton, "SELECT") == 0) {
		buttonMask = 0x04;
	} else if (strcmp(upperButton, "START") == 0) {
		buttonMask = 0x08;
	} else if (strcmp(upperButton, "UP") == 0) {
		buttonMask = 0x10;
	} else if (strcmp(upperButton, "DOWN") == 0) {
		buttonMask = 0x20;
	} else if (strcmp(upperButton, "LEFT") == 0) {
		buttonMask = 0x40;
	} else if (strcmp(upperButton, "RIGHT") == 0) {
		buttonMask = 0x80;
	} else {
		return luaL_error(L, "isbuttonpressed: invalid button name '%s'. Valid NES buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT. Valid Xbox buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", buttonName);
	}
	
	// Check if button is pressed (bit is set)
	bool isPressed = (buttons & buttonMask) != 0;
	lua_pushboolean(L, isPressed ? 1 : 0);
	return 1;
}

static int lua_getbuttonname(lua_State* L)
{
	int buttonMask = (int)luaL_checkinteger(L, 1);
	
	// Validate bitmask range
	if (buttonMask < 0 || buttonMask > 0xFF) {
		return luaL_error(L, "getbuttonname: buttonMask must be in range 0x00-0xFF");
	}
	
	// Build comma-separated list of button names
	char result[128];
	result[0] = '\0';
	int first = 1;
	
	if (buttonMask & 0x01) {
		if (!first) strcat(result, ", ");
		strcat(result, "A");
		first = 0;
	}
	if (buttonMask & 0x02) {
		if (!first) strcat(result, ", ");
		strcat(result, "B");
		first = 0;
	}
	if (buttonMask & 0x04) {
		if (!first) strcat(result, ", ");
		strcat(result, "SELECT");
		first = 0;
	}
	if (buttonMask & 0x08) {
		if (!first) strcat(result, ", ");
		strcat(result, "START");
		first = 0;
	}
	if (buttonMask & 0x10) {
		if (!first) strcat(result, ", ");
		strcat(result, "UP");
		first = 0;
	}
	if (buttonMask & 0x20) {
		if (!first) strcat(result, ", ");
		strcat(result, "DOWN");
		first = 0;
	}
	if (buttonMask & 0x40) {
		if (!first) strcat(result, ", ");
		strcat(result, "LEFT");
		first = 0;
	}
	if (buttonMask & 0x80) {
		if (!first) strcat(result, ", ");
		strcat(result, "RIGHT");
		first = 0;
	}
	
	lua_pushstring(L, result);
	return 1;
}

static int lua_getbuttonmask(lua_State* L)
{
	const char* buttonName = luaL_checkstring(L, 1);
	if (!buttonName || !buttonName[0]) {
		return luaL_error(L, "getbuttonmask: button name cannot be empty");
	}
	
	// Convert to uppercase for case-insensitive comparison
	char upperButton[16];
	int i = 0;
	for (; buttonName[i] && i < 15; ++i) {
		char c = buttonName[i];
		if (c >= 'a' && c <= 'z') {
			upperButton[i] = c - 'a' + 'A';
		} else {
			upperButton[i] = c;
		}
	}
	upperButton[i] = '\0';
	
	// Map button name to bitmask
	uint8 buttonMask = 0;
	if (strcmp(upperButton, "A") == 0) {
		buttonMask = 0x01;
	} else if (strcmp(upperButton, "B") == 0) {
		buttonMask = 0x02;
	} else if (strcmp(upperButton, "SELECT") == 0) {
		buttonMask = 0x04;
	} else if (strcmp(upperButton, "START") == 0) {
		buttonMask = 0x08;
	} else if (strcmp(upperButton, "UP") == 0) {
		buttonMask = 0x10;
	} else if (strcmp(upperButton, "DOWN") == 0) {
		buttonMask = 0x20;
	} else if (strcmp(upperButton, "LEFT") == 0) {
		buttonMask = 0x40;
	} else if (strcmp(upperButton, "RIGHT") == 0) {
		buttonMask = 0x80;
	} else {
		return luaL_error(L, "getbuttonmask: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	}
	
	lua_pushinteger(L, (int)buttonMask);
	return 1;
}

static int lua_setjoypad(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "setjoypad(player, buttons) requires 2 arguments");
	}
	
	int player  = (int)luaL_checkinteger(L, 1);
	int buttons = (int)luaL_checkinteger(L, 2);
	
	if (player < 0 || player > 3) {
		return luaL_error(L, "setjoypad: player must be 0..3");
	}
	
	if (buttons < 0)     buttons = 0;
	if (buttons > 0xFF)  buttons &= 0xFF;
	
	s_luaJoypadValue[player]   = (uint8)buttons;
	s_luaJoypadMask[player]    = 0xFF;        // force all buttons by default
	s_luaJoypadLatched[player] = 1;
	
	// Apply immediately so scripts see effect right away
	joy[player] = (uint8)((joy[player] & ~s_luaJoypadMask[player]) |
						   (s_luaJoypadValue[player] & s_luaJoypadMask[player]));
	
	return 0;
}

static int lua_clearjoypad(lua_State* L)
{
	int player = (int)luaL_checkinteger(L, 1);
	
	if (player < -1 || player > 3) {
		return luaL_error(L, "clearjoypad: player must be -1 (all) or 0..3");
	}
	
	if (player == -1) {
		for (int p = 0; p < 4; ++p) {
			s_luaJoypadMask[p] = 0;
			s_luaJoypadLatched[p] = 0;
		}
	} else if (player >= 0 && player < 4) {
		s_luaJoypadMask[player] = 0;
		s_luaJoypadLatched[player] = 0;
	}
	
	return 0;
}

static int lua_pressbutton(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "pressbutton(player, button) requires 2 arguments");
	}
	
	int player = (int)luaL_checkinteger(L, 1);
	const char* buttonName = luaL_checkstring(L, 2);
	
	if (player < 0 || player > 3) {
		return luaL_error(L, "pressbutton: player must be 0..3");
	}
	
	if (!buttonName || !buttonName[0]) {
		return luaL_error(L, "pressbutton: button name cannot be empty");
	}
	
	// Get button mask using the same logic as lua_isbuttonpressed (case-insensitive)
	uint8 buttonMask = 0;
	
	// Convert to uppercase for case-insensitive comparison
	char upperButton[16];
	int i = 0;
	for (; buttonName[i] && i < 15; ++i) {
		char c = buttonName[i];
		if (c >= 'a' && c <= 'z') {
			upperButton[i] = c - 'a' + 'A';
		} else {
			upperButton[i] = c;
		}
	}
	upperButton[i] = '\0';
	
	// Map button name to bitmask
	if (strcmp(upperButton, "A") == 0) {
		buttonMask = 0x01;
	} else if (strcmp(upperButton, "B") == 0) {
		buttonMask = 0x02;
	} else if (strcmp(upperButton, "SELECT") == 0) {
		buttonMask = 0x04;
	} else if (strcmp(upperButton, "START") == 0) {
		buttonMask = 0x08;
	} else if (strcmp(upperButton, "UP") == 0) {
		buttonMask = 0x10;
	} else if (strcmp(upperButton, "DOWN") == 0) {
		buttonMask = 0x20;
	} else if (strcmp(upperButton, "LEFT") == 0) {
		buttonMask = 0x40;
	} else if (strcmp(upperButton, "RIGHT") == 0) {
		buttonMask = 0x80;
	} else {
		return luaL_error(L, "pressbutton: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	}
	
	// Set the button bit for one-frame press
	s_oneFramePress[player] |= (uint8)buttonMask;
	
	return 0;
}

static int lua_releasebutton(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "releasebutton(player, button) requires 2 arguments");
	}
	
	int player = (int)luaL_checkinteger(L, 1);
	const char* buttonName = luaL_checkstring(L, 2);
	
	if (player < 0 || player > 3) {
		return luaL_error(L, "releasebutton: player must be 0..3");
	}
	
	if (!buttonName || !buttonName[0]) {
		return luaL_error(L, "releasebutton: button name cannot be empty");
	}
	
	// Get button mask using the same logic as lua_isbuttonpressed (case-insensitive)
	uint8 buttonMask = 0;
	
	// Convert to uppercase for case-insensitive comparison
	char upperButton[16];
	int i = 0;
	for (; buttonName[i] && i < 15; ++i) {
		char c = buttonName[i];
		if (c >= 'a' && c <= 'z') {
			upperButton[i] = c - 'a' + 'A';
		} else {
			upperButton[i] = c;
		}
	}
	upperButton[i] = '\0';
	
	// Map button name to bitmask
	if (strcmp(upperButton, "A") == 0) {
		buttonMask = 0x01;
	} else if (strcmp(upperButton, "B") == 0) {
		buttonMask = 0x02;
	} else if (strcmp(upperButton, "SELECT") == 0) {
		buttonMask = 0x04;
	} else if (strcmp(upperButton, "START") == 0) {
		buttonMask = 0x08;
	} else if (strcmp(upperButton, "UP") == 0) {
		buttonMask = 0x10;
	} else if (strcmp(upperButton, "DOWN") == 0) {
		buttonMask = 0x20;
	} else if (strcmp(upperButton, "LEFT") == 0) {
		buttonMask = 0x40;
	} else if (strcmp(upperButton, "RIGHT") == 0) {
		buttonMask = 0x80;
	} else {
		return luaL_error(L, "releasebutton: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	}
	
	// Set the button bit for one-frame release
	s_oneFrameRelease[player] |= (uint8)buttonMask;
	
	return 0;
}

static int lua_setrumble(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "setrumble(ms, intensity) requires 2 arguments");
	}
	
	int ms = (int)luaL_checkinteger(L, 1);
	double intensity = luaL_checknumber(L, 2);
	
	// Validate parameters
	if (ms < 0) {
		return luaL_error(L, "setrumble: duration (ms) must be >= 0");
	}
	if (intensity < 0.0) intensity = 0.0;
	if (intensity > 1.0) intensity = 1.0;
	
	// Default to player 0 (first controller) as per spec
	int player = 0;
	
	// Set rumble state
	DWORD currentTime = GetTickCount();
	s_rumbleState[player].startTime = currentTime;
	s_rumbleState[player].duration = (DWORD)ms;
	s_rumbleState[player].intensity = (float)intensity;
	s_rumbleState[player].active = true;
	
	// Apply rumble immediately
	if (Gamepads[player].bConnected) {
		XINPUT_VIBRATION vibration;
		// Convert intensity (0.0-1.0) to motor speed (0-65535)
		WORD motorSpeed = (WORD)(intensity * 65535.0);
		vibration.wLeftMotorSpeed = motorSpeed;
		vibration.wRightMotorSpeed = motorSpeed;
		XInputSetState(player, &vibration);
	}
	
	return 0;
}

static int lua_mapinput(lua_State* L)
{
	if (lua_gettop(L) < 2) {
		return luaL_error(L, "mapinput(virtualBtn, physicalSpec) requires 2 arguments");
	}
	
	const char* virtualBtn = luaL_checkstring(L, 1);
	const char* physicalSpec = luaL_checkstring(L, 2);
	
	if (!virtualBtn || !virtualBtn[0]) {
		return luaL_error(L, "mapinput: virtual button name cannot be empty");
	}
	
	if (!physicalSpec || !physicalSpec[0]) {
		return luaL_error(L, "mapinput: physical spec cannot be empty");
	}
	
	// Convert virtual button name to uppercase for case-insensitive storage
	char upperVirtual[64];
	int i = 0;
	for (; virtualBtn[i] && i < 63; ++i) {
		char c = virtualBtn[i];
		if (c >= 'a' && c <= 'z') {
			upperVirtual[i] = c - 'a' + 'A';
		} else {
			upperVirtual[i] = c;
		}
	}
	upperVirtual[i] = '\0';
	
	// Validate that physicalSpec is a valid button name
	char upperSpec[64];
	i = 0;
	for (; physicalSpec[i] && i < 63; ++i) {
		char c = physicalSpec[i];
		if (c >= 'a' && c <= 'z') {
			upperSpec[i] = c - 'a' + 'A';
		} else {
			upperSpec[i] = c;
		}
	}
	upperSpec[i] = '\0';
	
	bool isValidNES = (strcmp(upperSpec, "A") == 0 ||
	                   strcmp(upperSpec, "B") == 0 ||
	                   strcmp(upperSpec, "SELECT") == 0 ||
	                   strcmp(upperSpec, "START") == 0 ||
	                   strcmp(upperSpec, "UP") == 0 ||
	                   strcmp(upperSpec, "DOWN") == 0 ||
	                   strcmp(upperSpec, "LEFT") == 0 ||
	                   strcmp(upperSpec, "RIGHT") == 0);
	
	// Check if it's a valid Xbox button name
	EnsureButtonNameMap();
	WORD xboxMask = 0;
	const char* dummyCanonical = NULL;
	bool isValidXbox = MapXboxButtonName(physicalSpec, xboxMask, dummyCanonical);
	
	if (!isValidNES && !isValidXbox) {
		return luaL_error(L, "mapinput: physical spec '%s' is not a valid button name. Valid NES buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT. Valid Xbox buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", physicalSpec);
	}
	
	// Store the mapping for this Lua state
	s_virtualInputMappings[L][upperVirtual] = std::string(physicalSpec);
	
	return 0;
}

// ============================================================================
// Module Registrar and Lifecycle Hooks
// ============================================================================

void Lua_RegisterInput(lua_State* L)
{
	if (!L) {
		return;
	}

	lua_register(L, "getjoypad", lua_getjoypad);
	lua_register(L, "gethardwarejoypad", lua_gethardwarejoypad);
	lua_pushcfunction(L, lua_setjoypad);
	lua_setglobal(L, "setjoypad");
	lua_pushcfunction(L, lua_clearjoypad);
	lua_setglobal(L, "clearjoypad");
	lua_pushcfunction(L, lua_pressbutton);
	lua_setglobal(L, "pressbutton");
	lua_pushcfunction(L, lua_releasebutton);
	lua_setglobal(L, "releasebutton");
	lua_register(L, "setrumble", lua_setrumble);
	lua_register(L, "mapinput", lua_mapinput);
	lua_register(L, "onbuttonpress", lua_onbuttonpress);
	lua_register(L, "onbuttonrelease", lua_onbuttonrelease);
	lua_register(L, "getbuttonheldms", lua_getbuttonheldms);
	lua_register(L, "isbuttonpressed", lua_isbuttonpressed);
	lua_register(L, "getbuttonname", lua_getbuttonname);
	lua_register(L, "getbuttonmask", lua_getbuttonmask);
	lua_register(L, "isxboxbuttonpressed", lua_isxboxbuttonpressed);
}

void Lua_InputCleanup(lua_State* L)
{
	if (L != NULL) {
		// Clean up Lua refs for button callbacks
		for (std::map<WORD, ButtonCallbackInfo>::iterator it = s_buttonPressCallbacks.begin();
			 it != s_buttonPressCallbacks.end(); ++it) {
			if (it->second.luaRef >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, it->second.luaRef);
			}
		}
		for (std::map<WORD, ButtonCallbackInfo>::iterator it = s_buttonReleaseCallbacks.begin();
			 it != s_buttonReleaseCallbacks.end(); ++it) {
			if (it->second.luaRef >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, it->second.luaRef);
			}
		}
		
		// Clean up virtual input mappings for this Lua state
		s_virtualInputMappings.erase(L);
	}
}

void Lua_InputReset(void)
{
	// Clear joypad overrides
	for (int p = 0; p < 4; ++p) {
		s_luaJoypadValue[p] = 0;
		s_luaJoypadMask[p] = 0;
		s_luaJoypadLatched[p] = 0;
		s_oneFramePress[p] = 0;
		s_oneFrameRelease[p] = 0;
		s_hardwareJoypad[p] = 0;
		s_prevXboxButtonState[p] = 0;
		ResetHoldStatesForPlayer(p);
	}
	
	// Clear button callbacks (Lua refs should be cleaned up via Lua_InputCleanup first)
	s_buttonPressCallbacks.clear();
	s_buttonReleaseCallbacks.clear();
	
	// Clear virtual input mappings
	s_virtualInputMappings.clear();
	
	// Clear rumble state
	for (int p = 0; p < 4; ++p) {
		s_rumbleState[p].active = false;
		s_rumbleState[p].startTime = 0;
		s_rumbleState[p].duration = 0;
		s_rumbleState[p].intensity = 0.0f;
	}
}

void Lua_InputOnFrame(lua_State* L)
{
	// Update rumble state
	DWORD currentTime = GetTickCount();
	for (int p = 0; p < 4; ++p) {
		if (s_rumbleState[p].active) {
			DWORD elapsed = currentTime - s_rumbleState[p].startTime;
			if (elapsed >= s_rumbleState[p].duration) {
				// Rumble duration expired - stop rumble
				s_rumbleState[p].active = false;
				if (Gamepads[p].bConnected) {
					XINPUT_VIBRATION vibration;
					vibration.wLeftMotorSpeed = 0;
					vibration.wRightMotorSpeed = 0;
					XInputSetState(p, &vibration);
				}
			}
		}
	}
	
	// Process button callbacks and hold states
	bool havePressCallbacks = !s_buttonPressCallbacks.empty();
	bool haveReleaseCallbacks = !s_buttonReleaseCallbacks.empty();
	
	extern int s_luaDisabled;
	extern int luaInitialized;
	
	if (havePressCallbacks || haveReleaseCallbacks) {
		if (!s_luaDisabled && luaInitialized && L != NULL) {
			for (int p = 0; p < 4; ++p) {
				WORD currentButtons = Gamepads[p].wButtons;
				if (!Gamepads[p].bConnected) {
					ResetHoldStatesForPlayer(p);
					s_prevXboxButtonState[p] = 0;
					continue;
				}
				DWORD now = GetTickCount();
				WORD previousButtons = s_prevXboxButtonState[p];
				WORD pressedThisFrame = (WORD)(currentButtons & (WORD)~previousButtons);
				WORD releasedThisFrame = (WORD)((~currentButtons) & previousButtons);

				UpdateHoldStatesForPlayer(p, currentButtons, now);

				if (havePressCallbacks && pressedThisFrame) {
					for (std::map<WORD, ButtonCallbackInfo>::const_iterator it = s_buttonPressCallbacks.begin();
						 it != s_buttonPressCallbacks.end(); ++it) {
						if ((pressedThisFrame & it->second.mask) != 0) {
							TriggerButtonCallback(it->second, p);
						}
					}
				}
				if (haveReleaseCallbacks && releasedThisFrame) {
					for (std::map<WORD, ButtonCallbackInfo>::const_iterator it = s_buttonReleaseCallbacks.begin();
						 it != s_buttonReleaseCallbacks.end(); ++it) {
						if ((releasedThisFrame & it->second.mask) != 0) {
							TriggerButtonCallback(it->second, p);
						}
					}
				}

				s_prevXboxButtonState[p] = currentButtons;
			}
		} else {
			for (int p = 0; p < 4; ++p) {
				WORD currentButtons = Gamepads[p].wButtons;
				if (!Gamepads[p].bConnected) {
					ResetHoldStatesForPlayer(p);
					s_prevXboxButtonState[p] = 0;
					continue;
				}
				DWORD now = GetTickCount();
				UpdateHoldStatesForPlayer(p, currentButtons, now);
				s_prevXboxButtonState[p] = currentButtons;
			}
		}
	} else {
		for (int p = 0; p < 4; ++p) {
			WORD currentButtons = Gamepads[p].wButtons;
			if (!Gamepads[p].bConnected) {
				ResetHoldStatesForPlayer(p);
				s_prevXboxButtonState[p] = 0;
				continue;
			}
			DWORD now = GetTickCount();
			UpdateHoldStatesForPlayer(p, currentButtons, now);
			s_prevXboxButtonState[p] = currentButtons;
		}
	}
}

void Lua_InputProcessJoypad(void)
{
	// Note: Hardware input should be stored BEFORE movie playback is applied
	// This function is called after movie playback has been applied to powerpadbuf
	// So we need to get the original hardware input from elsewhere or store it earlier
	// For now, we'll work with whatever is in powerpadbuf (which may have movie playback)
	
	// Override powerpadbuf (Xbox input buffer) for players 0 and 1
	// Note: powerpadbuf may already have movie playback applied by FCEU_LuaJoypadApply
	uint32 newPowerpadbuf = powerpadbuf;  // Start with current powerpadbuf (may include movie playback)
	
	// Override player 0 (low byte of powerpadbuf)
	if (s_luaJoypadLatched[0]) {
		uint8 oldPad0 = (uint8)(powerpadbuf & 0xFF);
		uint8 newPad0 = (uint8)((oldPad0 & (uint8)~s_luaJoypadMask[0]) |
								(s_luaJoypadValue[0] & s_luaJoypadMask[0]));
		newPowerpadbuf = (newPowerpadbuf & 0xFFFFFF00) | newPad0;
	}
	
	// Apply one-frame presses for player 0 (OR them in)
	if (s_oneFramePress[0] != 0) {
		uint8 pad0 = (uint8)(newPowerpadbuf & 0xFF);
		pad0 |= s_oneFramePress[0];
		newPowerpadbuf = (newPowerpadbuf & 0xFFFFFF00) | pad0;
	}
	
	// Apply one-frame releases for player 0 (AND ~mask to clear bits)
	if (s_oneFrameRelease[0] != 0) {
		uint8 pad0 = (uint8)(newPowerpadbuf & 0xFF);
		pad0 &= (uint8)~s_oneFrameRelease[0];
		newPowerpadbuf = (newPowerpadbuf & 0xFFFFFF00) | pad0;
	}
	
	// Override player 1 (second byte of powerpadbuf)
	if (s_luaJoypadLatched[1]) {
		uint8 oldPad1 = (uint8)((powerpadbuf >> 8) & 0xFF);
		uint8 newPad1 = (uint8)((oldPad1 & (uint8)~s_luaJoypadMask[1]) |
								(s_luaJoypadValue[1] & s_luaJoypadMask[1]));
		newPowerpadbuf = (newPowerpadbuf & 0xFFFF00FF) | ((uint32)newPad1 << 8);
	}
	
	// Apply one-frame presses for player 1 (OR them in)
	if (s_oneFramePress[1] != 0) {
		uint8 pad1 = (uint8)((newPowerpadbuf >> 8) & 0xFF);
		pad1 |= s_oneFramePress[1];
		newPowerpadbuf = (newPowerpadbuf & 0xFFFF00FF) | ((uint32)pad1 << 8);
	}
	
	// Apply one-frame releases for player 1 (AND ~mask to clear bits)
	if (s_oneFrameRelease[1] != 0) {
		uint8 pad1 = (uint8)((newPowerpadbuf >> 8) & 0xFF);
		pad1 &= (uint8)~s_oneFrameRelease[1];
		newPowerpadbuf = (newPowerpadbuf & 0xFFFF00FF) | ((uint32)pad1 << 8);
	}
	
	// Apply the override to powerpadbuf
	powerpadbuf = newPowerpadbuf;
	
	// Also override joy[] array for consistency (this is what the NES core reads)
	// Note: Movie processing is handled by FCEU_LuaJoypadApply after calling this
	for (int p = 0; p < 4; ++p) {
		uint8 finalButtons = joy[p];
		
		// Apply persistent override if latched
		if (s_luaJoypadLatched[p]) {
			finalButtons = (uint8)((joy[p] & (uint8)~s_luaJoypadMask[p]) |
								   (s_luaJoypadValue[p] & s_luaJoypadMask[p]));
		}
		
		// Apply one-frame button presses (OR them in, they work on top of any override)
		if (s_oneFramePress[p] != 0) {
			finalButtons |= s_oneFramePress[p];
			// Clear after applying (one-frame presses are cleared each frame)
			s_oneFramePress[p] = 0;
		}
		
		// Apply one-frame button releases (AND ~mask to clear bits)
		if (s_oneFrameRelease[p] != 0) {
			finalButtons &= (uint8)~s_oneFrameRelease[p];
			// Clear after applying (one-frame releases are cleared each frame)
			s_oneFrameRelease[p] = 0;
		}
		
		joy[p] = finalButtons;
	}
}

// Getter functions for input state
uint8_t Lua_InputGetHardwareJoypad(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_hardwareJoypad[player];
}

uint8_t Lua_InputGetLuaJoypadValue(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_luaJoypadValue[player];
}

uint8_t Lua_InputGetLuaJoypadMask(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_luaJoypadMask[player];
}

uint8_t Lua_InputGetLuaJoypadLatched(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_luaJoypadLatched[player];
}

uint8_t Lua_InputGetOneFramePress(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_oneFramePress[player];
}

uint8_t Lua_InputGetOneFrameRelease(int player) {
	if (player < 0 || player >= 4) return 0;
	return s_oneFrameRelease[player];
}

// Setter functions for input state
void Lua_InputSetHardwareJoypad(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_hardwareJoypad[player] = value;
}

void Lua_InputSetLuaJoypadValue(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_luaJoypadValue[player] = value;
}

void Lua_InputSetLuaJoypadMask(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_luaJoypadMask[player] = value;
}

void Lua_InputSetLuaJoypadLatched(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_luaJoypadLatched[player] = value;
}

void Lua_InputSetOneFramePress(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_oneFramePress[player] = value;
}

void Lua_InputSetOneFrameRelease(int player, uint8_t value) {
	if (player < 0 || player >= 4) return;
	s_oneFrameRelease[player] = value;
}

#endif // USE_LUA

