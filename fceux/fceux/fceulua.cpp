/* FCE Ultra Lua Integration for Xbox 360
 * Implementation
 * 
 * Enhanced for fce360-enhanced
 * GitHub: https://github.com/frankischilling/fce360-enhanced
 * 
 * Contributors:
 * @frankischilling
 * Ced2911 (original Xbox 360 port)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

 #include "../stdafx.h"

 #ifdef USE_LUA
 
#include "fceulua.h"
#include "fceu.h"
#include "sound.h"
#include "drawing.h"
#include "video.h"
#include "driver.h"
#include "state.h"
#include "ppu.h"
#include "git.h"
#include "cart.h"
#include "ines.h"
#include "movie.h"
#include "x6502.h"
#include "../xbox/Cemulator.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <vector>
#include <map>

// Extern PPU data for tile rendering
extern uint8 PALRAM[0x20];
extern uint8 PPU[4];
extern uint8 *VPage[8];
extern uint8 *CHRptr[32];

// Extern font data from drawing.cpp
extern uint8 Font6x7[792];
extern int FixJoedChar(uint8 ch);
extern int JoedCharWidth(uint8 ch);

// Extern joypad state from input.cpp
extern uint8 joy[4];

// Extern powerpadbuf from Cemulator.cpp (Xbox input buffer)
extern uint32 powerpadbuf;

// --- Overlay geometry and font metrics ---
enum { OVL_W = 256, OVL_H = 240, GLYPH_H = 8 };

static int s_consoleLineGap = 2; // pixels of extra leading between lines
static inline int CON_LINE_ADV(void) { return GLYPH_H + s_consoleLineGap; }

// Forward declaration for gamepad input (defined in Cemulator.cpp)
extern struct GAMEPAD {
	WORD wButtons;
	float fX1, fY1;
} Gamepads[];
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

// Minimal Lua API forward declarations (avoid changing symbol mappings)
extern "C" {
struct lua_State;
const char* lua_tolstring(lua_State* L, int idx, size_t* len);
const char* lua_tostring(lua_State* L, int idx);
int lua_gettop(lua_State* L);
void lua_settop(lua_State* L, int idx);
void lua_pushcfunction(lua_State* L, int (*fn)(lua_State*));
void lua_setglobal(lua_State* L, const char* name);
int luaL_error(lua_State* L, const char* fmt, ...);
int luaL_checkinteger(lua_State* L, int idx);
double luaL_checknumber(lua_State* L, int idx);
const char* luaL_checkstring(lua_State* L, int idx);
void lua_pushinteger(lua_State* L, int n);
}
 
// On-screen status message for debugging (shows last load attempt)
static char g_luaStatusMsg[128] = "Lua: disabled";
// stdafx.h already includes xtl.h which provides GetTickCount() and DWORD
// Script callback interval (ms); default ~33ms (~30 Hz)
static DWORD s_scriptIntervalMs = 33;

// Mode constants (matching Cemulator::Settings::LuaAutoloadMode enum)
enum { LUA_AUTO_ALL = 0, LUA_AUTO_ONE = 1, LUA_AUTO_NONE = 2 };

// ---- Master kill switch: single owner in this TU only ----
// PRIVATE static - no other translation unit can access this directly
// This is the hard gate that every load/boot path must respect
static volatile int s_luaDisabled = 1;  // Start disabled; UI will enable explicitly
static int s_luaMode = LUA_AUTO_NONE;
static char s_luaOneScript[256] = {0};

// ---- pending selection (UI -> core) ----
static int s_hasPending = 0;
static int s_pendingMode = LUA_AUTO_NONE;
static char s_pendingScript[256] = {0};

// Legacy global for compatibility (maps to s_pendingMode)
int g_pendingLuaMode = -1;
char g_pendingLuaScript[256] = {0};

// ---- Memory watchpoint system ----
static std::map<unsigned int, uint8> s_watchedAddresses;  // address -> previous value

// Total frame count since game start
static int s_totalFrameCount = 0;

// Public accessor
extern "C" int FCEU_LuaIsDisabled(void) { 
	return s_luaDisabled; 
}

// Public setter - nukes any running state when disabling
extern "C" void FCEU_LuaSetDisabled(int disabled) {
	s_luaDisabled = disabled ? 1 : 0;
	if (s_luaDisabled) {
		FCEU_LuaKillAll(); // make it a real kill-switch
		strncpy(g_luaStatusMsg, "Lua: disabled", sizeof(g_luaStatusMsg)-1);
		g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
	} else {
		strncpy(g_luaStatusMsg, "Lua: enabled", sizeof(g_luaStatusMsg)-1);
		g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
	}
	printf("FCEU_LuaSetDisabled: %s\n", s_luaDisabled ? "disabled" : "enabled");
}

// Kill all Lua scripts (stops and clears everything)
extern "C" void FCEU_LuaKillAll(void) {
	FCEU_LuaStop();  // This already stops Lua and clears overlays
	s_totalFrameCount = 0;  // Reset frame counter when game is closed
	s_watchedAddresses.clear();  // Clear all watchpoints
}

// Status message accessor
extern "C" const char* FCEU_LuaGetStatusMsg(void) {
	return g_luaStatusMsg;
}

// Cemulator UI calls this right before launching a game
extern "C" void FCEU_SetPendingLua(int mode, const char* scriptUtf8OrNull)
{
	s_hasPending = 1;
	s_pendingMode = mode;
	
	// Keep legacy global in sync for compatibility
	g_pendingLuaMode = mode;
	
	if (scriptUtf8OrNull) {
		strncpy(s_pendingScript, scriptUtf8OrNull, sizeof(s_pendingScript)-1);
		s_pendingScript[sizeof(s_pendingScript)-1] = '\0';
		strncpy(g_pendingLuaScript, scriptUtf8OrNull, sizeof(g_pendingLuaScript)-1);
		g_pendingLuaScript[sizeof(g_pendingLuaScript)-1] = '\0';
	} else {
		s_pendingScript[0] = '\0';
		g_pendingLuaScript[0] = '\0';
	}
	printf("FCEU_SetPendingLua: mode=%d, script='%s'\n", mode, scriptUtf8OrNull ? scriptUtf8OrNull : "(null)");
}
 
 // Double-buffered overlay for Lua-drawn content (updated at 30Hz, composited at 60Hz to prevent flicker)
 // Front buffer: currently displayed (what we composite)
 // Back buffer: where Lua draws next frame (only published on success)
 static uint8* s_overlay_front = NULL; // currently displayed
 static uint8* s_overlay_back  = NULL; // where Lua draws next
 
 static void EnsureOverlay() {
	 if (!s_overlay_front) {
		 s_overlay_front = (uint8*)malloc(OVL_W * OVL_H);
		 if (s_overlay_front) {
			 memset(s_overlay_front, 0, OVL_W * OVL_H);
		 }
	 }
	 if (!s_overlay_back) {
		 s_overlay_back = (uint8*)malloc(OVL_W * OVL_H);
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, OVL_W * OVL_H);
		 }
	 }
 }
 
 static inline void CompositeOverlay(uint8* XBuf) {
	 // Blit the front overlay onto the NES frame every frame (prevents flicker when Lua runs at 30Hz)
	 if (!s_overlay_front || !XBuf) return;
	 
	 const int N = OVL_W * OVL_H;
	 const uint8* src = s_overlay_front;
	 for (int i = 0; i < N; ++i) {
		 uint8 v = src[i];
		 if (v) XBuf[i] = v;  // Only overwrite non-zero overlay pixels
	 }
 }
 
 static inline void SwapOverlays() {
	 // Swap back and front buffers (publish the new overlay only on success)
	 uint8* t = s_overlay_front;
	 s_overlay_front = s_overlay_back;
	 s_overlay_back = t;
 }
 
 static void ClearOverlaysIfAny() {
	 EnsureOverlay();
	 if (s_overlay_back)  memset(s_overlay_back,  0, OVL_W * OVL_H);
	 if (s_overlay_front) memset(s_overlay_front, 0, OVL_W * OVL_H);
 }
 
// ---------------- Lua Console (logs + on-screen panel) ----------------
static bool s_consoleVisible = false;
static const int LUA_CONSOLE_X       = 0;
static const int LUA_CONSOLE_Y       = 8;      // wherever you draw the console
static const int LUA_CONSOLE_W_PX    = 256;    // use full width of the overlay
static const int LUA_CONSOLE_MAX_LINES = 256; // Increased to support more console lines
static const int LUA_CONSOLE_LINE_CHARS = 256; // storage capacity per line (chars)
static char s_luaConsoleLines[LUA_CONSOLE_MAX_LINES][LUA_CONSOLE_LINE_CHARS];
static int s_luaConsoleCount = 0;
static int s_consoleScrollOffset = 0; // Scroll offset for console (0 = show most recent)
static int s_consoleScrollOffsetH = 0; // Horizontal scroll offset for console (in pixels, 0 = leftmost)
static bool s_consoleDpadUpLast = false;
static bool s_consoleDpadDownLast = false;
static bool s_consoleDpadLeftLast = false;
static bool s_consoleDpadRightLast = false;
static int s_consoleScrollHoldFrames = 0; // Frame counter for continuous scrolling
static int s_consoleScrollHoldFramesH = 0; // Frame counter for continuous horizontal scrolling

// --- Lua-forced joypad state (per player) ---
static uint8 s_luaJoypadValue[4]  = {0,0,0,0};   // full 8-bit NES mask
static uint8 s_luaJoypadMask[4]   = {0,0,0,0};   // which bits to force (0xFF = all)
static uint8 s_luaJoypadLatched[4]= {0,0,0,0};   // 1 if override active
static uint8 s_hardwareJoypad[4]  = {0,0,0,0};   // hardware input before override (for reading real controller state)
static uint8 s_oneFramePress[4]   = {0,0,0,0};   // one-frame button presses (cleared after each frame)
static uint8 s_oneFrameRelease[4] = {0,0,0,0};   // one-frame button releases (cleared after each frame)

// --- Input recording and playback ---
static bool s_inputRecording = false;              // true if recording input
static std::vector<uint8> s_recordedInput[4];      // recorded input per player (frame-by-frame)
static bool s_inputPlayback = false;               // true if playing back input
static std::vector<uint8> s_playbackInput[4];      // playback input per player
static int s_playbackFrame = 0;                    // current frame in playback

static void LuaConsolePushLine(const char* msg) {
     if (!msg || !msg[0]) return;
     if (s_luaConsoleCount < LUA_CONSOLE_MAX_LINES) {
         strncpy(s_luaConsoleLines[s_luaConsoleCount], msg, LUA_CONSOLE_LINE_CHARS - 1);
         s_luaConsoleLines[s_luaConsoleCount][LUA_CONSOLE_LINE_CHARS - 1] = '\0';
         s_luaConsoleCount++;
     } else {
         for (int i = 1; i < LUA_CONSOLE_MAX_LINES; ++i) {
             memcpy(s_luaConsoleLines[i - 1], s_luaConsoleLines[i], LUA_CONSOLE_LINE_CHARS);
         }
         strncpy(s_luaConsoleLines[LUA_CONSOLE_MAX_LINES - 1], msg, LUA_CONSOLE_LINE_CHARS - 1);
         s_luaConsoleLines[LUA_CONSOLE_MAX_LINES - 1][LUA_CONSOLE_LINE_CHARS - 1] = '\0';
     }
}

void FCEU_SetLuaConsoleVisible(int visible) { 
	s_consoleVisible = (visible != 0);
	if (!s_consoleVisible) {
		s_consoleScrollOffset = 0; // Reset scroll when console is hidden
		s_consoleScrollOffsetH = 0; // Reset horizontal scroll when console is hidden
	}
}
int  FCEU_IsLuaConsoleVisible(void) { return s_consoleVisible ? 1 : 0; }

extern "C" void FCEU_SetLuaConsoleLineGap(int px) {
	if (px < 0) px = 0;
	if (px > 8) px = 8;
	s_consoleLineGap = px;
}
extern "C" int FCEU_GetLuaConsoleLineGap(void) { return s_consoleLineGap; }

void FCEU_ToggleLuaConsole(void) { 
	s_consoleVisible = !s_consoleVisible;
	if (!s_consoleVisible) {
		s_consoleScrollOffset = 0; // Reset scroll when console is hidden
		s_consoleScrollOffsetH = 0; // Reset horizontal scroll when console is hidden
	}
}

 static int lua_print_redirect(lua_State* L) {
	 int n = lua_gettop(L);
	 if (n == 0) { LuaConsolePushLine(""); return 0; }
	 char buffer[512]; buffer[0] = '\0';
	 for (int i = 1; i <= n; ++i) {
		 size_t slen = 0;
		 const char* s = lua_tolstring(L, i, &slen);
		 if (!s) continue; // only append string arguments
		 if (i > 1 && buffer[0] != '\0') strncat(buffer, "\t", sizeof(buffer) - strlen(buffer) - 1);
		 strncat(buffer, s, sizeof(buffer) - strlen(buffer) - 1);
	 }
	 LuaConsolePushLine(buffer);
	 return 0;
 }

static int lua_log(lua_State* L) { return lua_print_redirect(L); }

// setscriptinterval(ms) -- clamp 16..1000 ms
static int lua_setscriptinterval(lua_State* L) {
    int n = lua_gettop(L);
    if (n < 1) {
        return luaL_error(L, "setscriptinterval(ms) requires 1 argument");
    }
    int ms = (int)luaL_checkinteger(L, 1);
    if (ms < 16) ms = 16;
    if (ms > 1000) ms = 1000;
    s_scriptIntervalMs = (DWORD)ms;
    return 0;
}

// getscriptinterval() -> ms
static int lua_getscriptinterval(lua_State* L) {
    lua_pushinteger(L, (int)s_scriptIntervalMs);
    return 1;
}
 
 // Helper: Clear a rectangle in the overlay buffer with bounds checking
 static inline void clear_rect(uint8* buf, int x, int y, int w, int h) {
	 if (!buf) return;
	 
	 // Clamp to valid bounds
	 if (x < 0) { w += x; x = 0; }
	 if (y < 0) { h += y; y = 0; }
	 if (x + w > OVL_W) w = OVL_W - x;
	 if (y + h > OVL_H) h = OVL_H - y;
	 if (w <= 0 || h <= 0) return;
	 
	 // Clear each row of the rectangle
	 for (int dy = 0; dy < h; ++dy) {
		 memset(buf + (y + dy) * OVL_W + x, 0, w);
	 }
 }

 // Map 0x00-0x3F to overlay-coded 0x80-0xBF (never dim)
 static inline uint8 map_overlay_color(int c) {
	 return (uint8)((c & 0x3F) | 0x80);
 }

 // Drawing mode enum
 enum DrawMode {
	 DRAW_MODE_NORMAL = 0,
	 DRAW_MODE_ADD = 1,
	 DRAW_MODE_SUB = 2,
	 DRAW_MODE_MULTIPLY = 3,
	 DRAW_MODE_ALPHA = 4
 };

// Current drawing mode
static DrawMode s_drawMode = DRAW_MODE_NORMAL;

// Default drawing color (for functions that don't specify color)
static int s_defaultDrawColor = 0x39;  // Default to yellow-green

// Clipping region (defaults to full screen - no clipping)
 static int s_clipX = 0;
 static int s_clipY = 0;
 static int s_clipW = OVL_W;
 static int s_clipH = OVL_H;
 static bool s_clipEnabled = false;  // Only enable clipping when explicitly set

 // Helper function to check if a point is within the clipping region
 static inline bool is_point_clipped(int x, int y) {
	 if (!s_clipEnabled) return false;  // No clipping if not enabled
	 return (x < s_clipX || x >= s_clipX + s_clipW || y < s_clipY || y >= s_clipY + s_clipH);
 }

 // Helper function to apply blending mode
 static inline uint8 apply_blend_mode(uint8 dest, uint8 src) {
	 // If destination is transparent (0), just write source (for all modes)
	 if (dest == 0) {
		 return src;
	 }
	 
	 // Extract color indices from overlay values (0x80-0xBF -> 0-63)
	 int destColor = (dest & 0x3F);
	 int srcColor = (src & 0x3F);
	 int resultColor = 0;
	 
	 switch (s_drawMode) {
		 case DRAW_MODE_NORMAL:
			 return src;
		 
		 case DRAW_MODE_ADD: {
			 // Add color indices, clamp at 63 (brightest)
			 resultColor = destColor + srcColor;
			 if (resultColor > 63) resultColor = 63;
			 break;
		 }
		 
		 case DRAW_MODE_SUB: {
			 // Subtract color indices, clamp at 0 (darkest)
			 resultColor = destColor - srcColor;
			 if (resultColor < 0) resultColor = 0;
			 break;
		 }
		 
		 case DRAW_MODE_MULTIPLY: {
			 // Multiply color indices (normalized)
			 resultColor = (destColor * srcColor) / 63;
			 if (resultColor > 63) resultColor = 63;
			 break;
		 }
		 
		 case DRAW_MODE_ALPHA: {
			 // Simple alpha blending (50% mix)
			 resultColor = (destColor + srcColor) / 2;
			 break;
		 }
		 
		 default:
			 return src;
	 }
	 
	 // Map result back to overlay range
	 return (uint8)((resultColor & 0x3F) | 0x80);
 }

 // Measure pixels we'll actually draw on the first line (8px per glyph)
 static inline int measure_line_px(const char* s, int max_w_px) {
	 if (!s || max_w_px <= 0) return 0;
	 int w = 0;
	 for (const char* p = s; *p && *p != '\n' && w + 8 <= max_w_px; ++p) w += 8;
	 return w;
 }

 // Clear one line of text area (glyph-height = 8 px)
 static inline void clear_text_line(uint8* buf, int x, int y, int w_px) {
	 if (!buf || w_px <= 0) return;
	 clear_rect(buf, x, y, w_px, 8);
 }
 
 // Utility: Check if overlay actually changed (fast path with stripes, then full compare)
 static inline bool overlay_has_changes(const uint8* a, const uint8* b) {
	 if (!a || !b) return true;  // If either is NULL, consider it changed
	 // Scan a few stripes first (fast path), then fall back to full compare
	 const int pitch = OVL_W, h = OVL_H;
	 for (int y = 0; y < h; y += 16) {
		 if (memcmp(a + y*pitch, b + y*pitch, pitch) != 0) return true;
	 }
	 return memcmp(a, b, OVL_W*OVL_H) != 0;
 }
 
 // Dirty flag: tracks if anything was actually drawn to the overlay
 // Only publish new overlay if Lua succeeded AND drew something
static bool g_overlayDirty = false;
 
 // Performance: Disable printf spam in retail builds
 #if !defined(DEBUG) && !defined(_DEBUG)
 #undef printf
 #define printf(...) ((void)0)
 #endif
 
 // Log budget system to prevent excessive debug output
 static int g_log_budget = 200; // print at most 200 lines total per run
 #define LOGF(...) do { if (g_log_budget > 0) { --g_log_budget; printf(__VA_ARGS__); } } while(0)
 
 // Debug logging helper
 static void dbg(const char* s) { 
 #ifdef _XBOX
	 OutputDebugStringA(s); 
 #else
	 printf("%s", s);
 #endif
 }
 
 #define printf(...) do { \
	 char b[512]; \
	 _snprintf(b, 511, __VA_ARGS__); \
	 b[511] = 0; \
	 dbg(b); \
 } while(0)
 
 // Lua headers
 extern "C" {
 #include "../xbox/lua/src/lua.h"
 #include "../xbox/lua/src/lauxlib.h"
 #include "../xbox/lua/src/lualib.h"
 }
 
 lua_State* luaState = NULL;
 static bool luaInitialized = false;
 
 // FPS tracking
 static DWORD lastFPSUpdate = 0;
static int frameCount = 0;  // FPS calculation counter (resets every second)
 static double currentFPS = 0.0;
 static DWORD lastFrameTime = 0;
 
 // Forward declaration
 static uint8* currentXBuf = NULL;
 // Store the actual frame buffer passed to FCEU_LuaGui (not the overlay)
 static uint8* s_frameXBuf = NULL;
 // Pre-initialize screenshot directory path to avoid lag on first screenshot
 static bool s_screenshotDirInitialized = false;

 // Transparent text with no outline/shadow/background.
 static inline void DrawTextNoBorder(uint8* base, int pitch, const char* s, uint8 color)
 {
	 // Draws only glyph pixels; does not touch surrounding pixels.
	 DrawTextTrans(base, pitch, (uint8*)s, color);
 }

 // Transparent text with simple width/height clipping & newline handling.
 // No border, no background.
 static void DrawTextNoBorderWH(uint8* base, int pitch, const char* s, uint8 color, int max_w, int max_h)
 {
	 if (!base || !s || max_w <= 0 || max_h <= 0) return;
	 DrawTextTransWH(base, pitch, (uint8*)s, color, max_w, max_h, 0);
 }

// ---- Rotated text (glyph-only, no background/outline) ----

static inline void DrawTextTransRotated(
	uint8* base, int pitch,
	int x, int y,
	const char* text, uint8 fgcolor, float angleDeg)
{
	if(!base || !text || !*text) return;

	// Fast path: angle == 0 -> regular draw
	int anglei = (int)angleDeg;
	if(((anglei % 360) + 360) % 360 == 0) {
		uint8* dest = base + y * pitch + x;
		if(dest >= base && dest < base + OVL_W * OVL_H)
			DrawTextTrans(dest, pitch, (uint8*)text, fgcolor);
		return;
	}

	// Clamp once; we'll bounds-check each pixel anyway
	if(x <= -OVL_W || x >= OVL_W || y <= -OVL_H || y >= OVL_H) {
		// Still may draw if rotation swings pixels in; keep going.
	}

	// Precompute rotation
	// Use standard counter-clockwise rotation matrix
	// Standard: x' = x*cos - y*sin, y' = x*sin + y*cos
	// This gives: 0°=right, 90°=down, 180°=left, 270°=up (for screen Y-down coords)
	const float PI = 3.14159265358979323846f;
	float a = angleDeg * (PI / 180.0f);
	float cs = cosf(a), sn = sinf(a);

	// Pen position in source (unrotated) text space
	int penX = 0, penY = 0;

	for(const char* p = text; *p; ++p)
	{
		if(*p == '\n') { penX = 0; penY += 8; continue; }

		int ch  = FixJoedChar((uint8)*p);
		int wid = JoedCharWidth((uint8)*p);

		for(int ny = 0; ny < 7; ++ny)
		{
			uint8 row = Font6x7[ch*8 + 1 + ny];
			for(int nx = 0; nx < wid; ++nx)
			{
				if(((row >> (7 - nx)) & 1) == 0) continue;

				// source local coords (right = +x, down = +y)
				float fx = (float)(penX + nx);
				float fy = (float)(penY + ny);

				// rotate around origin (x,y) in screen space
				// Standard 2D rotation: x' = x*cos - y*sin, y' = x*sin + y*cos
				int rx = (int)floorf(x + (cs*fx - sn*fy) + 0.5f);
				int ry = (int)floorf(y + (sn*fx + cs*fy) + 0.5f);

				if(rx < 0 || rx >= OVL_W || ry < 0 || ry >= OVL_H) continue;
				if(is_point_clipped(rx, ry)) continue;

				uint8* dst = base + ry * pitch + rx;
				if(dst >= base && dst < base + OVL_W * OVL_H) {
					*dst = apply_blend_mode(*dst, fgcolor);
				}
			}
		}
		penX += wid;
	}
 }

static void DrawLuaConsole(uint8* buf) {
	if (!s_consoleVisible || !buf) return;

	// Draw within the console box (cx=4, cy=40, with text starting at cy+12=52)
	const int cx = 4, cy = 40;
	const int textLeftMargin = cx + 2;
	const int lineStartY = cy + 12;
	int maxX = 4 + 248 - 1; if (maxX > OVL_W - 1) maxX = OVL_W - 1;
	int maxY = 40 + 180 - 1; if (maxY > OVL_H - 1) maxY = OVL_H - 1;
	const uint8 bg = 0x80 | 0x10;
	const int textWidthPx = (maxX - textLeftMargin + 1);
	
	const int adv = CON_LINE_ADV();
	const int availableHeight = maxY - lineStartY + 1;
	int maxLines = availableHeight / adv;
	if (maxLines < 1) return;

	// Calculate which lines to show based on scroll offset
	// When scrollOffset = 0, show newest lines (highest indices)
	// When scrollOffset increases, show older lines (lower indices)
	// last is the highest index we'll show (newest line)
	int last = s_luaConsoleCount - s_consoleScrollOffset;
	if (last < 0) last = 0;
	// first is the lowest index we'll show (oldest line)
	int first = last - maxLines;
	if (first < 0) {
		// We're at the top - show from the oldest available line (index 0)
		first = 0;
		// Keep last as is, but ensure we don't exceed available lines
		if (last > s_luaConsoleCount) last = s_luaConsoleCount;
	}

	// Calculate maximum horizontal scroll needed for visible lines
	int maxLineWidthPx = 0;
	for (int i = first; i < last; ++i) {
		int lineWidth = 0;
		for (const unsigned char* p = (const unsigned char*)s_luaConsoleLines[i]; *p && *p != '\n'; ++p) {
			int cw = JoedCharWidth(*p);
			if (cw <= 0) cw = 6; // fallback
			lineWidth += cw;
		}
		if (lineWidth > maxLineWidthPx) maxLineWidthPx = lineWidth;
	}
	
	// Clamp horizontal scroll offset
	int maxScrollH = (maxLineWidthPx > textWidthPx) ? (maxLineWidthPx - textWidthPx) : 0;
	if (s_consoleScrollOffsetH > maxScrollH) s_consoleScrollOffsetH = maxScrollH;
	if (s_consoleScrollOffsetH < 0) s_consoleScrollOffsetH = 0;

	int y = lineStartY;
	for (int i = first; i < last && y + GLYPH_H <= maxY + 1; ++i, y += adv) {
		// Clear exactly this line's 8px band within the box area
		for (int py = y; py < y + GLYPH_H && py <= maxY; ++py) {
			for (int px = textLeftMargin; px <= maxX; ++px) {
				buf[py*OVL_W + px] = bg;
			}
		}
		
		// Calculate how many characters to skip based on horizontal scroll
		// We need to find the character position that corresponds to s_consoleScrollOffsetH pixels
		const char* lineStart = s_luaConsoleLines[i];
		const char* drawStart = lineStart;
		int skippedPx = 0;
		if (s_consoleScrollOffsetH > 0) {
			for (const unsigned char* p = (const unsigned char*)lineStart; *p && *p != '\n'; ++p) {
				int cw = JoedCharWidth(*p);
				if (cw <= 0) cw = 6; // fallback
				if (skippedPx + cw > s_consoleScrollOffsetH) {
					drawStart = (const char*)p;
					break;
				}
				skippedPx += cw;
			}
		}
		
		// Draw text starting from the horizontal scroll offset
		// Adjust width to account for the skipped pixels
		int adjustedWidth = textWidthPx + (s_consoleScrollOffsetH - skippedPx);
		DrawTextTransWH(buf + y*OVL_W + textLeftMargin, OVL_W, (uint8*)drawStart, 0x20 | 0x80, adjustedWidth, adv, 0);
	}
 }

 // Lua drawing function - allows scripts to draw text
 int lua_drawtext(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 3) {
		 return luaL_error(L, "drawtext(x, y, text [, color]) requires at least 3 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 
	 // CRITICAL FIRST CHECK: If y is too large, text cannot fit - skip drawing immediately
	 // This MUST be checked before any other processing to prevent crashes
	 // Check if y itself is at or past the bottom edge
	 if (y >= OVL_H) return 0;
	 // Check if text would extend below screen (y + 8 pixels tall)
	 if (y + GLYPH_H > OVL_H) return 0;
	 // Additional safety: if y is negative after this point, something is wrong
	 if (y < 0) {
		 y = 0; // Clamp to top, but verify it still fits
		 if (y + GLYPH_H > OVL_H) return 0;
	 }
	 
	 const char* text = luaL_checkstring(L, 3);
	 int color_in = (n >= 4) ? (int)luaL_optinteger(L, 4, 0x20) : 0x20;
	 
	 if (!currentXBuf || !text || !*text) return 0;
	 
	 // Text is 8 pixels tall - clamp coordinates to prevent drawing past screen bounds
	 // Clamp x to valid range
	 if (x < 0) x = 0;
	 if (x >= OVL_W) return 0; // Completely off-screen to the right, skip drawing
	 
	 // Strict clamping for y to ensure text NEVER goes below screen
	 if (y < 0) y = 0;
	 // Critical: Ensure y + GLYPH_H never exceeds OVL_H (text must fit completely on screen)
	 // Note: This check should already be passed due to early exit above, but keep for safety
	 if (y + GLYPH_H > OVL_H) {
		 // Text would go below screen - skip drawing entirely (no clamping, just abort)
		 return 0;
	 }
	 
	 // Verify final coordinates are still valid (defensive check)
	 if (x < 0 || x >= OVL_W || y < 0 || y >= OVL_H) return 0;
	 // Critical check: text must fit completely within screen bounds
	 if (y + GLYPH_H > OVL_H) return 0; // This should never happen after clamping, but double-check
	 if (y >= OVL_H) return 0; // y itself must be on-screen
	 
	 // Calculate text width and clamp to remaining screen space
	 int maxWidth = OVL_W - x;
	 if (maxWidth <= 0) return 0; // No space to draw
	 
	 int wpx = measure_line_px(text, maxWidth);
	 if (wpx <= 0) return 0; // No text to draw
	 
	 // CRITICAL: Final validation before any drawing operations
	 // Ensure text will not draw below screen under any circumstances
	 if (y < 0 || y >= OVL_H) return 0;
	 if (y + GLYPH_H > OVL_H) return 0; // Text must fit completely on screen
	 if (x < 0 || x >= OVL_W) return 0;
	 if (wpx <= 0) return 0;
	 if (x + wpx > OVL_W) {
		 // Clamp width if it exceeds remaining space
		 wpx = OVL_W - x;
		 if (wpx <= 0) return 0;
	 }
	 
	 // Verify buffer pointer calculation is safe (prevent integer overflow)
	 int bufferOffset = y * OVL_W + x;
	 if (bufferOffset < 0 || bufferOffset >= OVL_W * OVL_H) return 0;
	 
	 // Verify the entire text area (including all pixels) is within bounds
	 int maxY = y + GLYPH_H - 1;
	 if (maxY >= OVL_H) return 0; // This should never happen, but double-check
	 int maxX = x + wpx - 1;
	 if (maxX >= OVL_W) {
		 wpx = OVL_W - x; // Clamp width
		 if (wpx <= 0) return 0;
	 }
	 
	 // Note: We don't clear the area - DrawTextTrans only draws glyph pixels,
	 // so text will draw over existing content (colors, fills, etc.)
	 
	 // Final buffer pointer validation before drawing
	 if (y * OVL_W + x < 0 || y * OVL_W + x >= OVL_W * OVL_H) return 0;
	 
	 // Additional safety check: ensure we're not drawing past the buffer
	 uint8 *dest = currentXBuf + y * OVL_W + x;
	 if (dest < currentXBuf || dest >= currentXBuf + OVL_W * OVL_H) return 0;
	 
	 uint8 mapped = map_overlay_color(color_in);
	 DrawTextNoBorder(dest, OVL_W, text, mapped); // glyphs only, no bg
	 g_overlayDirty = true;
	 return 0;
 }

 // Lua function to set console line spacing
 static int lua_setconsolespacing(lua_State* L) {
	 int px = (int)luaL_checkinteger(L, 1);
	 FCEU_SetLuaConsoleLineGap(px);
	 return 0;
 }

 // Lua drawing function - allows scripts to draw text with width/height limits and border
 int lua_drawtextwh(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 7) {
		 return luaL_error(L, "drawtextwh(x, y, text, color, max_w, max_h, border) requires 7 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 const char* text = luaL_checkstring(L, 3);
	 int color = (int)luaL_checkinteger(L, 4);
	 int max_w = (int)luaL_checkinteger(L, 5);
	 int max_h = (int)luaL_checkinteger(L, 6);
	 int border = (int)luaL_checkinteger(L, 7);
	 
	 if (!currentXBuf || !text || !*text) return 0;
	 
	 // Early return if completely off-screen to the right
	 if (x >= OVL_W) return 0;
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (y < 0) y = 0;
	 
	 // Ensure requested box stays on-screen
	 if (max_h <= 0) max_h = GLYPH_H;
	 if (max_w <= 0) return 0; // No width to draw
	 
	 if (y + max_h > OVL_H) {
		 y = OVL_H - max_h;
		 if (y < 0) { 
			 y = 0; 
			 max_h = OVL_H; 
			 if (max_h <= 0) return 0; // Still no room after adjustment
		 }
	 }
	 if (max_h <= 0) return 0; // No room to draw
	 
	 // Clamp max_w to remaining screen space
	 if (x + max_w > OVL_W) {
		 max_w = OVL_W - x;
		 if (max_w <= 0) return 0; // No space to draw
	 }
	 
	 // Verify final coordinates are still valid (defensive check)
	 if (x < 0 || x >= OVL_W || y < 0 || y >= OVL_H) return 0;
	 if (y + max_h > OVL_H) return 0; // Ensure height fits on screen
	 if (x + max_w > OVL_W) return 0; // Ensure width fits on screen
	 
	 // Verify buffer pointer is valid before drawing
	 if (y * OVL_W + x < 0 || y * OVL_W + x >= OVL_W * OVL_H) return 0;
	 
	 uint8 *dest = currentXBuf + y * OVL_W + x;
	 uint8 mapped = map_overlay_color(color);
	 
	 // Clamp border and choose the truly borderless path when border == 0
	 if (border < 0) border = 0;
	 if (border > 2) border = 2;
	 
	 // Note: We don't clear the area - DrawTextTransWH only draws glyph pixels when border==0,
	 // so text will draw over existing content (colors, fills, etc.)
	 
	 DrawTextTransWH(dest, OVL_W, (uint8*)text, mapped, max_w, max_h, border <= 0 ? 0 : border);
	 
	 g_overlayDirty = true;
	 return 0;
 }

 // Lua drawing function - allows scripts to draw text with custom scaling
 int lua_drawtextscaled(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return luaL_error(L, "drawtextscaled(x, y, text, color, scaleX, scaleY) requires 6 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 const char* text = luaL_checkstring(L, 3);
	 int color_in = (int)luaL_checkinteger(L, 4);
	 float scaleX = (float)luaL_checknumber(L, 5);
	 float scaleY = (float)luaL_checknumber(L, 6);
	 
	 if (!currentXBuf || !text || !*text) return 0;
	 
	 // Clamp scale values to valid range (0.5-4.0)
	 if (scaleX < 0.5f) scaleX = 0.5f;
	 if (scaleX > 4.0f) scaleX = 4.0f;
	 if (scaleY < 0.5f) scaleY = 0.5f;
	 if (scaleY > 4.0f) scaleY = 4.0f;
	 
	 // Early return if completely off-screen
	 if (x >= OVL_W) return 0;
	 if (y >= OVL_H) return 0;
	 
	 // Clamp coordinates to safe bounds
	 if (x < 0) x = 0;
	 if (y < 0) y = 0;
	 
	 // Calculate maximum scaled text dimensions for bounds checking
	 // Each character is up to 6 pixels wide and 7 pixels tall (scaled)
	 int maxCharWidth = (int)(6 * scaleX + 0.5f);
	 int maxCharHeight = (int)(7 * scaleY + 0.5f);
	 
	 // Estimate text width (rough approximation)
	 int textWidth = 0;
	 const char* p = text;
	 while (*p && *p != '\n') {
		 textWidth += maxCharWidth;
		 p++;
	 }
	 
	 // Ensure text fits on screen (DrawTextTransScaled will handle pixel-level clipping)
	 if (y + maxCharHeight > OVL_H) {
		 // Text would extend past screen bottom - skip drawing
		 return 0;
	 }
	 
	 // Verify buffer pointer is valid before drawing
	 if (y * OVL_W + x < 0 || y * OVL_W + x >= OVL_W * OVL_H) return 0;
	 
	 uint8 *dest = currentXBuf + y * OVL_W + x;
	 if (dest < currentXBuf || dest >= currentXBuf + OVL_W * OVL_H) return 0;
	 
	 uint8 mapped = map_overlay_color(color_in);
	 DrawTextTransScaled(dest, OVL_W, (uint8*)text, mapped, scaleX, scaleY);
	 g_overlayDirty = true;
	 return 0;
 }

 // drawtextrotated(x, y, text, color, angleDeg)
 int lua_drawtextrotated(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if(n < 5)
		 return luaL_error(L, "drawtextrotated(x, y, text, color, angle) requires 5 arguments");

	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 const char* text = luaL_checkstring(L, 3);
	 int color_in = (int)luaL_checkinteger(L, 4);
	 int angleDeg = (int)luaL_checkinteger(L, 5);

	 if(!currentXBuf || !text || !*text) return 0;

	 // Normalize & map color, clamp angle
	 if(angleDeg < 0) angleDeg %= 360, angleDeg += 360;
	 else              angleDeg %= 360;

	 uint8 mapped = map_overlay_color(color_in);

	 // Defensive pointer check for starting row; per-pixel checks are inside
	 if(y * OVL_W + x < -OVL_W || y * OVL_W + x > OVL_W * OVL_H) {
		 // Not a hard error; we still try (rotation may move pixels onscreen)
	 }

	 DrawTextTransRotated(currentXBuf, OVL_W, x, y, text, mapped, (float)angleDeg);
	 g_overlayDirty = true;
	 return 0;
 }

 // gettextwidth(text) -> integer pixels
 // Returns the pixel width of the longest line in `text` using the same
 // variable-width metrics as DrawTextTrans (Font6x7 + JoedCharWidth).
 static int lua_gettextwidth(lua_State* L)
 {
	 const char* s = luaL_checkstring(L, 1);
	 if (!s || !*s) { lua_pushinteger(L, 0); return 1; }

	 int maxw = 0;
	 int curw = 0;

	 // Space/tab handling
	 int space_w = JoedCharWidth((uint8)' ');
	 if (space_w <= 0) space_w = 4;   // safe fallback if font says 0/unknown
	 const int tab_w = space_w * 4; // 4-space tab stops

	 for (const unsigned char* p = (const unsigned char*)s; *p; ++p)
	 {
		 unsigned char ch = *p;

		 // newline -> finalize this line, start next
		 if (ch == '\n') {
			 if (curw > maxw) maxw = curw;
			 curw = 0;
			 continue;
		 }

		 // carriage return: ignore (Windows line endings)
		 if (ch == '\r') {
			 continue;
		 }

		 // tabs: advance by 4 spaces (no hard tab-stop alignment in pixels)
		 if (ch == '\t') {
			 curw += tab_w;
			 continue;
		 }

		 // Measure variable width using the same mapping as drawing
		 int cw = JoedCharWidth(ch);
		 if (cw > 0) curw += cw;
		 // else non-printable/unknown -> zero advance
	 }

	 if (curw > maxw) maxw = curw;
	 lua_pushinteger(L, maxw);
	 return 1;
 }

 // gettextheight(text) -> integer pixels
 // Counts lines separated by '\n'. Empty string => 0 pixels.
 // Trailing '\n' adds an empty line (so "a\n" -> 2 lines -> 16 px).
 static int lua_gettextheight(lua_State* L)
 {
	 const char* s = luaL_checkstring(L, 1);
	 if (!s || !*s) { lua_pushinteger(L, 0); return 1; }

	 int lines = 1;
	 for (const unsigned char* p = (const unsigned char*)s; *p; ++p)
		 if (*p == '\n') ++lines;

	 lua_pushinteger(L, lines * GLYPH_H); // GLYPH_H is 8
	 return 1;
 }

 // getjoypad(player) -> integer bitmask
 // Gets current controller state for a player (0-3, 0=Player 1)
 // Returns button bitmask: bit 0=A, 1=B, 2=Select, 3=Start, 4=Up, 5=Down, 6=Left, 7=Right
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
 // Gets hardware controller state for a player BEFORE Lua override is applied
 // Useful for detecting real controller input even when setjoypad() is active
 // Returns button bitmask: bit 0=A, 1=B, 2=Select, 3=Start, 4=Up, 5=Down, 6=Left, 7=Right
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
 // Checks if a specific Xbox 360 controller button is pressed
 // Button names: "A", "B", "X", "Y", "START", "BACK", "LEFT_SHOULDER", "RIGHT_SHOULDER", "LEFT_THUMB", "RIGHT_THUMB", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT" (case-insensitive)
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
	 
	 // Map button name to XINPUT bitmask (case-insensitive)
	 WORD buttonMask = 0;
	 
	 // Convert to uppercase for case-insensitive comparison
	 char upperButton[32];
	 int i = 0;
	 for (; buttonName[i] && i < 31; ++i) {
		 char c = buttonName[i];
		 if (c >= 'a' && c <= 'z') {
			 upperButton[i] = c - 'a' + 'A';
		 } else {
			 upperButton[i] = c;
		 }
	 }
	 upperButton[i] = '\0';
	 
	 // Map button name to XINPUT bitmask
	 if (strcmp(upperButton, "A") == 0) {
		 buttonMask = XINPUT_GAMEPAD_A;
	 } else if (strcmp(upperButton, "B") == 0) {
		 buttonMask = XINPUT_GAMEPAD_B;
	 } else if (strcmp(upperButton, "X") == 0) {
		 buttonMask = XINPUT_GAMEPAD_X;
	 } else if (strcmp(upperButton, "Y") == 0) {
		 buttonMask = XINPUT_GAMEPAD_Y;
	 } else if (strcmp(upperButton, "START") == 0) {
		 buttonMask = XINPUT_GAMEPAD_START;
	 } else if (strcmp(upperButton, "BACK") == 0) {
		 buttonMask = XINPUT_GAMEPAD_BACK;
	 } else if (strcmp(upperButton, "LEFT_SHOULDER") == 0 || strcmp(upperButton, "LB") == 0) {
		 buttonMask = XINPUT_GAMEPAD_LEFT_SHOULDER;
	 } else if (strcmp(upperButton, "RIGHT_SHOULDER") == 0 || strcmp(upperButton, "RB") == 0) {
		 buttonMask = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	 } else if (strcmp(upperButton, "LEFT_THUMB") == 0 || strcmp(upperButton, "LS") == 0) {
		 buttonMask = XINPUT_GAMEPAD_LEFT_THUMB;
	 } else if (strcmp(upperButton, "RIGHT_THUMB") == 0 || strcmp(upperButton, "RS") == 0) {
		 buttonMask = XINPUT_GAMEPAD_RIGHT_THUMB;
	 } else if (strcmp(upperButton, "DPAD_UP") == 0 || strcmp(upperButton, "UP") == 0) {
		 buttonMask = XINPUT_GAMEPAD_DPAD_UP;
	 } else if (strcmp(upperButton, "DPAD_DOWN") == 0 || strcmp(upperButton, "DOWN") == 0) {
		 buttonMask = XINPUT_GAMEPAD_DPAD_DOWN;
	 } else if (strcmp(upperButton, "DPAD_LEFT") == 0 || strcmp(upperButton, "LEFT") == 0) {
		 buttonMask = XINPUT_GAMEPAD_DPAD_LEFT;
	 } else if (strcmp(upperButton, "DPAD_RIGHT") == 0 || strcmp(upperButton, "RIGHT") == 0) {
		 buttonMask = XINPUT_GAMEPAD_DPAD_RIGHT;
	 } else {
		 return luaL_error(L, "isxboxbuttonpressed: invalid button name '%s'. Valid buttons: A, B, X, Y, START, BACK, LEFT_SHOULDER, RIGHT_SHOULDER, LEFT_THUMB, RIGHT_THUMB, DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT", buttonName);
	 }
	 
	 // Check if button is pressed (bit is set)
	 bool isPressed = (buttons & buttonMask) != 0;
	 lua_pushboolean(L, isPressed ? 1 : 0);
	 return 1;
 }
 
 // isbuttonpressed(player, button) -> boolean
 // Checks if a specific button is pressed for a player
 // Button names: "A", "B", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT" (case-insensitive)
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
	 
	 // Get current button state
	 uint8 buttons = joy[player];
	 
	 // Map button name to bitmask (case-insensitive)
	 // FCEUX bit order: A=0x01, B=0x02, Select=0x04, Start=0x08, Up=0x10, Down=0x20, Left=0x40, Right=0x80
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
		 return luaL_error(L, "isbuttonpressed: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	 }
	 
	 // Check if button is pressed (bit is set)
	 bool isPressed = (buttons & buttonMask) != 0;
	 lua_pushboolean(L, isPressed ? 1 : 0);
	 return 1;
 }

 // getbuttonname(buttonMask) -> string
 // Converts button bitmask to comma-separated string of button names
 // Returns empty string if no buttons are pressed
 static int lua_getbuttonname(lua_State* L)
 {
	 int buttonMask = (int)luaL_checkinteger(L, 1);
	 
	 // Validate bitmask range
	 if (buttonMask < 0 || buttonMask > 0xFF) {
		 return luaL_error(L, "getbuttonname: buttonMask must be in range 0x00-0xFF");
	 }
	 
	 // Build comma-separated list of button names
	 // FCEUX bit order: A=0x01, B=0x02, Select=0x04, Start=0x08, Up=0x10, Down=0x20, Left=0x40, Right=0x80
	 char result[128];  // Enough space for all button names
	 result[0] = '\0';
	 int first = 1;  // Track if this is the first button (for comma handling)
	 
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
	 
	 // Return the result (empty string if no buttons)
	 lua_pushstring(L, result);
	 return 1;
 }

 // setjoypad(player, buttons) -> nothing
 // Sets controller state for a player (from Lua)
 // player: 0..3 (P1..P4), buttons: 0x00..0xFF (A=1, B=2, Select=4, Start=8, Up=16, Down=32, Left=64, Right=128)
 // Overrides the polled pad state; persists until changed or cleared.
 // Note: May conflict with joypad() callback if both are used
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
	 
	 // Apply immediately so scripts see effect right away; input poller should call FCEU_LuaJoypadApply() each frame.
	 joy[player] = (uint8)((joy[player] & ~s_luaJoypadMask[player]) |
						   (s_luaJoypadValue[player] & s_luaJoypadMask[player]));
	 
	 return 0;
 }

 // clearjoypad(player) -> nothing
 // Clears Lua joypad override for a player, allowing hardware input to work again
 // player: 0..3 (P1..P4), or -1 to clear all players
 static int lua_clearjoypad(lua_State* L)
 {
	 int player = (int)luaL_checkinteger(L, 1);
	 
	 if (player < -1 || player > 3) {
		 return luaL_error(L, "clearjoypad: player must be -1 (all) or 0..3");
	 }
	 
	 FCEU_LuaJoypadClear(player);
	 
	 return 0;
 }

 // pressbutton(player, button) -> nothing
 // Simulates pressing a button for one frame
 // player: 0..3 (P1..P4), button: string name ("A", "B", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT")
 // The button will be pressed for the current frame only, then automatically released
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

 // releasebutton(player, button) -> nothing
 // Simulates releasing a button for one frame
 // player: 0..3 (P1..P4), button: string name ("A", "B", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT")
 // The button will be released for the current frame only, then return to previous state
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

 // startinputrecording() -> boolean
 // Starts recording input for all players
 // Returns true if recording started successfully, false if already recording
 static int lua_startinputrecording(lua_State* L)
 {
	 if (s_inputRecording) {
		 lua_pushboolean(L, 0);  // false - already recording
		 return 1;
	 }
	 
	 // Clear any existing recording
	 for (int p = 0; p < 4; ++p) {
		 s_recordedInput[p].clear();
	 }
	 
	 s_inputRecording = true;
	 lua_pushboolean(L, 1);  // true - recording started
	 return 1;
 }

 // stopinputrecording() -> table
 // Stops recording input and returns recorded data as a table
 // Returns a table with keys "player0", "player1", "player2", "player3"
 // Each player's data is a table of button states (frame-by-frame)
 static int lua_stopinputrecording(lua_State* L)
 {
	 if (!s_inputRecording) {
		 return luaL_error(L, "stopinputrecording: not currently recording");
	 }
	 
	 s_inputRecording = false;
	 
	 // Create Lua table to hold all recorded data
	 lua_createtable(L, 0, 4);  // 0 array part, 4 hash part
	 
	 // For each player, create a table with their recorded input
	 for (int p = 0; p < 4; ++p) {
		 // Create table for this player's input
		 lua_createtable(L, (int)s_recordedInput[p].size(), 0);
		 
		 // Fill table with recorded input (1-indexed)
		 for (size_t i = 0; i < s_recordedInput[p].size(); ++i) {
			 lua_pushinteger(L, (int)s_recordedInput[p][i]);
			 lua_rawseti(L, -2, (int)(i + 1));  // Lua tables are 1-indexed
		 }
		 
		 // Set this player's table in the main table
		 char key[16];
		 snprintf(key, sizeof(key), "player%d", p);
		 lua_setfield(L, -2, key);
	 }
	 
	 return 1;  // Return the table
 }

 // playinputrecording(data) -> nothing
 // Plays back recorded input from a table
 // data: table from stopinputrecording() with keys "player0", "player1", "player2", "player3"
 static int lua_playinputrecording(lua_State* L)
 {
	 if (lua_gettop(L) < 1) {
		 return luaL_error(L, "playinputrecording(data) requires 1 argument");
	 }
	 
	 if (!lua_istable(L, 1)) {
		 return luaL_error(L, "playinputrecording: data must be a table");
	 }
	 
	 // Stop any current playback
	 s_inputPlayback = false;
	 s_playbackFrame = 0;
	 
	 // Clear playback data
	 for (int p = 0; p < 4; ++p) {
		 s_playbackInput[p].clear();
	 }
	 
	 // Read data from Lua table
	 for (int p = 0; p < 4; ++p) {
		 char key[16];
		 snprintf(key, sizeof(key), "player%d", p);
		 
		 // Get player's table
		 lua_getfield(L, 1, key);
		 if (lua_istable(L, -1)) {
			 // Read all entries from the table (1-indexed)
			 int i = 1;
			 while (true) {
				 lua_rawgeti(L, -1, i);
				 if (!lua_isnumber(L, -1)) {
					 lua_pop(L, 1);
					 break;  // End of table
				 }
				 int value = (int)luaL_checkinteger(L, -1);
				 lua_pop(L, 1);
				 
				 // Clamp to valid range
				 if (value < 0) value = 0;
				 if (value > 0xFF) value = 0xFF;
				 
				 s_playbackInput[p].push_back((uint8)(value & 0xFF));
				 ++i;
			 }
		 }
		 lua_pop(L, 1);  // Pop player table
	 }
	 
	 // Start playback
	 s_inputPlayback = true;
	 s_playbackFrame = 0;
	 
	 return 0;
 }

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

 // isframeadvancing() -> boolean
 // Checks if emulation is advancing frames
 // Returns true if frames are advancing, false if paused
 static int lua_isframeadvancing(lua_State* L)
 {
	 extern int EmulationPaused;
	 
	 // EmulationPaused bit 0 indicates if paused
	 // If bit 0 is set, emulation is paused (frames NOT advancing)
	 // If bit 0 is clear, emulation is running (frames ARE advancing)
	 lua_pushboolean(L, (EmulationPaused & 1) == 0);
	 return 1;
 }

 // isrewinding() -> boolean
 // Checks if currently rewinding
 // Returns true if rewinding, false otherwise
 static int lua_isrewinding(lua_State* L)
 {
	 // Access the global Cemulator instance
	 extern Cemulator emul;
	 
	 // Return rewind state
	 lua_pushboolean(L, emul.IsRewinding() ? 1 : 0);
	 return 1;
 }

 static int lua_isfastforwarding(lua_State* L)
 {
	 // Access the global Cemulator instance
	 extern Cemulator emul;
	 
	 // Return fast-forward state
	 lua_pushboolean(L, emul.IsFastForwarding() ? 1 : 0);
	 return 1;
 }

 // getgamegeniecode(address, value, compare) -> string
 // Generates Game Genie code from address, value, and optional compare
 // Returns 6-character code (no compare) or 8-character code (with compare)
 static int lua_getgamegeniecode(lua_State* L)
 {
	 // Get parameters
	 int n = lua_gettop(L);
	 if (n < 2 || n > 3) {
		 return luaL_error(L, "getgamegeniecode() requires 2 or 3 parameters: address, value, [compare]");
	 }
	 
	 // Validate and get address (must be 0x8000-0xFFFF)
	 int address = (int)luaL_checkinteger(L, 1);
	 if (address < 0x8000 || address > 0xFFFF) {
		 return luaL_error(L, "getgamegeniecode() address must be between 0x8000 and 0xFFFF");
	 }
	 
	 // Validate and get value (0-255)
	 int value = (int)luaL_checkinteger(L, 2);
	 if (value < 0 || value > 255) {
		 return luaL_error(L, "getgamegeniecode() value must be between 0 and 255");
	 }
	 
	 // Optional compare value
	 bool hasCompare = (n >= 3 && !lua_isnil(L, 3));
	 int compare = 0;
	 if (hasCompare) {
		 compare = (int)luaL_checkinteger(L, 3);
		 if (compare < 0 || compare > 255) {
			 return luaL_error(L, "getgamegeniecode() compare must be between 0 and 255");
		 }
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
	 const char* codeStr = luaL_checkstring(L, 1);
	 if (!codeStr) {
		 return luaL_error(L, "decodegamegenie() requires a string parameter");
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

 // getframecount() -> integer
 // Gets total frame count since game start
 // Returns the number of frames that have been emulated since the game started
 static int lua_getframecount(lua_State* L)
 {
	 // Use our own frame counter that increments every frame
	 // This is more reliable than FCEUMOV_GetFrame() which only works during movie playback
	 lua_pushinteger(L, s_totalFrameCount);
	 return 1;
 }

 // getframecycles() -> integer
 // Gets cycles executed in the current frame
 // Returns the number of CPU cycles accumulated since the start of the current frame
 static int lua_getframecycles(lua_State* L)
 {
	 // Get cycles from x6502.cpp - this is the timestamp variable
	 // which accumulates cycles during the frame and resets at frame end
	 uint32 cycles = FCEU_GetFrameCycles();
	 
	 // If we're after frame end (timestamp already reset to 0), use the latched value
	 // This happens when called from gui() callback which runs after the frame completes
	 if (cycles == 0) {
		 cycles = FCEU_GetLastFrameCycles();
	 }
	 
	 lua_pushinteger(L, (lua_Integer)cycles);
	 return 1;
 }

 // getelapsedtime() -> float
 // Gets elapsed time since game start in seconds
 // Returns the elapsed time as a floating-point number
 static int lua_getelapsedtime(lua_State* L)
 {
	 // Use our frame counter and divide by NTSC frame rate
	 // NTSC frame rate: 60.0988118623484 Hz
	 static const double NTSC_FRAME_RATE = 60.0988118623484;
	 
	 double elapsedTime = (double)s_totalFrameCount / NTSC_FRAME_RATE;
	 
	 lua_pushnumber(L, elapsedTime);
	 return 1;
 }

 // getelapsedframes() -> integer
 // Gets elapsed frames since game start
 // Returns the total number of frames that have elapsed since the ROM was loaded
 static int lua_getelapsedframes(lua_State* L)
 {
	 // Return the total frame count since game start
	 // This is the same as getframecount() but with a name that pairs with getelapsedtime()
	 lua_pushinteger(L, s_totalFrameCount);
	 return 1;
 }

 // gettime() -> integer
 // Gets current system time in milliseconds since system boot
 // Returns: Integer (milliseconds since system boot)
 // Use case: Time-based logic, timestamps, relative time measurements
 // Note: This returns milliseconds since system boot, not since epoch. Use for relative timing.
 static int lua_gettime(lua_State* L)
 {
	 // GetTickCount() returns milliseconds since system boot
	 // This is useful for relative time measurements and timestamps
	 DWORD currentTime = GetTickCount();
	 lua_pushinteger(L, (lua_Integer)currentTime);
	 return 1;
 }

 // getscreenwidth() -> integer
 // Gets screen width in pixels
 // Returns: Integer (256 for NES)
 // Use case: Dynamic positioning, centering
 static int lua_getscreenwidth(lua_State* L)
 {
	 // NES screen width is always 256 pixels
	 lua_pushinteger(L, OVL_W);  // OVL_W is 256
	 return 1;
 }

 // getscreenheight() -> integer
 // Gets screen height in pixels
 // Returns: Integer (240 for NES)
 // Use case: Dynamic positioning
 static int lua_getscreenheight(lua_State* L)
 {
	 // NES screen height is always 240 pixels
	 lua_pushinteger(L, OVL_H);  // OVL_H is 240
	 return 1;
 }

 // getscreensize() -> table
 // Gets screen dimensions
 // Returns: Table {width, height}
 // Use case: Screen size queries
 static int lua_getscreensize(lua_State* L)
 {
	 // Create a table with width and height
	 lua_newtable(L);
	 
	 // Push width
	 lua_pushstring(L, "width");
	 lua_pushinteger(L, OVL_W);  // 256
	 lua_settable(L, -3);
	 
	 // Push height
	 lua_pushstring(L, "height");
	 lua_pushinteger(L, OVL_H);  // 240
	 lua_settable(L, -3);
	 
	 // Also push as array indices for convenience
	 lua_pushinteger(L, 1);
	 lua_pushinteger(L, OVL_W);
	 lua_settable(L, -3);
	 
	 lua_pushinteger(L, 2);
	 lua_pushinteger(L, OVL_H);
	 lua_settable(L, -3);
	 
	 return 1;  // Return the table
 }

 // getaudioenabled() -> boolean
 // Checks if audio is enabled
 // Returns: Boolean
 // Use case: Audio-dependent scripts
 static int lua_getaudioenabled(lua_State* L)
 {
 	 lua_pushboolean(L, FSettings.SndRate != 0);
 	 return 1;
 }
 
 // getaudiosample() -> integer
 // Gets current audio sample from the final mixed buffer.
 // Returns: Integer (sample value; 32-bit signed, typically within 16-bit range)
 // Use case: Audio visualization, audio analysis
 static int lua_getaudiosample(lua_State* L)
 {
 	 // If audio is disabled, return 0 to indicate silence
 	 if (FSettings.SndRate == 0) {
 		 lua_pushinteger(L, 0);
 		 return 1;
 	 }
 
 	 int32* buffer = NULL;
 	 int count = GetSoundBuffer(&buffer);
 	 if (count > 0 && buffer) {
 		 // Return the most recent mixed sample
 		 lua_pushinteger(L, buffer[count - 1]);
 	 } else {
 		 // No samples available yet this frame
 		 lua_pushinteger(L, 0);
 	 }
 	 return 1;
 }

 // getcolorrgb(paletteIndex) -> table
 // Gets RGB values for a palette color
 // Parameters: paletteIndex (0-63)
 // Returns: Table {r, g, b} (0-255 each)
 // Use case: Color conversion, color analysis
 static int lua_getcolorrgb(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "getcolorrgb(paletteIndex) requires 1 argument");
	 }
	 
	 int paletteIndex = (int)luaL_checkinteger(L, 1);
	 
	 // Validate palette index range (0-63)
	 if (paletteIndex < 0 || paletteIndex > 63) {
		 return luaL_error(L, "getcolorrgb: paletteIndex must be in range 0-63");
	 }
	 
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
 // Gets palette color index for a position
 // Parameters: index (0-31, palette index)
 // Returns: Integer (0-63, actual color)
 // Use case: Palette reading
 static int lua_getpalettecolor(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "getpalettecolor(index) requires 1 argument");
	 }
	 
	 int index = (int)luaL_checkinteger(L, 1);
	 
	 // Validate palette index range (0-31)
	 if (index < 0 || index > 31) {
		 return luaL_error(L, "getpalettecolor: index must be in range 0-31");
	 }
	 
	 // Read palette color from PALRAM
	 uint8 colorValue = PALRAM[index] & 0x3F;  // Mask to 6 bits (0-63)
	 
	 lua_pushinteger(L, colorValue);
	 return 1;  // Return the color value
 }

 // setpalettecolor(index, color) -> void
 // Sets palette color (temporary, frame-only)
 // Parameters: index (0-31), color (0-63)
 // Returns: Nothing
 // Use case: Palette effects, color cycling
 static int lua_setpalettecolor(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "setpalettecolor(index, color) requires 2 arguments");
	 }
	 
	 int index = (int)luaL_checkinteger(L, 1);
	 int color = (int)luaL_checkinteger(L, 2);
	 
	 // Validate palette index range (0-31)
	 if (index < 0 || index > 31) {
		 return luaL_error(L, "setpalettecolor: index must be in range 0-31");
	 }
	 
	 // Validate color range (0-63)
	 if (color < 0 || color > 63) {
		 return luaL_error(L, "setpalettecolor: color must be in range 0-63");
	 }
	 
	 // Mask color to 6 bits (0x3F)
	 uint8 colorValue = (uint8)(color & 0x3F);
	 
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

 // getnescolor(index) -> integer
 // Gets NES color value (0-63) from RGB
 // Parameters: index (0-63)
 // Returns: Integer (RGB value as packed 0xRRGGBB)
 // Use case: Color lookup
 static int lua_getnescolor(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "getnescolor(index) requires 1 argument");
	 }
	 
	 int index = (int)luaL_checkinteger(L, 1);
	 
	 // Validate palette index range (0-63)
	 if (index < 0 || index > 63) {
		 return luaL_error(L, "getnescolor: index must be in range 0-63");
	 }
	 
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
 // Blends two colors
 // Parameters: color1, color2 (0-63), ratio (0.0-1.0)
 // Returns: Integer (blended color index 0-63)
 // Use case: Color mixing, gradients
 static int lua_blendcolors(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 3) {
		 return luaL_error(L, "blendcolors(color1, color2, ratio) requires 3 arguments");
	 }
	 
	 int color1 = (int)luaL_checkinteger(L, 1);
	 int color2 = (int)luaL_checkinteger(L, 2);
	 double ratio = luaL_checknumber(L, 3);
	 
	 // Validate color indices (0-63)
	 if (color1 < 0 || color1 > 63) {
		 return luaL_error(L, "blendcolors: color1 must be in range 0-63");
	 }
	 if (color2 < 0 || color2 > 63) {
		 return luaL_error(L, "blendcolors: color2 must be in range 0-63");
	 }
	 
	 // Validate ratio (0.0-1.0)
	 if (ratio < 0.0 || ratio > 1.0) {
		 return luaL_error(L, "blendcolors: ratio must be in range 0.0-1.0");
	 }
	 
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

 // gettimedelta() -> float
 // Gets time since last frame in seconds
 // Returns: Float (time in seconds since last frame)
 // Use case: Delta time calculations, physics, frame-independent movement
 // Note: Returns 0.0 on first call, then the actual delta time on subsequent calls
 static int lua_gettimedelta(lua_State* L)
 {
	 DWORD currentTime = GetTickCount();
	 
	 // Get last frame time from Lua registry
	 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 
	 if (lua_isnil(L, -1)) {
		 // First call - no previous frame time, return 0.0
		 lua_pop(L, 1);
		 
		 // Store current time for next call
		 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
		 lua_pushinteger(L, (lua_Integer)currentTime);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushnumber(L, 0.0);
		 return 1;
	 }
	 
	 // Get last frame time
	 lua_Integer lastFrameTime = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 // Calculate delta in milliseconds
	 DWORD deltaMs = currentTime - (DWORD)lastFrameTime;
	 
	 // Convert to seconds (float)
	 double deltaSeconds = (double)deltaMs / 1000.0;
	 
	 // Update last frame time for next call
	 lua_pushstring(L, "FCEU_LAST_FRAME_TIME");
	 lua_pushinteger(L, (lua_Integer)currentTime);
	 lua_settable(L, LUA_REGISTRYINDEX);
	 
	 lua_pushnumber(L, deltaSeconds);
	 return 1;
 }

 // sleepframes(frames) -> nil
 // Delays script execution for N frames
 // Parameters: frames (integer) - number of frames to sleep
 // Returns: Nothing
 // Use case: Frame-accurate delays
 // Note: Pauses emulation during sleep, freezing the game for the specified number of frames
 static int lua_sleepframes(lua_State* L)
 {
	 // Get number of frames to sleep
	 if (lua_gettop(L) < 1) {
		 return luaL_error(L, "sleepframes() requires 1 argument (frames)");
	 }
	 
	 if (!lua_isnumber(L, 1)) {
		 return luaL_error(L, "sleepframes() argument must be a number");
	 }
	 
	 lua_Integer frames = lua_tointeger(L, 1);
	 if (frames < 0) {
		 return luaL_error(L, "sleepframes() frames must be >= 0");
	 }
	 
	 // Calculate sleep duration in milliseconds
	 // NTSC frame rate: 60.0988118623484 Hz
	 // Each frame is approximately 16.639 ms
	 static const double NTSC_FRAME_RATE = 60.0988118623484;
	 static const double MS_PER_FRAME = 1000.0 / NTSC_FRAME_RATE;
	 
	 DWORD sleepDurationMs = (DWORD)(frames * MS_PER_FRAME);
	 DWORD sleepStartTime = GetTickCount();
	 
	 // Store sleep start time and duration in Lua registry
	 lua_pushstring(L, "FCEU_SLEEP_START_TIME");
	 lua_pushinteger(L, (lua_Integer)sleepStartTime);
	 lua_settable(L, LUA_REGISTRYINDEX);
	 
	 lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
	 lua_pushinteger(L, (lua_Integer)sleepDurationMs);
	 lua_settable(L, LUA_REGISTRYINDEX);
	 
	 // Store original pause state so we can restore it
	 extern int FCEUI_EmulationPaused(void);
	 int wasPaused = FCEUI_EmulationPaused();
	 lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
	 lua_pushboolean(L, wasPaused != 0);
	 lua_settable(L, LUA_REGISTRYINDEX);
	 
	 // Pause emulation to freeze the game during sleep
	 extern void FCEUI_SetEmulationPaused(int val);
	 FCEUI_SetEmulationPaused(1);
	 
	 return 0; // Returns nothing
 }

 // Helper function to check if script is currently sleeping
 // Returns true if script should skip execution, false otherwise
 // Also handles unpausing emulation when sleep completes
 static bool Lua_IsSleeping(lua_State* L)
 {
	 // Check if sleep is active
	 lua_pushstring(L, "FCEU_SLEEP_START_TIME");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 
	 if (lua_isnil(L, -1)) {
		 // No sleep state - not sleeping
		 lua_pop(L, 1);
		 return false;
	 }
	 
	 // Get sleep start time and duration
	 lua_Integer sleepStartTime = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 lua_Integer sleepDurationMs = lua_tointeger(L, -1);
	 lua_pop(L, 1);
	 
	 // Get original pause state
	 lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
	 lua_gettable(L, LUA_REGISTRYINDEX);
	 int wasPaused = lua_toboolean(L, -1);
	 lua_pop(L, 1);
	 
	 // Check if sleep duration has elapsed
	 DWORD currentTime = GetTickCount();
	 DWORD elapsed = currentTime - (DWORD)sleepStartTime;
	 
	 if (elapsed >= (DWORD)sleepDurationMs) {
		 // Sleep complete - clear sleep state and restore pause state
		 lua_pushstring(L, "FCEU_SLEEP_START_TIME");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushstring(L, "FCEU_SLEEP_DURATION_MS");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 lua_pushstring(L, "FCEU_SLEEP_WAS_PAUSED");
		 lua_pushnil(L);
		 lua_settable(L, LUA_REGISTRYINDEX);
		 
		 // Restore original pause state
		 extern void FCEUI_SetEmulationPaused(int val);
		 FCEUI_SetEmulationPaused(wasPaused ? 1 : 0);
		 
		 return false; // Sleep complete
	 }
	 
	 // Still sleeping - ensure emulation is paused
	 extern void FCEUI_SetEmulationPaused(int val);
	 FCEUI_SetEmulationPaused(1);
	 
	 return true; // Still sleeping
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
 // Returns mapper name, or "Unknown" if mapper is not recognized
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
			 // For unknown mappers, return "Unknown" or format as "Mapper X"
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

 // getbuttonmask(buttonName) -> integer bitmask
 // Converts button name to bitmask
 // Button names: "A", "B", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT" (case-insensitive)
 static int lua_getbuttonmask(lua_State* L)
 {
	 const char* buttonName = luaL_checkstring(L, 1);
	 
	 if (!buttonName || !buttonName[0]) {
		 return luaL_error(L, "getbuttonmask: button name cannot be empty");
	 }
	 
	 // Map button name to bitmask (case-insensitive)
	 // FCEUX bit order: A=0x01, B=0x02, Select=0x04, Start=0x08, Up=0x10, Down=0x20, Left=0x40, Right=0x80
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
		 return luaL_error(L, "getbuttonmask: invalid button name '%s'. Valid buttons: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT", buttonName);
	 }
	 
	 // Return the bitmask
	 lua_pushinteger(L, (int)buttonMask);
	 return 1;
 }

 // drawtextbox(x, y, width, height, text, color, bgColor, borderColor)
 // Draws text in a bordered box with background
 // bgColor and borderColor are optional (nil or -1 for no background/border)
 static int lua_drawtextbox(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return luaL_error(L, "drawtextbox(x, y, width, height, text, color [, bgColor, borderColor]) requires at least 6 arguments");
	 }

	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int width = (int)luaL_checkinteger(L, 3);
	 int height = (int)luaL_checkinteger(L, 4);
	 const char* text = luaL_checkstring(L, 5);
	 int color = (int)luaL_checkinteger(L, 6);
	 
	 // Optional parameters
	 int bgColor = -1;
	 int borderColor = -1;
	 if (n >= 7 && !lua_isnil(L, 7)) {
		 bgColor = (int)luaL_checkinteger(L, 7);
	 }
	 if (n >= 8 && !lua_isnil(L, 8)) {
		 borderColor = (int)luaL_checkinteger(L, 8);
	 }

	 if (!currentXBuf || !text) return 0;

	 // Clamp dimensions
	 if (width <= 0 || height <= 0) return 0;
	 if (x < 0) x = 0;
	 if (y < 0) y = 0;
	 if (x + width > OVL_W) width = OVL_W - x;
	 if (y + height > OVL_H) height = OVL_H - y;
	 if (width <= 0 || height <= 0) return 0;

	 // Border thickness (3 pixels as requested for testing)
	 const int borderThickness = 3;

	 // Calculate inner area (accounting for border)
	 int innerX = x;
	 int innerY = y;
	 int innerW = width;
	 int innerH = height;
	 
	 if (borderColor >= 0) {
		 innerX += borderThickness;
		 innerY += borderThickness;
		 innerW -= borderThickness * 2;
		 innerH -= borderThickness * 2;
		 
		 // Ensure inner area is valid
		 if (innerW <= 0 || innerH <= 0) {
			 // Box too small for border, just draw border
			 innerW = 0;
			 innerH = 0;
		 }
	 }

	 // Draw background (if specified)
	 if (bgColor >= 0 && innerW > 0 && innerH > 0) {
		 uint8 mappedBg = map_overlay_color(bgColor);
		 for (int py = innerY; py < innerY + innerH && py < OVL_H; ++py) {
			 if (py < 0) continue;
			 for (int px = innerX; px < innerX + innerW && px < OVL_W; ++px) {
				 if (px < 0) continue;
				 if (!is_point_clipped(px, py)) {
					 uint8* dest = currentXBuf + py * OVL_W + px;
					 *dest = apply_blend_mode(*dest, mappedBg);
				 }
			 }
		 }
	 }

	 // Draw border (if specified) - 3 pixel thick border
	 if (borderColor >= 0) {
		 uint8 mappedBorder = map_overlay_color(borderColor);
		 
		 // Draw border as filled rectangles on each side
		 // Top border
		 for (int by = 0; by < borderThickness && by < height; ++by) {
			 int py = y + by;
			 if (py >= 0 && py < OVL_H) {
				 for (int px = x; px < x + width && px < OVL_W; ++px) {
					 if (px >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Bottom border
		 for (int by = height - borderThickness; by < height; ++by) {
			 int py = y + by;
			 if (py >= 0 && py < OVL_H) {
				 for (int px = x; px < x + width && px < OVL_W; ++px) {
					 if (px >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Left border
		 for (int bx = 0; bx < borderThickness && bx < width; ++bx) {
			 int px = x + bx;
			 if (px >= 0 && px < OVL_W) {
				 for (int py = y; py < y + height && py < OVL_H; ++py) {
					 if (py >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
		 
		 // Right border
		 for (int bx = width - borderThickness; bx < width; ++bx) {
			 int px = x + bx;
			 if (px >= 0 && px < OVL_W) {
				 for (int py = y; py < y + height && py < OVL_H; ++py) {
					 if (py >= 0 && !is_point_clipped(px, py)) {
						 uint8* dest = currentXBuf + py * OVL_W + px;
						 *dest = apply_blend_mode(*dest, mappedBorder);
					 }
				 }
			 }
		 }
	 }

	 // Draw text inside the box (with padding)
	 if (innerW > 0 && innerH > 0 && text && *text) {
		 int textX = innerX + 2;  // 2 pixel padding
		 int textY = innerY + 2;  // 2 pixel padding
		 int textW = innerW - 4;  // Account for padding on both sides
		 int textH = innerH - 4;  // Account for padding on both sides
		 
		 if (textW > 0 && textH > 0 && textX >= 0 && textY >= 0 && textX < OVL_W && textY < OVL_H) {
			 uint8 mapped = map_overlay_color(color);
			 uint8* dest = currentXBuf + textY * OVL_W + textX;
			 
			 // Use DrawTextTransWH with border=0 (no text border, we have box border)
			 DrawTextTransWH(dest, OVL_W, (uint8*)text, mapped, textW, textH, 0);
		 }
	 }

	 g_overlayDirty = true;
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a single pixel
 int lua_drawpixel(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 3) {
		 return luaL_error(L, "drawpixel(x, y, color) requires 3 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int color = (int)luaL_checkinteger(L, 3);
	 
	 if (!currentXBuf) return 0;
	 
	 // Early return if completely off-screen (better performance and safety)
	 if (x < 0 || x >= OVL_W || y < 0 || y >= OVL_H) return 0;
	 
	 // Draw pixel on the current frame buffer (set by FCEU_LuaGui)
	 // Additional defensive check on buffer pointer
	 if (y * OVL_W + x < 0 || y * OVL_W + x >= OVL_W * OVL_H) return 0;
	 
	 // Check clipping
	 if (is_point_clipped(x, y)) return 0;
	 
	 uint8 *dest = currentXBuf + y * OVL_W + x;
		 uint8 srcColor = map_overlay_color(color);
		 *dest = apply_blend_mode(*dest, srcColor);
		 g_overlayDirty = true;  // Mark that something was drawn
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a line between two points
 int lua_drawline(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return luaL_error(L, "drawline(x1, y1, x2, y2, color) requires 5 arguments");
	 }
	 
	 int x1 = (int)luaL_checkinteger(L, 1);
	 int y1 = (int)luaL_checkinteger(L, 2);
	 int x2 = (int)luaL_checkinteger(L, 3);
	 int y2 = (int)luaL_checkinteger(L, 4);
	 int color = (int)luaL_checkinteger(L, 5);
	 
	 if (!currentXBuf) return 0;
	 
	 // Early return if both points are completely off-screen
	 if ((x1 < 0 || x1 >= OVL_W || y1 < 0 || y1 >= OVL_H) &&
	     (x2 < 0 || x2 >= OVL_W || y2 < 0 || y2 >= OVL_H)) {
		 return 0; // Both points off-screen, skip drawing
	 }
	 
	 // Clamp coordinates to safe bounds (for line algorithm, we'll check bounds in the loop)
	 // But we still need to clamp to prevent integer overflow in calculations
	 if (x1 < -OVL_W) x1 = -OVL_W;
	 if (x1 > OVL_W * 2) x1 = OVL_W * 2;
	 if (x2 < -OVL_W) x2 = -OVL_W;
	 if (x2 > OVL_W * 2) x2 = OVL_W * 2;
	 if (y1 < -OVL_H) y1 = -OVL_H;
	 if (y1 > OVL_H * 2) y1 = OVL_H * 2;
	 if (y2 < -OVL_H) y2 = -OVL_H;
	 if (y2 > OVL_H * 2) y2 = OVL_H * 2;
	 
	 // Use Bresenham's line algorithm to draw the line
	 int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	 int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	 int sx = (x1 < x2) ? 1 : -1;
	 int sy = (y1 < y2) ? 1 : -1;
	 int err = dx - dy;
	 
	 int x = x1;
	 int y = y1;
	 bool drewSomething = false;
	 
	 // Safety limit to prevent infinite loops (max pixels we could draw)
	 int maxIterations = (OVL_W + OVL_H) * 2;
	 int iterations = 0;
	 
	 while (iterations < maxIterations) {
		 iterations++;
		 
		 // Check bounds and draw pixel
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 // Additional defensive check on buffer pointer
				 if (y * OVL_W + x >= 0 && y * OVL_W + x < OVL_W * OVL_H) {
					 // Check clipping
					 if (!is_point_clipped(x, y)) {
						 uint8 *dest = currentXBuf + y * OVL_W + x;
						 uint8 srcColor = map_overlay_color(color);
						 *dest = apply_blend_mode(*dest, srcColor);
						 drewSomething = true;
					 }
				 }
			 }
		 
		 // Check if we've reached the end point
		 if (x == x2 && y == y2) break;
		 
		 int e2 = 2 * err;
		 
		 if (e2 > -dy) {
			 err -= dy;
			 x += sx;
		 }
		 
		 if (e2 < dx) {
			 err += dx;
			 y += sy;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
 }

 int lua_drawthickline(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return luaL_error(L, "drawthickline(x1, y1, x2, y2, thickness, color) requires 6 arguments");
	 }
	 
	 int x1 = (int)luaL_checkinteger(L, 1);
	 int y1 = (int)luaL_checkinteger(L, 2);
	 int x2 = (int)luaL_checkinteger(L, 3);
	 int y2 = (int)luaL_checkinteger(L, 4);
	 int thickness = (int)luaL_checkinteger(L, 5);
	 int color = (int)luaL_checkinteger(L, 6);
	 
	 if (!currentXBuf) return 0;
	 
	 if (thickness < 1) thickness = 1;
	 if (thickness > 50) thickness = 50; // Reasonable max to prevent performance issues
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x1 < 0) x1 = 0;
	 if (x1 >= OVL_W) x1 = OVL_W - 1;
	 if (x2 < 0) x2 = 0;
	 if (x2 >= OVL_W) x2 = OVL_W - 1;
	 if (y1 < 0) y1 = 0;
	 if (y1 >= OVL_H) y1 = OVL_H - 1; // Clamp to safe position
	 if (y2 < 0) y2 = 0;
	 if (y2 >= OVL_H) y2 = OVL_H - 1; // Clamp to safe position
	 
	 // Calculate line direction
	 int dx = x2 - x1;
	 int dy = y2 - y1;
	 
	 // For very short lines or same point, just draw a filled circle
	 if ((dx == 0 && dy == 0) || (abs(dx) <= 1 && abs(dy) <= 1)) {
		 int radius = (thickness - 1) / 2;
		 // Draw a filled circle at the point
		 for (int cy = y1 - radius; cy <= y1 + radius; ++cy) {
			 if (cy < 0 || cy >= OVL_H) continue;
			 for (int cx = x1 - radius; cx <= x1 + radius; ++cx) {
				 if (cx < 0 || cx >= OVL_W) continue;
				 int distSq = (cx - x1) * (cx - x1) + (cy - y1) * (cy - y1);
				 if (distSq <= radius * radius) {
					 // Check clipping
					 if (!is_point_clipped(cx, cy)) {
						 uint8 *dest = currentXBuf + cy * OVL_W + cx;
						 *dest = apply_blend_mode(*dest, mappedColor);
					 }
				 }
			 }
		 }
		 g_overlayDirty = true;
		 return 0;
	 }
	 
	 // Use Bresenham's line algorithm to draw the line, drawing perpendicular lines at each point
	 int absDx = (dx > 0) ? dx : -dx;
	 int absDy = (dy > 0) ? dy : -dy;
	 int sx = (x1 < x2) ? 1 : -1;
	 int sy = (y1 < y2) ? 1 : -1;
	 int err = absDx - absDy;
	 
	 int x = x1;
	 int y = y1;
	 bool drewSomething = false;
	 
	 // Calculate perpendicular direction (normalized)
	 double lineLen = sqrt((double)(dx * dx + dy * dy));
	 if (lineLen < 0.1) lineLen = 1.0; // Avoid division by zero
	 double perpX = -dy / lineLen;
	 double perpY = dx / lineLen;
	 
	 int halfThick = thickness / 2;
	 int maxSteps = (absDx > absDy ? absDx : absDy) * 3 + 100; // Safety limit
	 int steps = 0;
	 
	 while (steps < maxSteps) {
		 // Draw a perpendicular line segment at this point
		 for (int t = -halfThick; t <= halfThick; ++t) {
			 int px = (int)(x + perpX * t + 0.5);
			 int py = (int)(y + perpY * t + 0.5);
			 
			 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(px, py)) {
					 uint8 *dest = currentXBuf + py * OVL_W + px;
					 *dest = apply_blend_mode(*dest, mappedColor);
					 drewSomething = true;
				 }
			 }
		 }
		 
		 // Check if we've reached the end point
		 if (x == x2 && y == y2) break;
		 
		 int e2 = 2 * err;
		 int oldX = x;
		 int oldY = y;
		 
		 if (e2 > -absDy) {
			 err -= absDy;
			 x += sx;
		 }
		 
		 if (e2 < absDx) {
			 err += absDx;
			 y += sy;
		 }
		 
		 // Safety: if we didn't move at all, break to prevent infinite loop
		 if (x == oldX && y == oldY) break;
		 
		 steps++;
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;
	 }
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a polygon outline
 int lua_drawpolygon(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4 || (n % 2) == 0) {
		 return luaL_error(L, "drawpolygon(x1, y1, x2, y2, ..., color) requires at least 4 arguments (pairs of x,y coordinates plus color)");
	 }
	 
	 if (!currentXBuf) return 0;
	 
	 // Last argument is color
	 int color = (int)luaL_checkinteger(L, n);
	 uint8 mappedColor = map_overlay_color(color);
	 
	 // Number of points (excluding color)
	 int pointCount = (n - 1) / 2;
	 if (pointCount < 2) {
		 return luaL_error(L, "drawpolygon requires at least 2 points");
	 }
	 
	 bool drewSomething = false;
	 
		 // Draw lines connecting consecutive points, then close the polygon
		 for (int i = 0; i < pointCount; ++i) {
		 int x1 = (int)luaL_checkinteger(L, i * 2 + 1);
		 int y1 = (int)luaL_checkinteger(L, i * 2 + 2);
		 
		 // Next point (wraps around for last point)
		 int nextIdx = (i + 1) % pointCount;
		 int x2 = (int)luaL_checkinteger(L, nextIdx * 2 + 1);
		 int y2 = (int)luaL_checkinteger(L, nextIdx * 2 + 2);
		 
		 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
		 if (x1 < 0) x1 = 0;
		 if (x1 >= OVL_W) x1 = OVL_W - 1;
		 if (x2 < 0) x2 = 0;
		 if (x2 >= OVL_W) x2 = OVL_W - 1;
		 if (y1 < 0) y1 = 0;
		 if (y1 >= OVL_H) y1 = OVL_H - 1; // Clamp to safe position
		 if (y2 < 0) y2 = 0;
		 if (y2 >= OVL_H) y2 = OVL_H - 1; // Clamp to safe position
		 
		 // Handle same-point case (degenerate segment)
		 if (x1 == x2 && y1 == y2) {
			 if (x1 >= 0 && x1 < OVL_W && y1 >= 0 && y1 < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(x1, y1)) {
					 uint8 *dest = currentXBuf + y1 * OVL_W + x1;
					 *dest = apply_blend_mode(*dest, mappedColor);
					 drewSomething = true;
				 }
			 }
			 continue;
		 }
		 
		 // Use Bresenham's line algorithm to draw the line segment (same as drawline)
		 int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
		 int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
		 int sx = (x1 < x2) ? 1 : -1;
		 int sy = (y1 < y2) ? 1 : -1;
		 int err = dx - dy;
		 
		 int x = x1;
		 int y = y1;
		 int maxSteps = (dx > dy ? dx : dy) * 3 + 100; // Safety limit (generous to prevent infinite loops)
		 int steps = 0;
		 
		 while (steps < maxSteps) {
			 // Check bounds and draw pixel
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(x, y)) {
					 uint8 *dest = currentXBuf + y * OVL_W + x;
					 *dest = apply_blend_mode(*dest, mappedColor);
					 drewSomething = true;
				 }
			 }
			 
			 // Check if we've reached the end point
			 if (x == x2 && y == y2) break;
			 
			 int e2 = 2 * err;
			 int oldX = x;
			 int oldY = y;
			 
			 if (e2 > -dy) {
				 err -= dy;
				 x += sx;
			 }
			 
			 if (e2 < dx) {
				 err += dx;
				 y += sy;
			 }
			 
			 // Safety: if we didn't move at all, break to prevent infinite loop
			 if (x == oldX && y == oldY) break;
			 
			 steps++;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;
	 }
	 
	 return 0;
 }

 int lua_drawpolyline(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4 || (n % 2) == 0) {
		 return luaL_error(L, "drawpolyline(x1, y1, x2, y2, ..., color) requires at least 4 arguments (pairs of x,y coordinates plus color)");
	 }
	 
	 if (!currentXBuf) return 0;
	 
	 // Last argument is color
	 int color = (int)luaL_checkinteger(L, n);
	 uint8 mappedColor = map_overlay_color(color);
	 
	 // Number of points (excluding color)
	 int pointCount = (n - 1) / 2;
	 if (pointCount < 2) {
		 return luaL_error(L, "drawpolyline requires at least 2 points");
	 }
	 
	 bool drewSomething = false;
	 
	 // Draw lines connecting consecutive points (does NOT close the shape like drawpolygon)
	 for (int i = 0; i < pointCount - 1; ++i) {
		 int x1 = (int)luaL_checkinteger(L, i * 2 + 1);
		 int y1 = (int)luaL_checkinteger(L, i * 2 + 2);
		 
		 // Next point (don't wrap around - this is an open path)
		 int nextIdx = i + 1;
		 int x2 = (int)luaL_checkinteger(L, nextIdx * 2 + 1);
		 int y2 = (int)luaL_checkinteger(L, nextIdx * 2 + 2);
		 
		 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
		 if (x1 < 0) x1 = 0;
		 if (x1 >= OVL_W) x1 = OVL_W - 1;
		 if (x2 < 0) x2 = 0;
		 if (x2 >= OVL_W) x2 = OVL_W - 1;
		 if (y1 < 0) y1 = 0;
		 if (y1 >= OVL_H) y1 = OVL_H - 1; // Clamp to safe position
		 if (y2 < 0) y2 = 0;
		 if (y2 >= OVL_H) y2 = OVL_H - 1; // Clamp to safe position
		 
		 // Handle same-point case (degenerate segment)
		 if (x1 == x2 && y1 == y2) {
			 if (x1 >= 0 && x1 < OVL_W && y1 >= 0 && y1 < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(x1, y1)) {
					 uint8 *dest = currentXBuf + y1 * OVL_W + x1;
					 *dest = apply_blend_mode(*dest, mappedColor);
					 drewSomething = true;
				 }
			 }
			 continue;
		 }
		 
		 // Use Bresenham's line algorithm to draw the line segment (same as drawline)
		 int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
		 int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
		 int sx = (x1 < x2) ? 1 : -1;
		 int sy = (y1 < y2) ? 1 : -1;
		 int err = dx - dy;
		 
		 int x = x1;
		 int y = y1;
		 int maxSteps = (dx > dy ? dx : dy) * 3 + 100; // Safety limit (generous to prevent infinite loops)
		 int steps = 0;
		 
		 while (steps < maxSteps) {
			 // Check bounds and draw pixel
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(x, y)) {
					 uint8 *dest = currentXBuf + y * OVL_W + x;
					 *dest = apply_blend_mode(*dest, mappedColor);
					 drewSomething = true;
				 }
			 }
			 
			 // Check if we've reached the end point
			 if (x == x2 && y == y2) break;
			 
			 int e2 = 2 * err;
			 int oldX = x;
			 int oldY = y;
			 
			 if (e2 > -dy) {
				 err -= dy;
				 x += sx;
			 }
			 
			 if (e2 < dx) {
				 err += dx;
				 y += sy;
			 }
			 
			 // Safety: if we didn't move at all, break to prevent infinite loop
			 if (x == oldX && y == oldY) break;
			 
			 steps++;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;
	 }
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a filled polygon
 int lua_fillpolygon(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4 || (n % 2) == 0) {
		 return luaL_error(L, "fillpolygon(x1, y1, x2, y2, ..., color) requires at least 4 arguments (pairs of x,y coordinates plus color)");
	 }
	 
	 if (!currentXBuf) return 0;
	 
	 // Last argument is color
	 int color = (int)luaL_checkinteger(L, n);
	 uint8 mappedColor = map_overlay_color(color);
	 
	 // Number of points (excluding color)
	 int pointCount = (n - 1) / 2;
	 if (pointCount < 3) {
		 return luaL_error(L, "fillpolygon requires at least 3 points");
	 }
	 
	 // Collect and clamp all vertices
	 int* xCoords = new int[pointCount];
	 int* yCoords = new int[pointCount];
	 
	 for (int i = 0; i < pointCount; ++i) {
		 int x = (int)luaL_checkinteger(L, i * 2 + 1);
		 int y = (int)luaL_checkinteger(L, i * 2 + 2);
		 
		 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
		 if (x < 0) x = 0;
		 if (x >= OVL_W) x = OVL_W - 1;
		 if (y < 0) y = 0;
		 if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
		 
		 xCoords[i] = x;
		 yCoords[i] = y;
	 }
	 
	 // Find bounding box
	 int minY = yCoords[0];
	 int maxY = yCoords[0];
	 for (int i = 1; i < pointCount; ++i) {
		 if (yCoords[i] < minY) minY = yCoords[i];
		 if (yCoords[i] > maxY) maxY = yCoords[i];
	 }
	 
	 // Clamp to safe bounds
	 if (minY < 0) minY = 0;
	 if (maxY >= OVL_H) maxY = OVL_H - 1;
	 if (minY > maxY || minY >= OVL_H || maxY < 0) {
		 delete[] xCoords;
		 delete[] yCoords;
		 return 0;
	 }
	 
	 bool drewSomething = false;
	 
	 // Scanline fill using even-odd rule
	 for (int y = minY; y <= maxY; ++y) {
		 if (y < 0 || y >= OVL_H) continue;
		 
		 // Find all intersections with polygon edges at this y
		 int intersections[256]; // Max 256 intersections (should never need that many)
		 int intersectionCount = 0;
		 
		 for (int i = 0; i < pointCount; ++i) {
			 int nextIdx = (i + 1) % pointCount;
			 int y1 = yCoords[i];
			 int y2 = yCoords[nextIdx];
			 
			 // Check if edge crosses this scanline
			 if ((y1 < y && y2 >= y) || (y2 < y && y1 >= y)) {
				 if (y1 != y2) {
					 int x1 = xCoords[i];
					 int x2 = xCoords[nextIdx];
					 
					 // Calculate x intersection using linear interpolation
					 int x = x1 + ((x2 - x1) * (y - y1)) / (y2 - y1);
					 
					 if (x >= 0 && x < 256) {
						 intersections[intersectionCount++] = x;
					 }
				 }
			 }
		 }
		 
		 // Sort intersections by x
		 for (int i = 0; i < intersectionCount - 1; ++i) {
			 for (int j = i + 1; j < intersectionCount; ++j) {
				 if (intersections[i] > intersections[j]) {
					 int temp = intersections[i];
					 intersections[i] = intersections[j];
					 intersections[j] = temp;
				 }
			 }
		 }
		 
		 // Fill between pairs (even-odd rule)
		 for (int i = 0; i < intersectionCount - 1; i += 2) {
			 int xStart = intersections[i];
			 int xEnd = intersections[i + 1];
			 
			 if (xStart < 0) xStart = 0;
			 if (xEnd >= OVL_W) xEnd = OVL_W - 1;
			 
			 for (int x = xStart; x <= xEnd; ++x) {
				 if (x >= 0 && x < OVL_W) {
					 // Check clipping
					 if (!is_point_clipped(x, y)) {
						 uint8 *dest = currentXBuf + y * OVL_W + x;
						 *dest = apply_blend_mode(*dest, mappedColor);
						 drewSomething = true;
					 }
				 }
			 }
		 }
	 }
	 
	 delete[] xCoords;
	 delete[] yCoords;
	 
	 if (drewSomething) {
		 g_overlayDirty = true;
	 }
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a rectangle outline
 int lua_drawrect(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return luaL_error(L, "drawrect(x, y, w, h, color) requires 5 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int w = (int)luaL_checkinteger(L, 3);
	 int h = (int)luaL_checkinteger(L, 4);
	 int color = (int)luaL_checkinteger(L, 5);
	 
	 if (!currentXBuf) return 0;
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > OVL_H) h = OVL_H - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 bool drewSomething = false;
	 
	 // Draw top and bottom horizontal lines
	 for (int dx = 0; dx < w; ++dx) {
		 int px = x + dx;
		 
		 // Top line
		 if (px >= 0 && px < OVL_W && y >= 0 && y < OVL_H) {
			 // Check clipping
			 if (!is_point_clipped(px, y)) {
				 uint8 *dest = currentXBuf + y * OVL_W + px;
				 *dest = apply_blend_mode(*dest, mappedColor);
				 drewSomething = true;
			 }
		 }
		 
		 // Bottom line
		 if (px >= 0 && px < OVL_W && (y + h - 1) >= 0 && (y + h - 1) < OVL_H) {
			 // Check clipping
			 if (!is_point_clipped(px, y + h - 1)) {
				 uint8 *dest = currentXBuf + (y + h - 1) * OVL_W + px;
				 *dest = apply_blend_mode(*dest, mappedColor);
				 drewSomething = true;
			 }
		 }
	 }
	 
	 // Draw left and right vertical lines
	 for (int dy = 1; dy < h - 1; ++dy) {  // Start at 1, end at h-1 to avoid redrawing corners
		 int py = y + dy;
		 
		 // Left line
		 if (x >= 0 && x < OVL_W && py >= 0 && py < OVL_H) {
			 // Check clipping
			 if (!is_point_clipped(x, py)) {
				 uint8 *dest = currentXBuf + py * OVL_W + x;
				 *dest = apply_blend_mode(*dest, mappedColor);
				 drewSomething = true;
			 }
		 }
		 
		 // Right line
		 if ((x + w - 1) >= 0 && (x + w - 1) < OVL_W && py >= 0 && py < OVL_H) {
			 // Check clipping
			 if (!is_point_clipped(x + w - 1, py)) {
				 uint8 *dest = currentXBuf + py * OVL_W + (x + w - 1);
				 *dest = apply_blend_mode(*dest, mappedColor);
				 drewSomething = true;
			 }
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a filled rectangle
 int lua_fillrect(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return luaL_error(L, "fillrect(x, y, w, h, color) requires 5 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int w = (int)luaL_checkinteger(L, 3);
	 int h = (int)luaL_checkinteger(L, 4);
	 int color = (int)luaL_checkinteger(L, 5);
	 
	 if (!currentXBuf) return 0;
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > OVL_H) h = OVL_H - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 bool drewSomething = false;
	 
	 // Clamp rectangle to screen bounds
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + w > OVL_W) ? OVL_W : (x + w);
	 int endY = (y + h > OVL_H) ? OVL_H : (y + h);
	 
	 // Adjust start positions if rectangle is completely off-screen
	 if (startX >= OVL_W || startY >= OVL_H || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 // Fill the rectangle row by row
	 for (int py = startY; py < endY; ++py) {
		 for (int px = startX; px < endX; ++px) {
			 // Check clipping
			 if (!is_point_clipped(px, py)) {
				 uint8 *dest = currentXBuf + py * OVL_W + px;
				 *dest = apply_blend_mode(*dest, mappedColor);
				 drewSomething = true;
			 }
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to clear a rectangle area (make transparent)
 int lua_clearrect(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4) {
		 return luaL_error(L, "clearrect(x, y, w, h) requires 4 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int w = (int)luaL_checkinteger(L, 3);
	 int h = (int)luaL_checkinteger(L, 4);
	 
	 if (!currentXBuf) return 0;
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > OVL_H) h = OVL_H - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 // Clamp rectangle to screen bounds
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + w > OVL_W) ? OVL_W : (x + w);
	 int endY = (y + h > OVL_H) ? OVL_H : (y + h);
	 
	 // Adjust start positions if rectangle is completely off-screen
	 if (startX >= OVL_W || startY >= OVL_H || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 bool clearedSomething = false;
	 
	 // Clear the rectangle row by row (set to 0 = transparent)
	 for (int py = startY; py < endY; ++py) {
		 for (int px = startX; px < endX; ++px) {
			 uint8 *dest = currentXBuf + py * OVL_W + px;
			 *dest = 0;  // 0 means transparent (won't overwrite NES frame)
			 clearedSomething = true;
		 }
	 }
	 
	 if (clearedSomething) {
		 g_overlayDirty = true;  // Mark that something was changed
	 }
	 
	 return 0;
}

// Lua drawing function - clears entire overlay screen
int lua_clearscreen(lua_State *L) {
	 if (!currentXBuf) return 0;
	 
	 // Clear entire overlay buffer (set all pixels to 0 = transparent)
	 memset(currentXBuf, 0, OVL_W * OVL_H);
	 g_overlayDirty = true;  // Mark that something was changed
	 
	 return 0;
}

// Lua drawing function - fills entire overlay screen with color
int lua_fillscreen(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "fillscreen(color) requires 1 argument");
	 }
	 
	 int color = (int)luaL_checkinteger(L, 1);
	 
	 if (!currentXBuf) return 0;
	 
	 // Map color to overlay format (0x00-0x3F -> 0x80-0xBF)
	 uint8 mappedColor = map_overlay_color(color);
	 
	 // Fill entire overlay buffer (respects blending modes like fillrect)
	 for (int y = 0; y < OVL_H; ++y) {
		 for (int x = 0; x < OVL_W; ++x) {
			 uint8 *dest = currentXBuf + y * OVL_W + x;
			 *dest = apply_blend_mode(*dest, mappedColor);
		 }
	 }
	 
	 g_overlayDirty = true;  // Mark that something was drawn
	 
	 return 0;
}

// Lua function - takes screenshot with optional filename
int lua_screenshot(lua_State *L) {
	int n = lua_gettop(L);
	
	// Use s_frameXBuf which is set by FCEU_LuaGui with the actual frame buffer
	// This ensures we're capturing the correct frame data (not the overlay)
	extern uint8 *XBuf;
	extern uint8 *s_frameXBuf;
	
	// Save the original XBuf pointer
	uint8 *oldXBuf = XBuf;
	
	// Use s_frameXBuf if available (set by FCEU_LuaGui with current frame)
	// This is the bitmap buffer passed to FCEU_LuaGui, which should be the current frame
	if (s_frameXBuf) {
		XBuf = s_frameXBuf;
	} else if (!XBuf) {
		return luaL_error(L, "screenshot() failed: XBuf not available");
	}
	// If XBuf is already set and s_frameXBuf is NULL, use XBuf directly
	
	char filename[512] = {0};
	int result = 0;
	
	if (n >= 1 && !lua_isnil(L, 1)) {
		// Filename provided - use it
		const char *customFilename = luaL_checkstring(L, 1);
		if (!customFilename || strlen(customFilename) == 0) {
			return luaL_error(L, "screenshot() failed: filename cannot be empty");
		}
		
		// Get snapshot directory - cache the path calculation to avoid repeated FCEU_MakeFName calls
		static std::string cachedSnapPath = "";
		static bool snapPathCached = false;
		
		std::string snapPath;
		if (!snapPathCached) {
			// Calculate directory path once and cache it
			extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
			std::string tempPath = FCEU_MakeFName(2, 0, "png");  // 2 = FCEUMKF_SNAP
			
			// Extract directory from the generated path
			size_t lastSlash = tempPath.find_last_of("\\/");
			if (lastSlash != std::string::npos) {
				cachedSnapPath = tempPath.substr(0, lastSlash + 1);
			} else {
				// Fallback: use current directory
				cachedSnapPath = ".\\";
			}
			snapPathCached = true;
		}
		snapPath = cachedSnapPath;
		
		// Ensure directory exists (Windows/Xbox) - cache result to avoid repeated checks
		// Pre-create directory on first use to avoid lag during screenshot
		static std::string lastCheckedDir = "";
		static bool dirInitialized = false;
		
		// Convert path separators if needed
		std::string dirPath = snapPath;
		// Replace forward slashes with backslashes for Windows
		for (size_t i = 0; i < dirPath.length(); i++) {
			if (dirPath[i] == '/') {
				dirPath[i] = '\\';
			}
		}
		// Remove trailing backslash for CreateDirectoryA
		if (dirPath.length() > 0 && (dirPath[dirPath.length() - 1] == '\\' || dirPath[dirPath.length() - 1] == '/')) {
			dirPath = dirPath.substr(0, dirPath.length() - 1);
		}
		
		// Only create directory once (on first screenshot) to avoid lag
		if (!dirInitialized || dirPath != lastCheckedDir) {
			// Create directory if it doesn't exist (Windows/Xbox API)
			// CreateDirectoryA is available via stdafx.h on Xbox
			// This is fast even if directory already exists
			CreateDirectoryA(dirPath.c_str(), NULL);
			lastCheckedDir = dirPath;
			dirInitialized = true;
		}
		
		// Copy base filename
		std::string baseFilename = customFilename;
		
		// Add .png extension if not present
		if (baseFilename.length() < 4 || baseFilename.substr(baseFilename.length() - 4) != ".png") {
			baseFilename += ".png";
		}
		
		// Build full path (ensure backslash separator)
		std::string fullPath = snapPath;
		if (fullPath.length() > 0 && fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
			fullPath += "\\";
		}
		fullPath += baseFilename;
		
		// Ensure path uses backslashes for Windows
		for (size_t i = 0; i < fullPath.length(); i++) {
			if (fullPath[i] == '/') {
				fullPath[i] = '\\';
			}
		}
		
		// Copy to char array for SaveSnapshot
		strncpy(filename, fullPath.c_str(), sizeof(filename) - 1);
		filename[sizeof(filename) - 1] = '\0';
		
		// Save with custom filename
		extern int SaveSnapshot(char fileName[512]);
		result = SaveSnapshot(filename);
		
		if (result == 0) {
			// SaveSnapshot with filename returns 0 on success
			// Restore XBuf before returning
			XBuf = oldXBuf;
			// Return just the filename (not full path) for consistency
			lua_pushstring(L, baseFilename.c_str());
			return 1;
		} else {
			// Restore XBuf before returning error
			XBuf = oldXBuf;
			return luaL_error(L, "screenshot() failed: could not save to '%s'", filename);
		}
	} else {
		// No filename provided - use auto-generated name
		extern int SaveSnapshot(void);
		result = SaveSnapshot();
		
		if (result > 0) {
			// SaveSnapshot() returns index (1-based), construct filename
			int index = result - 1;  // Convert to 0-based index
			
			// Construct filename using FCEU_MakeFName
			// FCEUMKF_SNAP is defined in file.h
			extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
			std::string autoFilename = FCEU_MakeFName(2, index, "png");  // 2 = FCEUMKF_SNAP
			
			// Restore XBuf before returning
			XBuf = oldXBuf;
			lua_pushstring(L, autoFilename.c_str());
			return 1;
		} else {
			// Restore XBuf before returning error
			XBuf = oldXBuf;
			return luaL_error(L, "screenshot() failed: could not save screenshot (XBuf may not be set correctly)");
		}
	}
}

// Lua function - saves state to slot
int lua_savestate(lua_State *L) {
	int n = lua_gettop(L);
	int slot = 0;
	
	// Get slot parameter (optional, default 0)
	if (n >= 1 && !lua_isnil(L, 1)) {
		slot = (int)luaL_checkinteger(L, 1);
	}
	
	// Validate slot range (0-9)
	if (slot < 0 || slot > 9) {
		return luaL_error(L, "savestate(slot) failed: slot must be 0-9, got %d", slot);
	}
	
	// Generate filename for state slot
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);  // 1 = FCEUMKF_STATE
	
	// Check if game is loaded (required for save states)
	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "savestate(slot) failed: no game loaded");
	}
	
	// Ensure state directory exists (extract directory from path)
	size_t lastSlash = stateFilename.find_last_of("\\/");
	if (lastSlash != std::string::npos) {
		std::string stateDir = stateFilename.substr(0, lastSlash);
		// Normalize path separators for Windows
		for (size_t i = 0; i < stateDir.length(); i++) {
			if (stateDir[i] == '/') {
				stateDir[i] = '\\';
			}
		}
		// Remove trailing backslash for CreateDirectoryA
		if (stateDir.length() > 0 && (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
			stateDir = stateDir.substr(0, stateDir.length() - 1);
		}
		// Create directory if it doesn't exist
		CreateDirectoryA(stateDir.c_str(), NULL);
	}
	
	// Save state directly using FCEUSS_Save (bypasses UI validation)
	extern void FCEUSS_Save(const char *fname);
	FCEUSS_Save(stateFilename.c_str());
	
	// Verify save succeeded by checking if file exists and has content
	extern bool file_exists(const char * filename);
	bool success = false;
	if (file_exists(stateFilename.c_str())) {
		// Check if file has content (savestates should be at least a few KB)
		FILE *fp = fopen(stateFilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			fclose(fp);
			success = (size > 100);  // Savestates should be at least 100 bytes
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

// Lua function - loads state from slot
int lua_loadstate(lua_State *L) {
	int n = lua_gettop(L);
	int slot = 0;
	
	// Get slot parameter (optional, default 0)
	if (n >= 1 && !lua_isnil(L, 1)) {
		slot = (int)luaL_checkinteger(L, 1);
	}
	
	// Validate slot range (0-9)
	if (slot < 0 || slot > 9) {
		return luaL_error(L, "loadstate(slot) failed: slot must be 0-9, got %d", slot);
	}
	
	// Generate filename for state slot
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);  // 1 = FCEUMKF_STATE
	
	// Check if game is loaded (required for load states)
	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "loadstate(slot) failed: no game loaded");
	}
	
	// Check if state file exists
	extern bool file_exists(const char * filename);
	if (!file_exists(stateFilename.c_str())) {
		// State file doesn't exist
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Load state (FCEUSS_Load returns bool)
	extern bool FCEUSS_Load(const char *fname);
	bool success = FCEUSS_Load(stateFilename.c_str());
	
	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

// Lua function - checks if save state exists in slot
int lua_hasstate(lua_State *L) {
	int n = lua_gettop(L);
	int slot = 0;
	
	// Get slot parameter (optional, default 0)
	if (n >= 1 && !lua_isnil(L, 1)) {
		slot = (int)luaL_checkinteger(L, 1);
	}
	
	// Validate slot range (0-9)
	if (slot < 0 || slot > 9) {
		return luaL_error(L, "hasstate(slot) failed: slot must be 0-9, got %d", slot);
	}
	
	// Generate filename for state slot
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string stateFilename = FCEU_MakeFName(1, slot, 0);  // 1 = FCEUMKF_STATE
	
	// Check if state file exists
	extern bool file_exists(const char * filename);
	bool exists = file_exists(stateFilename.c_str());
	
	lua_pushboolean(L, exists ? 1 : 0);
	return 1;
}

// Lua function - saves state to custom filename
int lua_savestatefile(lua_State *L) {
	int n = lua_gettop(L);
	
	// Get filename parameter (required)
	if (n < 1 || lua_isnil(L, 1)) {
		return luaL_error(L, "savestatefile(filename) failed: filename is required");
	}
	
	const char* customFilename = luaL_checkstring(L, 1);
	if (!customFilename || strlen(customFilename) == 0) {
		return luaL_error(L, "savestatefile(filename) failed: filename cannot be empty");
	}
	
	// Check if game is loaded (required for save states)
	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "savestatefile(filename) failed: no game loaded");
	}
	
	// Get state directory (same as savestate uses)
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string tempStatePath = FCEU_MakeFName(1, 0, 0);  // 1 = FCEUMKF_STATE, use slot 0 to get directory
	size_t lastSlash = tempStatePath.find_last_of("\\/");
	std::string stateDir;
	if (lastSlash != std::string::npos) {
		stateDir = tempStatePath.substr(0, lastSlash);
	} else {
		// Fallback to game:\states if path extraction fails
		stateDir = "game:\\states";
	}
	
	// Normalize path separators for Windows
	for (size_t i = 0; i < stateDir.length(); i++) {
		if (stateDir[i] == '/') {
			stateDir[i] = '\\';
		}
	}
	// Remove trailing backslash for CreateDirectoryA
	if (stateDir.length() > 0 && (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
		stateDir = stateDir.substr(0, stateDir.length() - 1);
	}
	
	// Ensure state directory exists
	CreateDirectoryA(stateDir.c_str(), NULL);
	
	// Build filename with extension if not present
	std::string baseFilename = customFilename;
	
	// Add .fc0 extension if no extension present (check for common extensions)
	size_t lastDot = baseFilename.find_last_of(".");
	bool hasExtension = (lastDot != std::string::npos && lastDot < baseFilename.length() - 1);
	if (!hasExtension) {
		baseFilename += ".fc0";  // Default save state extension
	}
	
	// Build full path (ensure backslash separator)
	std::string fullPath = stateDir;
	if (fullPath.length() > 0 && fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
		fullPath += "\\";
	}
	fullPath += baseFilename;
	
	// Ensure path uses backslashes for Windows
	for (size_t i = 0; i < fullPath.length(); i++) {
		if (fullPath[i] == '/') {
			fullPath[i] = '\\';
		}
	}
	
	// Save state directly using FCEUSS_Save
	extern void FCEUSS_Save(const char *fname);
	FCEUSS_Save(fullPath.c_str());
	
	// Verify save succeeded by checking if file exists and has content
	extern bool file_exists(const char * filename);
	bool success = false;
	if (file_exists(fullPath.c_str())) {
		// Check if file has content (savestates should be at least a few KB)
		FILE *fp = fopen(fullPath.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			fclose(fp);
			success = (size > 100);  // Savestates should be at least 100 bytes
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

// Lua function - loads state from custom filename
int lua_loadstatefile(lua_State *L) {
	int n = lua_gettop(L);
	
	// Get filename parameter (required)
	if (n < 1 || lua_isnil(L, 1)) {
		return luaL_error(L, "loadstatefile(filename) failed: filename is required");
	}
	
	const char* customFilename = luaL_checkstring(L, 1);
	if (!customFilename || strlen(customFilename) == 0) {
		return luaL_error(L, "loadstatefile(filename) failed: filename cannot be empty");
	}
	
	// Check if game is loaded (required for load states)
	extern FCEUGI *GameInfo;
	if (!GameInfo) {
		return luaL_error(L, "loadstatefile(filename) failed: no game loaded");
	}
	
	// Get state directory (same as loadstate uses)
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	std::string tempStatePath = FCEU_MakeFName(1, 0, 0);  // 1 = FCEUMKF_STATE, use slot 0 to get directory
	size_t lastSlash = tempStatePath.find_last_of("\\/");
	std::string stateDir;
	if (lastSlash != std::string::npos) {
		stateDir = tempStatePath.substr(0, lastSlash);
	} else {
		// Fallback to game:\states if path extraction fails
		stateDir = "game:\\states";
	}
	
	// Normalize path separators for Windows
	for (size_t i = 0; i < stateDir.length(); i++) {
		if (stateDir[i] == '/') {
			stateDir[i] = '\\';
		}
	}
	// Remove trailing backslash
	if (stateDir.length() > 0 && (stateDir[stateDir.length() - 1] == '\\' || stateDir[stateDir.length() - 1] == '/')) {
		stateDir = stateDir.substr(0, stateDir.length() - 1);
	}
	
	// Build filename (use as-is, but add extension if not present)
	std::string baseFilename = customFilename;
	size_t lastDot = baseFilename.find_last_of(".");
	bool hasExtension = (lastDot != std::string::npos && lastDot < baseFilename.length() - 1);
	if (!hasExtension) {
		baseFilename += ".fc0";  // Default save state extension
	}
	
	// Build full path (ensure backslash separator)
	std::string fullPath = stateDir;
	if (fullPath.length() > 0 && fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
		fullPath += "\\";
	}
	fullPath += baseFilename;
	
	// Ensure path uses backslashes for Windows
	for (size_t i = 0; i < fullPath.length(); i++) {
		if (fullPath[i] == '/') {
			fullPath[i] = '\\';
		}
	}
	
	// Check if state file exists
	extern bool file_exists(const char * filename);
	if (!file_exists(fullPath.c_str())) {
		// State file doesn't exist
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// Load state (FCEUSS_Load returns bool)
	extern bool FCEUSS_Load(const char *fname);
	bool success = FCEUSS_Load(fullPath.c_str());
	
	lua_pushboolean(L, success ? 1 : 0);
	return 1;
}

// Lua drawing function - allows scripts to draw an image from byte data
int lua_drawimage(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return luaL_error(L, "drawimage(x, y, imageData, width, height) requires 5 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 
	 // Check if imageData is a table
	 if (!lua_istable(L, 3)) {
		 return luaL_error(L, "drawimage: imageData (3rd argument) must be a table");
	 }
	 
	 int width = (int)luaL_checkinteger(L, 4);
	 int height = (int)luaL_checkinteger(L, 5);
	 
	 if (!currentXBuf) return 0;
	 
	 // Validate dimensions
	 if (width <= 0 || height <= 0) {
		 return luaL_error(L, "drawimage: width and height must be positive");
	 }
	 
	 // Calculate expected data size
	 int expectedSize = width * height;
	 
	 // Read image data from table (Lua tables are 1-indexed)
	 std::vector<uint8> imageData;
	 imageData.reserve(expectedSize);
	 
	 int dataCount = 0;
	 for (int i = 1; i <= expectedSize; ++i) {
		 lua_rawgeti(L, 3, i);
		 if (!lua_isnumber(L, -1)) {
			 lua_pop(L, 1);
			 break; // End of table
		 }
		 int colorValue = (int)luaL_checkinteger(L, -1);
		 lua_pop(L, 1);
		 
		 // Clamp color value to valid range (0x00-0x3F)
		 if (colorValue < 0) colorValue = 0;
		 if (colorValue > 0x3F) colorValue = 0x3F;
		 
		 imageData.push_back((uint8)(colorValue & 0xFF));
		 dataCount++;
	 }
	 
	 if (dataCount < expectedSize) {
		 return luaL_error(L, "drawimage: imageData table must contain at least %d color values", expectedSize);
	 }
	 
	 // Clamp coordinates to safe bounds
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1;
	 
	 // Calculate actual drawable area (clamp to screen bounds)
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + width > OVL_W) ? OVL_W : (x + width);
	 int endY = (y + height > OVL_H) ? OVL_H : (y + height);
	 
	 // Adjust start positions if image is completely off-screen
	 if (startX >= OVL_W || startY >= OVL_H || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 bool drewSomething = false;
	 
	 // Draw image row by row
	 int srcIndex = 0;
	 for (int py = 0; py < height; ++py) {
		 int screenY = y + py;
		 if (screenY < 0 || screenY >= OVL_H) {
			 srcIndex += width; // Skip this row
			 continue;
		 }
		 
		 for (int px = 0; px < width; ++px) {
			 int screenX = x + px;
			 if (screenX >= 0 && screenX < OVL_W && screenY >= 0 && screenY < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(screenX, screenY)) {
					 uint8 colorValue = imageData[srcIndex];
					 uint8 *dest = currentXBuf + screenY * OVL_W + screenX;
					 uint8 srcColor = map_overlay_color(colorValue);
					 *dest = apply_blend_mode(*dest, srcColor);
					 drewSomething = true;
				 }
			 }
			 srcIndex++;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
}

// Lua drawing function - allows scripts to draw an image using indexed palette
int lua_drawimageindexed(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return luaL_error(L, "drawimageindexed(x, y, imageData, palette, width, height) requires 6 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 
	 // Check if imageData is a table
	 if (!lua_istable(L, 3)) {
		 return luaL_error(L, "drawimageindexed: imageData (3rd argument) must be a table");
	 }
	 
	 // Check if palette is a table
	 if (!lua_istable(L, 4)) {
		 return luaL_error(L, "drawimageindexed: palette (4th argument) must be a table");
	 }
	 
	 int width = (int)luaL_checkinteger(L, 5);
	 int height = (int)luaL_checkinteger(L, 6);
	 
	 if (!currentXBuf) return 0;
	 
	 // Validate dimensions
	 if (width <= 0 || height <= 0) {
		 return luaL_error(L, "drawimageindexed: width and height must be positive");
	 }
	 
	 // Calculate expected data size
	 int expectedSize = width * height;
	 
	 // Read palette table first (Lua tables are 1-indexed)
	 std::vector<uint8> palette;
	 palette.reserve(256); // Max palette size
	 
	 int paletteCount = 0;
	 for (int i = 1; i <= 256; ++i) {
		 lua_rawgeti(L, 4, i);
		 if (!lua_isnumber(L, -1)) {
			 lua_pop(L, 1);
			 break; // End of palette table
		 }
		 int colorValue = (int)luaL_checkinteger(L, -1);
		 lua_pop(L, 1);
		 
		 // Clamp color value to valid range (0x00-0x3F)
		 if (colorValue < 0) colorValue = 0;
		 if (colorValue > 0x3F) colorValue = 0x3F;
		 
		 palette.push_back((uint8)(colorValue & 0xFF));
		 paletteCount++;
	 }
	 
	 if (paletteCount <= 0) {
		 return luaL_error(L, "drawimageindexed: palette table must contain at least one color value");
	 }
	 
	 // Read image data from table (Lua tables are 1-indexed)
	 // imageData contains indices into the palette table
	 std::vector<uint8> imageData;
	 imageData.reserve(expectedSize);
	 
	 int dataCount = 0;
	 for (int i = 1; i <= expectedSize; ++i) {
		 lua_rawgeti(L, 3, i);
		 if (!lua_isnumber(L, -1)) {
			 lua_pop(L, 1);
			 break; // End of table
		 }
		 int paletteIndex = (int)luaL_checkinteger(L, -1);
		 lua_pop(L, 1);
		 
		 // Convert from Lua 1-based indexing to C++ 0-based indexing
		 // User provides 1, 2, 3... which should map to palette[0], palette[1], palette[2]...
		 paletteIndex = paletteIndex - 1;
		 
		 // Clamp palette index to valid range (0 to paletteCount-1)
		 if (paletteIndex < 0) paletteIndex = 0;
		 if (paletteIndex >= paletteCount) paletteIndex = paletteCount - 1;
		 
		 imageData.push_back((uint8)(paletteIndex & 0xFF));
		 dataCount++;
	 }
	 
	 if (dataCount < expectedSize) {
		 return luaL_error(L, "drawimageindexed: imageData table must contain at least %d palette indices", expectedSize);
	 }
	 
	 // Clamp coordinates to safe bounds
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1;
	 
	 // Calculate actual drawable area (clamp to screen bounds)
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + width > OVL_W) ? OVL_W : (x + width);
	 int endY = (y + height > OVL_H) ? OVL_H : (y + height);
	 
	 // Adjust start positions if image is completely off-screen
	 if (startX >= OVL_W || startY >= OVL_H || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 bool drewSomething = false;
	 
	 // Draw image row by row
	 int srcIndex = 0;
	 for (int py = 0; py < height; ++py) {
		 int screenY = y + py;
		 if (screenY < 0 || screenY >= OVL_H) {
			 srcIndex += width; // Skip this row
			 continue;
		 }
		 
		 for (int px = 0; px < width; ++px) {
			 int screenX = x + px;
			 if (screenX >= 0 && screenX < OVL_W && screenY >= 0 && screenY < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(screenX, screenY)) {
					 uint8 paletteIndex = imageData[srcIndex];
					 uint8 colorValue = palette[paletteIndex]; // Look up color from palette
					 uint8 *dest = currentXBuf + screenY * OVL_W + screenX;
					 uint8 srcColor = map_overlay_color(colorValue);
					 *dest = apply_blend_mode(*dest, srcColor);
					 drewSomething = true;
				 }
			 }
			 srcIndex++;
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
}

// Lua drawing function - allows scripts to draw a single NES tile (8x8 pixels)
int lua_drawtile(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4) {
		 return luaL_error(L, "drawtile(x, y, tileIndex, paletteIndex) requires 4 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int tileIndex = (int)luaL_checkinteger(L, 3);
	 int paletteIndex = (int)luaL_checkinteger(L, 4);
	 
	 if (!currentXBuf) return 0;
	 
	 // Validate tile index (0-255)
	 if (tileIndex < 0) tileIndex = 0;
	 if (tileIndex > 255) tileIndex = 255;
	 
	 // Validate palette index (0-3)
	 if (paletteIndex < 0) paletteIndex = 0;
	 if (paletteIndex > 3) paletteIndex = 3;
	 
	 // Read tile data from pattern table (0x0000-0x1FFF)
	 // Each tile is 16 bytes: 8 rows * 2 bytes per row
	 // Tile address = tileIndex * 16
	 // Check which pattern table to use based on PPU register 0 bit 4 (BGAdrHI)
	 uint32 patternTableBase = (PPU[0] & 0x10) ? 0x1000 : 0x0000;
	 uint32 tileAddr = patternTableBase + (tileIndex * 16);
	 if (tileAddr >= 0x2000) {
		 tileAddr = (tileAddr & 0x1FFF) | patternTableBase; // Wrap within pattern table range
	 }
	 
	 // Read tile data directly from VPage which contains the pattern table
	 uint8 tileData[16];
	 for (int i = 0; i < 16; ++i) {
		 uint32 addr = tileAddr + i;
		 if (addr < 0x2000) {
			 // Read from pattern table (VPage)
			 tileData[i] = VPage[addr>>10][addr];
		 } else {
			 tileData[i] = 0; // Should not happen for pattern table
		 }
	 }
	 
	 // Get palette base address
	 // Background palettes: PALRAM[0x01-0x03] = palette 0, [0x05-0x07] = palette 1, [0x09-0x0B] = palette 2, [0x0D-0x0F] = palette 3
	 // Each palette has 3 colors (plus transparent color 0 which is always PALRAM[0x00])
	 int paletteBase = 0x01 + (paletteIndex * 4);
	 uint8 palette[4];
	 palette[0] = PALRAM[0x00]; // Universal background color
	 palette[1] = PALRAM[paletteBase + 0];
	 palette[2] = PALRAM[paletteBase + 1];
	 palette[3] = PALRAM[paletteBase + 2];
	 
	 // Clamp coordinates to safe bounds
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1;
	 
	 // Draw tile (8x8 pixels)
	 bool drewSomething = false;
	 
	 for (int py = 0; py < 8; ++py) {
		 int screenY = y + py;
		 if (screenY < 0 || screenY >= OVL_H) {
			 continue;
		 }
		 
		 // Get two bytes for this row (low and high bit planes)
		 uint8 lowByte = tileData[py];
		 uint8 highByte = tileData[py + 8];
		 
		 for (int px = 0; px < 8; ++px) {
			 int screenX = x + px;
			 if (screenX >= 0 && screenX < OVL_W && screenY >= 0 && screenY < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(screenX, screenY)) {
					 // Extract 2-bit color index from bit planes
					 // Bit 7-px from lowByte and highByte form the color index
					 int bitPos = 7 - px;
					 int colorIndex = ((lowByte >> bitPos) & 1) | (((highByte >> bitPos) & 1) << 1);
					 
					 // Skip transparent pixels (colorIndex 0)
					 if (colorIndex == 0) {
						 continue;
					 }
					 
					 // Get color from palette
					 uint8 colorValue = palette[colorIndex];
					 
					 // Draw pixel
					 uint8 *dest = currentXBuf + screenY * OVL_W + screenX;
					 uint8 srcColor = map_overlay_color(colorValue);
					 *dest = apply_blend_mode(*dest, srcColor);
					 drewSomething = true;
				 }
			 }
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
}

// Lua drawing function - allows scripts to draw a CHR-ROM tile (8x8 pixels)
int lua_drawchrtile(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 4) {
		 return luaL_error(L, "drawchrtile(x, y, tileIndex, paletteIndex) requires 4 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 int tileIndex = (int)luaL_checkinteger(L, 3);
	 int paletteIndex = (int)luaL_checkinteger(L, 4);
	 
	 if (!currentXBuf) return 0;
	 
	 // Validate tile index (0-255)
	 if (tileIndex < 0) tileIndex = 0;
	 if (tileIndex > 255) tileIndex = 255;
	 
	 // Validate palette index (0-3)
	 if (paletteIndex < 0) paletteIndex = 0;
	 if (paletteIndex > 3) paletteIndex = 3;
	 
	 // Check if CHR-ROM data is available
	 if (!CHRptr[0]) {
		 return luaL_error(L, "drawchrtile: CHR-ROM data not available");
	 }
	 
	 // Read tile data directly from CHR-ROM
	 // Each tile is 16 bytes: 8 rows * 2 bytes per row
	 // Tile address = tileIndex * 16
	 uint32 tileOffset = tileIndex * 16;
	 
	 uint8 tileData[16];
	 for (int i = 0; i < 16; ++i) {
		 tileData[i] = CHRptr[0][tileOffset + i];
	 }
	 
	 // Get palette base address
	 // Background palettes: PALRAM[0x01-0x03] = palette 0, [0x05-0x07] = palette 1, [0x09-0x0B] = palette 2, [0x0D-0x0F] = palette 3
	 // Each palette has 3 colors (plus transparent color 0 which is always PALRAM[0x00])
	 int paletteBase = 0x01 + (paletteIndex * 4);
	 uint8 palette[4];
	 palette[0] = PALRAM[0x00]; // Universal background color
	 palette[1] = PALRAM[paletteBase + 0];
	 palette[2] = PALRAM[paletteBase + 1];
	 palette[3] = PALRAM[paletteBase + 2];
	 
	 // Clamp coordinates to safe bounds
	 if (x < 0) x = 0;
	 if (x >= OVL_W) x = OVL_W - 1;
	 if (y < 0) y = 0;
	 if (y >= OVL_H) y = OVL_H - 1;
	 
	 // Draw tile (8x8 pixels)
	 bool drewSomething = false;
	 
	 for (int py = 0; py < 8; ++py) {
		 int screenY = y + py;
		 if (screenY < 0 || screenY >= OVL_H) {
			 continue;
		 }
		 
		 // Get two bytes for this row (low and high bit planes)
		 uint8 lowByte = tileData[py];
		 uint8 highByte = tileData[py + 8];
		 
		 for (int px = 0; px < 8; ++px) {
			 int screenX = x + px;
			 if (screenX >= 0 && screenX < OVL_W && screenY >= 0 && screenY < OVL_H) {
				 // Check clipping
				 if (!is_point_clipped(screenX, screenY)) {
					 // Extract 2-bit color index from bit planes
					 // Bit 7-px from lowByte and highByte form the color index
					 int bitPos = 7 - px;
					 int colorIndex = ((lowByte >> bitPos) & 1) | (((highByte >> bitPos) & 1) << 1);
					 
					 // Skip transparent pixels (colorIndex 0)
					 if (colorIndex == 0) {
						 continue;
					 }
					 
					 // Get color from palette
					 uint8 colorValue = palette[colorIndex];
					 
					 // Draw pixel
					 uint8 *dest = currentXBuf + screenY * OVL_W + screenX;
					 uint8 srcColor = map_overlay_color(colorValue);
					 *dest = apply_blend_mode(*dest, srcColor);
					 drewSomething = true;
				 }
			 }
		 }
	 }
	 
	 if (drewSomething) {
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
	 return 0;
}

// Lua function to set drawing mode
int lua_setdrawmode(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "setdrawmode(mode) requires 1 argument");
	 }
	 
	 if (!lua_isstring(L, 1)) {
		 return luaL_error(L, "setdrawmode: mode must be a string");
	 }
	 
	 const char* modeStr = lua_tostring(L, 1);
	 
	 if (strcmp(modeStr, "normal") == 0) {
		 s_drawMode = DRAW_MODE_NORMAL;
	 } else if (strcmp(modeStr, "add") == 0) {
		 s_drawMode = DRAW_MODE_ADD;
	 } else if (strcmp(modeStr, "sub") == 0) {
		 s_drawMode = DRAW_MODE_SUB;
	 } else if (strcmp(modeStr, "multiply") == 0) {
		 s_drawMode = DRAW_MODE_MULTIPLY;
	 } else if (strcmp(modeStr, "alpha") == 0) {
		 s_drawMode = DRAW_MODE_ALPHA;
	 } else {
		 return luaL_error(L, "setdrawmode: invalid mode. Valid modes are: \"normal\", \"add\", \"sub\", \"multiply\", \"alpha\"");
	 }
	 
	return 0;
}

// Lua function to set clipping region
int lua_setclipregion(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 4) {
		return luaL_error(L, "setclipregion(x, y, width, height) requires 4 arguments");
	}
	
	int x = (int)luaL_checkinteger(L, 1);
	int y = (int)luaL_checkinteger(L, 2);
	int width = (int)luaL_checkinteger(L, 3);
	int height = (int)luaL_checkinteger(L, 4);
	
	// Validate and clamp clipping region
	if (width <= 0 || height <= 0) {
		// Disable clipping if invalid dimensions
		s_clipEnabled = false;
		return 0;
	}
	
	// Clamp to screen bounds
	if (x < 0) {
		width += x;
		x = 0;
	}
	if (y < 0) {
		height += y;
		y = 0;
	}
	if (x + width > OVL_W) {
		width = OVL_W - x;
	}
	if (y + height > OVL_H) {
		height = OVL_H - y;
	}
	
	// If region is still valid, set it
	if (width > 0 && height > 0) {
		s_clipX = x;
		s_clipY = y;
		s_clipW = width;
		s_clipH = height;
		s_clipEnabled = true;
	} else {
		// Disable clipping if region is invalid
		s_clipEnabled = false;
	}
	
	return 0;
}

// Lua function to clear clipping region
int lua_clearclipregion(lua_State *L) {
	// Disable clipping by setting the enabled flag to false
	s_clipEnabled = false;
	return 0;
}

// Lua function to set default drawing color
int lua_setdrawcolor(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "setdrawcolor(color) requires 1 argument");
	}
	
	int color = (int)luaL_checkinteger(L, 1);
	
	// Validate color range (0x00-0x3F)
	if (color < 0 || color > 0x3F) {
		return luaL_error(L, "setdrawcolor: color must be in range 0x00-0x3F");
	}
	
	s_defaultDrawColor = color;
	return 0;
}

// Lua drawing function - allows scripts to draw a circle outline
int lua_drawcircle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 4) {
		return luaL_error(L, "drawcircle(x, y, radius, color) requires 4 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int radius = (int)luaL_checkinteger(L, 3);
	int color = (int)luaL_checkinteger(L, 4);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radius if circle would extend past safe bounds
	if (radius <= 0) return 0;
	if (cy + radius > 231) radius = 231 - cy;
	if (cy - radius < 0 && radius > cy) radius = cy;
	if (radius < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Use midpoint circle algorithm to draw circle outline
	int x = 0;
	int y = radius;
	int d = 1 - radius;
	
	// Helper function to draw 8 symmetric points (for complete circle)
	// Using inline macro-style approach for VS2008 compatibility (no C++11 lambdas)
	while (true) {
		// Draw 8 symmetric points for current (x, y) position
		int points[8][2];
		points[0][0] = cx + x; points[0][1] = cy + y;
		points[1][0] = cx - x; points[1][1] = cy + y;
		points[2][0] = cx + x; points[2][1] = cy - y;
		points[3][0] = cx - x; points[3][1] = cy - y;
		points[4][0] = cx + y; points[4][1] = cy + x;
		points[5][0] = cx - y; points[5][1] = cy + x;
		points[6][0] = cx + y; points[6][1] = cy - x;
		points[7][0] = cx - y; points[7][1] = cy - x;
		
		for (int i = 0; i < 8; ++i) {
			int px = points[i][0];
			int py = points[i][1];
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
				uint8 *dest = currentXBuf + py * OVL_W + px;
				*dest = apply_blend_mode(*dest, mappedColor);
				drewSomething = true;
			}
		}
		
		// Check if we're done (midpoint algorithm termination)
		if (x >= y) break;
		
		// Update decision parameter and position
		if (d < 0) {
			d += 2 * x + 3;
		} else {
			d += 2 * (x - y) + 5;
			y--;
		}
		x++;
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a filled circle
int lua_fillcircle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 4) {
		return luaL_error(L, "fillcircle(x, y, radius, color) requires 4 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int radius = (int)luaL_checkinteger(L, 3);
	int color = (int)luaL_checkinteger(L, 4);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radius if circle would extend past safe bounds
	if (radius <= 0) return 0;
	if (cy + radius > 231) radius = 231 - cy;
	if (cy - radius < 0 && radius > cy) radius = cy;
	if (radius < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Clamp circle bounds to screen
	int minX = (cx - radius < 0) ? 0 : (cx - radius);
	int maxX = (cx + radius >= OVL_W) ? OVL_W - 1 : (cx + radius);
	int minY = (cy - radius < 0) ? 0 : (cy - radius);
	int maxY = (cy + radius >= OVL_H) ? OVL_H - 1 : (cy + radius);
	
	if (minX >= OVL_W || minY >= OVL_H || maxX < 0 || maxY < 0) return 0;
	
	// Fill circle by checking if each pixel is inside the circle
	for (int py = minY; py <= maxY; ++py) {
		for (int px = minX; px <= maxX; ++px) {
			// Calculate distance from center
			int dx = px - cx;
			int dy = py - cy;
			int distSq = dx * dx + dy * dy;
			int radiusSq = radius * radius;
			
			// If pixel is inside or on the circle, fill it
			if (distSq <= radiusSq) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
					uint8 *dest = currentXBuf + py * OVL_W + px;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a triangle outline
int lua_drawtriangle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 7) {
		return luaL_error(L, "drawtriangle(x1, y1, x2, y2, x3, y3, color) requires 7 arguments");
	}
	
	int x1 = (int)luaL_checkinteger(L, 1);
	int y1 = (int)luaL_checkinteger(L, 2);
	int x2 = (int)luaL_checkinteger(L, 3);
	int y2 = (int)luaL_checkinteger(L, 4);
	int x3 = (int)luaL_checkinteger(L, 5);
	int y3 = (int)luaL_checkinteger(L, 6);
	int color = (int)luaL_checkinteger(L, 7);
	
	if (!currentXBuf) return 0;
	
	// Clamp coordinates to safe bounds (auto-adjust if too low/high)
	if (x1 < 0) x1 = 0;
	if (x1 >= OVL_W) x1 = OVL_W - 1;
	if (x2 < 0) x2 = 0;
	if (x2 >= OVL_W) x2 = OVL_W - 1;
	if (x3 < 0) x3 = 0;
	if (x3 >= OVL_W) x3 = OVL_W - 1;
	if (y1 < 0) y1 = 0;
	if (y1 >= OVL_H) y1 = OVL_H - 1; // Clamp to safe position
	if (y2 < 0) y2 = 0;
	if (y2 >= OVL_H) y2 = OVL_H - 1; // Clamp to safe position
	if (y3 < 0) y3 = 0;
	 if (y3 >= OVL_H) y3 = OVL_H - 1; // Clamp to safe position
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Draw three lines to form triangle outline
	// Line 1: (x1, y1) to (x2, y2)
	int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	int sx = (x1 < x2) ? 1 : -1;
	int sy = (y1 < y2) ? 1 : -1;
	int err = dx - dy;
	
	int x = x1;
	int y = y1;
	
	while (true) {
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 uint8 *dest = currentXBuf + y * OVL_W + x;
			*dest = apply_blend_mode(*dest, mappedColor);
			drewSomething = true;
		}
		if (x == x2 && y == y2) break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x += sx;
		}
		if (e2 < dx) {
			err += dx;
			y += sy;
		}
	}
	
	// Line 2: (x2, y2) to (x3, y3)
	dx = (x3 > x2) ? (x3 - x2) : (x2 - x3);
	dy = (y3 > y2) ? (y3 - y2) : (y2 - y3);
	sx = (x2 < x3) ? 1 : -1;
	sy = (y2 < y3) ? 1 : -1;
	err = dx - dy;
	x = x2;
	y = y2;
	
	while (true) {
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 uint8 *dest = currentXBuf + y * OVL_W + x;
			*dest = apply_blend_mode(*dest, mappedColor);
			drewSomething = true;
		}
		if (x == x3 && y == y3) break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x += sx;
		}
		if (e2 < dx) {
			err += dx;
			y += sy;
		}
	}
	
	// Line 3: (x3, y3) to (x1, y1)
	dx = (x1 > x3) ? (x1 - x3) : (x3 - x1);
	dy = (y1 > y3) ? (y1 - y3) : (y3 - y1);
	sx = (x3 < x1) ? 1 : -1;
	sy = (y3 < y1) ? 1 : -1;
	err = dx - dy;
	x = x3;
	y = y3;
	
	while (true) {
			 if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H) {
				 uint8 *dest = currentXBuf + y * OVL_W + x;
			*dest = apply_blend_mode(*dest, mappedColor);
			drewSomething = true;
		}
		if (x == x1 && y == y1) break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x += sx;
		}
		if (e2 < dx) {
			err += dx;
			y += sy;
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a filled triangle
int lua_filltriangle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 7) {
		return luaL_error(L, "filltriangle(x1, y1, x2, y2, x3, y3, color) requires 7 arguments");
	}
	
	int x1 = (int)luaL_checkinteger(L, 1);
	int y1 = (int)luaL_checkinteger(L, 2);
	int x2 = (int)luaL_checkinteger(L, 3);
	int y2 = (int)luaL_checkinteger(L, 4);
	int x3 = (int)luaL_checkinteger(L, 5);
	int y3 = (int)luaL_checkinteger(L, 6);
	int color = (int)luaL_checkinteger(L, 7);
	
	if (!currentXBuf) return 0;
	
	// Clamp coordinates to safe bounds (auto-adjust if too low/high)
	if (x1 < 0) x1 = 0;
	if (x1 >= OVL_W) x1 = OVL_W - 1;
	if (x2 < 0) x2 = 0;
	if (x2 >= OVL_W) x2 = OVL_W - 1;
	if (x3 < 0) x3 = 0;
	if (x3 >= OVL_W) x3 = OVL_W - 1;
	if (y1 < 0) y1 = 0;
	if (y1 >= OVL_H) y1 = OVL_H - 1; // Clamp to safe position
	if (y2 < 0) y2 = 0;
	if (y2 >= OVL_H) y2 = OVL_H - 1; // Clamp to safe position
	if (y3 < 0) y3 = 0;
	 if (y3 >= OVL_H) y3 = OVL_H - 1; // Clamp to safe position
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Sort vertices by Y coordinate (top to bottom)
	int v[3][2];
	v[0][0] = x1; v[0][1] = y1;
	v[1][0] = x2; v[1][1] = y2;
	v[2][0] = x3; v[2][1] = y3;
	
	// Bubble sort by Y coordinate (simple 3-element sort)
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2 - i; ++j) {
			if (v[j][1] > v[j + 1][1]) {
				int tx = v[j][0];
				int ty = v[j][1];
				v[j][0] = v[j + 1][0];
				v[j][1] = v[j + 1][1];
				v[j + 1][0] = tx;
				v[j + 1][1] = ty;
			}
		}
	}
	
	// Now v[0] is top, v[1] is middle, v[2] is bottom
	int topX = v[0][0], topY = v[0][1];
	int midX = v[1][0], midY = v[1][1];
	int botX = v[2][0], botY = v[2][1];
	
	// Check if triangle is valid (has area)
	if (topY == botY) return 0;  // Flat triangle
	
	// Fill triangle using scanline algorithm
	// Split into two parts: top half (top to mid) and bottom half (mid to bot)
	
	// Top half: from topY to midY
	if (topY != midY) {
		int dy1 = midY - topY;
		for (int y = topY; y <= midY; ++y) {
			if (y < 0 || y >= 232) continue;
			
			// Calculate x positions on left and right edges
			// Left edge: top to mid
			int xLeft1 = topX + ((midX - topX) * (y - topY)) / dy1;
			// Right edge: top to bot
			int dy2 = botY - topY;
			int xRight1 = topX + ((botX - topX) * (y - topY)) / dy2;
			
			// Ensure left < right
			int xStart = (xLeft1 < xRight1) ? xLeft1 : xRight1;
			int xEnd = (xLeft1 < xRight1) ? xRight1 : xLeft1;
			
			// Fill scanline
			for (int x = xStart; x <= xEnd; ++x) {
				if (x >= 0 && x < 256) {
					uint8 *dest = currentXBuf + y * 256 + x;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
	}
	
	// Bottom half: from midY to botY
	if (midY != botY) {
		int dy1 = botY - midY;
		for (int y = midY; y <= botY; ++y) {
			if (y < 0 || y >= 232) continue;
			
			// Calculate x positions on left and right edges
			// Left edge: mid to bot
			int xLeft2 = midX + ((botX - midX) * (y - midY)) / dy1;
			// Right edge: top to bot
			int dy2 = botY - topY;
			int xRight2 = topX + ((botX - topX) * (y - topY)) / dy2;
			
			// Ensure left < right
			int xStart = (xLeft2 < xRight2) ? xLeft2 : xRight2;
			int xEnd = (xLeft2 < xRight2) ? xRight2 : xLeft2;
			
			// Fill scanline
			for (int x = xStart; x <= xEnd; ++x) {
				if (x >= 0 && x < 256) {
					uint8 *dest = currentXBuf + y * 256 + x;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
	} else if (topY == midY && midY == botY) {
		// All points on same line - draw a line
		int xStart = (topX < midX) ? ((topX < botX) ? topX : botX) : ((midX < botX) ? midX : botX);
		int xEnd = (topX > midX) ? ((topX > botX) ? topX : botX) : ((midX > botX) ? midX : botX);
		if (topY >= 0 && topY < OVL_H) {
			for (int x = xStart; x <= xEnd; ++x) {
				if (x >= 0 && x < OVL_W) {
					uint8 *dest = currentXBuf + topY * OVL_W + x;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw an ellipse outline
int lua_drawellipse(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 5) {
		return luaL_error(L, "drawellipse(x, y, rx, ry, color) requires 5 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int rx = (int)luaL_checkinteger(L, 3);
	int ry = (int)luaL_checkinteger(L, 4);
	int color = (int)luaL_checkinteger(L, 5);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radii if ellipse would extend past safe bounds
	if (rx <= 0 || ry <= 0) return 0;
	if (cy + ry > 231) ry = 231 - cy;
	if (cy - ry < 0 && ry > cy) ry = cy;
	if (rx < 0 || ry < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Use midpoint ellipse algorithm to draw ellipse outline
	// Draw 4 symmetric points for each (x, y) position
	int x = 0;
	int y = ry;
	
	// Decision parameters for first region
	int rx2 = rx * rx;
	int ry2 = ry * ry;
	int rx2y = 0;
	int ry2x = 2 * rx2 * ry;
	int d1 = ry2 - (rx2 * ry) + (rx2 / 4);
	
	// Draw first region
	while (rx2y < ry2x) {
		// Draw 4 symmetric points
		if (cx + x >= 0 && cx + x < OVL_W && cy + y >= 0 && cy + y < OVL_H) {
			currentXBuf[(cy + y) * OVL_W + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < OVL_W && cy + y >= 0 && cy + y < OVL_H) {
			currentXBuf[(cy + y) * OVL_W + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		if (cx + x >= 0 && cx + x < OVL_W && cy - y >= 0 && cy - y < OVL_H) {
			currentXBuf[(cy - y) * OVL_W + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < OVL_W && cy - y >= 0 && cy - y < OVL_H) {
			currentXBuf[(cy - y) * OVL_W + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		
		if (d1 < 0) {
			x++;
			rx2y += 2 * ry2 * x;
			d1 += ry2 * (2 * x + 1);
		} else {
			x++;
			y--;
			rx2y += 2 * ry2 * x;
			ry2x -= 2 * rx2 * y;
			d1 += ry2 * (2 * x + 1) + rx2 * (1 - 2 * y);
		}
	}
	
	// Decision parameter for second region (integer-only calculation)
	// Using: ry^2 * (x + 0.5)^2 + rx^2 * (y - 1)^2 - rx^2 * ry^2
	// For integer math: (x+0.5)^2 = (2x+1)^2 / 4
	int d2 = (ry2 * (2*x + 1) * (2*x + 1) / 4) + (rx2 * (y - 1) * (y - 1)) - (rx2 * ry2);
	
	// Draw second region
	while (y >= 0) {
		// Draw 4 symmetric points
		if (cx + x >= 0 && cx + x < OVL_W && cy + y >= 0 && cy + y < OVL_H) {
			currentXBuf[(cy + y) * OVL_W + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < OVL_W && cy + y >= 0 && cy + y < OVL_H) {
			currentXBuf[(cy + y) * OVL_W + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		if (cx + x >= 0 && cx + x < OVL_W && cy - y >= 0 && cy - y < OVL_H) {
			currentXBuf[(cy - y) * OVL_W + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < OVL_W && cy - y >= 0 && cy - y < OVL_H) {
			currentXBuf[(cy - y) * OVL_W + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		
		if (d2 > 0) {
			y--;
			ry2x -= 2 * rx2 * y;
			d2 += rx2 * (1 - 2 * y);
		} else {
			x++;
			y--;
			rx2y += 2 * ry2 * x;
			ry2x -= 2 * rx2 * y;
			d2 += ry2 * (2 * x + 1) + rx2 * (1 - 2 * y);
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a filled ellipse
int lua_fillellipse(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 5) {
		return luaL_error(L, "fillellipse(x, y, rx, ry, color) requires 5 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int rx = (int)luaL_checkinteger(L, 3);
	int ry = (int)luaL_checkinteger(L, 4);
	int color = (int)luaL_checkinteger(L, 5);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radii if ellipse would extend past safe bounds
	if (rx <= 0 || ry <= 0) return 0;
	if (cy + ry > 231) ry = 231 - cy;
	if (cy - ry < 0 && ry > cy) ry = cy;
	if (rx < 0 || ry < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Calculate bounding box for filled ellipse
	int minX = (cx - rx < 0) ? 0 : (cx - rx);
	int maxX = (cx + rx >= OVL_W) ? OVL_W - 1 : (cx + rx);
	int minY = (cy - ry < 0) ? 0 : (cy - ry);
	int maxY = (cy + ry >= OVL_H) ? OVL_H - 1 : (cy + ry);
	
	if (minX >= OVL_W || minY >= OVL_H || maxX < 0 || maxY < 0) return 0;
	
	// Pre-calculate squared values for efficiency
	int rx2 = rx * rx;
	int ry2 = ry * ry;
	int rx2ry2 = rx2 * ry2;
	
	// Fill ellipse using distance calculation
	for (int py = minY; py <= maxY; ++py) {
		for (int px = minX; px <= maxX; ++px) {
			// Calculate distance from center
			int dx = px - cx;
			int dy = py - cy;
			
			// Ellipse equation: (dx^2 / rx^2) + (dy^2 / ry^2) <= 1
			// Rearranged to avoid division: (dx^2 * ry^2) + (dy^2 * rx^2) <= rx^2 * ry^2
			int distSq = (dx * dx * ry2) + (dy * dy * rx2);
			
			if (distSq <= rx2ry2) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
					uint8 *dest = currentXBuf + py * OVL_W + px;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw an arc outline
int lua_drawarc(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 6) {
		return luaL_error(L, "drawarc(x, y, radius, startAngle, endAngle, color) requires 6 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int radius = (int)luaL_checkinteger(L, 3);
	int startAngle = (int)luaL_checkinteger(L, 4);
	int endAngle = (int)luaL_checkinteger(L, 5);
	int color = (int)luaL_checkinteger(L, 6);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radius if arc would extend past safe bounds
	if (radius <= 0) return 0;
	if (cy + radius > 231) radius = 231 - cy;
	if (cy - radius < 0 && radius > cy) radius = cy;
	if (radius < 0) return 0;
	
	// Normalize angles to 0-360 range
	startAngle = ((startAngle % 360) + 360) % 360;
	endAngle = ((endAngle % 360) + 360) % 360;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Use midpoint circle algorithm but only draw points within angle range
	int x = 0;
	int y = radius;
	int d = 1 - radius;
	
	// Draw arc using midpoint circle algorithm
	while (x <= y) {
		// Draw 8 symmetric points for current (x, y) position
		int points[8][2];
		points[0][0] = cx + x; points[0][1] = cy + y;
		points[1][0] = cx - x; points[1][1] = cy + y;
		points[2][0] = cx + x; points[2][1] = cy - y;
		points[3][0] = cx - x; points[3][1] = cy - y;
		points[4][0] = cx + y; points[4][1] = cy + x;
		points[5][0] = cx - y; points[5][1] = cy + x;
		points[6][0] = cx + y; points[6][1] = cy - x;
		points[7][0] = cx - y; points[7][1] = cy - x;
		
		for (int i = 0; i < 8; ++i) {
			int px = points[i][0];
			int py = points[i][1];
			
			// Check bounds
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
				// Calculate angle of point relative to center
				int dx = px - cx;
				int dy = py - cy;
				double angleRad = atan2((double)dy, (double)dx);
				int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
				int a = ((angleDeg % 360) + 360) % 360;
				
				// Check if angle is within arc range
				bool inRange;
				if (startAngle <= endAngle) {
					inRange = (a >= startAngle && a <= endAngle);
				} else {
					// Arc crosses 0°/360° boundary (e.g., 350° to 10°)
					inRange = (a >= startAngle || a <= endAngle);
				}
				
				if (inRange) {
					uint8 *dest = currentXBuf + py * OVL_W + px;
					*dest = apply_blend_mode(*dest, mappedColor);
					drewSomething = true;
				}
			}
		}
		
		// Update decision parameter and position
		if (d < 0) {
			d += 2 * x + 3;
		} else {
			d += 2 * (x - y) + 5;
			y--;
		}
		x++;
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a filled arc (pie slice)
int lua_fillarc(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 6) {
		return luaL_error(L, "fillarc(x, y, radius, startAngle, endAngle, color) requires 6 arguments");
	}
	
	int cx = (int)luaL_checkinteger(L, 1);
	int cy = (int)luaL_checkinteger(L, 2);
	int radius = (int)luaL_checkinteger(L, 3);
	int startAngle = (int)luaL_checkinteger(L, 4);
	int endAngle = (int)luaL_checkinteger(L, 5);
	int color = (int)luaL_checkinteger(L, 6);
	
	if (!currentXBuf) return 0;
	
	// Clamp center coordinates to safe bounds (auto-adjust if too low/high)
	if (cx < 0) cx = 0;
	if (cx >= OVL_W) cx = OVL_W - 1;
	if (cy < 0) cy = 0;
	if (cy >= OVL_H) cy = OVL_H - 1; // Clamp to safe position
	
	// Validate and reduce radius if arc would extend past safe bounds
	if (radius <= 0) return 0;
	if (cy + radius > 231) radius = 231 - cy;
	if (cy - radius < 0 && radius > cy) radius = cy;
	if (radius < 0) return 0;
	
	// Normalize angles to 0-360 range
	startAngle = ((startAngle % 360) + 360) % 360;
	endAngle = ((endAngle % 360) + 360) % 360;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Calculate bounding box for filled arc
	int minX = (cx - radius < 0) ? 0 : (cx - radius);
	int maxX = (cx + radius >= OVL_W) ? OVL_W - 1 : (cx + radius);
	int minY = (cy - radius < 0) ? 0 : (cy - radius);
	int maxY = (cy + radius >= OVL_H) ? OVL_H - 1 : (cy + radius);
	
	if (minX >= OVL_W || minY >= OVL_H || maxX < 0 || maxY < 0) return 0;
	
	// Fill arc by checking if each pixel is inside the arc
	for (int py = minY; py <= maxY; ++py) {
		for (int px = minX; px <= maxX; ++px) {
			// Calculate distance from center
			int dx = px - cx;
			int dy = py - cy;
			int distSq = dx * dx + dy * dy;
			int radiusSq = radius * radius;
			
			// Check if pixel is within circle radius
			if (distSq <= radiusSq) {
				// Calculate angle of point relative to center
				double angleRad = atan2((double)dy, (double)dx);
				int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
				int a = ((angleDeg % 360) + 360) % 360;
				
				// Check if angle is within arc range
				bool inRange;
				if (startAngle <= endAngle) {
					inRange = (a >= startAngle && a <= endAngle);
				} else {
					// Arc crosses 0°/360° boundary (e.g., 350° to 10°)
					inRange = (a >= startAngle || a <= endAngle);
				}
				
				if (inRange) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
						uint8 *dest = currentXBuf + py * OVL_W + px;
						*dest = apply_blend_mode(*dest, mappedColor);
						drewSomething = true;
					}
				}
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Helper function to draw an arc segment for rounded rectangle corners
// This draws only the arc pixels within the specified angle range
static void draw_rounded_corner(uint8* buf, int cx, int cy, int radius, int startAngle, int endAngle, uint8 color) {
	if (radius <= 0) return;
	int x = 0;
	int y = radius;
	int d = 1 - radius;
	while (x <= y) {
		int points[8][2];
		points[0][0] = cx + x; points[0][1] = cy + y;
		points[1][0] = cx - x; points[1][1] = cy + y;
		points[2][0] = cx + x; points[2][1] = cy - y;
		points[3][0] = cx - x; points[3][1] = cy - y;
		points[4][0] = cx + y; points[4][1] = cy + x;
		points[5][0] = cx - y; points[5][1] = cy + x;
		points[6][0] = cx + y; points[6][1] = cy - x;
		points[7][0] = cx - y; points[7][1] = cy - x;
		for (int i = 0; i < 8; ++i) {
			int px = points[i][0];
			int py = points[i][1];
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
				int dx = px - cx;
				int dy = py - cy;
				double angleRad = atan2((double)dy, (double)dx);
				int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
				int a = ((angleDeg % 360) + 360) % 360;
				bool inRange;
				if (startAngle <= endAngle) {
					inRange = (a >= startAngle && a <= endAngle);
				} else {
					inRange = (a >= startAngle || a <= endAngle);
				}
				if (inRange) {
					buf[py * OVL_W + px] = color;
				}
			}
		}
		if (d < 0) {
			d += 2 * x + 3;
		} else {
			d += 2 * (x - y) + 5;
			y--;
		}
		x++;
	}
}

// Lua drawing function - allows scripts to draw a rounded rectangle outline
int lua_drawroundrect(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 6) {
		return luaL_error(L, "drawroundrect(x, y, w, h, radius, color) requires 6 arguments");
	}
	
	int x = (int)luaL_checkinteger(L, 1);
	int y = (int)luaL_checkinteger(L, 2);
	int w = (int)luaL_checkinteger(L, 3);
	int h = (int)luaL_checkinteger(L, 4);
	int radius = (int)luaL_checkinteger(L, 5);
	int color = (int)luaL_checkinteger(L, 6);
	
	if (!currentXBuf) return 0;
	
	// Clamp coordinates to safe bounds (auto-adjust if too low/high)
	if (x < 0) x = 0;
	if (x >= OVL_W) x = OVL_W - 1;
	if (y < 0) y = 0;
	if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
	
	// Clamp rectangle to valid bounds
	if (w <= 0 || h <= 0) return 0;
	if (y + h > OVL_H) h = OVL_H - y; // Reduce height to fit
	if (h <= 0) return 0;
	
	// Validate dimensions
	if (w <= 0 || h <= 0) return 0;
	if (radius < 0) radius = 0;
	
	// Clamp radius to not exceed half the width or height
	if (radius > w / 2) radius = w / 2;
	if (radius > h / 2) radius = h / 2;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	int x2 = x + w - 1;
	int y2 = y + h - 1;
	
	// Draw rounded corners if radius > 0
	if (radius > 0) {
		// Top-left corner (180° to 270°, going clockwise)
		draw_rounded_corner(currentXBuf, x + radius, y + radius, radius, 180, 270, mappedColor);
		
		// Top-right corner (270° to 360°/0°, going clockwise)
		draw_rounded_corner(currentXBuf, x2 - radius, y + radius, radius, 270, 360, mappedColor);
		
		// Bottom-right corner (0° to 90°)
		draw_rounded_corner(currentXBuf, x2 - radius, y2 - radius, radius, 0, 90, mappedColor);
		
		// Bottom-left corner (90° to 180°)
		draw_rounded_corner(currentXBuf, x + radius, y2 - radius, radius, 90, 180, mappedColor);
	}
	
	// Draw straight edges (connecting the rounded corners)
	// Top edge (from top-left corner end to top-right corner start)
	if (radius < w / 2) {
		int topStartX = x + radius;
		int topEndX = x2 - radius;
		for (int px = topStartX; px <= topEndX; ++px) {
			if (px >= 0 && px < OVL_W && y >= 0 && y < OVL_H) {
				currentXBuf[y * OVL_W + px] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Bottom edge
	if (radius < w / 2) {
		int botStartX = x + radius;
		int botEndX = x2 - radius;
		for (int px = botStartX; px <= botEndX; ++px) {
			if (px >= 0 && px < OVL_W && y2 >= 0 && y2 < OVL_H) {
				currentXBuf[y2 * OVL_W + px] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Left edge (from top-left corner end to bottom-left corner start)
	if (radius < h / 2) {
		int leftStartY = y + radius;
		int leftEndY = y2 - radius;
		for (int py = leftStartY; py <= leftEndY; ++py) {
			if (x >= 0 && x < OVL_W && py >= 0 && py < OVL_H) {
				currentXBuf[py * OVL_W + x] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Right edge
	if (radius < h / 2) {
		int rightStartY = y + radius;
		int rightEndY = y2 - radius;
		for (int py = rightStartY; py <= rightEndY; ++py) {
			if (x2 >= 0 && x2 < OVL_W && py >= 0 && py < OVL_H) {
				currentXBuf[py * OVL_W + x2] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Special case: if radius is 0 or rectangle is too small, draw as regular rectangle
	if (radius == 0 || (radius >= w / 2 && radius >= h / 2)) {
		// Draw all four edges
		for (int px = x; px <= x2; ++px) {
			if (px >= 0 && px < OVL_W && y >= 0 && y < OVL_H) {
				currentXBuf[y * OVL_W + px] = mappedColor;
				drewSomething = true;
			}
			if (px >= 0 && px < OVL_W && y2 >= 0 && y2 < OVL_H) {
				currentXBuf[y2 * OVL_W + px] = mappedColor;
				drewSomething = true;
			}
		}
		for (int py = y; py <= y2; ++py) {
			if (x >= 0 && x < OVL_W && py >= 0 && py < OVL_H) {
				currentXBuf[py * OVL_W + x] = mappedColor;
				drewSomething = true;
			}
			if (x2 >= 0 && x2 < OVL_W && py >= 0 && py < OVL_H) {
				currentXBuf[py * OVL_W + x2] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Lua drawing function - allows scripts to draw a filled rounded rectangle
int lua_fillroundrect(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 6) {
		return luaL_error(L, "fillroundrect(x, y, w, h, radius, color) requires 6 arguments");
	}
	
	int x = (int)luaL_checkinteger(L, 1);
	int y = (int)luaL_checkinteger(L, 2);
	int w = (int)luaL_checkinteger(L, 3);
	int h = (int)luaL_checkinteger(L, 4);
	int radius = (int)luaL_checkinteger(L, 5);
	int color = (int)luaL_checkinteger(L, 6);
	
	if (!currentXBuf) return 0;
	
	// Clamp coordinates to safe bounds (auto-adjust if too low/high)
	if (x < 0) x = 0;
	if (x >= OVL_W) x = OVL_W - 1;
	if (y < 0) y = 0;
	if (y >= OVL_H) y = OVL_H - 1; // Clamp to safe position
	
	// Clamp rectangle to valid bounds
	if (w <= 0 || h <= 0) return 0;
	if (y + h > OVL_H) h = OVL_H - y; // Reduce height to fit
	if (h <= 0) return 0;
	
	// Validate dimensions
	if (w <= 0 || h <= 0) return 0;
	if (radius < 0) radius = 0;
	
	// Clamp radius to not exceed half the width or height
	if (radius > w / 2) radius = w / 2;
	if (radius > h / 2) radius = h / 2;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	int x2 = x + w - 1;
	int y2 = y + h - 1;
	
	// Fill the rounded rectangle by checking each pixel
	int minX = (x < 0) ? 0 : x;
	int maxX = (x2 >= OVL_W) ? OVL_W - 1 : x2;
	int minY = (y < 0) ? 0 : y;
	int maxY = (y2 >= OVL_H) ? OVL_H - 1 : y2;
	
	if (minX >= OVL_W || minY >= OVL_H || maxX < 0 || maxY < 0) return 0;
	
	// Fill center rectangle (if there's a center area)
	if (radius < w / 2 && radius < h / 2) {
		int centerX1 = x + radius;
		int centerX2 = x2 - radius;
		int centerY1 = y + radius;
		int centerY2 = y2 - radius;
		for (int py = centerY1; py <= centerY2; ++py) {
			for (int px = centerX1; px <= centerX2; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
					currentXBuf[py * OVL_W + px] = mappedColor;
					drewSomething = true;
				}
			}
		}
	}
	
	// Fill corner areas using filled arcs
	if (radius > 0) {
		// Top-left corner (pie slice from 180° to 270°)
		int tlCx = x + radius;
		int tlCy = y + radius;
		for (int py = minY; py < y + radius; ++py) {
			for (int px = minX; px < x + radius; ++px) {
				int dx = px - tlCx;
				int dy = py - tlCy;
				int distSq = dx * dx + dy * dy;
				int radiusSq = radius * radius;
				if (distSq <= radiusSq) {
					double angleRad = atan2((double)dy, (double)dx);
					int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
					int a = ((angleDeg % 360) + 360) % 360;
					if (a >= 180 && a <= 270) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
							currentXBuf[py * OVL_W + px] = mappedColor;
							drewSomething = true;
						}
					}
				}
			}
		}
		
		// Top-right corner (pie slice from 270° to 360°/0°)
		int trCx = x2 - radius;
		int trCy = y + radius;
		for (int py = minY; py < y + radius; ++py) {
			for (int px = x2 - radius + 1; px <= maxX; ++px) {
				int dx = px - trCx;
				int dy = py - trCy;
				int distSq = dx * dx + dy * dy;
				int radiusSq = radius * radius;
				if (distSq <= radiusSq) {
					double angleRad = atan2((double)dy, (double)dx);
					int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
					int a = ((angleDeg % 360) + 360) % 360;
					if (a >= 270 || a <= 0) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
							currentXBuf[py * OVL_W + px] = mappedColor;
							drewSomething = true;
						}
					}
				}
			}
		}
		
		// Bottom-right corner (pie slice from 0° to 90°)
		int brCx = x2 - radius;
		int brCy = y2 - radius;
		for (int py = y2 - radius + 1; py <= maxY; ++py) {
			for (int px = x2 - radius + 1; px <= maxX; ++px) {
				int dx = px - brCx;
				int dy = py - brCy;
				int distSq = dx * dx + dy * dy;
				int radiusSq = radius * radius;
				if (distSq <= radiusSq) {
					double angleRad = atan2((double)dy, (double)dx);
					int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
					int a = ((angleDeg % 360) + 360) % 360;
					if (a >= 0 && a <= 90) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
							currentXBuf[py * OVL_W + px] = mappedColor;
							drewSomething = true;
						}
					}
				}
			}
		}
		
		// Bottom-left corner (pie slice from 90° to 180°)
		int blCx = x + radius;
		int blCy = y2 - radius;
		for (int py = y2 - radius + 1; py <= maxY; ++py) {
			for (int px = minX; px < x + radius; ++px) {
				int dx = px - blCx;
				int dy = py - blCy;
				int distSq = dx * dx + dy * dy;
				int radiusSq = radius * radius;
				if (distSq <= radiusSq) {
					double angleRad = atan2((double)dy, (double)dx);
					int angleDeg = (int)(angleRad * 180.0 / 3.14159265358979323846);
					int a = ((angleDeg % 360) + 360) % 360;
					if (a >= 90 && a <= 180) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
							currentXBuf[py * OVL_W + px] = mappedColor;
							drewSomething = true;
						}
					}
				}
			}
		}
		
		// Fill edge rectangles (between corners)
		// Top edge
		if (radius < w / 2) {
			for (int py = minY; py < y + radius; ++py) {
				for (int px = x + radius; px <= x2 - radius; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
						currentXBuf[py * OVL_W + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Bottom edge
		if (radius < w / 2) {
			for (int py = y2 - radius + 1; py <= maxY; ++py) {
				for (int px = x + radius; px <= x2 - radius; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
						currentXBuf[py * OVL_W + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Left edge
		if (radius < h / 2) {
			for (int py = y + radius; py <= y2 - radius; ++py) {
				for (int px = minX; px < x + radius; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
						currentXBuf[py * OVL_W + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Right edge
		if (radius < h / 2) {
			for (int py = y + radius; py <= y2 - radius; ++py) {
				for (int px = x2 - radius + 1; px <= maxX; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
						currentXBuf[py * OVL_W + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
	} else {
		// No rounding: fill entire rectangle (same as fillrect)
		for (int py = minY; py <= maxY; ++py) {
			for (int px = minX; px <= maxX; ++px) {
		 if (px >= 0 && px < OVL_W && py >= 0 && py < OVL_H) {
					currentXBuf[py * OVL_W + px] = mappedColor;
					drewSomething = true;
				}
			}
		}
	}
	
	if (drewSomething) {
		g_overlayDirty = true;  // Mark that something was drawn
	}
	
	return 0;
}

// Get current FPS
int lua_getfps(lua_State *L) {
	 lua_pushnumber(L, currentFPS);
	 return 1;
 }

int lua_readbyte(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "readbyte(address) requires 1 argument");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "readbyte: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Use ARead which handles all memory mapping correctly
	 // This reads through the proper memory handlers for RAM, ROM, PPU, etc.
	 uint8 value = ARead[address](address);
	 
	 lua_pushinteger(L, value);
	 return 1;
 }

int lua_readword(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "readword(address) requires 1 argument");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "readword: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Read two bytes in little-endian format (low byte first, high byte second)
	 uint8 lowByte = ARead[address](address);
	 uint8 highByte = 0;
	 
	 if (address + 1 <= 0xFFFF) {
		 highByte = ARead[address + 1](address + 1);
	 }
	 
	 // Combine into 16-bit value (little-endian)
	 uint16 value = lowByte + (highByte * 256);
	 
	 lua_pushinteger(L, value);
	 return 1;
 }

int lua_readbytes(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 1) {
		 return luaL_error(L, "readbytes(address, count) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int count = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range
	 if (address > 0xFFFF) {
		 return luaL_error(L, "readbytes: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate count
	 if (count < 1) {
		 return luaL_error(L, "readbytes: count must be at least 1");
	 }
	 if (count > 256) {
		 return luaL_error(L, "readbytes: count cannot exceed 256");
	 }
	 
	 // Adjust count if it would read past address space
	 if (address + count > 0x10000) {
		 count = 0x10000 - address;
	 }
	 
	 // Create Lua table to hold results
	 lua_createtable(L, count, 0);
	 
	 // Read each byte and add to table
	 for (int i = 0; i < count; ++i) {
		 unsigned int currentAddr = address + i;
		 if (currentAddr > 0xFFFF) break;
		 
		 uint8 value = ARead[currentAddr](currentAddr);
		 lua_pushinteger(L, value);
		 lua_rawseti(L, -2, i + 1);  // Lua tables are 1-indexed
	 }
	 
	 return 1;  // Return the table
 }

int lua_readram(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "readram(startAddr, count) requires 2 arguments");
	}
	
	unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);
	
	// Validate address is in RAM range (0x0000-0x1FFF)
	if (startAddr > 0x1FFF) {
		return luaL_error(L, "readram: startAddr must be in RAM range 0x0000-0x1FFF");
	}
	
	// Validate count
	if (count < 1) {
		return luaL_error(L, "readram: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "readram: count cannot exceed 256");
	}
	
	// Adjust count if it would read past RAM boundary (0x1FFF)
	if (startAddr + count > 0x2000) {
		count = 0x2000 - startAddr;
	}
	
	// Create Lua table to hold results
	lua_createtable(L, count, 0);
	
	// Read each byte from RAM and add to table
	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = startAddr + i;
		if (currentAddr > 0x1FFF) break;
		
		uint8 value = ARead[currentAddr](currentAddr);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, i + 1);  // Lua tables are 1-indexed
	}
	
	return 1;  // Return the table
}

int lua_scanbyte(lua_State *L) {
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

int lua_setbit(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "setbit(address, bit) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int bit = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "setbit: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate bit range (must be 0-7)
	 if (bit < 0 || bit > 7) {
		 return luaL_error(L, "setbit: bit must be in range 0-7");
	 }
	 
	 // Read current byte value
	 uint8 currentValue = ARead[address](address);
	 
	 // Set the specified bit using bitwise OR
	 uint8 newValue = currentValue | (1 << bit);
	 
	 // Write back using BWrite which handles all memory mapping correctly
	 BWrite[address](address, newValue);
	 
	 return 0;
 }

int lua_clearbit(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "clearbit(address, bit) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int bit = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "clearbit: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate bit range (must be 0-7)
	 if (bit < 0 || bit > 7) {
		 return luaL_error(L, "clearbit: bit must be in range 0-7");
	 }
	 
	 // Read current byte value
	 uint8 currentValue = ARead[address](address);
	 
	 // Clear the specified bit using bitwise AND with inverted mask
	 uint8 newValue = currentValue & ~(1 << bit);
	 
	 // Write back using BWrite which handles all memory mapping correctly
	 BWrite[address](address, newValue);
	 
	 return 0;
 }

int lua_writebyte(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "writebyte(address, value) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int value = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "writebyte: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate value range (byte must be 0-255)
	 if (value < 0 || value > 255) {
		 return luaL_error(L, "writebyte: value must be in range 0-255");
	 }
	 
	 uint8 byteValue = (uint8)(value & 0xFF);
	 
	 // Use BWrite which handles all memory mapping correctly
	 // This writes through the proper memory handlers for RAM, PPU, etc.
	 // Note: Writing to ROM addresses will typically be ignored by the mapper
	 BWrite[address](address, byteValue);
	 
	 return 0;
 }

int lua_writeword(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "writeword(address, value) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int value = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "writeword: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate value range (16-bit word must be 0-65535)
	 if (value < 0 || value > 65535) {
		 return luaL_error(L, "writeword: value must be in range 0-65535");
	 }
	 
	 // Write low byte (little-endian: low byte first)
	 uint8 lowByte = (uint8)(value & 0xFF);
	 uint8 highByte = (uint8)((value >> 8) & 0xFF);
	 
	 // Write both bytes
	 if (address <= 0xFFFF) {
		 BWrite[address](address, lowByte);
	 }
	 if (address + 1 <= 0xFFFF) {
		 BWrite[address + 1](address + 1, highByte);
	 }
	 
	 return 0;
 }

int lua_writebytes(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "writebytes(address, value1, value2, ...) requires at least 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 
	 // Validate address range
	 if (address > 0xFFFF) {
		 return luaL_error(L, "writebytes: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Number of values to write (excluding address)
	 int count = n - 1;
	 
	 // Write each byte
	 for (int i = 0; i < count; ++i) {
		 int value = (int)luaL_checkinteger(L, i + 2);
		 
		 // Validate value range (byte must be 0-255)
		 if (value < 0 || value > 255) {
			 return luaL_error(L, "writebytes: value %d must be in range 0-255", i + 1);
		 }
		 
		 unsigned int currentAddr = address + i;
		 if (currentAddr > 0xFFFF) {
			 // Don't write past address space, but don't error - just stop
			 break;
		 }
		 
		 uint8 byteValue = (uint8)(value & 0xFF);
		 BWrite[currentAddr](currentAddr, byteValue);
	 }
	 
	 return 0;
 }

int lua_writeprg(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "writeprg(address, value) requires 2 arguments");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int value = (int)luaL_checkinteger(L, 2);
	
	// Validate address is in program ROM range (0x8000-0xFFFF)
	if (address < 0x8000 || address > 0xFFFF) {
		return luaL_error(L, "writeprg: address must be in program ROM range 0x8000-0xFFFF");
	}
	
	// Validate value range (byte must be 0-255)
	if (value < 0 || value > 255) {
		return luaL_error(L, "writeprg: value must be in range 0-255");
	}
	
	uint8 byteValue = (uint8)(value & 0xFF);
	
	// Attempt to write to program ROM using BWrite
	// Note: Most ROM is read-only, but some mappers support ROM writes
	// BWrite will handle mapper-specific behavior (may ignore if ROM is read-only)
	BWrite[address](address, byteValue);
	 
	 return 0;
 }

int lua_fillbytes(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return luaL_error(L, "fillbytes(address, count, value) requires 3 arguments");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);
	int value = (int)luaL_checkinteger(L, 3);
	
	// Validate address range
	if (address > 0xFFFF) {
		return luaL_error(L, "fillbytes: address must be in range 0x0000-0xFFFF");
	}
	
	// Validate count
	if (count < 1) {
		return luaL_error(L, "fillbytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "fillbytes: count cannot exceed 256");
	}
	
	// Validate value range (byte must be 0-255)
	if (value < 0 || value > 255) {
		return luaL_error(L, "fillbytes: value must be in range 0-255");
	}
	
	uint8 byteValue = (uint8)(value & 0xFF);
	
	// Adjust count if it would write past address space
	if (address + count > 0x10000) {
		count = 0x10000 - address;
	}
	
	// Fill the memory region
	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) break;
		
		 BWrite[currentAddr](currentAddr, byteValue);
	 }
	 
	 return 0;
 }

int lua_copybytes(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return luaL_error(L, "copybytes(sourceAddr, destAddr, count) requires 3 arguments");
	}
	
	unsigned int sourceAddr = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int destAddr = (unsigned int)luaL_checkinteger(L, 2);
	int count = (int)luaL_checkinteger(L, 3);
	
	// Validate address ranges
	if (sourceAddr > 0xFFFF) {
		return luaL_error(L, "copybytes: sourceAddr must be in range 0x0000-0xFFFF");
	}
	if (destAddr > 0xFFFF) {
		return luaL_error(L, "copybytes: destAddr must be in range 0x0000-0xFFFF");
	}
	
	// Validate count
	if (count < 1) {
		return luaL_error(L, "copybytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "copybytes: count cannot exceed 256");
	}
	
	// Adjust count if it would read/write past address space
	if (sourceAddr + count > 0x10000) {
		count = 0x10000 - sourceAddr;
	}
	if (destAddr + count > 0x10000) {
		count = 0x10000 - destAddr;
	}
	
	// Handle overlapping regions correctly
	// If destAddr > sourceAddr and they overlap, we need to copy backwards
	// to avoid overwriting source data before it's read
	if (destAddr > sourceAddr && destAddr < sourceAddr + count) {
		// Overlapping region: copy backwards from end to beginning
		for (int i = count - 1; i >= 0; --i) {
			unsigned int srcAddr = sourceAddr + i;
			unsigned int dstAddr = destAddr + i;
			if (srcAddr > 0xFFFF || dstAddr > 0xFFFF) break;
			
			uint8 value = ARead[srcAddr](srcAddr);
			BWrite[dstAddr](dstAddr, value);
		}
	} else {
		// Non-overlapping or destAddr <= sourceAddr: copy forwards
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

int lua_comparebytes(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return luaL_error(L, "comparebytes(addr1, addr2, count) requires 3 arguments");
	}
	
	unsigned int addr1 = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int addr2 = (unsigned int)luaL_checkinteger(L, 2);
	int count = (int)luaL_checkinteger(L, 3);
	
	// Validate address ranges
	if (addr1 > 0xFFFF) {
		return luaL_error(L, "comparebytes: addr1 must be in range 0x0000-0xFFFF");
	}
	if (addr2 > 0xFFFF) {
		return luaL_error(L, "comparebytes: addr2 must be in range 0x0000-0xFFFF");
	}
	
	// Validate count
	if (count < 1) {
		return luaL_error(L, "comparebytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "comparebytes: count cannot exceed 256");
	}
	
	// Adjust count if it would read past address space
	if (addr1 + count > 0x10000) {
		count = 0x10000 - addr1;
	}
	if (addr2 + count > 0x10000) {
		int count2 = 0x10000 - addr2;
		if (count2 < count) {
			count = count2;
		}
	}
	
	// Compare bytes
	for (int i = 0; i < count; ++i) {
		unsigned int addr1_current = addr1 + i;
		unsigned int addr2_current = addr2 + i;
		if (addr1_current > 0xFFFF || addr2_current > 0xFFFF) break;
		
		uint8 value1 = ARead[addr1_current](addr1_current);
		uint8 value2 = ARead[addr2_current](addr2_current);
		
		if (value1 != value2) {
			lua_pushboolean(L, 0);  // false (not identical)
			return 1;
		}
	}
	
	lua_pushboolean(L, 1);  // true (identical)
	return 1;
}

int lua_backupbytes(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "backupbytes(address, count) requires 2 arguments");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	int count = (int)luaL_checkinteger(L, 2);
	
	// Validate address range
	if (address > 0xFFFF) {
		return luaL_error(L, "backupbytes: address must be in range 0x0000-0xFFFF");
	}
	
	// Validate count
	if (count < 1) {
		return luaL_error(L, "backupbytes: count must be at least 1");
	}
	if (count > 256) {
		return luaL_error(L, "backupbytes: count cannot exceed 256");
	}
	
	// Adjust count if it would read past address space
	if (address + count > 0x10000) {
		count = 0x10000 - address;
	}
	
	// Create Lua table to hold backup
	lua_createtable(L, count, 0);
	
	// Read each byte and add to table
	for (int i = 0; i < count; ++i) {
		unsigned int currentAddr = address + i;
		if (currentAddr > 0xFFFF) break;
		
		uint8 value = ARead[currentAddr](currentAddr);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, i + 1);  // Lua tables are 1-indexed
	}
	
	return 1;  // Return the backup table
}

int lua_restorebytes(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "restorebytes(address, data) requires 2 arguments");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	
	// Validate address range
	if (address > 0xFFFF) {
		return luaL_error(L, "restorebytes: address must be in range 0x0000-0xFFFF");
	}
	
	// Validate that second argument is a table
	if (!lua_istable(L, 2)) {
		return luaL_error(L, "restorebytes: data must be a table (from backupbytes)");
	}
	
	// Read table entries (1-indexed), Lua 5.1-compatible
	int count = 0;
	for (int i = 1; i <= 256; ++i) {
		lua_rawgeti(L, 2, i);
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			break;  // End of table
		}
		int value = (int)luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		
		// Validate value range (byte must be 0-255)
		if (value < 0 || value > 255) {
			return luaL_error(L, "restorebytes: value at index %d must be in range 0-255", i);
		}
		
		unsigned int currentAddr = address + count;
		if (currentAddr > 0xFFFF) {
			// Don't write past address space, but don't error - just stop
			break;
		}
		
		uint8 byteValue = (uint8)(value & 0xFF);
		BWrite[currentAddr](currentAddr, byteValue);
		++count;
	}
	
	if (count <= 0) {
		return luaL_error(L, "restorebytes: data table must contain at least one byte");
	}
	
	return 0;  // Return nothing
}

int lua_getmemorytype(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "getmemorytype(address) requires 1 argument");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	
	// Validate address range
	if (address > 0xFFFF) {
		return luaL_error(L, "getmemorytype: address must be in range 0x0000-0xFFFF");
	}
	
	// Determine memory type based on address range
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
		// 0x4020-0x7FFF: Expansion ROM, Save RAM, or mapper-specific
		memType = "UNKNOWN";
	}
	
	// Push string to Lua (using full Lua API from lua.h)
	lua_pushstring(L, memType);
	
	return 1;  // Return the string
}

int lua_ismemorywritable(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "ismemorywritable(address) requires 1 argument");
	}
	
	unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	
	// Validate address range
	if (address > 0xFFFF) {
		return luaL_error(L, "ismemorywritable: address must be in range 0x0000-0xFFFF");
	}
	
	// Determine if address is writable based on memory type
	// RAM, PPU, and APU are writable
	// ROM and UNKNOWN regions are typically not writable (though ROM writes may be mapper-specific)
	int isWritable;
	if (address <= 0x1FFF) {
		// RAM: writable
		isWritable = 1;
	} else if (address >= 0x2000 && address <= 0x3FFF) {
		// PPU: writable (though writes have side effects)
		isWritable = 1;
	} else if (address >= 0x4000 && address <= 0x401F) {
		// APU: writable (though writes have side effects)
		isWritable = 1;
	} else {
		// ROM (0x8000-0xFFFF) and UNKNOWN (0x4020-0x7FFF): typically not writable
		// Note: ROM writes may be supported by some mappers, but generally ROM is read-only
		isWritable = 0;
	}
	
	lua_pushboolean(L, isWritable);
	
	return 1;  // Return the boolean
 }
 
 // Initialize Lua (original version - checks disabled state)
 static void InitLua() {
	 // CRITICAL: Check disabled state BEFORE initializing Lua
	 if (s_luaDisabled) {
		 printf("InitLua: BLOCKED - Lua is disabled, refusing to initialize\n");
		 return;
	 }
	 
	 if (luaState != NULL) {
		 printf("InitLua: Closing existing Lua state\n");
		 lua_close(luaState);
	 }
	 
	 printf("InitLua: Opening new Lua state (disabled=%d)\n", FCEU_LuaIsDisabled());
	 luaState = lua_open();
	 if (luaState == NULL) {
		 printf("InitLua: FAILED - lua_open() returned NULL\n");
		 return;
	 }
	 
	 printf("InitLua: Loading Lua libraries\n");
	 luaL_openlibs(luaState);
	 
	 // Register FCEU functions
	 lua_register(luaState, "drawtext", lua_drawtext);
	 lua_register(luaState, "drawtextwh", lua_drawtextwh);
	 lua_register(luaState, "drawtextscaled", lua_drawtextscaled);
	 lua_register(luaState, "drawtextrotated", lua_drawtextrotated);
	 lua_pushcfunction(luaState, lua_gettextwidth);
	 lua_setglobal(luaState, "gettextwidth");
	 lua_pushcfunction(luaState, lua_gettextheight);
	 lua_setglobal(luaState, "gettextheight");
	 lua_register(luaState, "drawtextbox", lua_drawtextbox);
	 lua_register(luaState, "getjoypad", lua_getjoypad);
	 lua_register(luaState, "gethardwarejoypad", lua_gethardwarejoypad);
	 lua_pushcfunction(luaState, lua_setjoypad);
	 lua_setglobal(luaState, "setjoypad");
	 lua_pushcfunction(luaState, lua_clearjoypad);
	 lua_setglobal(luaState, "clearjoypad");
	 lua_pushcfunction(luaState, lua_pressbutton);
	 lua_setglobal(luaState, "pressbutton");
	 lua_pushcfunction(luaState, lua_releasebutton);
	 lua_setglobal(luaState, "releasebutton");
	 lua_register(luaState, "startinputrecording", lua_startinputrecording);
	 lua_register(luaState, "stopinputrecording", lua_stopinputrecording);
	 lua_pushcfunction(luaState, lua_playinputrecording);
	 lua_setglobal(luaState, "playinputrecording");
	 lua_register(luaState, "getromname", lua_getromname);
	 lua_register(luaState, "getframecount", lua_getframecount);
	 lua_register(luaState, "getframecycles", lua_getframecycles);
	 lua_register(luaState, "getelapsedtime", lua_getelapsedtime);
	 lua_register(luaState, "getelapsedframes", lua_getelapsedframes);
	 lua_register(luaState, "gettime", lua_gettime);
	 lua_register(luaState, "gettimedelta", lua_gettimedelta);
	 lua_register(luaState, "getscreenwidth", lua_getscreenwidth);
	 lua_register(luaState, "getscreenheight", lua_getscreenheight);
	 lua_register(luaState, "getscreensize", lua_getscreensize);
	 lua_register(luaState, "getaudioenabled", lua_getaudioenabled);
	 lua_register(luaState, "getaudiosample", lua_getaudiosample);
	 lua_register(luaState, "getcolorrgb", lua_getcolorrgb);
	 lua_register(luaState, "getpalettecolor", lua_getpalettecolor);
	 lua_register(luaState, "setpalettecolor", lua_setpalettecolor);
	 lua_register(luaState, "getnescolor", lua_getnescolor);
	 lua_register(luaState, "blendcolors", lua_blendcolors);
	 lua_register(luaState, "sleepframes", lua_sleepframes);
	 lua_register(luaState, "getromsize", lua_getromsize);
	 lua_register(luaState, "getprgsize", lua_getprgsize);
	 lua_register(luaState, "getchrsize", lua_getchrsize);
	 lua_register(luaState, "hasbattery", lua_hasbattery);
	 lua_register(luaState, "isframeadvancing", lua_isframeadvancing);
	 lua_register(luaState, "isrewinding", lua_isrewinding);
	 lua_register(luaState, "isfastforwarding", lua_isfastforwarding);
	 lua_register(luaState, "getgamegeniecode", lua_getgamegeniecode);
	 lua_register(luaState, "decodegamegenie", lua_decodegamegenie);
	 lua_register(luaState, "getmapper", lua_getmapper);
	 lua_register(luaState, "getmapperstring", lua_getmapperstring);
	 lua_register(luaState, "isbuttonpressed", lua_isbuttonpressed);
	 lua_register(luaState, "isxboxbuttonpressed", lua_isxboxbuttonpressed);
	 lua_register(luaState, "getbuttonname", lua_getbuttonname);
	 lua_register(luaState, "getbuttonmask", lua_getbuttonmask);
	 lua_register(luaState, "drawpixel", lua_drawpixel);
	 lua_register(luaState, "drawline", lua_drawline);
	 lua_register(luaState, "drawthickline", lua_drawthickline);
	 lua_register(luaState, "drawpolygon", lua_drawpolygon);
	 lua_register(luaState, "drawpolyline", lua_drawpolyline);
	 lua_register(luaState, "fillpolygon", lua_fillpolygon);
	 lua_register(luaState, "drawrect", lua_drawrect);
	 lua_register(luaState, "fillrect", lua_fillrect);
	 lua_register(luaState, "clearrect", lua_clearrect);
	 lua_register(luaState, "clearscreen", lua_clearscreen);
	 lua_register(luaState, "fillscreen", lua_fillscreen);
	 lua_register(luaState, "screenshot", lua_screenshot);
	 lua_register(luaState, "savestate", lua_savestate);
	 lua_register(luaState, "loadstate", lua_loadstate);
	 lua_register(luaState, "hasstate", lua_hasstate);
	 lua_register(luaState, "savestatefile", lua_savestatefile);
	 lua_register(luaState, "loadstatefile", lua_loadstatefile);
	 lua_register(luaState, "drawimage", lua_drawimage);
	 lua_register(luaState, "drawimageindexed", lua_drawimageindexed);
	 lua_register(luaState, "drawtile", lua_drawtile);
	 lua_register(luaState, "drawchrtile", lua_drawchrtile);
	 lua_register(luaState, "setdrawmode", lua_setdrawmode);
	 lua_register(luaState, "setclipregion", lua_setclipregion);
	 lua_register(luaState, "clearclipregion", lua_clearclipregion);
	 lua_register(luaState, "setdrawcolor", lua_setdrawcolor);
	 lua_register(luaState, "drawcircle", lua_drawcircle);
	 lua_register(luaState, "fillcircle", lua_fillcircle);
	 lua_register(luaState, "drawtriangle", lua_drawtriangle);
	 lua_register(luaState, "filltriangle", lua_filltriangle);
	 lua_register(luaState, "drawellipse", lua_drawellipse);
	 lua_register(luaState, "fillellipse", lua_fillellipse);
	 lua_register(luaState, "drawarc", lua_drawarc);
	 lua_register(luaState, "fillarc", lua_fillarc);
	 lua_register(luaState, "drawroundrect", lua_drawroundrect);
	 lua_register(luaState, "fillroundrect", lua_fillroundrect);
	 lua_register(luaState, "getfps", lua_getfps);
	 lua_register(luaState, "readbyte", lua_readbyte);
	 lua_register(luaState, "readword", lua_readword);
	 lua_register(luaState, "readbytes", lua_readbytes);
	 lua_register(luaState, "readram", lua_readram);
	 lua_register(luaState, "scanbyte", lua_scanbyte);
     lua_register(luaState, "scanword", lua_scanword);
	 lua_register(luaState, "scanbytes", lua_scanbytes);
	 lua_register(luaState, "findpattern", lua_findpattern);
	 lua_register(luaState, "scanchanged", lua_scanchanged);
	 lua_register(luaState, "watchbyte", lua_watchbyte);
	 lua_register(luaState, "unwatchbyte", lua_unwatchbyte);
	 lua_register(luaState, "getmemorysnapshot", lua_getmemorysnapshot);
	 lua_register(luaState, "setbit", lua_setbit);
	 lua_register(luaState, "clearbit", lua_clearbit);
	 lua_register(luaState, "togglebit", lua_togglebit);
	 lua_register(luaState, "testbit", lua_testbit);
	 lua_register(luaState, "writebyte", lua_writebyte);
	 lua_register(luaState, "writeword", lua_writeword);
	 lua_register(luaState, "writebytes", lua_writebytes);
	 lua_register(luaState, "writeprg", lua_writeprg);
	 lua_register(luaState, "fillbytes", lua_fillbytes);
	 lua_register(luaState, "copybytes", lua_copybytes);
	 lua_register(luaState, "comparebytes", lua_comparebytes);
	 lua_register(luaState, "backupbytes", lua_backupbytes);
	 lua_register(luaState, "restorebytes", lua_restorebytes);
	 lua_register(luaState, "getmemorytype", lua_getmemorytype);
	 lua_register(luaState, "ismemorywritable", lua_ismemorywritable);
	 // logging
	 lua_register(luaState, "log", lua_log);
	 // Console spacing control
	 lua_register(luaState, "setconsolespacing", lua_setconsolespacing);
	 // Script timing controls
	 lua_register(luaState, "setscriptinterval", lua_setscriptinterval);
	 lua_register(luaState, "getscriptinterval", lua_getscriptinterval);
	 lua_pushcfunction(luaState, lua_print_redirect);
	 lua_setglobal(luaState, "print");
	 
	 luaInitialized = true;
 }

// Simple init helper - checks disabled state first
static void EnsureLuaInit() {
	if (s_luaDisabled) {
		// Do not even allocate a Lua state when disabled
		return;
	}
	if (luaInitialized && luaState) return;
	if (luaState) { lua_close(luaState); luaState = NULL; }
	luaState = lua_open();
	if (!luaState) {
		snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: alloc fail");
		return;
	}
	luaL_openlibs(luaState);
	#define REG(n,f) lua_register(luaState, n, f)
	REG("drawtext",       lua_drawtext);
	REG("drawtextwh",     lua_drawtextwh);
	REG("drawtextscaled", lua_drawtextscaled);
	REG("drawtextrotated", lua_drawtextrotated);
	lua_pushcfunction(luaState, lua_gettextwidth);
	lua_setglobal(luaState, "gettextwidth");
	lua_pushcfunction(luaState, lua_gettextheight);
	lua_setglobal(luaState, "gettextheight");
	REG("drawtextbox",    lua_drawtextbox);
	REG("getjoypad",      lua_getjoypad);
	REG("gethardwarejoypad", lua_gethardwarejoypad);
	lua_pushcfunction(luaState, lua_setjoypad);
	lua_setglobal(luaState, "setjoypad");
	lua_pushcfunction(luaState, lua_clearjoypad);
	lua_setglobal(luaState, "clearjoypad");
	lua_pushcfunction(luaState, lua_pressbutton);
	lua_setglobal(luaState, "pressbutton");
	lua_pushcfunction(luaState, lua_releasebutton);
	lua_setglobal(luaState, "releasebutton");
	REG("startinputrecording", lua_startinputrecording);
	REG("stopinputrecording", lua_stopinputrecording);
	lua_pushcfunction(luaState, lua_playinputrecording);
	lua_setglobal(luaState, "playinputrecording");
	REG("getromname", lua_getromname);
	REG("getframecount", lua_getframecount);
	REG("getframecycles", lua_getframecycles);
	REG("getelapsedtime", lua_getelapsedtime);
	REG("getelapsedframes", lua_getelapsedframes);
	REG("gettime", lua_gettime);
	REG("gettimedelta", lua_gettimedelta);
	REG("getscreenwidth", lua_getscreenwidth);
	REG("getscreenheight", lua_getscreenheight);
	REG("getscreensize", lua_getscreensize);
	REG("getaudioenabled", lua_getaudioenabled);
	REG("getaudiosample", lua_getaudiosample);
	REG("getcolorrgb", lua_getcolorrgb);
	REG("getpalettecolor", lua_getpalettecolor);
	REG("setpalettecolor", lua_setpalettecolor);
	REG("getnescolor", lua_getnescolor);
	REG("blendcolors", lua_blendcolors);
	REG("sleepframes", lua_sleepframes);
	REG("getromsize", lua_getromsize);
	REG("getprgsize", lua_getprgsize);
	REG("getchrsize", lua_getchrsize);
	REG("hasbattery", lua_hasbattery);
	REG("isframeadvancing", lua_isframeadvancing);
	REG("isrewinding", lua_isrewinding);
	REG("isfastforwarding", lua_isfastforwarding);
	REG("getgamegeniecode", lua_getgamegeniecode);
	REG("decodegamegenie", lua_decodegamegenie);
	REG("getmapper", lua_getmapper);
	REG("getmapperstring", lua_getmapperstring);
	REG("isbuttonpressed", lua_isbuttonpressed);
	REG("isxboxbuttonpressed", lua_isxboxbuttonpressed);
	REG("getbuttonname",  lua_getbuttonname);
	REG("getbuttonmask",  lua_getbuttonmask);
	REG("drawpixel",      lua_drawpixel);
	REG("drawline",       lua_drawline);
	REG("drawthickline",  lua_drawthickline);
	REG("drawpolygon",    lua_drawpolygon);
	REG("drawpolyline",   lua_drawpolyline);
	REG("fillpolygon",    lua_fillpolygon);
	REG("drawrect",       lua_drawrect);
	REG("fillrect",       lua_fillrect);
	REG("clearrect",      lua_clearrect);
	REG("clearscreen",    lua_clearscreen);
	REG("fillscreen",     lua_fillscreen);
	REG("screenshot",     lua_screenshot);
	REG("savestate",      lua_savestate);
	REG("loadstate",      lua_loadstate);
	REG("hasstate",       lua_hasstate);
	REG("savestatefile",  lua_savestatefile);
	REG("loadstatefile",  lua_loadstatefile);
	REG("drawimage",      lua_drawimage);
	REG("drawimageindexed", lua_drawimageindexed);
	REG("drawtile",       lua_drawtile);
	REG("drawchrtile",    lua_drawchrtile);
	REG("setdrawmode",    lua_setdrawmode);
	REG("setclipregion",  lua_setclipregion);
	REG("clearclipregion", lua_clearclipregion);
	REG("setdrawcolor",   lua_setdrawcolor);
	REG("drawcircle",     lua_drawcircle);
	REG("fillcircle",     lua_fillcircle);
	REG("drawtriangle",   lua_drawtriangle);
	REG("filltriangle",   lua_filltriangle);
	REG("drawellipse",    lua_drawellipse);
	REG("fillellipse",    lua_fillellipse);
	REG("drawarc",        lua_drawarc);
	REG("fillarc",        lua_fillarc);
	REG("drawroundrect",  lua_drawroundrect);
	REG("fillroundrect",  lua_fillroundrect);
	REG("getfps",         lua_getfps);
	REG("readbyte",       lua_readbyte);
	REG("readword",       lua_readword);
	REG("readbytes",      lua_readbytes);
	REG("readram",        lua_readram);
	REG("scanbyte",       lua_scanbyte);
 REG("scanword",       lua_scanword);
REG("scanbytes",      lua_scanbytes);
REG("findpattern",    lua_findpattern);
REG("scanchanged",    lua_scanchanged);
REG("watchbyte",      lua_watchbyte);
REG("unwatchbyte",    lua_unwatchbyte);
REG("getmemorysnapshot", lua_getmemorysnapshot);
REG("setbit",         lua_setbit);
REG("clearbit",       lua_clearbit);
REG("togglebit",      lua_togglebit);
REG("testbit",        lua_testbit);
	REG("writebyte",      lua_writebyte);
	REG("writeword",      lua_writeword);
	REG("writebytes",     lua_writebytes);
	REG("writeprg",       lua_writeprg);
	REG("fillbytes",      lua_fillbytes);
	REG("copybytes",      lua_copybytes);
	REG("comparebytes",   lua_comparebytes);
	REG("backupbytes",    lua_backupbytes);
	REG("restorebytes",   lua_restorebytes);
	REG("getmemorytype",  lua_getmemorytype);
	REG("ismemorywritable", lua_ismemorywritable);
	REG("log",            lua_log);
	REG("setconsolespacing", lua_setconsolespacing);
REG("setscriptinterval", lua_setscriptinterval);
REG("getscriptinterval", lua_getscriptinterval);
	lua_pushcfunction(luaState, lua_print_redirect);
	lua_setglobal(luaState, "print");
	#undef REG
	luaInitialized = true;
	snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: init OK");
}
 
 // Load and run Lua script
 int FCEU_LoadLuaScript(const char* filename) {
	 printf("FCEU_LoadLuaScript: ENTERING with filename='%s'\n", filename ? filename : "(null)");
	 if (s_luaDisabled) { 
		 strncpy(g_luaStatusMsg, "Lua: disabled", sizeof(g_luaStatusMsg)-1);
		 g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		 printf("FCEU_LoadLuaScript: BLOCKED - Lua is disabled\n");
		 return 0; 
	 }

	 if (!luaInitialized) { InitLua(); if (!luaInitialized) { printf("FCEU_LoadLuaScript: Failed to initialize Lua\n"); return 0; } }
	 if (luaState == NULL) { printf("FCEU_LoadLuaScript: luaState is NULL\n"); return 0; }

	 char fullpath[512]; const char* workingPath = NULL;

	 // Try direct path if it looks like one
	 if (filename && (strchr(filename, ':') || strchr(filename, '\\') || strchr(filename, '/'))) {
		 printf("FCEU_LoadLuaScript: Detected direct path, trying: %s\n", filename);
		 FILE* f = fopen(filename, "rb");
		 if (f) {
			 fseek(f, 0, SEEK_END);
			 long sz = ftell(f);
			 fclose(f);
			 if (sz > 0) {
				 workingPath = filename;
				 printf("FCEU_LoadLuaScript: Direct path found and valid (size=%ld): %s\n", sz, filename);
			 } else {
				 printf("FCEU_LoadLuaScript: Direct path exists but file is empty: %s\n", filename);
			 }
		 } else {
			 printf("FCEU_LoadLuaScript: Direct path not accessible: %s\n", filename);
		 }
	 }

	 // If not a direct path or missing, search known places
	 if (!workingPath) {
		 printf("FCEU_LoadLuaScript: Searching known directories for: %s\n", filename ? filename : "(null)");
		 const char* paths[] = {
			 "hdd1:\\fce360-enhanced\\lua\\%s",
			 "hdd1:\\fce360-enhanced\\Lua\\%s",
			 "game:\\lua\\%s", "game:/lua/%s", "game:\\Lua\\%s", "game:/Lua/%s",
			 "hdd1:\\lua\\%s", "hdd1:\\Lua\\%s", "usb0:\\lua\\%s", "usb0:\\Lua\\%s",
			 "game:\\%s", "game:/%s",
			 "hdd1:\\_Emus\\fce360-enhanced\\lua\\%s"
		 };
		 for (int i=0;i<(int)(sizeof(paths)/sizeof(paths[0]));++i) {
			 _snprintf(fullpath, sizeof(fullpath), paths[i], filename ? filename : "");
			 FILE* f = fopen(fullpath, "rb");
			 if (f) { 
				 fseek(f,0,SEEK_END); 
				 long sz=ftell(f); 
				 fclose(f); 
				 if (sz>0) { 
					 workingPath = fullpath; 
					 printf("FCEU_LoadLuaScript: Found in search path %d (size=%ld): %s\n", i, sz, fullpath);
					 break; 
				 }
			 }
		 }
	 }

	 if (!workingPath) { 
		 snprintf(g_luaStatusMsg,sizeof(g_luaStatusMsg),"Lua: %s NOT FOUND", filename?filename:"(null)"); 
		 printf("FCEU_LoadLuaScript: FAILED - script not found: %s\n", filename ? filename : "(null)");
		 return 0; 
	 }

	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loading %s", workingPath);

	 // Final disabled check
	 if (s_luaDisabled) return 0;

	 printf("FCEU_LoadLuaScript: Loading %s\n", workingPath);
	 int rc = luaL_dofile(luaState, workingPath);
	 if (rc != 0) {
		 const char* err = lua_tostring(luaState, -1);
		 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: ERROR - %s", err ? err : "load failed");
		 printf("LUA ERROR (load): %s\n", err ? err : "load failed");
		 if (err && err[0]) LuaConsolePushLine(err);
		 if (err) lua_pop(luaState, 1);
		 return 0;
	 }

	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loaded %s", workingPath);
	 // Check for script() function, fallback to gui() for backward compatibility
	 lua_getglobal(luaState, "script");
	 if (!lua_isfunction(luaState, -1)) {
		 lua_pop(luaState, 1);
	 lua_getglobal(luaState, "gui");
	 }
	 if (!lua_isfunction(luaState, -1)) { lua_pop(luaState, 1); }
	 else { lua_pop(luaState, 1); }
	 return 1;
 }
 
// Ensure MAX_PATH is defined (standard Windows value is 260)
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Ensure INVALID_FILE_ATTRIBUTES is defined (for Xbox compatibility)
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// Helper: Check if file exists (using GetFileAttributesA for Xbox compatibility)
static bool file_existsA(const char* path) {
	DWORD a = GetFileAttributesA(path);
	return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int lua_togglebit(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 2) {
		 return luaL_error(L, "togglebit(address, bit) requires 2 arguments");
	 }
	 
	 unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
	 int bit = (int)luaL_checkinteger(L, 2);
	 
	 // Validate address range (NES address space is 0x0000-0xFFFF)
	 if (address > 0xFFFF) {
		 return luaL_error(L, "togglebit: address must be in range 0x0000-0xFFFF");
	 }
	 
	 // Validate bit range (must be 0-7)
	 if (bit < 0 || bit > 7) {
		 return luaL_error(L, "togglebit: bit must be in range 0-7");
	 }
	 
	 // Read current byte value
	 uint8 currentValue = ARead[address](address);
	 
	 // Toggle the specified bit using XOR
	 uint8 newValue = currentValue ^ (1 << bit);
	 
	 // Write back using BWrite which handles all memory mapping correctly
	 BWrite[address](address, newValue);
	 
	 return 0;
}

int lua_testbit(lua_State *L) {
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

int lua_scanword(lua_State *L) {
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
        // Read low then high (little-endian)
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

int lua_scanbytes(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 3) {
        return luaL_error(L, "scanbytes requires either (table, startAddr, endAddr) or (b1, b2, ..., startAddr, endAddr)");
    }

    // Collect pattern bytes
    unsigned int startAddr = 0;
    unsigned int endAddr = 0;
    std::vector<uint8> pattern;
    pattern.reserve(64);

    if (lua_istable(L, 1)) {
        // Expect: table, start, end
        if (n < 3) {
            return luaL_error(L, "scanbytes(table, startAddr, endAddr) requires 3 arguments");
        }
        startAddr = (unsigned int)luaL_checkinteger(L, 2);
        endAddr   = (unsigned int)luaL_checkinteger(L, 3);

        // Read table entries (1-indexed), Lua 5.1-compatible (no luaL_len)
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
        // Expect: b1, b2, ..., start, end  (last two args are addresses)
        if (n < 3) {
            return luaL_error(L, "scanbytes(b1, b2, ..., startAddr, endAddr) requires at least 3 arguments");
        }
        startAddr = (unsigned int)luaL_checkinteger(L, n - 1);
        endAddr   = (unsigned int)luaL_checkinteger(L, n);
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

    // Validate addresses
    if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
        return luaL_error(L, "scanbytes: addresses must be in range 0x0000-0xFFFF");
    }
    if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }

    // If pattern longer than range, return empty table
    if (pattern.empty()) {
        return luaL_error(L, "scanbytes: pattern cannot be empty");
    }

    lua_createtable(L, 0, 0);
    int outIndex = 1;

    // Compute last start index inclusive to avoid overflow
    unsigned int maxStart;
    if (pattern.size() - 1 > (size_t)0xFFFF) {
        maxStart = 0; // impossible, but keep compiler happy
    }
    if (endAddr < (unsigned int)(pattern.size() - 1)) {
        maxStart = 0; // will be < startAddr and loop won't run
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

int lua_findpattern(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 3) {
        return luaL_error(L, "findpattern requires (pattern, startAddr, endAddr, [mask])");
    }

    // Check if first argument is a table
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "findpattern: first argument must be a table (pattern)");
    }

    // Collect pattern bytes
    unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 2);
    unsigned int endAddr   = (unsigned int)luaL_checkinteger(L, 3);
    std::vector<uint8> pattern;
    std::vector<uint8> mask;
    pattern.reserve(64);
    mask.reserve(64);

    // Read pattern table entries (1-indexed)
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

    // Read optional mask table (4th argument)
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
            // Mask values: 0 = wildcard (ignore), non-zero = match
            mask.push_back((v != 0) ? 1 : 0);
            ++maskCount;
        }
        if (maskCount != patternCount) {
            return luaL_error(L, "findpattern: mask table length must match pattern table length");
        }
    }

    // Validate addresses
    if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
        return luaL_error(L, "findpattern: addresses must be in range 0x0000-0xFFFF");
    }
    if (startAddr > endAddr) { unsigned int t = startAddr; startAddr = endAddr; endAddr = t; }

    // If pattern longer than range, return empty table
    if (pattern.empty()) {
        return luaL_error(L, "findpattern: pattern cannot be empty");
    }

    lua_createtable(L, 0, 0);
    int outIndex = 1;

    // Compute last start index inclusive to avoid overflow
    unsigned int maxStart;
    if (pattern.size() - 1 > (size_t)0xFFFF) {
        maxStart = 0; // impossible, but keep compiler happy
    }
    if (endAddr < (unsigned int)(pattern.size() - 1)) {
        maxStart = 0; // will be < startAddr and loop won't run
    } else {
        maxStart = endAddr - (unsigned int)(pattern.size() - 1);
    }

    for (unsigned int addr = startAddr; addr <= endAddr && addr <= maxStart; ++addr) {
        bool match = true;
        for (size_t i = 0; i < pattern.size(); ++i) {
            // If mask is provided and this position is wildcard (0), skip comparison
            if (hasMask && mask[i] == 0) {
                continue; // Wildcard - any byte matches
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

int lua_scanchanged(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 3) {
        return luaL_error(L, "scanchanged requires (oldSnapshot, newSnapshot, startAddr)");
    }

    // Validate that first two arguments are tables
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "scanchanged: oldSnapshot (1st argument) must be a table");
    }
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "scanchanged: newSnapshot (2nd argument) must be a table");
    }

    unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 3);

    // Validate address range
    if (startAddr > 0xFFFF) {
        return luaL_error(L, "scanchanged: startAddr must be in range 0x0000-0xFFFF");
    }

    // Read both snapshot tables (1-indexed)
    std::vector<uint8> oldSnapshot;
    std::vector<uint8> newSnapshot;
    oldSnapshot.reserve(256);
    newSnapshot.reserve(256);

    // Read old snapshot
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

    // Read new snapshot
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

    // Validate that both snapshots have the same length
    if (oldCount != newCount) {
        return luaL_error(L, "scanchanged: oldSnapshot and newSnapshot must have the same length");
    }

    if (oldCount <= 0) {
        return luaL_error(L, "scanchanged: snapshots must contain at least one byte");
    }

    // Create result table (address-indexed, not array-indexed)
    lua_createtable(L, 0, 0);  // Create empty table for address-indexed results

    // Compare snapshots and add changed addresses to result
    for (int i = 0; i < oldCount; ++i) {
        if (oldSnapshot[i] != newSnapshot[i]) {
            unsigned int addr = startAddr + (unsigned int)i;
            if (addr > 0xFFFF) break;  // Don't exceed address space
            
            // Add to result table: address as key, new value as value
            lua_pushinteger(L, addr);
            lua_pushinteger(L, newSnapshot[i]);
            lua_rawset(L, -3);  // Set table[addr] = newValue
        }
    }

    return 1;  // Return the result table
}

int lua_watchbyte(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 1) {
        return luaL_error(L, "watchbyte(address) requires 1 argument");
    }

    unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

    // Validate address range
    if (address > 0xFFFF) {
        return luaL_error(L, "watchbyte: address must be in range 0x0000-0xFFFF");
    }

    // Read current value and add to watch list
    uint8 currentValue = ARead[address](address);
    s_watchedAddresses[address] = currentValue;

    return 0;  // Return nothing
}

int lua_unwatchbyte(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 1) {
        return luaL_error(L, "unwatchbyte(address) requires 1 argument");
    }

    unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

    // Validate address range
    if (address > 0xFFFF) {
        return luaL_error(L, "unwatchbyte: address must be in range 0x0000-0xFFFF");
    }

    // Remove from watch list if it exists
    s_watchedAddresses.erase(address);

    return 0;  // Return nothing
}

int lua_getmemorysnapshot(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 2) {
        return luaL_error(L, "getmemorysnapshot requires (startAddr, endAddr)");
    }

    unsigned int startAddr = (unsigned int)luaL_checkinteger(L, 1);
    unsigned int endAddr   = (unsigned int)luaL_checkinteger(L, 2);

    // Validate address range
    if (startAddr > 0xFFFF || endAddr > 0xFFFF) {
        return luaL_error(L, "getmemorysnapshot: addresses must be in range 0x0000-0xFFFF");
    }

    // Swap if start > end
    if (startAddr > endAddr) {
        unsigned int t = startAddr;
        startAddr = endAddr;
        endAddr = t;
    }

    // Limit range to prevent excessive memory usage (max 65536 bytes)
    if (endAddr - startAddr > 0xFFFF) {
        return luaL_error(L, "getmemorysnapshot: range cannot exceed 65536 bytes");
    }

    // Create result table (address-indexed, not array-indexed)
    lua_createtable(L, 0, 0);

    // Read each byte and store with address as key
    for (unsigned int addr = startAddr; addr <= endAddr; ++addr) {
        uint8 value = ARead[addr](addr);
        lua_pushinteger(L, addr);
        lua_pushinteger(L, value);
        lua_rawset(L, -3);  // Set table[addr] = value
    }

    return 1;  // Return the snapshot table
}

// Helper: Check watched addresses for changes and call callback if needed
static void CheckWatchedAddresses() {
    if (!luaInitialized || luaState == NULL) {
        return;
    }

    // Check if onwatch callback function exists
    lua_getglobal(luaState, "onwatch");
    if (!lua_isfunction(luaState, -1)) {
        lua_pop(luaState, 1);
        return;  // No callback function, skip checking
    }

    // Check each watched address for changes
    for (std::map<unsigned int, uint8>::iterator it = s_watchedAddresses.begin(); 
         it != s_watchedAddresses.end(); ++it) {
        unsigned int addr = it->first;
        uint8 oldValue = it->second;
        uint8 newValue = ARead[addr](addr);

        if (oldValue != newValue) {
            // Value changed - update stored value
            it->second = newValue;

            // Call onwatch(address, oldValue, newValue)
            lua_pushvalue(luaState, -1);  // Copy onwatch function
            lua_pushinteger(luaState, addr);
            lua_pushinteger(luaState, oldValue);
            lua_pushinteger(luaState, newValue);
            if (lua_pcall(luaState, 3, 0, 0) != 0) {
                // Error in callback - log it
                const char* err = lua_tostring(luaState, -1);
                printf("LUA ERROR (onwatch callback): %s\n", err ? err : "unknown error");
                if (err && err[0]) LuaConsolePushLine(err);
                lua_pop(luaState, 1);
            }
        }
    }

    lua_pop(luaState, 1);  // Pop onwatch function
}

// Helper: Try to run a script in a specific directory
static int try_run_in(const char* dir, const char* file) {
	// CRITICAL: Check disabled state FIRST - this is the master switch
	if (s_luaDisabled) return 0;
	
	char full[MAX_PATH];
	full[0] = 0;
	_snprintf(full, MAX_PATH-1, "%s\\%s", dir, file);
	full[MAX_PATH-1] = 0;
		if (file_existsA(full)) {
			// File exists at this path - use the full path directly since we know it exists
			// FCEU_LoadLuaScript will try direct paths first, then search if needed
			if (!s_luaDisabled) {
			// Try full path first (direct path mode in FCEU_LoadLuaScript)
			int result = FCEU_LoadLuaScript(full);
			if (result) {
				printf("try_run_in: Successfully loaded %s from %s\n", file, dir);
				return result;
			}
			// Fallback: try just the filename (let FCEU_LoadLuaScript search)
			const char* filename = strrchr(file, '\\');
			if (!filename) filename = strrchr(file, '/');
			if (filename) filename++; // skip the slash
			else filename = file; // no path separator, use as-is
			
			result = FCEU_LoadLuaScript(filename);
			if (result) {
				printf("try_run_in: Successfully loaded %s (searched) from %s\n", filename, dir);
			} else {
				printf("try_run_in: FCEU_LoadLuaScript failed for %s (file exists at %s)\n", filename, full);
			}
			return result;
		}
	}
	return 0;
}

// Forward declarations for helper functions
static bool LoadOneScriptByName(const char* nameUtf8);
static void LoadAllFromFolder(const char* folder);

// Wrapper functions that respect the master disabled gate
static void Lua_LoadAllInKnownFolders(void) {
	if (s_luaDisabled) { 
		printf("Lua: blocked load-all (disabled)\n"); 
		return; 
	}
	
	const char* dirs[] = {
		"game:\\lua",
		"game:\\media\\lua",
		"hdd1:\\fce360-enhanced\\lua"
	};
	
	for (int i = 0; i < (int)(sizeof(dirs)/sizeof(dirs[0])); ++i) {
		if (s_luaDisabled) break; // Check again in case it was disabled during loop
		LoadAllFromFolder(dirs[i]);
	}
}

static void Lua_LoadSingleScript(const char* utf8Path) {
	if (s_luaDisabled || !utf8Path || !utf8Path[0]) {
		printf("Lua: blocked load-one (disabled or empty)\n"); 
		return;
	}
	LoadOneScriptByName(utf8Path);
}

static void Lua_OnNewRomBoot(void) {
	// This is the critical autoload hook that was ignoring the gate
	if (s_luaDisabled) { 
		printf("Lua: new ROM -> disabled; no autoload\n"); 
		return; 
	}

	if (s_luaMode == LUA_AUTO_ALL) {
		Lua_LoadAllInKnownFolders();
	} else if (s_luaMode == LUA_AUTO_ONE && s_luaOneScript[0]) {
		Lua_LoadSingleScript(s_luaOneScript);
	} else {
		printf("Lua: new ROM -> mode=NONE or no script; nothing to load\n");
	}
}

// Helper: Run all .lua files in a directory
static void run_all_in(const char* dir) {
	// CRITICAL: Check disabled state FIRST - this is the master switch
	if (s_luaDisabled) { 
		printf("run_all_in blocked (disabled), dir=%s\n", dir); 
		return; 
	}
	
	char pat[MAX_PATH];
	pat[0] = 0;
	_snprintf(pat, MAX_PATH-1, "%s\\*.lua", dir);
	pat[MAX_PATH-1] = 0;

	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(pat, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		// FCEU_LoadLuaScript searches paths, so pass just the filename
		// FCEU_LoadLuaScript will also check disabled state
		if (!s_luaDisabled) {
			FCEU_LoadLuaScript(fd.cFileName);
		}
		}
	} while (FindNextFileA(h, &fd));
	
	FindClose(h);
}

// Load one script by name - uses FCEU_LoadLuaScript which respects disabled flag
static bool LoadOneScriptByName(const char* nameUtf8) {
	if (s_luaDisabled || !nameUtf8 || !*nameUtf8) {
		printf("LoadOneScriptByName: blocked (disabled or empty)\n");
		return false;
	}
	EnsureLuaInit();
	return FCEU_LoadLuaScript(nameUtf8) != 0;
}

// Load all .lua files from a folder
static void LoadAllFromFolder(const char* folder) {
	if (s_luaDisabled || !folder || !*folder) {
		printf("LoadAllFromFolder skipped (disabled or empty)\n");
		return;
	}
	EnsureLuaInit();
	WIN32_FIND_DATAA fd;
	memset(&fd, 0, sizeof(fd));
	char pattern[256];
	_snprintf(pattern, sizeof(pattern)-1, "%s\\*.lua", folder);
	pattern[sizeof(pattern)-1] = 0;
	HANDLE h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Use the safe loader that respects the disabled flag and search paths
			char path[512];
			_snprintf(path, sizeof(path)-1, "%s\\%s", folder, fd.cFileName);
			path[sizeof(path)-1] = 0;
			FCEU_LoadLuaScript(path);
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

// Helper: Check if selection means "none" (NULL, empty, whitespace, or keywords)
static bool is_none_selection(const char* s) {
	if (!s) return true;  // NULL => NONE (not ALL)
	
	// Skip leading whitespace
	const char* p = s;
	while (*p && isspace((unsigned char)*p)) ++p;
	if (!*p) return true;  // "" or whitespace => NONE
	
	// Case-insensitive checks for common labels
#ifdef _MSC_VER
	return *s == 0 || _stricmp(p, "none") == 0 || _stricmp(p, "(none)") == 0
		|| _stricmp(p, "off") == 0 || _stricmp(p, "disabled") == 0;
#else
	return *s == 0 || strcasecmp(p, "none") == 0 || strcasecmp(p, "(none)") == 0
		|| strcasecmp(p, "off") == 0 || strcasecmp(p, "disabled") == 0;
#endif
}

// Helper: Check if selection explicitly means "all"
static bool is_all_selection(const char* s) {
	if (!s) return false;
#ifdef _MSC_VER
	return (_stricmp(s, "all") == 0 || _stricmp(s, "*") == 0);
#else
	return (strcasecmp(s, "all") == 0 || strcmp(s, "*") == 0);
#endif
}

// Centralized application of the menu setting.
// Call this *after* a ROM is loaded and anytime the user changes the selection.
extern "C" void FCEU_ApplyLuaMode(int mode, const char* scriptUtf8OrNull)
{
	// Always start clean
	FCEU_LuaKillAll();

	s_luaMode = mode;

	if (mode == LUA_AUTO_NONE) {
		FCEU_LuaSetDisabled(1);
		printf("Lua: Apply -> NONE (master OFF)\n");
		return;
	}

	FCEU_LuaSetDisabled(0);

	if (mode == LUA_AUTO_ALL) {
		s_luaOneScript[0] = '\0';
		printf("Lua: Apply -> ALL\n");
		EnsureLuaInit(); // ensure Lua is initialized before loading
		Lua_LoadAllInKnownFolders();
		return;
	}

	if (mode == LUA_AUTO_ONE) {
		if (scriptUtf8OrNull && scriptUtf8OrNull[0]) {
			strncpy(s_luaOneScript, scriptUtf8OrNull, sizeof(s_luaOneScript)-1);
			s_luaOneScript[sizeof(s_luaOneScript)-1] = '\0';
			printf("Lua: Apply -> ONE (%s)\n", s_luaOneScript);
			EnsureLuaInit(); // ensure Lua is initialized before loading
			Lua_LoadSingleScript(s_luaOneScript);
			char msg[256];
			_snprintf(msg, sizeof(msg)-1, "Lua: ONE %s", s_luaOneScript);
			msg[sizeof(msg)-1] = '\0';
			strncpy(g_luaStatusMsg, msg, sizeof(g_luaStatusMsg)-1);
			g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		} else {
			// No script provided → safest is OFF
			printf("Lua: Apply -> ONE but no script; forcing NONE\n");
			s_luaMode = LUA_AUTO_NONE;
			FCEU_LuaSetDisabled(1);
			strncpy(g_luaStatusMsg, "Lua: NONE", sizeof(g_luaStatusMsg)-1);
			g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
		}
		return;
	}

	// Unknown → OFF
	printf("Lua: Apply -> unknown mode %d; forcing NONE\n", mode);
	s_luaMode = LUA_AUTO_NONE;
	FCEU_LuaSetDisabled(1);
	strncpy(g_luaStatusMsg, "Lua: NONE", sizeof(g_luaStatusMsg)-1);
	g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
}

// Call this immediately AFTER the ROM is loaded and the core is powered.
extern "C" void FCEU_ApplyPendingLuaForNewGame(void) {
	// Consume pending at the one place core calls "ROM is ready"
	if (s_hasPending) {
		FCEU_ApplyLuaMode(s_pendingMode,
		                  (s_pendingMode == LUA_AUTO_ONE) ? s_pendingScript : NULL);
		s_hasPending = 0;
		s_pendingScript[0] = '\0';
		// Clear legacy globals too
		g_pendingLuaMode = -1;
		g_pendingLuaScript[0] = '\0';
	} else {
		// No UI change pending → apply current mode (still respects NONE)
		Lua_OnNewRomBoot();
	}
}


// Load Lua scripts based on user selection
// NULL/empty/"none" = load nothing (NONE)
// "all"/"*" = load all scripts from known directories
// Otherwise = load ONE specific script by name
void FCEU_AutoLoadLuaScripts(const char* selectedScript) {
	// Master kill-switch first
	if (s_luaDisabled) { 
		printf("FCEU_AutoLoadLuaScripts: disabled -> noop\n"); 
		return; 
	}
	
	// Treat NULL, "", "none", "(none)", "off", "disabled" as NO-OP
	if (is_none_selection(selectedScript)) {
		printf("FCEU_AutoLoadLuaScripts: NONE selection -> no scripts loaded\n");
		return;
	}
	
	// If the UI explicitly asked for ALL, then load from known dirs
	if (is_all_selection(selectedScript)) {
		const char* D1 = "hdd1:\\fce360-enhanced\\lua";
		const char* D2 = "game:\\lua";
		const char* D3 = "game:\\Lua";
		printf("FCEU_AutoLoadLuaScripts: ALL\n");
		run_all_in(D1);
		run_all_in(D2);
		run_all_in(D3);
		return;
	}
	
	// Otherwise: load ONE specific script by name
	if (selectedScript && selectedScript[0] != '\0') {
		if (!FCEU_LoadLuaScript(selectedScript)) {
			printf("FCEU_AutoLoadLuaScripts: failed to load '%s'\n", selectedScript);
		}
		return;
	}
	
	// Defensive default: if we got here, do nothing
	printf("FCEU_AutoLoadLuaScripts: no valid selection -> noop\n");
}
 
 // Get list of available Lua scripts
 int FCEU_GetLuaScriptList(char names[][256], int maxNames) {
	 const char* searchDirs[] = {
		 "hdd1:\\fce360-enhanced\\lua",
		 "hdd1:\\fce360-enhanced\\Lua",
		 "game:\\lua",
		 "game:/lua",
		 "game:\\Lua",
		 "game:/Lua",
		 "hdd1:\\lua",
		 "hdd1:\\Lua",
		 "usb0:\\lua",
		 "usb0:\\Lua"
	 };
	 
	 const int numDirs = sizeof(searchDirs) / sizeof(searchDirs[0]);
	 int count = 0;
	 
	 // Use a simple set to avoid duplicates
	 for (int dirIdx = 0; dirIdx < numDirs && count < maxNames; dirIdx++) {
		 char searchPattern[512];
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.lua", searchDirs[dirIdx]);
		 
		 WIN32_FIND_DATAA ffd;
		 HANDLE h = FindFirstFileA(searchPattern, &ffd);
		 
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 // Check if we already have this script
					 bool duplicate = false;
					 for (int i = 0; i < count; i++) {
						 if (strcmp(names[i], ffd.cFileName) == 0) {
							 duplicate = true;
							 break;
						 }
					 }
					 
					 if (!duplicate && count < maxNames) {
						 strncpy(names[count], ffd.cFileName, 255);
						 names[count][255] = '\0';
						 count++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd) && count < maxNames);
			 FindClose(h);
		 }
		 
		 // Also try uppercase pattern
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.LUA", searchDirs[dirIdx]);
		 h = FindFirstFileA(searchPattern, &ffd);
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 bool duplicate = false;
					 for (int i = 0; i < count; i++) {
						 if (strcmp(names[i], ffd.cFileName) == 0) {
							 duplicate = true;
							 break;
						 }
					 }
					 
					 if (!duplicate && count < maxNames) {
						 strncpy(names[count], ffd.cFileName, 255);
						 names[count][255] = '\0';
						 count++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd) && count < maxNames);
			 FindClose(h);
		 }
	 }
	 
	return count;
}

// Stub implementations for missing Lua functions (not yet implemented in Xbox port)
#ifdef USE_LUA
// Lua save/load data structure implementation
void LuaSaveData::ExportRecords(void* f) { 
	(void)f; 
	// Not implemented yet
}

void LuaSaveData::ImportRecords(void* f) { 
	(void)f; 
	// Not implemented yet
}

// Stub functions for save/load state integration
void CallRegisteredLuaSaveFunctions(void* state, LuaSaveData& saveData) {
	(void)state;
	(void)saveData;
	// Not implemented yet
}

void CallRegisteredLuaLoadFunctions(void* state, LuaSaveData& saveData) {
	(void)state;
	(void)saveData;
	// Not implemented yet
}

// Stub function for palette updates
void FCEU_LuaUpdatePalette(void) {
	// Not implemented yet - palette is handled in video path
}

// Stub function for reloading Lua code
void FCEU_ReloadLuaCode(void) {
	// Not implemented yet - would require full script reload
}
#endif

 // Frame boundary callback
 void FCEU_LuaFrameBoundary() {
	 if (s_luaDisabled) {
		 // Paranoid: make sure nothing is running and no overlay remains
		 FCEU_LuaStop();
		 return;
	 }
	 
	 if (!luaInitialized || luaState == NULL) {
		 return;
	 }
	 
	 // Update FPS
	 DWORD currentTime = GetTickCount();
	 frameCount++;
	 s_totalFrameCount++;  // Increment total frame counter
	 
	 if (lastFPSUpdate == 0) {
		 lastFPSUpdate = currentTime;
	 }
	 
	 if (currentTime - lastFPSUpdate >= 1000) {
		 currentFPS = (double)frameCount * 1000.0 / (double)(currentTime - lastFPSUpdate);
		 frameCount = 0;
		 lastFPSUpdate = currentTime;
	 }
	 
	 lastFrameTime = currentTime;
	 
	 // Check watched addresses for changes
	 if (!s_watchedAddresses.empty()) {
		 CheckWatchedAddresses();
	 }
	 
	 // Call "beforeframe" function if it exists - this runs BEFORE input polling
	 // This allows scripts to set joypad state before FCEU_UpdateInput() is called
	 // Check if script is sleeping - if so, skip callback execution
	 if (!Lua_IsSleeping(luaState)) {
		 lua_getglobal(luaState, "beforeframe");
		 if (lua_isfunction(luaState, -1)) {
			 if (lua_pcall(luaState, 0, 0, 0) != 0) {
				 // Error occurred - pop error message
				 lua_pop(luaState, 1);
			 }
		 } else {
			 lua_pop(luaState, 1);
		 }
	 }
	 
	 // Note: powerpadbuf override is now handled in Cemulator::UpdateInput()
	 // right after hardware input is read, ensuring it happens before UpdateGP() reads it
 }
 
 // GUI drawing callback - called from video.cpp
 // Update Lua at 30Hz, but composite the last overlay every frame to prevent flicker
 // Double-buffered: only publish new overlay if Lua succeeds (fail-safe)
 void FCEU_LuaGui(uint8 *XBuf) {
	 // Store the frame buffer for screenshot() function to use
	 // This MUST be set before any early returns, so screenshot() always has valid data
	 s_frameXBuf = XBuf;
	 
	 // Pre-initialize screenshot directory path on first call to avoid lag
	 if (!s_screenshotDirInitialized) {
		 extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
		 std::string tempPath = FCEU_MakeFName(2, 0, "png");  // 2 = FCEUMKF_SNAP
		 size_t lastSlash = tempPath.find_last_of("\\/");
		 if (lastSlash != std::string::npos) {
			 std::string dirPath = tempPath.substr(0, lastSlash);
			 // Normalize path separators
			 for (size_t i = 0; i < dirPath.length(); i++) {
				 if (dirPath[i] == '/') {
					 dirPath[i] = '\\';
				 }
			 }
			 // Ensure directory exists (fast if already exists)
			 CreateDirectoryA(dirPath.c_str(), NULL);
		 }
		 s_screenshotDirInitialized = true;
	 }
	 
	 // Safety check: ensure overlay is initialized before proceeding
	 EnsureOverlay();
	 if (!s_overlay_back || !s_overlay_front) {
		 // Overlay buffers not initialized - skip this frame
		 // But keep s_frameXBuf set so screenshots can still work
		 return;
	 }
	 
	 // If console is visible, we need to draw it even when Lua is disabled
	 // Otherwise, if Lua is disabled, clear overlays and return early
	 if (s_luaDisabled && !s_consoleVisible) {
		 ClearOverlaysIfAny();
		 // Keep s_frameXBuf set even when Lua is disabled, so screenshots can work
		 return; // no composite, no "LUA OFF" banner, truly silent
	 }
	 
     static DWORD lastGuiTime = 0;
     static bool prevConsoleVisible = false;
     DWORD now = GetTickCount();
     DWORD step = s_scriptIntervalMs; // script()-cadence (default 33ms)
	 
	 // Check if console visibility changed - if so, force an update immediately
	 bool consoleVisibilityChanged = (s_consoleVisible != prevConsoleVisible);
	 
	 // Update overlay contents at ~30Hz (only when Lua needs to run)
	 // Also update immediately if console visibility changed
	 if (now - lastGuiTime >= step || consoleVisibilityChanged) {
		 lastGuiTime = now;
		 
		 // Always start fresh to avoid "ghost" rectangles from prior frames
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, OVL_W * OVL_H);
		 }
		 
		 // Optional status message for a few seconds - clears automatically after timeout
		 // Status banner (top-left)
		 static int statusTicks = 0;
		 static bool statusShown = false;
		 static char lastMsg[128] = {0};
		 
		 // If message text changed since last frame, restart TTL
		 if (strncmp(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1) != 0) {
			 strncpy(lastMsg, g_luaStatusMsg, sizeof(lastMsg) - 1);
			 lastMsg[sizeof(lastMsg) - 1] = '\0';
			 statusTicks = 0;
			 statusShown = false;
		 }
		 
         // Suppress status banner while console is visible to avoid duplicate text
         if (s_overlay_back && !s_consoleVisible) {
			 // Status message is drawn at y=20 (below "LUA ON" at y=4), x=4
			 const int statusY = 20;
			 
			 if (statusTicks < 180) {  // ~6s @ 30Hz
				 // Draw status message below "LUA ON"
				 if (statusY >= 0 && statusY < OVL_H && g_luaStatusMsg[0] != '\0') {
					 DrawTextTrans(s_overlay_back + statusY*OVL_W + 4, OVL_W, (uint8*)g_luaStatusMsg, 0x2E | 0x80);
				 }
				 statusTicks++;
				 statusShown = true;
			 } else {
				 statusShown = false;
				 // Next frame will start blank, so status won't appear
			 }
		 }
		 
		 bool ok = false;  // Track if Lua succeeded
		 g_overlayDirty = false;  // Reset before calling Lua
		 
		 // Only run Lua scripts if Lua is enabled and initialized
		 if (!s_luaDisabled && luaInitialized && luaState != NULL) {
			 // Check if script is sleeping - if so, skip callback execution
			 if (!Lua_IsSleeping(luaState)) {
				 // Point Lua draw calls at the back buffer, not the front buffer
				 currentXBuf = s_overlay_back;
				 
			 // Call script() function if it exists (also support legacy gui() for backward compatibility)
			 lua_getglobal(luaState, "script");
			 if (!lua_isfunction(luaState, -1)) {
				 lua_pop(luaState, 1);
				 // Try legacy gui() function for backward compatibility
				 lua_getglobal(luaState, "gui");
			 }
				 if (lua_isfunction(luaState, -1)) {
				 if (lua_pcall(luaState, 0, 0, 0) == 0) {
					 ok = true;  // Script executed successfully
				 } else {
					 // Lua error - draw a visible error marker
					 const char* err = lua_tostring(luaState, -1);
					 printf("LUA ERROR (runtime): %s\n", err ? err : "unknown error");
					 if (err && err[0]) LuaConsolePushLine(err);
					 // Draw error indicator on screen so failures are visible
					 if (s_overlay_back) {
						 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"LUA ERR", 0x0F | 0x80);
						 g_overlayDirty = true;  // Error marker counts as drawing
					 }
					 lua_pop(luaState, 1);
					 // Leave g_overlayDirty as-is (true if error marker drawn)
				 }
			 } else {
			 // script() function doesn't exist
				 lua_pop(luaState, 1);
				 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"NO script()", 0x0F | 0x80);
					 g_overlayDirty = true;  // Error indicator counts as drawing
				 }
			 }
			 
			 currentXBuf = NULL;
			 } // End of sleep check block
		 } else {
			 // Lua not initialized
			 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*OVL_W + 10, OVL_W, (uint8*)"LUA OFF", 0x0F | 0x80);
				 g_overlayDirty = true;  // Status indicator counts as drawing
			 }
		 }
		 
         // If console visible, draw it now onto back buffer and mark dirty
		 if (s_consoleVisible && s_overlay_back) {
			 // Handle D-pad scrolling (vertical)
			 bool dpadUp = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
			 bool dpadDown = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
			 
			 // Scroll on button press (immediate) or hold (throttled)
			 if (dpadUp) {
				 if (!s_consoleDpadUpLast) {
					 // First press - scroll immediately
					 s_consoleScrollOffset++;
					 s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFrames++;
					 if (s_consoleScrollHoldFrames >= 3) {
						 s_consoleScrollOffset++;
						 s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 if (dpadDown) {
				 if (!s_consoleDpadDownLast) {
					 // First press - scroll immediately
				 if (s_consoleScrollOffset > 0) {
					 s_consoleScrollOffset--;
				 }
					 s_consoleScrollHoldFrames = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFrames++;
					 if (s_consoleScrollHoldFrames >= 3) {
						 if (s_consoleScrollOffset > 0) {
							 s_consoleScrollOffset--;
						 }
						 s_consoleScrollHoldFrames = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadUp && !dpadDown) {
				 s_consoleScrollHoldFrames = 0;
			 }
			 
			 s_consoleDpadUpLast = dpadUp;
			 s_consoleDpadDownLast = dpadDown;
			 
			 // Handle D-pad scrolling (horizontal)
			 bool dpadLeft = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
			 bool dpadRight = (Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
			 
			 // Scroll horizontally on button press (immediate) or hold (throttled)
			 if (dpadLeft) {
				 if (!s_consoleDpadLeftLast) {
					 // First press - scroll immediately (scroll left = decrease offset)
					 if (s_consoleScrollOffsetH > 0) {
						 s_consoleScrollOffsetH -= 8; // Scroll by ~1 character width
						 if (s_consoleScrollOffsetH < 0) s_consoleScrollOffsetH = 0;
					 }
					 s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFramesH++;
					 if (s_consoleScrollHoldFramesH >= 3) {
						 if (s_consoleScrollOffsetH > 0) {
							 s_consoleScrollOffsetH -= 8;
							 if (s_consoleScrollOffsetH < 0) s_consoleScrollOffsetH = 0;
						 }
						 s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 if (dpadRight) {
				 if (!s_consoleDpadRightLast) {
					 // First press - scroll immediately (scroll right = increase offset)
					 s_consoleScrollOffsetH += 8; // Scroll by ~1 character width
					 s_consoleScrollHoldFramesH = 0;
				 } else {
					 // Holding - scroll every 3 frames for smooth but controlled speed
					 s_consoleScrollHoldFramesH++;
					 if (s_consoleScrollHoldFramesH >= 3) {
						 s_consoleScrollOffsetH += 8;
						 s_consoleScrollHoldFramesH = 0;
					 }
				 }
			 }
			 
			 // Reset hold counter when button is released
			 if (!dpadLeft && !dpadRight) {
				 s_consoleScrollHoldFramesH = 0;
			 }
			 
			 s_consoleDpadLeftLast = dpadLeft;
			 s_consoleDpadRightLast = dpadRight;
			 
			 const int cx = 4, cy = 40;
			 int maxX = 4 + 248 - 1; if (maxX > OVL_W - 1) maxX = OVL_W - 1;
			 int maxY = 40 + 180 - 1; if (maxY > OVL_H - 1) maxY = OVL_H - 1;
			 const uint8 bg = 0x80 | 0x10;
			 const uint8 bd = 0x80 | 0x2E;
			 for (int py = cy; py <= maxY; ++py) {
				 for (int px = cx; px <= maxX; ++px) s_overlay_back[py*OVL_W + px] = bg;
			 }
			 for (int px = cx; px <= maxX; ++px) { s_overlay_back[cy*OVL_W + px] = bd; s_overlay_back[maxY*OVL_W + px] = bd; }
			 DrawTextTrans(s_overlay_back + cy*OVL_W + (cx + 6), OVL_W, (uint8*)"Lua Console (LS+RS)", 0x2E | 0x80);
			 
			 // Clamp scroll offset (using new line advance) - DrawLuaConsole will handle the actual drawing
			 const int lineStartY = cy + 12;
			 const int adv = CON_LINE_ADV();
			 const int availableHeight = maxY - lineStartY + 1;
			 int maxLines = availableHeight / adv;
			 if (maxLines < 1) maxLines = 1;
			 // Allow scrolling to see all lines, including the oldest (line 0)
			 // maxScroll should allow viewing from line 0 to line (count - maxLines)
			 int maxScroll = (s_luaConsoleCount > maxLines) ? (s_luaConsoleCount - maxLines) : 0;
			 if (s_consoleScrollOffset > maxScroll) s_consoleScrollOffset = maxScroll;
			 if (s_consoleScrollOffset < 0) s_consoleScrollOffset = 0;
			 
			 g_overlayDirty = true;
			 ok = true; // force publish when console drew
         } else if (!s_consoleVisible && prevConsoleVisible && s_overlay_back) {
             // Console just turned off: push a cleared overlay once to remove it immediately
             memset(s_overlay_back, 0, 256 * 240);
             g_overlayDirty = true;
             ok = true;
		 }
		 
		 // Draw console using the new drawer with proper line spacing
		 DrawLuaConsole(s_overlay_back);
		 
		 // Only publish the new overlay if something was drawn
         if (g_overlayDirty) {
			 // Only publish if the back buffer actually differs from the front
			 // This prevents unnecessary swaps when content hasn't changed
			 if (!s_overlay_front || overlay_has_changes(s_overlay_back, s_overlay_front)) {
				 SwapOverlays();
			 }
		 }
         prevConsoleVisible = (s_consoleVisible != 0);
	 }
	 
	 // ALWAYS composite the last published overlay every frame (prevents flicker when Lua runs at 30Hz)
	 // This ensures smooth 60Hz display even though Lua only updates at 30Hz
	 if (XBuf && s_overlay_front) {
		 CompositeOverlay(XBuf);
	 }
 }
 
 // Stop Lua
 void FCEU_LuaStop() {
	 if (luaState != NULL) {
		 lua_close(luaState);
		 luaState = NULL;
	 }
	 luaInitialized = false;
	 ClearOverlaysIfAny();
	 s_watchedAddresses.clear();  // Clear all watchpoints
	 printf("FCEU_LuaStop: Lua state closed and overlays cleared\n");
 }
 
 // Call registered Lua functions
 void CallRegisteredLuaFunctions(LUACALL callID) {
	 // Not implemented yet - can be extended for more callbacks
	 (void)callID;
 }
 
 // Memory hook callback
 void CallRegisteredLuaMemHook(unsigned int address, int size, uint8 value, LUAMEMHOOK hookType) {
	 // Not implemented yet - can be extended for memory hooks
	 (void)address;
	 (void)size;
	 (void)value;
	 (void)hookType;
 }
 
 // Joypad read callback
 uint32 FCEU_LuaReadJoypad(int n, uint32 ret) {
	 if (!luaInitialized || luaState == NULL) {
		 return ret;
	 }
	 
	 // Call joypad() function if it exists
	 lua_getglobal(luaState, "joypad");
	 if (lua_isfunction(luaState, -1)) {
		 lua_pushinteger(luaState, n);
		 lua_pushinteger(luaState, ret);
		 if (lua_pcall(luaState, 2, 1, 0) == 0) {
			 if (lua_isnumber(luaState, -1)) {
				 ret = (uint32)lua_tointeger(luaState, -1);
			 }
			 lua_pop(luaState, 1);
		 } else {
			 lua_pop(luaState, 1);
		 }
	 } else {
		 lua_pop(luaState, 1);
	 }
	 
	 return ret;
 }

// Apply Lua joypad overrides after input polling
// Call this each frame at the END of FCEU_UpdateInput() to ensure Lua's override wins over hardware
// This merges Lua overrides into the final pad state the NES core reads
extern "C" void FCEU_LuaJoypadApply(void)
{
	// Store hardware input before override (for gethardwarejoypad() function)
	// powerpadbuf format: pad[0] in low byte, pad[1] in next byte
	s_hardwareJoypad[0] = (uint8)(powerpadbuf & 0xFF);
	s_hardwareJoypad[1] = (uint8)((powerpadbuf >> 8) & 0xFF);
	s_hardwareJoypad[2] = (uint8)((powerpadbuf >> 16) & 0xFF);
	s_hardwareJoypad[3] = (uint8)((powerpadbuf >> 24) & 0xFF);
	
	// Override powerpadbuf (Xbox input buffer) for players 0 and 1
	// This is the source that UpdateGP() reads from, so we need to override it here
	// Players: 0=pad[0], 1=pad[1], 2=pad[0]>>16, 3=pad[1]>>24 (but Xbox only uses first 2)
	
	uint32 newPowerpadbuf = powerpadbuf;  // Start with current hardware input
	
	// Apply input playback FIRST if active (playback overrides everything)
	// This must happen before other overrides so playback takes precedence
	if (s_inputPlayback) {
		// Apply playback for player 0 (low byte of powerpadbuf)
		if (s_playbackFrame < (int)s_playbackInput[0].size()) {
			uint8 pad0 = s_playbackInput[0][s_playbackFrame];
			newPowerpadbuf = (newPowerpadbuf & 0xFFFFFF00) | pad0;
		}
		
		// Apply playback for player 1 (second byte of powerpadbuf)
		if (s_playbackFrame < (int)s_playbackInput[1].size()) {
			uint8 pad1 = s_playbackInput[1][s_playbackFrame];
			newPowerpadbuf = (newPowerpadbuf & 0xFFFF00FF) | ((uint32)pad1 << 8);
		}
	} else {
		// Only apply other overrides if NOT playing back
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
	}
	
	// Apply the override to powerpadbuf
	powerpadbuf = newPowerpadbuf;
	
	// Also override joy[] array for consistency (this is what the NES core reads)
	for (int p = 0; p < 4; ++p) {
		uint8 finalButtons = joy[p];
		
		// Apply persistent override if latched
		if (s_luaJoypadLatched[p]) {
			// Store old value for debugging
			uint8 oldJoy = joy[p];
			// Merge Lua override into joy[p] - this is the buffer the NES core ultimately reads
			// Mask determines which bits to override (0xFF = all bits)
			// Formula: clear masked bits from hardware, then OR in Lua's values
			// When mask is 0xFF, this completely replaces hardware input with Lua's value
			finalButtons = (uint8)((joy[p] & (uint8)~s_luaJoypadMask[p]) |
								   (s_luaJoypadValue[p] & s_luaJoypadMask[p]));
			// Debug output for player 0 (uncomment to verify override is being applied)
			if (p == 0) {
				static int debugCounter = 0;
				if (++debugCounter % 60 == 0) {  // Print every 60 frames
					printf("FCEU_LuaJoypadApply: P%d: joy[%d]=0x%02X, powerpadbuf=0x%08X (value=0x%02X, mask=0x%02X, latched=%d)\n",
						   p, p, joy[p], powerpadbuf, s_luaJoypadValue[p], s_luaJoypadMask[p], s_luaJoypadLatched[p]);
				}
			}
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
		
		// Apply input playback if active (playback overrides everything)
		// This must happen before recording so we record the playback, not the original input
		if (s_inputPlayback && p < 4) {
			if (s_playbackFrame < (int)s_playbackInput[p].size()) {
				// Use recorded input for this frame
				finalButtons = s_playbackInput[p][s_playbackFrame];
			} else {
				// This player's playback finished, continue with normal input
			}
		}
		
		// Record input if recording is active (record the final button state)
		// Record after playback is applied so we can record either original input or playback
		if (s_inputRecording && p < 4) {
			s_recordedInput[p].push_back(finalButtons);
		}
		
		joy[p] = finalButtons;
	}
	
	// Advance playback frame counter
	if (s_inputPlayback) {
		s_playbackFrame++;
		// Check if all players have finished playback
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
		}
	}
}

// Clear Lua joypad overrides
// player: -1 = all players, 0-3 = specific player
extern "C" void FCEU_LuaJoypadClear(int player)
{
	if (player == -1) {
		for (int p = 0; p < 4; ++p) {
			s_luaJoypadMask[p] = 0;
			s_luaJoypadLatched[p] = 0;
		}
	} else if (player >= 0 && player < 4) {
		s_luaJoypadMask[player] = 0;
		s_luaJoypadLatched[player] = 0;
	}
	// leave s_luaJoypadValue as-is; it won't be applied while latched==0
 }
  
 #endif // USE_LUA
