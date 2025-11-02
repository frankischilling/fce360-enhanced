/* FCE Ultra Lua Integration for Xbox 360
 * Implementation
 * frankischilling - Ced2911 
 */

 #include "../stdafx.h"

 #ifdef USE_LUA
 
#include "fceulua.h"
#include "fceu.h"
#include "drawing.h"
#include "video.h"
#include "driver.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
 
// On-screen status message for debugging (shows last load attempt)
static char g_luaStatusMsg[128] = "Lua: disabled";
// stdafx.h already includes xtl.h which provides GetTickCount() and DWORD

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
		 s_overlay_front = (uint8*)malloc(256 * 240);
		 if (s_overlay_front) {
			 memset(s_overlay_front, 0, 256 * 240);
		 }
	 }
	 if (!s_overlay_back) {
		 s_overlay_back = (uint8*)malloc(256 * 240);
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, 256 * 240);
		 }
	 }
 }
 
 static inline void CompositeOverlay(uint8* XBuf) {
	 // Blit the front overlay onto the NES frame every frame (prevents flicker when Lua runs at 30Hz)
	 if (!s_overlay_front || !XBuf) return;
	 
	 const int N = 256 * 240;
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
	 if (s_overlay_back)  memset(s_overlay_back,  0, 256 * 240);
	 if (s_overlay_front) memset(s_overlay_front, 0, 256 * 240);
 }
 
 // Helper: Clear a rectangle in the overlay buffer with bounds checking
 static inline void clear_rect(uint8* buf, int x, int y, int w, int h) {
	 if (!buf) return;
	 
	 // Clamp to valid bounds
	 if (x < 0) { w += x; x = 0; }
	 if (y < 0) { h += y; y = 0; }
	 if (x + w > 256) w = 256 - x;
	 if (y + h > 240) h = 240 - y;
	 if (w <= 0 || h <= 0) return;
	 
	 // Clear each row of the rectangle
	 for (int dy = 0; dy < h; ++dy) {
		 memset(buf + (y + dy) * 256 + x, 0, w);
	 }
 }

 // Map 0x00-0x3F to overlay-coded 0x80-0xBF (never dim)
 static inline uint8 map_overlay_color(int c) {
	 return (uint8)((c & 0x3F) | 0x80);
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
	 const int pitch = 256, h = 240;
	 for (int y = 0; y < h; y += 16) {
		 if (memcmp(a + y*pitch, b + y*pitch, pitch) != 0) return true;
	 }
	 return memcmp(a, b, 256*240) != 0;
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
 static int frameCount = 0;
 static double currentFPS = 0.0;
 static DWORD lastFrameTime = 0;
 
 // Forward declaration
 static uint8* currentXBuf = NULL;

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

 // Lua drawing function - allows scripts to draw text
 int lua_drawtext(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 3) {
		 return luaL_error(L, "drawtext(x, y, text [, color]) requires at least 3 arguments");
	 }
	 
	 int x = (int)luaL_checkinteger(L, 1);
	 int y = (int)luaL_checkinteger(L, 2);
	 const char* text = luaL_checkstring(L, 3);
	 int color_in = (n >= 4) ? (int)luaL_optinteger(L, 4, 0x20) : 0x20;
	 
	 if (!currentXBuf || !text || !*text) return 0;
	 
	 // Text is 8 pixels tall - clamp y to prevent drawing past y=232
	 // Maximum safe y is 224 (224+8=232, well within bounds)
	 if (x < 0) x = 0;
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 if (y > 224) y = 224; // Clamp to safe position (auto-move up if too low)
	 
	 // Clear the whole 8-px line to nuke any stale box on that row
	 // (Ultra-defensive: ensures no ghost rectangles from previous frames)
	 clear_rect(currentXBuf, 0, y, 256, 8);
	 
	 uint8 mapped = map_overlay_color(color_in);
	 DrawTextNoBorder(currentXBuf + y * 256 + x, 256, text, mapped); // glyphs only, no bg
	 g_overlayDirty = true;
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
	 
	 if (!currentXBuf) return 0;
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 
	 // drawtextwh can draw multi-line text - ensure y + max_h doesn't exceed safe bounds
	 // Auto-adjust y position if it would draw past y=232
	 if (y + max_h > 232) {
		 y = 232 - max_h; // Move text up to fit
		 if (y < 0) y = 0; // Ensure y doesn't go negative
	 }
	 if (y > 224) y = 224; // Clamp y to safe position (text is 8px tall minimum)
	 
	 // Clamp max_h to ensure we don't draw past y=232
	 if (max_h <= 0) max_h = 8; // Minimum height
	 if (y + max_h > 232) max_h = 232 - y;
	 if (max_h <= 0) return 0; // No room to draw
	 
	 uint8 *dest = currentXBuf + y * 256 + x;
	 uint8 mapped = map_overlay_color(color);
	 
	 // Clamp border and choose the truly borderless path when border == 0
	 if (border < 0) border = 0;
	 if (border > 2) border = 2;
	 
	 if (border == 0) {
		 // Borderless: proactively clear the WH region we're going to use,
		 // so no stale boxed pixels from previous frames peek through.
		 int cw = max_w; if (cw < 0) cw = 0;
		 int ch = max_h; if (ch < 0) ch = 0;
		 if (cw > 0 && ch > 0) clear_rect(currentXBuf, x, y, cw, ch);
		 
		 DrawTextTransWH(dest, 256, (uint8*)text, mapped, max_w, max_h, 0); // glyphs only
	 } else {
		 // With border: use the WH renderer (it draws the box/outline on purpose)
		 DrawTextTransWH(dest, 256, (uint8*)text, mapped, max_w, max_h, border);
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
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x < 0) x = 0;
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	 
	 // Draw pixel on the current frame buffer (set by FCEU_LuaGui)
	 uint8 *dest = currentXBuf + y * 256 + x;
	 *dest = map_overlay_color(color);
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
	 
	 // Clamp coordinates to safe bounds (auto-adjust if too low/high)
	 if (x1 < 0) x1 = 0;
	 if (x1 >= 256) x1 = 255;
	 if (x2 < 0) x2 = 0;
	 if (x2 >= 256) x2 = 255;
	 if (y1 < 0) y1 = 0;
	 if (y1 >= 232) y1 = 231; // Clamp to safe position
	 if (y2 < 0) y2 = 0;
	 if (y2 >= 232) y2 = 231; // Clamp to safe position
	 
	 // Use Bresenham's line algorithm to draw the line
	 int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	 int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	 int sx = (x1 < x2) ? 1 : -1;
	 int sy = (y1 < y2) ? 1 : -1;
	 int err = dx - dy;
	 
	 int x = x1;
	 int y = y1;
	 bool drewSomething = false;
	 
	 while (true) {
		 // Check bounds and draw pixel - never draw past y=232 to avoid buffer overflows
		 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
			 uint8 *dest = currentXBuf + y * 256 + x;
			 *dest = map_overlay_color(color);
			 drewSomething = true;
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
	 if (x1 >= 256) x1 = 255;
	 if (x2 < 0) x2 = 0;
	 if (x2 >= 256) x2 = 255;
	 if (y1 < 0) y1 = 0;
	 if (y1 >= 232) y1 = 231; // Clamp to safe position
	 if (y2 < 0) y2 = 0;
	 if (y2 >= 232) y2 = 231; // Clamp to safe position
	 
	 // Calculate line direction
	 int dx = x2 - x1;
	 int dy = y2 - y1;
	 
	 // For very short lines or same point, just draw a filled circle
	 if ((dx == 0 && dy == 0) || (abs(dx) <= 1 && abs(dy) <= 1)) {
		 int radius = (thickness - 1) / 2;
		 // Draw a filled circle at the point
		 for (int cy = y1 - radius; cy <= y1 + radius; ++cy) {
			 if (cy < 0 || cy >= 232) continue;
			 for (int cx = x1 - radius; cx <= x1 + radius; ++cx) {
				 if (cx < 0 || cx >= 256) continue;
				 int distSq = (cx - x1) * (cx - x1) + (cy - y1) * (cy - y1);
				 if (distSq <= radius * radius) {
					 uint8 *dest = currentXBuf + cy * 256 + cx;
					 *dest = mappedColor;
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
			 
			 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
				 uint8 *dest = currentXBuf + py * 256 + px;
				 *dest = mappedColor;
				 drewSomething = true;
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
		 if (x1 >= 256) x1 = 255;
		 if (x2 < 0) x2 = 0;
		 if (x2 >= 256) x2 = 255;
		 if (y1 < 0) y1 = 0;
		 if (y1 >= 232) y1 = 231; // Clamp to safe position
		 if (y2 < 0) y2 = 0;
		 if (y2 >= 232) y2 = 231; // Clamp to safe position
		 
		 // Handle same-point case (degenerate segment)
		 if (x1 == x2 && y1 == y2) {
			 if (x1 >= 0 && x1 < 256 && y1 >= 0 && y1 < 232) {
				 uint8 *dest = currentXBuf + y1 * 256 + x1;
				 *dest = mappedColor;
				 drewSomething = true;
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
			 // Check bounds and draw pixel - never draw past y=232 to avoid buffer overflows
			 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
				 uint8 *dest = currentXBuf + y * 256 + x;
				 *dest = mappedColor;
				 drewSomething = true;
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
		 if (x1 >= 256) x1 = 255;
		 if (x2 < 0) x2 = 0;
		 if (x2 >= 256) x2 = 255;
		 if (y1 < 0) y1 = 0;
		 if (y1 >= 232) y1 = 231; // Clamp to safe position
		 if (y2 < 0) y2 = 0;
		 if (y2 >= 232) y2 = 231; // Clamp to safe position
		 
		 // Handle same-point case (degenerate segment)
		 if (x1 == x2 && y1 == y2) {
			 if (x1 >= 0 && x1 < 256 && y1 >= 0 && y1 < 232) {
				 uint8 *dest = currentXBuf + y1 * 256 + x1;
				 *dest = mappedColor;
				 drewSomething = true;
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
			 // Check bounds and draw pixel - never draw past y=232 to avoid buffer overflows
			 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
				 uint8 *dest = currentXBuf + y * 256 + x;
				 *dest = mappedColor;
				 drewSomething = true;
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
		 if (x >= 256) x = 255;
		 if (y < 0) y = 0;
		 if (y >= 232) y = 231; // Clamp to safe position
		 
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
	 if (maxY >= 232) maxY = 231;
	 if (minY > maxY || minY >= 232 || maxY < 0) {
		 delete[] xCoords;
		 delete[] yCoords;
		 return 0;
	 }
	 
	 bool drewSomething = false;
	 
	 // Scanline fill using even-odd rule
	 for (int y = minY; y <= maxY; ++y) {
		 if (y < 0 || y >= 232) continue;
		 
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
			 if (xEnd >= 256) xEnd = 255;
			 
			 for (int x = xStart; x <= xEnd; ++x) {
				 if (x >= 0 && x < 256) {
					 uint8 *dest = currentXBuf + y * 256 + x;
					 *dest = mappedColor;
					 drewSomething = true;
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
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	 
	 // Clamp rectangle to valid bounds and ensure it doesn't extend past y=232
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > 232) h = 232 - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 bool drewSomething = false;
	 
	 // Draw top and bottom horizontal lines
	 for (int dx = 0; dx < w; ++dx) {
		 int px = x + dx;
		 
		 // Top line
		 if (px >= 0 && px < 256 && y >= 0 && y < 232) {
			 uint8 *dest = currentXBuf + y * 256 + px;
			 *dest = mappedColor;
			 drewSomething = true;
		 }
		 
		 // Bottom line
		 if (px >= 0 && px < 256 && (y + h - 1) >= 0 && (y + h - 1) < 240) {
			 uint8 *dest = currentXBuf + (y + h - 1) * 256 + px;
			 *dest = mappedColor;
			 drewSomething = true;
		 }
	 }
	 
	 // Draw left and right vertical lines
	 for (int dy = 1; dy < h - 1; ++dy) {  // Start at 1, end at h-1 to avoid redrawing corners
		 int py = y + dy;
		 
		 // Left line
		 if (x >= 0 && x < 256 && py >= 0 && py < 232) {
			 uint8 *dest = currentXBuf + py * 256 + x;
			 *dest = mappedColor;
			 drewSomething = true;
		 }
		 
		 // Right line
		 if ((x + w - 1) >= 0 && (x + w - 1) < 256 && py >= 0 && py < 232) {
			 uint8 *dest = currentXBuf + py * 256 + (x + w - 1);
			 *dest = mappedColor;
			 drewSomething = true;
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
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	 
	 // Clamp rectangle to valid bounds and ensure it doesn't extend past y=232
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > 232) h = 232 - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 bool drewSomething = false;
	 
	 // Clamp rectangle to screen bounds
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + w > 256) ? 256 : (x + w);
	 int endY = (y + h > 240) ? 240 : (y + h);
	 
	 // Adjust start positions if rectangle is completely off-screen
	 if (startX >= 256 || startY >= 240 || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 // Fill the rectangle row by row
	 for (int py = startY; py < endY; ++py) {
		 for (int px = startX; px < endX; ++px) {
			 uint8 *dest = currentXBuf + py * 256 + px;
			 *dest = mappedColor;
			 drewSomething = true;
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
	 if (x >= 256) x = 255;
	 if (y < 0) y = 0;
	 if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	 
	 // Clamp rectangle to valid bounds and ensure it doesn't extend past y=232
	 if (w <= 0 || h <= 0) return 0;
	 if (y + h > 232) h = 232 - y; // Reduce height to fit
	 if (h <= 0) return 0;
	 
	 // Clamp rectangle to screen bounds
	 int startX = (x < 0) ? 0 : x;
	 int startY = (y < 0) ? 0 : y;
	 int endX = (x + w > 256) ? 256 : (x + w);
	 int endY = (y + h > 240) ? 240 : (y + h);
	 
	 // Adjust start positions if rectangle is completely off-screen
	 if (startX >= 256 || startY >= 240 || endX <= 0 || endY <= 0) {
		 return 0;
	 }
	 
	 bool clearedSomething = false;
	 
	 // Clear the rectangle row by row (set to 0 = transparent)
	 for (int py = startY; py < endY; ++py) {
		 for (int px = startX; px < endX; ++px) {
			 uint8 *dest = currentXBuf + py * 256 + px;
			 *dest = 0;  // 0 means transparent (won't overwrite NES frame)
			 clearedSomething = true;
		 }
	 }
	 
	 if (clearedSomething) {
		 g_overlayDirty = true;  // Mark that something was changed
	 }
	 
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
				uint8 *dest = currentXBuf + py * 256 + px;
				*dest = mappedColor;
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
	// Validate and reduce radius if circle would extend past safe bounds
	if (radius <= 0) return 0;
	if (cy + radius > 231) radius = 231 - cy;
	if (cy - radius < 0 && radius > cy) radius = cy;
	if (radius < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Clamp circle bounds to screen
	int minX = (cx - radius < 0) ? 0 : (cx - radius);
	int maxX = (cx + radius >= 256) ? 255 : (cx + radius);
	int minY = (cy - radius < 0) ? 0 : (cy - radius);
	int maxY = (cy + radius >= 232) ? 231 : (cy + radius);
	
	if (minX >= 256 || minY >= 232 || maxX < 0 || maxY < 0) return 0;
	
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
					uint8 *dest = currentXBuf + py * 256 + px;
					*dest = mappedColor;
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
	if (x1 >= 256) x1 = 255;
	if (x2 < 0) x2 = 0;
	if (x2 >= 256) x2 = 255;
	if (x3 < 0) x3 = 0;
	if (x3 >= 256) x3 = 255;
	if (y1 < 0) y1 = 0;
	if (y1 >= 232) y1 = 231; // Clamp to safe position
	if (y2 < 0) y2 = 0;
	if (y2 >= 232) y2 = 231; // Clamp to safe position
	if (y3 < 0) y3 = 0;
	if (y3 >= 232) y3 = 231; // Clamp to safe position
	
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
		 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
			uint8 *dest = currentXBuf + y * 256 + x;
			*dest = mappedColor;
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
		 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
			uint8 *dest = currentXBuf + y * 256 + x;
			*dest = mappedColor;
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
		 if (x >= 0 && x < 256 && y >= 0 && y < 232) {
			uint8 *dest = currentXBuf + y * 256 + x;
			*dest = mappedColor;
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
	if (x1 >= 256) x1 = 255;
	if (x2 < 0) x2 = 0;
	if (x2 >= 256) x2 = 255;
	if (x3 < 0) x3 = 0;
	if (x3 >= 256) x3 = 255;
	if (y1 < 0) y1 = 0;
	if (y1 >= 232) y1 = 231; // Clamp to safe position
	if (y2 < 0) y2 = 0;
	if (y2 >= 232) y2 = 231; // Clamp to safe position
	if (y3 < 0) y3 = 0;
	if (y3 >= 232) y3 = 231; // Clamp to safe position
	
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
					*dest = mappedColor;
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
					*dest = mappedColor;
					drewSomething = true;
				}
			}
		}
	} else if (topY == midY && midY == botY) {
		// All points on same line - draw a line
		int xStart = (topX < midX) ? ((topX < botX) ? topX : botX) : ((midX < botX) ? midX : botX);
		int xEnd = (topX > midX) ? ((topX > botX) ? topX : botX) : ((midX > botX) ? midX : botX);
		if (topY >= 0 && topY < 240) {
			for (int x = xStart; x <= xEnd; ++x) {
				if (x >= 0 && x < 256) {
					uint8 *dest = currentXBuf + topY * 256 + x;
					*dest = mappedColor;
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
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
		if (cx + x >= 0 && cx + x < 256 && cy + y >= 0 && cy + y < 232) {
			currentXBuf[(cy + y) * 256 + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < 256 && cy + y >= 0 && cy + y < 232) {
			currentXBuf[(cy + y) * 256 + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		if (cx + x >= 0 && cx + x < 256 && cy - y >= 0 && cy - y < 232) {
			currentXBuf[(cy - y) * 256 + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < 256 && cy - y >= 0 && cy - y < 232) {
			currentXBuf[(cy - y) * 256 + (cx - x)] = mappedColor;
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
		if (cx + x >= 0 && cx + x < 256 && cy + y >= 0 && cy + y < 232) {
			currentXBuf[(cy + y) * 256 + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < 256 && cy + y >= 0 && cy + y < 232) {
			currentXBuf[(cy + y) * 256 + (cx - x)] = mappedColor;
			drewSomething = true;
		}
		if (cx + x >= 0 && cx + x < 256 && cy - y >= 0 && cy - y < 232) {
			currentXBuf[(cy - y) * 256 + (cx + x)] = mappedColor;
			drewSomething = true;
		}
		if (cx - x >= 0 && cx - x < 256 && cy - y >= 0 && cy - y < 232) {
			currentXBuf[(cy - y) * 256 + (cx - x)] = mappedColor;
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
	// Validate and reduce radii if ellipse would extend past safe bounds
	if (rx <= 0 || ry <= 0) return 0;
	if (cy + ry > 231) ry = 231 - cy;
	if (cy - ry < 0 && ry > cy) ry = cy;
	if (rx < 0 || ry < 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Calculate bounding box for filled ellipse
	int minX = (cx - rx < 0) ? 0 : (cx - rx);
	int maxX = (cx + rx >= 256) ? 255 : (cx + rx);
	int minY = (cy - ry < 0) ? 0 : (cy - ry);
	int maxY = (cy + ry >= 232) ? 231 : (cy + ry);
	
	if (minX >= 256 || minY >= 232 || maxX < 0 || maxY < 0) return 0;
	
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
					uint8 *dest = currentXBuf + py * 256 + px;
					*dest = mappedColor;
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
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
					uint8 *dest = currentXBuf + py * 256 + px;
					*dest = mappedColor;
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
	if (cx >= 256) cx = 255;
	if (cy < 0) cy = 0;
	if (cy >= 232) cy = 231; // Clamp to safe position (auto-move up if at/past 232)
	
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
	int maxX = (cx + radius >= 256) ? 255 : (cx + radius);
	int minY = (cy - radius < 0) ? 0 : (cy - radius);
	int maxY = (cy + radius >= 232) ? 231 : (cy + radius);
	
	if (minX >= 256 || minY >= 232 || maxX < 0 || maxY < 0) return 0;
	
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
						uint8 *dest = currentXBuf + py * 256 + px;
						*dest = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
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
					buf[py * 256 + px] = color;
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
	if (x >= 256) x = 255;
	if (y < 0) y = 0;
	if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	
	// Clamp rectangle to valid bounds and ensure it doesn't extend past y=232
	if (w <= 0 || h <= 0) return 0;
	if (y + h > 232) h = 232 - y; // Reduce height to fit
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
			if (px >= 0 && px < 256 && y >= 0 && y < 232) {
				currentXBuf[y * 256 + px] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Bottom edge
	if (radius < w / 2) {
		int botStartX = x + radius;
		int botEndX = x2 - radius;
		for (int px = botStartX; px <= botEndX; ++px) {
			if (px >= 0 && px < 256 && y2 >= 0 && y2 < 232) {
				currentXBuf[y2 * 256 + px] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Left edge (from top-left corner end to bottom-left corner start)
	if (radius < h / 2) {
		int leftStartY = y + radius;
		int leftEndY = y2 - radius;
		for (int py = leftStartY; py <= leftEndY; ++py) {
			if (x >= 0 && x < 256 && py >= 0 && py < 232) {
				currentXBuf[py * 256 + x] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Right edge
	if (radius < h / 2) {
		int rightStartY = y + radius;
		int rightEndY = y2 - radius;
		for (int py = rightStartY; py <= rightEndY; ++py) {
			if (x2 >= 0 && x2 < 256 && py >= 0 && py < 232) {
				currentXBuf[py * 256 + x2] = mappedColor;
				drewSomething = true;
			}
		}
	}
	
	// Special case: if radius is 0 or rectangle is too small, draw as regular rectangle
	if (radius == 0 || (radius >= w / 2 && radius >= h / 2)) {
		// Draw all four edges
		for (int px = x; px <= x2; ++px) {
			if (px >= 0 && px < 256 && y >= 0 && y < 232) {
				currentXBuf[y * 256 + px] = mappedColor;
				drewSomething = true;
			}
			if (px >= 0 && px < 256 && y2 >= 0 && y2 < 232) {
				currentXBuf[y2 * 256 + px] = mappedColor;
				drewSomething = true;
			}
		}
		for (int py = y; py <= y2; ++py) {
			if (x >= 0 && x < 256 && py >= 0 && py < 232) {
				currentXBuf[py * 256 + x] = mappedColor;
				drewSomething = true;
			}
			if (x2 >= 0 && x2 < 256 && py >= 0 && py < 232) {
				currentXBuf[py * 256 + x2] = mappedColor;
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
	if (x >= 256) x = 255;
	if (y < 0) y = 0;
	if (y >= 232) y = 231; // Clamp to safe position (auto-move up if at/past 232)
	
	// Clamp rectangle to valid bounds and ensure it doesn't extend past y=232
	if (w <= 0 || h <= 0) return 0;
	if (y + h > 232) h = 232 - y; // Reduce height to fit
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
	int maxX = (x2 >= 256) ? 255 : x2;
	int minY = (y < 0) ? 0 : y;
	int maxY = (y2 >= 232) ? 231 : y2;
	
	if (minX >= 256 || minY >= 232 || maxX < 0 || maxY < 0) return 0;
	
	// Fill center rectangle (if there's a center area)
	if (radius < w / 2 && radius < h / 2) {
		int centerX1 = x + radius;
		int centerX2 = x2 - radius;
		int centerY1 = y + radius;
		int centerY2 = y2 - radius;
		for (int py = centerY1; py <= centerY2; ++py) {
			for (int px = centerX1; px <= centerX2; ++px) {
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
					currentXBuf[py * 256 + px] = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
							currentXBuf[py * 256 + px] = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
							currentXBuf[py * 256 + px] = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
							currentXBuf[py * 256 + px] = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
							currentXBuf[py * 256 + px] = mappedColor;
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
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
						currentXBuf[py * 256 + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Bottom edge
		if (radius < w / 2) {
			for (int py = y2 - radius + 1; py <= maxY; ++py) {
				for (int px = x + radius; px <= x2 - radius; ++px) {
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
						currentXBuf[py * 256 + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Left edge
		if (radius < h / 2) {
			for (int py = y + radius; py <= y2 - radius; ++py) {
				for (int px = minX; px < x + radius; ++px) {
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
						currentXBuf[py * 256 + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
		
		// Right edge
		if (radius < h / 2) {
			for (int py = y + radius; py <= y2 - radius; ++py) {
				for (int px = x2 - radius + 1; px <= maxX; ++px) {
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
						currentXBuf[py * 256 + px] = mappedColor;
						drewSomething = true;
					}
				}
			}
		}
	} else {
		// No rounding: fill entire rectangle (same as fillrect)
		for (int py = minY; py <= maxY; ++py) {
			for (int px = minX; px <= maxX; ++px) {
		 if (px >= 0 && px < 256 && py >= 0 && py < 232) {
					currentXBuf[py * 256 + px] = mappedColor;
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
	 lua_register(luaState, "drawpixel", lua_drawpixel);
	 lua_register(luaState, "drawline", lua_drawline);
	 lua_register(luaState, "drawthickline", lua_drawthickline);
	 lua_register(luaState, "drawpolygon", lua_drawpolygon);
	 lua_register(luaState, "drawpolyline", lua_drawpolyline);
	 lua_register(luaState, "fillpolygon", lua_fillpolygon);
	 lua_register(luaState, "drawrect", lua_drawrect);
	 lua_register(luaState, "fillrect", lua_fillrect);
	 lua_register(luaState, "clearrect", lua_clearrect);
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
	REG("drawpixel",      lua_drawpixel);
	REG("drawline",       lua_drawline);
	REG("drawthickline",  lua_drawthickline);
	REG("drawpolygon",    lua_drawpolygon);
	REG("drawpolyline",   lua_drawpolyline);
	REG("fillpolygon",    lua_fillpolygon);
	REG("drawrect",       lua_drawrect);
	REG("fillrect",       lua_fillrect);
	REG("clearrect",      lua_clearrect);
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
		 if (err) lua_pop(luaState, 1);
		 return 0;
	 }

	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loaded %s", workingPath);
	 lua_getglobal(luaState, "gui");
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
		strncpy(g_luaStatusMsg, "Lua: ALL", sizeof(g_luaStatusMsg)-1);
		g_luaStatusMsg[sizeof(g_luaStatusMsg)-1] = '\0';
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
	 
	 if (lastFPSUpdate == 0) {
		 lastFPSUpdate = currentTime;
	 }
	 
	 if (currentTime - lastFPSUpdate >= 1000) {
		 currentFPS = (double)frameCount * 1000.0 / (double)(currentTime - lastFPSUpdate);
		 frameCount = 0;
		 lastFPSUpdate = currentTime;
	 }
	 
	 lastFrameTime = currentTime;
 }
 
 // GUI drawing callback - called from video.cpp
 // Update Lua at 30Hz, but composite the last overlay every frame to prevent flicker
 // Double-buffered: only publish new overlay if Lua succeeds (fail-safe)
 void FCEU_LuaGui(uint8 *XBuf) {
	 if (s_luaDisabled) {
		 ClearOverlaysIfAny();
		 return; // no composite, no "LUA OFF" banner, truly silent
	 }
	 
	 // Safety check: ensure overlay is initialized before proceeding
	 EnsureOverlay();
	 if (!s_overlay_back || !s_overlay_front) {
		 // Overlay buffers not initialized - skip this frame
		 return;
	 }
	 
	 static DWORD lastGuiTime = 0;
	 DWORD now = GetTickCount();
	 const DWORD step = 33; // ~30Hz Lua updates (33ms between calls)
	 
	 // Update overlay contents at ~30Hz (only when Lua needs to run)
	 if (now - lastGuiTime >= step) {
		 lastGuiTime = now;
		 
		 // Always start fresh to avoid "ghost" rectangles from prior frames
		 if (s_overlay_back) {
			 memset(s_overlay_back, 0, 256 * 240);
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
		 
		 if (s_overlay_back) {
			 // Status message is drawn at y=20 (below "LUA ON" at y=4), x=4
			 const int statusY = 20;
			 
			 if (statusTicks < 180) {  // ~6s @ 30Hz
				 // Draw status message below "LUA ON"
				 if (statusY >= 0 && statusY < 232 && g_luaStatusMsg[0] != '\0') {
					 DrawTextTrans(s_overlay_back + statusY*256 + 4, 256, (uint8*)g_luaStatusMsg, 0x2E | 0x80);
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
		 
		 if (luaInitialized && luaState != NULL) {
			 // Point Lua draw calls at the back buffer, not the front buffer
			 currentXBuf = s_overlay_back;
			 
			 // Call gui() function if it exists
			 lua_getglobal(luaState, "gui");
			 if (lua_isfunction(luaState, -1)) {
				 if (lua_pcall(luaState, 0, 0, 0) == 0) {
					 ok = true;  // Script executed successfully
				 } else {
					 // Lua error - draw a visible error marker
					 const char* err = lua_tostring(luaState, -1);
					 // Draw error indicator on screen so failures are visible
					 if (s_overlay_back) {
						 DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"LUA ERR", 0x0F | 0x80);
						 g_overlayDirty = true;  // Error marker counts as drawing
					 }
					 lua_pop(luaState, 1);
					 // Leave g_overlayDirty as-is (true if error marker drawn)
				 }
			 } else {
				 // gui() function doesn't exist
				 lua_pop(luaState, 1);
				 if (s_overlay_back) {
					 DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"NO gui()", 0x0F | 0x80);
					 g_overlayDirty = true;  // Error indicator counts as drawing
				 }
			 }
			 
			 currentXBuf = NULL;
		 } else {
			 // Lua not initialized
			 if (s_overlay_back) {
				 DrawTextTrans(s_overlay_back + 10*256 + 10, 256, (uint8*)"LUA OFF", 0x0F | 0x80);
				 g_overlayDirty = true;  // Status indicator counts as drawing
			 }
		 }
		 
		 // Only publish the new overlay if Lua succeeded AND actually drew something
		 if (ok && g_overlayDirty) {
			 // Only publish if the back buffer actually differs from the front
			 // This prevents unnecessary swaps when content hasn't changed
			 if (!s_overlay_front || overlay_has_changes(s_overlay_back, s_overlay_front)) {
				 SwapOverlays();
			 }
		 }
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
 
 #endif // USE_LUA