/* FCE Ultra Lua Integration for Xbox 360
 * Implementation
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
 
 // On-screen status message for debugging (shows last load attempt)
 static char g_luaStatusMsg[128] = "Lua: (not loaded)";
 // stdafx.h already includes xtl.h which provides GetTickCount() and DWORD
 
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
	 
	 if (!currentXBuf || x < 0 || y < 0 || x >= 256 || y >= 240 || !text || !*text) return 0;
	 
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
	 
	 if (!currentXBuf || x < 0 || y < 0 || x >= 256 || y >= 240) return 0;
	 
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
	 
	 // Draw pixel on the current frame buffer (set by FCEU_LuaGui)
	 if (currentXBuf && x >= 0 && y >= 0 && x < 256 && y < 240) {
		 uint8 *dest = currentXBuf + y * 256 + x;
		 *dest = map_overlay_color(color);
		 g_overlayDirty = true;  // Mark that something was drawn
	 }
	 
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
	 
	 // Draw line on the current frame buffer (set by FCEU_LuaGui)
	 if (!currentXBuf) return 0;
	 
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
		 // Check bounds and draw pixel
		 if (x >= 0 && x < 256 && y >= 0 && y < 240) {
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
	 
	 // Draw rectangle outline on the current frame buffer
	 if (!currentXBuf) return 0;
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 
	 uint8 mappedColor = map_overlay_color(color);
	 
	 bool drewSomething = false;
	 
	 // Draw top and bottom horizontal lines
	 for (int dx = 0; dx < w; ++dx) {
		 int px = x + dx;
		 
		 // Top line
		 if (px >= 0 && px < 256 && y >= 0 && y < 240) {
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
		 if (x >= 0 && x < 256 && py >= 0 && py < 240) {
			 uint8 *dest = currentXBuf + py * 256 + x;
			 *dest = mappedColor;
			 drewSomething = true;
		 }
		 
		 // Right line
		 if ((x + w - 1) >= 0 && (x + w - 1) < 256 && py >= 0 && py < 240) {
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
	 
	 // Draw filled rectangle on the current frame buffer
	 if (!currentXBuf) return 0;
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 
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
	 
	 // Clear rectangle area on the current frame buffer (set to 0 = transparent)
	 if (!currentXBuf) return 0;
	 
	 // Clamp rectangle to valid bounds
	 if (w <= 0 || h <= 0) return 0;
	 
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
	
	// Draw circle outline on the current frame buffer
	if (!currentXBuf) return 0;
	
	// Validate radius
	if (radius <= 0) return 0;
	
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
			if (px >= 0 && px < 256 && py >= 0 && py < 240) {
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
	
	// Draw filled circle on the current frame buffer
	if (!currentXBuf) return 0;
	
	// Validate radius
	if (radius <= 0) return 0;
	
	uint8 mappedColor = map_overlay_color(color);
	bool drewSomething = false;
	
	// Clamp circle bounds to screen
	int minX = (cx - radius < 0) ? 0 : (cx - radius);
	int maxX = (cx + radius >= 256) ? 255 : (cx + radius);
	int minY = (cy - radius < 0) ? 0 : (cy - radius);
	int maxY = (cy + radius >= 240) ? 239 : (cy + radius);
	
	if (minX >= 256 || minY >= 240 || maxX < 0 || maxY < 0) return 0;
	
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
				if (px >= 0 && px < 256 && py >= 0 && py < 240) {
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
	
	// Draw triangle outline on the current frame buffer
	if (!currentXBuf) return 0;
	
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
		if (x >= 0 && x < 256 && y >= 0 && y < 240) {
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
		if (x >= 0 && x < 256 && y >= 0 && y < 240) {
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
		if (x >= 0 && x < 256 && y >= 0 && y < 240) {
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
	
	// Draw filled triangle on the current frame buffer
	if (!currentXBuf) return 0;
	
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
			if (y < 0 || y >= 240) continue;
			
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
			if (y < 0 || y >= 240) continue;
			
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

// Get current FPS
int lua_getfps(lua_State *L) {
	 lua_pushnumber(L, currentFPS);
	 return 1;
 }
 
 // Initialize Lua
 static void InitLua() {
	 if (luaState != NULL) {
		 lua_close(luaState);
	 }
	 
	 luaState = lua_open();
	 if (luaState == NULL) {
		 return;
	 }
	 
	 luaL_openlibs(luaState);
	 
	 // Register FCEU functions
	 lua_register(luaState, "drawtext", lua_drawtext);
	 lua_register(luaState, "drawtextwh", lua_drawtextwh);
	 lua_register(luaState, "drawpixel", lua_drawpixel);
	 lua_register(luaState, "drawline", lua_drawline);
	 lua_register(luaState, "drawrect", lua_drawrect);
	 lua_register(luaState, "fillrect", lua_fillrect);
	 lua_register(luaState, "clearrect", lua_clearrect);
	 lua_register(luaState, "drawcircle", lua_drawcircle);
	 lua_register(luaState, "fillcircle", lua_fillcircle);
	 lua_register(luaState, "drawtriangle", lua_drawtriangle);
	 lua_register(luaState, "filltriangle", lua_filltriangle);
	 lua_register(luaState, "getfps", lua_getfps);
	 
	 luaInitialized = true;
 }
 
 // Load and run Lua script
 int FCEU_LoadLuaScript(const char* filename) {
	 if (!luaInitialized) {
		 InitLua();
		 if (!luaInitialized) {
			 return 0;
		 }
	 }
	 
	 if (luaState == NULL) {
		 return 0;
	 }
	 
	 // Try to load script from game:/lua/ folder (expanded paths for RGH/retail)
	 char fullpath[512];
	 const char* workingPath = NULL;
	 
	 // Try different path formats - prioritize user-writable locations first
	 // Note: Xbox file paths are case-sensitive and need correct separators
	 // hdd1: locations work even when game: is read-only (XZP/STFS packages)
	 const char* paths[] = {
		 "hdd1:\\fce360-enhanced\\lua\\%s",  // Primary: user-writable location
		 "hdd1:\\fce360-enhanced\\Lua\\%s",  // Uppercase variant
		 "game:\\lua\\%s",                   // Fallback: game folder (may be read-only)
		 "game:/lua/%s",
		 "game:lua/%s",
		 "game:\\Lua\\%s",                  // Try uppercase
		 "game:/Lua/%s",
		 "hdd1:\\fce360-enhanced\\lua\\%s",          // Legacy paths for compatibility
		 "hdd1:\\fce360-enhanced\\Lua\\%s",
		 "hdd1:\\lua\\%s",
		 "hdd1:\\Lua\\%s",
		 "usb0:\\lua\\%s",
		 "usb0:\\Lua\\%s",
		 "game:\\fce360-enhanced\\lua\\%s",
		 "game:\\fce360-enhanced\\Lua\\%s",
		 "game:\\%s",                        // Try directly in game: folder
		 "game:/%s",
		 "hdd1:\\_Emus\\fce360-enhanced\\lua\\%s"  // Alternative emulator organization path
	 };
	 
	 const int numPaths = sizeof(paths) / sizeof(paths[0]);
	 
	 for (int i = 0; i < numPaths; i++) {
		 snprintf(fullpath, sizeof(fullpath), paths[i], filename);
		 
		 // Check if file exists - try both read and binary mode
		 FILE* f = fopen(fullpath, "rb");
		 if (f != NULL) {
			 // File exists - get size to verify it's valid
			 fseek(f, 0, SEEK_END);
			 long size = ftell(f);
			 fseek(f, 0, SEEK_SET);
			 fclose(f);
			 
				 if (size > 0) {
				 workingPath = fullpath;
				 break;
			 }
		 }
	 }
	 
	 if (workingPath == NULL) {
		 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: %s NOT FOUND", filename);
		 return 0;
	 }
	 
	 // Update status message
	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loading %s", workingPath);
	 g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
	 
	 // Load and execute script
	 int result = luaL_dofile(luaState, workingPath);
	 if (result != 0) {
		 // Script failed to load - show error on screen
		 const char* err = lua_tostring(luaState, -1);
		 if (err) {
			 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: ERROR - %s", err);
		 } else {
			 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Load failed");
		 }
		 g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
		 lua_pop(luaState, 1);
		 return 0;
	 }
	 
	 snprintf(g_luaStatusMsg, sizeof(g_luaStatusMsg), "Lua: Loaded %s", workingPath);
	 g_luaStatusMsg[sizeof(g_luaStatusMsg) - 1] = '\0';  // Ensure null termination
	 // Verify gui() function exists
	 lua_getglobal(luaState, "gui");
	 if (lua_isfunction(luaState, -1)) {
		 lua_pop(luaState, 1);
	 } else {
		 lua_pop(luaState, 1);
	 }
	 
	 return 1;
 }
 
 // Auto-load all .lua scripts from lua directories
 void FCEU_AutoLoadLuaScripts(void) {
	 // Try multiple directories in priority order (user-writable first)
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
	 int totalLoaded = 0;
	 
	 for (int dirIdx = 0; dirIdx < numDirs; dirIdx++) {
		 char searchPattern[512];
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.lua", searchDirs[dirIdx]);
		 
		 WIN32_FIND_DATAA ffd;
		 HANDLE h = FindFirstFileA(searchPattern, &ffd);
		 
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 // Found a .lua file - load it
					 if (FCEU_LoadLuaScript(ffd.cFileName)) {
						 totalLoaded++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd));
			 FindClose(h);
		 }
		 
		 // Also try lowercase pattern
		 snprintf(searchPattern, sizeof(searchPattern), "%s\\*.LUA", searchDirs[dirIdx]);
		 h = FindFirstFileA(searchPattern, &ffd);
		 if (h != INVALID_HANDLE_VALUE) {
			 do {
				 if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					 if (FCEU_LoadLuaScript(ffd.cFileName)) {
						 totalLoaded++;
					 }
				 }
			 } while (FindNextFileA(h, &ffd));
			 FindClose(h);
		 }
	 }
	 
 }
 
 // Frame boundary callback
 void FCEU_LuaFrameBoundary() {
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