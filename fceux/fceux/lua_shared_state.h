#pragma once

#ifdef USE_LUA

// Include platform headers first to ensure WORD/DWORD are defined
#ifdef _XBOX
#	include <xtl.h>
#else
#	include <windows.h>
#	include <XInput.h>
#endif

#include "types.h"
#include <string>
#include <map>

struct lua_State;

// ============================================================================
// Input State Structures and Enums
// ============================================================================

namespace LuaInputState {

// Button callback information
struct ButtonCallbackInfo {
	WORD mask;
	std::string canonicalName;
	int luaRef;

	ButtonCallbackInfo() : mask(0), canonicalName(), luaRef(-1) {}
};

// Button index enumeration
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

// Rumble state tracking
struct RumbleState {
	DWORD startTime;      // When rumble started (GetTickCount())
	DWORD duration;       // Duration in milliseconds
	float intensity;      // Intensity (0.0-1.0)
	bool active;          // Whether rumble is currently active

	RumbleState() : startTime(0), duration(0), intensity(0.0f), active(false) {}
};

// Virtual input mapping type (per-script input remapping)
typedef std::map<lua_State*, std::map<std::string, std::string> > VirtualInputMappings;

} // namespace LuaInputState

// ============================================================================
// Profiler and Timing State
// ============================================================================

namespace LuaProfilerState {

// Timing constants (defined in lua_profiler.cpp)
extern const double kNTSCFrameRate;
extern const double kIdealFrameMs;

} // namespace LuaProfilerState

#endif // USE_LUA

