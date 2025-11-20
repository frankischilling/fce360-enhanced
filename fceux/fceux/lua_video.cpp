#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_video.h"
#include "lua_helpers.h"
#include "drawing.h"
#include "types.h"
#include "fceu.h"  // Must include before cart.h
#include "ppu.h"  // For PPU
#include "cart.h"  // For VPage, CHRptr

// Extern PALRAM from ppu.cpp
extern uint8 PALRAM[0x20];

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// Extern font data from drawing.cpp
extern uint8 Font6x7[792];
extern int FixJoedChar(uint8 ch);
extern int JoedCharWidth(uint8 ch);

// Overlay dimensions
enum { OVL_W = 256, OVL_H = 240, GLYPH_H = 8 };

// Double-buffered overlay for Lua-drawn content (updated at 30Hz, composited at 60Hz to prevent flicker)
// Front buffer: currently displayed (what we composite)
// Back buffer: where Lua draws next frame (only published on success)
static uint8* s_overlay_front = NULL; // currently displayed
static uint8* s_overlay_back  = NULL; // where Lua draws next
 
 void EnsureOverlay() {
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
 
 void CompositeOverlay(uint8* XBuf) {
	 // Blit the front overlay onto the NES frame every frame (prevents flicker when Lua runs at 30Hz)
	 if (!s_overlay_front || !XBuf) return;
	 
	 const int N = OVL_W * OVL_H;
	 const uint8* src = s_overlay_front;
	 for (int i = 0; i < N; ++i) {
		 uint8 v = src[i];
		 if (v) XBuf[i] = v;  // Only overwrite non-zero overlay pixels
	 }
 }
 
 void SwapOverlays() {
	 // Swap back and front buffers (publish the new overlay only on success)
	 uint8* t = s_overlay_front;
	 s_overlay_front = s_overlay_back;
	 s_overlay_back = t;
 }
 
 void ClearOverlaysIfAny() {
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
static uint8 s_oneFramePress[4]   = {0,0,0,0};   // one-frame button presses (cleared after each frame)
static uint8 s_oneFrameRelease[4] = {0,0,0,0};   // one-frame button releases (cleared after each frame)

void LuaConsolePushLine(const char* msg) {
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

static int s_consoleLineGap = 2; // pixels of extra leading between lines
static inline int CON_LINE_ADV(void) { return GLYPH_H + s_consoleLineGap; }

void FCEU_SetLuaConsoleLineGap(int px) {
	if (px < 0) px = 0;
	if (px > 8) px = 8;
	s_consoleLineGap = px;
}
int FCEU_GetLuaConsoleLineGap(void) { return s_consoleLineGap; }

void FCEU_ToggleLuaConsole(void) { 
	s_consoleVisible = !s_consoleVisible;
	if (!s_consoleVisible) {
		s_consoleScrollOffset = 0; // Reset scroll when console is hidden
		s_consoleScrollOffsetH = 0; // Reset horizontal scroll when console is hidden
	}
}

// Accessor functions for scroll offsets (used by fceulua.cpp for input handling)
int FCEU_GetLuaConsoleScrollOffset(void) { return s_consoleScrollOffset; }
void FCEU_SetLuaConsoleScrollOffset(int offset) { s_consoleScrollOffset = offset; }
int FCEU_GetLuaConsoleScrollOffsetH(void) { return s_consoleScrollOffsetH; }
void FCEU_SetLuaConsoleScrollOffsetH(int offset) { s_consoleScrollOffsetH = offset; }
int FCEU_GetLuaConsoleCount(void) { return s_luaConsoleCount; }

// Accessor functions for scroll state (used by fceulua.cpp for input handling)
bool* FCEU_GetLuaConsoleDpadUpLast(void) { return &s_consoleDpadUpLast; }
bool* FCEU_GetLuaConsoleDpadDownLast(void) { return &s_consoleDpadDownLast; }
bool* FCEU_GetLuaConsoleDpadLeftLast(void) { return &s_consoleDpadLeftLast; }
bool* FCEU_GetLuaConsoleDpadRightLast(void) { return &s_consoleDpadRightLast; }
int* FCEU_GetLuaConsoleScrollHoldFrames(void) { return &s_consoleScrollHoldFrames; }
int* FCEU_GetLuaConsoleScrollHoldFramesH(void) { return &s_consoleScrollHoldFramesH; }

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

// Transform state (defaults to identity - no transform)
static float s_transformTX = 0.0f;  // Translation X
static float s_transformTY = 0.0f;  // Translation Y
static float s_transformSX = 1.0f;  // Scale X
static float s_transformSY = 1.0f;  // Scale Y
static float s_transformRot = 0.0f; // Rotation in degrees
static bool s_transformEnabled = false;  // Only apply transform when explicitly set

// Batching state
static bool s_batching = false;  // True when between beginbatch() and endbatch()
static int s_batchDepth = 0;  // Track nested batch calls

// Image scaling mode
enum ImageScaleMode {
	IMAGE_SCALE_NEAREST = 0,  // Nearest-neighbor (pixelated, fast)
	IMAGE_SCALE_LINEAR = 1    // Linear interpolation (smooth, slower)
};
static ImageScaleMode s_imageScaleMode = IMAGE_SCALE_NEAREST;  // Default to nearest

// Drawing state structure for push/pop operations
struct DrawState {
	DrawMode drawMode;
	int defaultDrawColor;
	int clipX;
	int clipY;
	int clipW;
	int clipH;
	bool clipEnabled;
	float transformTX;
	float transformTY;
	float transformSX;
	float transformSY;
	float transformRot;
	bool transformEnabled;
	bool batching;
	int batchDepth;
	ImageScaleMode imageScaleMode;
};

// Stack for drawing state (for nested push/pop)
static std::vector<DrawState> s_drawStateStack;

// Canvas structure for offscreen rendering
struct Canvas {
	int width;
	int height;
	uint8* buffer;  // Allocated buffer for canvas pixels
	int handle;     // Unique handle ID
	
	Canvas() : width(0), height(0), buffer(NULL), handle(0) {}
	~Canvas() {
		if (buffer) {
			free(buffer);
			buffer = NULL;
		}
	}
};

// Gradient color stop
struct GradientStop {
	float position;  // 0.0 to 1.0
	int color;       // Color index (0x00-0x3F)
	
	GradientStop() : position(0.0f), color(0) {}
	GradientStop(float pos, int col) : position(pos), color(col) {}
};

// Linear gradient structure
struct LinearGradient {
	float x1, y1;  // Start point
	float x2, y2;  // End point
	std::vector<GradientStop> stops;  // Color stops
	int handle;     // Unique handle ID
	
	LinearGradient() : x1(0), y1(0), x2(0), y2(0), handle(0) {}
};

// Radial gradient structure
struct RadialGradient {
	float cx, cy;  // Center point
	float radius;   // Radius
	std::vector<GradientStop> stops;  // Color stops
	int handle;     // Unique handle ID
	
	RadialGradient() : cx(0), cy(0), radius(0), handle(0) {}
};

// Canvas management
static std::map<int, Canvas*> s_canvases;
static int s_nextCanvasHandle = 1;  // Start at 1, 0 is invalid

// Gradient management
static std::map<int, LinearGradient*> s_gradients;
static int s_nextGradientHandle = 1;  // Start at 1, 0 is invalid

// Radial gradient management
static std::map<int, RadialGradient*> s_radialGradients;
static int s_nextRadialGradientHandle = 1;  // Start at 1, 0 is invalid

// Render target state
static Canvas* s_currentRenderTarget = NULL;  // NULL = render to screen (overlay)
static int s_renderTargetWidth = OVL_W;  // Current render target width
static int s_renderTargetHeight = OVL_H;  // Current render target height

// Text style state
struct TextStyle {
	int font;        // Font index (0 = default, reserved for future use)
	float size;      // Text size/scale (1.0 = normal, 0.5-4.0 range)
	bool wrap;       // Word wrap enabled
	int align;       // Alignment: 0=left, 1=center, 2=right
	int outline;     // Outline/border: 0=none, 1=thin, 2=thick
	int shadow;      // Shadow: 0=none, 1=shadow enabled
	int spacing;     // Character spacing in pixels (0 = default)
	
	TextStyle() : font(0), size(1.0f), wrap(false), align(0), outline(0), shadow(0), spacing(0) {}
};

static TextStyle s_textStyle;  // Current text style

 // Helper function to transform a point (x, y) using current transform
 static inline void transform_point(float x, float y, float& outX, float& outY) {
	 if (!s_transformEnabled) {
		 outX = x;
		 outY = y;
		 return;
	 }
	 
	 // Convert rotation from degrees to radians
	 float rad = s_transformRot * 3.14159265358979323846f / 180.0f;
	 float cosR = cosf(rad);
	 float sinR = sinf(rad);
	 
	 // Apply rotation around origin
	 float rx = x * cosR - y * sinR;
	 float ry = x * sinR + y * cosR;
	 
	 // Apply scale
	 rx *= s_transformSX;
	 ry *= s_transformSY;
	 
	 // Apply translation
	 outX = rx + s_transformTX;
	 outY = ry + s_transformTY;
 }

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
 bool overlay_has_changes(const uint8* a, const uint8* b) {
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

// Accessor functions for overlay dirty flag
bool Lua_VideoGetOverlayDirty(void) {
	return g_overlayDirty;
}

void Lua_VideoSetOverlayDirty(bool dirty) {
	g_overlayDirty = dirty;
}

// Reset render target to screen (used by FCEU_LuaGui)
void Lua_VideoResetRenderTarget(void) {
	s_currentRenderTarget = NULL;
	s_renderTargetWidth = OVL_W;
	s_renderTargetHeight = OVL_H;
}
 
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

extern lua_State* luaState;
extern bool luaInitialized;
 
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

 // Draw text glyphs directly at position (0,0) in the buffer (no hardcoded offsets)
 static void DrawTextDirect(uint8* base, int pitch, const char* s, uint8 color, int max_w, int max_h)
 {
	 if (!base || !s || max_w <= 0 || max_h <= 0) return;
	 
	 int x = 0;
	 int y = 0;
	 
	 for (; *s; ++s) {
		 if (*s == '\n') {
			 x = 0;
			 y += 8;
			 if (y + 7 >= max_h) break;
			 continue;
		 }
		 
		 int ch = FixJoedChar((uint8)*s);
		 int wid = JoedCharWidth((uint8)*s);
		 
		 for (int ny = 0; ny < 7; ++ny) {
			 if (y + ny >= max_h) break;
			 uint8 d = Font6x7[ch * 8 + 1 + ny];
			 
			 for (int nx = 0; nx < wid; ++nx) {
				 if (x + nx >= max_w) break;
				 if ((d >> (7 - nx)) & 1) {
					 int px = x + nx;
					 int py = y + ny;
					 if (px >= 0 && px < max_w && py >= 0 && py < max_h) {
						 base[py * pitch + px] = color;
					 }
				 }
			 }
		 }
		 
		 x += wid;
		 if (x >= max_w) {
			 x = 0;
			 y += 8;
			 if (y + 7 >= max_h) break;
		 }
	 }
 }

 // Draw text with outline by drawing the text multiple times at offset positions
 // Uses the same approach as shadow - draw text at offset positions using DrawTextNoBorder
 static void DrawTextWithOutline(uint8* base, int pitch, const char* s, uint8 textColor, uint8 outlineColor, int outlineWidth, int max_w, int max_h)
 {
	 if (!base || !s || max_w <= 0 || max_h <= 0 || outlineWidth <= 0) {
		 // No outline, just draw text normally
		 DrawTextNoBorder(base, pitch, s, textColor);
		 return;
	 }
	 
	 // Draw outline first (draw text at multiple offset positions)
	 // For outlineWidth = 1: draw at 8 positions (N, S, E, W, NE, NW, SE, SW)
	 // For outlineWidth = 2: draw at more positions including 2-pixel offsets
	 
	 int offsets[][2] = {
		 {-1, -1}, {-1, 0}, {-1, 1},  // Top row
		 {0, -1},           {0, 1},   // Middle row (skip center)
		 {1, -1},  {1, 0},  {1, 1}    // Bottom row
	 };
	 int numOffsets = 8;
	 
	 if (outlineWidth >= 2) {
		 // Add 2-pixel offsets for thicker outline
		 int offsets2[][2] = {
			 {-2, -2}, {-2, 0}, {-2, 2},
			 {0, -2},           {0, 2},
			 {2, -2},  {2, 0},  {2, 2},
			 {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}
		 };
		 // Draw with 2-pixel offsets - use DrawTextNoBorder like shadow does
		 for (int i = 0; i < 16; i++) {
			 int dx = offsets2[i][0];
			 int dy = offsets2[i][1];
			 uint8* offsetBase = base + dy * pitch + dx;
			 // More lenient bounds check - allow drawing slightly outside
			 if (offsetBase >= currentXBuf && offsetBase < currentXBuf + OVL_W * OVL_H) {
				 DrawTextNoBorder(offsetBase, pitch, s, outlineColor);
			 }
		 }
	 }
	 
	 // Draw 1-pixel offset outline - use DrawTextNoBorder like shadow does
	 // Draw each offset position to create outline effect
	 for (int i = 0; i < numOffsets; i++) {
		 int dx = offsets[i][0];
		 int dy = offsets[i][1];
		 // Calculate offset pointer
		 uint8* offsetBase = base + dy * pitch + dx;
		 // More lenient bounds check - allow drawing slightly outside
		 if (offsetBase >= currentXBuf && offsetBase < currentXBuf + OVL_W * OVL_H) {
			 DrawTextNoBorder(offsetBase, pitch, s, outlineColor);
		 }
	 }
	 
	 // Draw main text on top
	 DrawTextNoBorder(base, pitch, s, textColor);
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

void DrawLuaConsole(uint8* buf) {
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
		 return LuaArgCountError(L, "drawtext", 3, 3, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawtext");
	 int y = LuaCheckInt(L, 2, "drawtext");
	 
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
	 
	 const char* text = LuaCheckString(L, 3, "drawtext");
	 int color_in = LuaCheckIntOpt(L, 4, 0x20, "drawtext");
	 
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
	 
	 // Apply text style options
	 // Draw shadow first (if enabled) so it appears behind the text
	 if (s_textStyle.shadow > 0) {
		 int shadowX = x + 1;
		 int shadowY = y + 1;
		 
		 // Check if shadow fits on screen
		 if (shadowX < OVL_W && shadowY < OVL_H && shadowY + GLYPH_H <= OVL_H) {
			 uint8 *shadowDest = currentXBuf + shadowY * OVL_W + shadowX;
			 if (shadowDest >= currentXBuf && shadowDest < currentXBuf + OVL_W * OVL_H) {
				 // Use darker color for shadow (reduce brightness)
				 int shadowColorValue = (color_in & 0x3F) >> 1;  // Darken by half
				 if (shadowColorValue < 0) shadowColorValue = 0;
				 uint8 shadowColor = map_overlay_color(shadowColorValue);
				 
				 if (s_textStyle.size != 1.0f) {
					 DrawTextTransScaled(shadowDest, OVL_W, (uint8*)text, shadowColor, s_textStyle.size, s_textStyle.size);
				 } else {
					 DrawTextNoBorder(shadowDest, OVL_W, text, shadowColor);
				 }
			 }
		 }
	 }
	 
	 // Draw main text
	 // If outline is enabled, use custom outline drawing
	 if (s_textStyle.outline > 0) {
		 // Calculate text dimensions
		 int textHeight = (int)(GLYPH_H * s_textStyle.size + 0.5f);
		 if (textHeight < 1) textHeight = GLYPH_H;
		 
		 // Create outline color (use bright red for maximum visibility - map it)
		 uint8 outlineColor = map_overlay_color(0x16); // Bright red for outline (very visible for testing)
		 
		 // Clamp outline width to 1 or 2
		 int outlineWidth = s_textStyle.outline;
		 if (outlineWidth > 2) outlineWidth = 2;
		 
		 if (s_textStyle.size != 1.0f) {
			 // For scaled text, we need to handle it differently
			 // Draw outline first with scaled text at offsets
			 // This is complex, so for now just draw scaled text normally
			 // TODO: Implement scaled text with outline
			 DrawTextTransScaled(dest, OVL_W, (uint8*)text, mapped, s_textStyle.size, s_textStyle.size);
		 } else {
			 // Draw text with outline
			 DrawTextWithOutline(dest, OVL_W, text, mapped, outlineColor, outlineWidth, wpx, textHeight);
		 }
	 } else {
		 // No outline - draw normally
		 if (s_textStyle.size != 1.0f) {
			 // Use scaled text if size is not 1.0
			 DrawTextTransScaled(dest, OVL_W, (uint8*)text, mapped, s_textStyle.size, s_textStyle.size);
		 } else {
			 // Normal text
			 DrawTextNoBorder(dest, OVL_W, text, mapped); // glyphs only, no bg
		 }
	 }
	 
	 g_overlayDirty = true;
	 return 0;
 }

 // Lua function to set console line spacing
 static int lua_setconsolespacing(lua_State* L) {
	 int px = LuaCheckInt(L, 1, "setconsolespacing");
	 FCEU_SetLuaConsoleLineGap(px);
	 return 0;
 }

 // Lua drawing function - allows scripts to draw text with width/height limits and border
 int lua_drawtextwh(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 7) {
		 return LuaArgCountError(L, "drawtextwh", 7, 7, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawtextwh");
	 int y = LuaCheckInt(L, 2, "drawtextwh");
	 const char* text = LuaCheckString(L, 3, "drawtextwh");
	 int color = LuaCheckInt(L, 4, "drawtextwh");
	 int max_w = LuaCheckInt(L, 5, "drawtextwh");
	 int max_h = LuaCheckInt(L, 6, "drawtextwh");
	 int border = LuaCheckInt(L, 7, "drawtextwh");
	 
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
		 return LuaArgCountError(L, "drawtextscaled", 6, 6, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawtextscaled");
	 int y = LuaCheckInt(L, 2, "drawtextscaled");
	 const char* text = LuaCheckString(L, 3, "drawtextscaled");
	 int color_in = LuaCheckInt(L, 4, "drawtextscaled");
	 float scaleX = (float)LuaCheckNumber(L, 5, "drawtextscaled");
	 float scaleY = (float)LuaCheckNumber(L, 6, "drawtextscaled");
	 
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

	 int x = LuaCheckInt(L, 1, "drawtextrotated");
	 int y = LuaCheckInt(L, 2, "drawtextrotated");
	 const char* text = LuaCheckString(L, 3, "drawtextrotated");
	 int color_in = LuaCheckInt(L, 4, "drawtextrotated");
	 int angleDeg = LuaCheckInt(L, 5, "drawtextrotated");

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
	 const char* s = LuaCheckString(L, 1, "gettextwidth");
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
	 const char* s = LuaCheckString(L, 1, "gettextheight");
	 if (!s || !*s) { lua_pushinteger(L, 0); return 1; }

	 int lines = 1;
	 for (const unsigned char* p = (const unsigned char*)s; *p; ++p)
		 if (*p == '\n') ++lines;

	 lua_pushinteger(L, lines * GLYPH_H); // GLYPH_H is 8
	 return 1;
 }

 // drawtextbox(x, y, width, height, text, color, bgColor, borderColor)
 // Draws text in a bordered box with background
 // bgColor and borderColor are optional (nil or -1 for no background/border)
 static int lua_drawtextbox(lua_State* L)
 {
	 int n = lua_gettop(L);
	 if (n < 6) {
		 return LuaArgCountError(L, "drawtextbox", 6, 8, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawtextbox");
	 int y = LuaCheckInt(L, 2, "drawtextbox");
	 int width = LuaCheckInt(L, 3, "drawtextbox");
	 int height = LuaCheckInt(L, 4, "drawtextbox");
	 const char* text = LuaCheckString(L, 5, "drawtextbox");
	 int color = LuaCheckInt(L, 6, "drawtextbox");
	 
	 // Optional parameters
	 int bgColor = -1;
	 int borderColor = -1;
	 if (n >= 7 && !lua_isnil(L, 7)) {
		 bgColor = LuaCheckInt(L, 7, "drawtextbox");
	 }
	 if (n >= 8 && !lua_isnil(L, 8)) {
		 borderColor = LuaCheckInt(L, 8, "drawtextbox");
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
		 return LuaArgCountError(L, "drawpixel", 3, 3, n);
	 }
	 
	 float x = (float)LuaCheckNumber(L, 1, "drawpixel");
	 float y = (float)LuaCheckNumber(L, 2, "drawpixel");
	 int color = LuaCheckInt(L, 3, "drawpixel");
	 
	 if (!currentXBuf) return 0;
	 
	 // Apply transform
	 float tx, ty;
	 transform_point(x, y, tx, ty);
	 
	 // Convert to integer coordinates
	 int ix = (int)(tx + 0.5f);  // Round to nearest
	 int iy = (int)(ty + 0.5f);
	 
	 // Early return if completely off-screen (better performance and safety)
	 if (ix < 0 || ix >= OVL_W || iy < 0 || iy >= OVL_H) return 0;
	 
	 // Draw pixel on the current frame buffer (set by FCEU_LuaGui)
	 // Additional defensive check on buffer pointer
	 if (iy * OVL_W + ix < 0 || iy * OVL_W + ix >= OVL_W * OVL_H) return 0;
	 
	 // Check clipping
	 if (is_point_clipped(ix, iy)) return 0;
	 
	 uint8 *dest = currentXBuf + iy * OVL_W + ix;
	 uint8 srcColor = map_overlay_color(color);
	 *dest = apply_blend_mode(*dest, srcColor);
	 g_overlayDirty = true;  // Mark that something was drawn
	 
	 return 0;
 }

 // Lua drawing function - allows scripts to draw a line between two points
 int lua_drawline(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return LuaArgCountError(L, "drawline", 5, 5, n);
	 }
	 
	 int x1 = LuaCheckInt(L, 1, "drawline");
	 int y1 = LuaCheckInt(L, 2, "drawline");
	 int x2 = LuaCheckInt(L, 3, "drawline");
	 int y2 = LuaCheckInt(L, 4, "drawline");
	 int color = LuaCheckInt(L, 5, "drawline");
	 
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
		 return LuaArgCountError(L, "drawthickline", 6, 6, n);
	 }
	 
	 int x1 = LuaCheckInt(L, 1, "drawthickline");
	 int y1 = LuaCheckInt(L, 2, "drawthickline");
	 int x2 = LuaCheckInt(L, 3, "drawthickline");
	 int y2 = LuaCheckInt(L, 4, "drawthickline");
	 int thickness = LuaCheckInt(L, 5, "drawthickline");
	 int color = LuaCheckInt(L, 6, "drawthickline");
	 
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
		 return LuaArgCountError(L, "drawrect", 5, 5, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawrect");
	 int y = LuaCheckInt(L, 2, "drawrect");
	 int w = LuaCheckInt(L, 3, "drawrect");
	 int h = LuaCheckInt(L, 4, "drawrect");
	 int color = LuaCheckInt(L, 5, "drawrect");
	 
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
		 return LuaArgCountError(L, "fillrect", 5, 5, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "fillrect");
	 int y = LuaCheckInt(L, 2, "fillrect");
	 int w = LuaCheckInt(L, 3, "fillrect");
	 int h = LuaCheckInt(L, 4, "fillrect");
	 int color = LuaCheckInt(L, 5, "fillrect");
	 
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
		 return LuaArgCountError(L, "clearrect", 4, 4, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "clearrect");
	 int y = LuaCheckInt(L, 2, "clearrect");
	 int w = LuaCheckInt(L, 3, "clearrect");
	 int h = LuaCheckInt(L, 4, "clearrect");
	 
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
		 return LuaArgCountError(L, "fillscreen", 1, 1, n);
	 }
	 
	 int color = LuaCheckInt(L, 1, "fillscreen");
	 
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
		const char *customFilename = LuaCheckPath(L, 1, "screenshot");
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

// Lua function - takes screenshot of a region with optional filename
int lua_screenshotregion(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 5) {
		return luaL_error(L, "screenshotregion(x, y, w, h, path) requires 5 arguments");
	}
	
	int x = LuaCheckInt(L, 1, "screenshotregion");
	int y = LuaCheckInt(L, 2, "screenshotregion");
	int w = LuaCheckPositive(L, 3, "screenshotregion", "width");
	int h = LuaCheckPositive(L, 4, "screenshotregion", "height");
	const char *path = LuaCheckPath(L, 5, "screenshotregion");
	
	if (!path || strlen(path) == 0) {
		return luaL_error(L, "screenshotregion() failed: path cannot be empty");
	}
	
	// Validate region parameters
	if (w <= 0 || h <= 0) {
		return luaL_error(L, "screenshotregion() failed: width and height must be positive");
	}
	if (x < 0 || y < 0) {
		return luaL_error(L, "screenshotregion() failed: x and y must be non-negative");
	}
	if (x + w > 256 || y + h > 240) {
		return luaL_error(L, "screenshotregion() failed: region extends beyond screen bounds (256x240)");
	}
	
	// Use s_frameXBuf which is set by FCEU_LuaGui with the actual frame buffer
	extern uint8 *XBuf;
	extern uint8 *s_frameXBuf;
	
	if (!s_frameXBuf && !XBuf) {
		return luaL_error(L, "screenshotregion() failed: frame buffer not available");
	}
	
	// Use s_frameXBuf if available, otherwise fall back to XBuf
	uint8 *sourceBuf = s_frameXBuf ? s_frameXBuf : XBuf;
	if (!sourceBuf) {
		return luaL_error(L, "screenshotregion() failed: frame buffer not available");
	}
	
	// Save the original XBuf pointer
	uint8 *oldXBuf = XBuf;
	
	// Create a composite buffer that includes both game frame and overlay
	uint8 *compositeBuf = (uint8*)malloc(256 * 240);
	if (!compositeBuf) {
		return luaL_error(L, "screenshotregion() failed: could not allocate composite buffer");
	}
	
	// First, copy the game frame to the composite buffer
	memcpy(compositeBuf, sourceBuf, 256 * 240);
	
	// Then, composite the overlay on top (same logic as CompositeOverlay)
	// The overlay uses 0x80-0xBF range, and we only overwrite non-zero pixels
	if (s_overlay_front) {
		for (int i = 0; i < 256 * 240; ++i) {
			uint8 overlayPixel = s_overlay_front[i];
			if (overlayPixel != 0) {
				// Overlay pixel is non-zero, composite it onto the frame
				compositeBuf[i] = overlayPixel;
			}
		}
	}
	
	// Create a temporary buffer for the region
	// SaveSnapshot expects a full 256x240 buffer, so we'll create one
	// and copy the region to the top-left corner
	uint8 *tempBuf = (uint8*)malloc(256 * 240);
	if (!tempBuf) {
		free(compositeBuf);
		return luaL_error(L, "screenshotregion() failed: could not allocate temporary buffer");
	}
	
	// Clear the temporary buffer (fill with black/transparent)
	memset(tempBuf, 0, 256 * 240);
	
	// Copy the region from composite buffer to temporary buffer (at position 0,0)
	for (int sy = 0; sy < h; ++sy) {
		int srcY = y + sy;
		if (srcY >= 0 && srcY < 240) {
			for (int sx = 0; sx < w; ++sx) {
				int srcX = x + sx;
				if (srcX >= 0 && srcX < 256) {
					// Copy pixel from composite region to temp buffer
					tempBuf[sy * 256 + sx] = compositeBuf[srcY * 256 + srcX];
				}
			}
		}
	}
	
	// Free the composite buffer (no longer needed)
	free(compositeBuf);
	
	// Temporarily replace XBuf with our region buffer
	XBuf = tempBuf;
	
	// Get snapshot directory - reuse cached path from screenshot()
	static std::string cachedSnapPath = "";
	static bool snapPathCached = false;
	
	std::string snapPath;
	if (!snapPathCached) {
		extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
		std::string tempPath = FCEU_MakeFName(2, 0, "png");  // 2 = FCEUMKF_SNAP
		
		size_t lastSlash = tempPath.find_last_of("\\/");
		if (lastSlash != std::string::npos) {
			cachedSnapPath = tempPath.substr(0, lastSlash + 1);
		} else {
			cachedSnapPath = ".\\";
		}
		snapPathCached = true;
	}
	snapPath = cachedSnapPath;
	
	// Ensure directory exists
	static std::string lastCheckedDir = "";
	static bool dirInitialized = false;
	
	std::string dirPath = snapPath;
	for (size_t i = 0; i < dirPath.length(); i++) {
		if (dirPath[i] == '/') {
			dirPath[i] = '\\';
		}
	}
	if (dirPath.length() > 0 && (dirPath[dirPath.length() - 1] == '\\' || dirPath[dirPath.length() - 1] == '/')) {
		dirPath = dirPath.substr(0, dirPath.length() - 1);
	}
	
	if (!dirInitialized || dirPath != lastCheckedDir) {
		CreateDirectoryA(dirPath.c_str(), NULL);
		lastCheckedDir = dirPath;
		dirInitialized = true;
	}
	
	// Process filename
	std::string baseFilename = path;
	
	// Add .png extension if not present
	if (baseFilename.length() < 4 || baseFilename.substr(baseFilename.length() - 4) != ".png") {
		baseFilename += ".png";
	}
	
	// Build full path
	std::string fullPath = snapPath;
	if (fullPath.length() > 0 && fullPath[fullPath.length() - 1] != '\\' && fullPath[fullPath.length() - 1] != '/') {
		fullPath += "\\";
	}
	fullPath += baseFilename;
	
	// Normalize path separators
	for (size_t i = 0; i < fullPath.length(); i++) {
		if (fullPath[i] == '/') {
			fullPath[i] = '\\';
		}
	}
	
	// Save the screenshot
	char filename[512] = {0};
	strncpy(filename, fullPath.c_str(), sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = '\0';
	
	extern int SaveSnapshot(char fileName[512]);
	int result = SaveSnapshot(filename);
	
	// Restore XBuf and free temporary buffer
	XBuf = oldXBuf;
	free(tempBuf);
	
	if (result == 0) {
		// Success - return true
		lua_pushboolean(L, 1);
		return 1;
	} else {
		// Failure - return false (don't error, just return false)
		lua_pushboolean(L, 0);
		return 1;
	}
}

// Lua drawing function - allows scripts to draw an image from byte data
int lua_drawimage(lua_State *L) {
	 int n = lua_gettop(L);
	 if (n < 5) {
		 return LuaArgCountError(L, "drawimage", 5, 5, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawimage");
	 int y = LuaCheckInt(L, 2, "drawimage");
	 
	 // Check if imageData is a table
	 LuaCheckTable(L, 3, "drawimage");
	 
	 int width = LuaCheckPositive(L, 4, "drawimage", "width");
	 int height = LuaCheckPositive(L, 5, "drawimage", "height");
	 
	 if (!currentXBuf) return 0;
	 
	 // Calculate expected data size
	 int expectedSize = width * height;
	 
	 // Read image data from table using helper (automatically clamps NES colors)
	 std::vector<uint8> imageData;
	 int dataCount = LuaTableToNESColorVector(L, 3, imageData, "drawimage");
	 
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
		 return LuaArgCountError(L, "drawimageindexed", 6, 6, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawimageindexed");
	 int y = LuaCheckInt(L, 2, "drawimageindexed");
	 
	 // Check if imageData is a table
	 LuaCheckTable(L, 3, "drawimageindexed");
	 
	 // Check if palette is a table
	 LuaCheckTable(L, 4, "drawimageindexed");
	 
	 int width = LuaCheckPositive(L, 5, "drawimageindexed", "width");
	 int height = LuaCheckPositive(L, 6, "drawimageindexed", "height");
	 
	 if (!currentXBuf) return 0;
	 
	 // Calculate expected data size
	 int expectedSize = width * height;
	 
	 // Read palette table using helper (automatically clamps NES colors)
	 std::vector<uint8> palette;
	 int paletteCount = LuaTableToNESColorVector(L, 4, palette, "drawimageindexed");
	 
	 if (paletteCount <= 0) {
		 return luaL_error(L, "drawimageindexed: palette table must contain at least one color value");
	 }
	 
	 // Read image data from table (Lua tables are 1-indexed)
	 // imageData contains indices into the palette table
	 std::vector<int> imageDataIndices;
	 int dataCount = LuaTableToIntVector(L, 3, imageDataIndices, "drawimageindexed");
	 
	 if (dataCount < expectedSize) {
		 return luaL_error(L, "drawimageindexed: imageData table must contain at least %d palette indices", expectedSize);
	 }
	 
	 // Convert indices to palette references (1-based Lua -> 0-based C++)
	 std::vector<uint8> imageData;
	 imageData.reserve(expectedSize);
	 for (int i = 0; i < expectedSize && i < dataCount; ++i) {
		 int paletteIndex = imageDataIndices[i] - 1; // Convert from 1-based to 0-based
		 // Clamp palette index to valid range
		 if (paletteIndex < 0) paletteIndex = 0;
		 if (paletteIndex >= paletteCount) paletteIndex = paletteCount - 1;
		 imageData.push_back((uint8)(paletteIndex & 0xFF));
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

// Lua drawing function - extended image drawing with options (rotation, scaling, flipping, tinting)
int lua_drawimageex(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 4) {
		return luaL_error(L, "drawimageex(img, x, y, options) requires at least 4 arguments");
	}
	
	// Check if imageData is a table
	if (!lua_istable(L, 1)) {
		return luaL_error(L, "drawimageex: img (1st argument) must be a table");
	}
	
	int x = (int)luaL_checkinteger(L, 2);
	int y = (int)luaL_checkinteger(L, 3);
	
	// Check if options is a table
	if (!lua_istable(L, 4)) {
		return luaL_error(L, "drawimageex: options (4th argument) must be a table");
	}
	
	if (!currentXBuf) return 0;
	
	// Parse options table
	int srcWidth = 0;
	int srcHeight = 0;
	int dstWidth = 0;
	int dstHeight = 0;
	float rotation = 0.0f;
	bool flipX = false;
	bool flipY = false;
	int tintColor = -1;  // -1 means no tint
	
	// Get source dimensions (required)
	lua_getfield(L, 4, "w");
	if (lua_isnumber(L, -1)) {
		srcWidth = (int)luaL_checkinteger(L, -1);
		dstWidth = srcWidth;  // Default to same as source
	}
	lua_pop(L, 1);
	
	lua_getfield(L, 4, "h");
	if (lua_isnumber(L, -1)) {
		srcHeight = (int)luaL_checkinteger(L, -1);
		dstHeight = srcHeight;  // Default to same as source
	}
	lua_pop(L, 1);
	
	if (srcWidth <= 0 || srcHeight <= 0) {
		return luaL_error(L, "drawimageex: options must contain 'w' and 'h' (source width and height)");
	}
	
	// Get destination dimensions (optional - for scaling)
	lua_getfield(L, 4, "dstW");
	if (lua_isnumber(L, -1)) {
		dstWidth = (int)luaL_checkinteger(L, -1);
	}
	lua_pop(L, 1);
	
	lua_getfield(L, 4, "dstH");
	if (lua_isnumber(L, -1)) {
		dstHeight = (int)luaL_checkinteger(L, -1);
	}
	lua_pop(L, 1);
	
	// Default to source dimensions if not specified
	if (dstWidth <= 0) dstWidth = srcWidth;
	if (dstHeight <= 0) dstHeight = srcHeight;
	
	// Get rotation (optional)
	lua_getfield(L, 4, "rot");
	if (lua_isnumber(L, -1)) {
		rotation = (float)luaL_checknumber(L, -1);
	}
	lua_pop(L, 1);
	
	// Get flipping flags (optional)
	lua_getfield(L, 4, "flipX");
	if (lua_isboolean(L, -1)) {
		flipX = lua_toboolean(L, -1) != 0;
	}
	lua_pop(L, 1);
	
	lua_getfield(L, 4, "flipY");
	if (lua_isboolean(L, -1)) {
		flipY = lua_toboolean(L, -1) != 0;
	}
	lua_pop(L, 1);
	
	// Get tint color (optional)
	lua_getfield(L, 4, "tint");
	if (lua_isnumber(L, -1)) {
		tintColor = (int)luaL_checkinteger(L, -1);
		if (tintColor < 0) tintColor = 0;
		if (tintColor > 0x3F) tintColor = 0x3F;
	}
	lua_pop(L, 1);
	
	// Calculate expected data size
	int expectedSize = srcWidth * srcHeight;
	
	// Read image data from table (Lua tables are 1-indexed)
	std::vector<uint8> imageData;
	imageData.reserve(expectedSize);
	
	int dataCount = 0;
	for (int i = 1; i <= expectedSize; ++i) {
		lua_rawgeti(L, 1, i);
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
		return luaL_error(L, "drawimageex: imageData table must contain at least %d color values", expectedSize);
	}
	
	// Calculate center point for rotation
	float centerX = x + dstWidth * 0.5f;
	float centerY = y + dstHeight * 0.5f;
	
	// Convert rotation to radians
	float rad = rotation * 3.14159265358979323846f / 180.0f;
	float cosR = cosf(rad);
	float sinR = sinf(rad);
	
	bool drewSomething = false;
	
	// Draw image with transformations
	// We iterate over destination pixels and sample from source
	for (int py = 0; py < dstHeight; ++py) {
		for (int px = 0; px < dstWidth; ++px) {
			// Start with destination pixel coordinates relative to center
			float relX = (float)px - dstWidth * 0.5f;
			float relY = (float)py - dstHeight * 0.5f;
			
			// Apply inverse rotation to get pre-rotation coordinates
			// (rotate back to find which source pixel to sample)
			float preRotX = relX * cosR + relY * sinR;  // Inverse rotation matrix
			float preRotY = -relX * sinR + relY * cosR;
			
			// Convert back to 0-based coordinates in destination space
			float unscaledX = preRotX + dstWidth * 0.5f;
			float unscaledY = preRotY + dstHeight * 0.5f;
			
			// Apply scaling to get source coordinates
			float srcX = unscaledX * (float)srcWidth / (float)dstWidth;
			float srcY = unscaledY * (float)srcHeight / (float)dstHeight;
			
			// Apply flipping
			if (flipX) {
				srcX = (float)srcWidth - 1.0f - srcX;
			}
			if (flipY) {
				srcY = (float)srcHeight - 1.0f - srcY;
			}
			
			// Sample source pixel (nearest neighbor)
			int srcPx = (int)(srcX + 0.5f);
			int srcPy = (int)(srcY + 0.5f);
			
			// Clamp source coordinates
			if (srcPx < 0) srcPx = 0;
			if (srcPx >= srcWidth) srcPx = srcWidth - 1;
			if (srcPy < 0) srcPy = 0;
			if (srcPy >= srcHeight) srcPy = srcHeight - 1;
			
			// Get source color
			int srcIndex = srcPy * srcWidth + srcPx;
			uint8 colorValue = imageData[srcIndex];
			
			// Apply tint if specified
			if (tintColor >= 0) {
				// Simple tint: blend with tint color (50% mix)
				int srcColorIdx = colorValue & 0x3F;
				int tintColorIdx = tintColor & 0x3F;
				// Average the color indices (simple tinting)
				colorValue = (uint8)((srcColorIdx + tintColorIdx) / 2);
			}
			
			// Calculate screen position (apply forward rotation to destination pixel)
			float rotX = relX * cosR - relY * sinR;
			float rotY = relX * sinR + relY * cosR;
			
			int screenX = (int)(centerX + rotX);
			int screenY = (int)(centerY + rotY);
			
			// Check bounds
			if (screenX >= 0 && screenX < OVL_W && screenY >= 0 && screenY < OVL_H) {
				// Check clipping
				if (!is_point_clipped(screenX, screenY)) {
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
		 return LuaArgCountError(L, "drawchrtile", 4, 4, n);
	 }
	 
	 int x = LuaCheckInt(L, 1, "drawchrtile");
	 int y = LuaCheckInt(L, 2, "drawchrtile");
	 int tileIndex = LuaCheckInt(L, 3, "drawchrtile");
	 int paletteIndex = LuaCheckInt(L, 4, "drawchrtile");
	 
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
		return LuaArgCountError(L, "setdrawcolor", 1, 1, n);
	}
	
	int color = LuaCheckNESColor(L, 1, "setdrawcolor", 1); // strict validation
	
	s_defaultDrawColor = color;
	return 0;
}

// Lua function to push drawing state (save current state)
int lua_pushdrawstate(lua_State *L) {
	// Create a snapshot of current drawing state
	DrawState state;
	state.drawMode = s_drawMode;
	state.defaultDrawColor = s_defaultDrawColor;
	state.clipX = s_clipX;
	state.clipY = s_clipY;
	state.clipW = s_clipW;
	state.clipH = s_clipH;
	state.clipEnabled = s_clipEnabled;
	state.transformTX = s_transformTX;
	state.transformTY = s_transformTY;
	state.transformSX = s_transformSX;
	state.transformSY = s_transformSY;
	state.transformRot = s_transformRot;
	state.transformEnabled = s_transformEnabled;
	state.batching = s_batching;
	state.batchDepth = s_batchDepth;
	state.imageScaleMode = s_imageScaleMode;
	
	// Push onto stack
	s_drawStateStack.push_back(state);
	
	return 0;
}

// Lua function to pop drawing state (restore saved state)
int lua_popdrawstate(lua_State *L) {
	// Check if stack is empty
	if (s_drawStateStack.empty()) {
		return luaL_error(L, "popdrawstate: no saved state to restore (stack is empty)");
	}
	
	// Get state from top of stack
	DrawState state = s_drawStateStack.back();
	s_drawStateStack.pop_back();
	
	// Restore all drawing state
	s_drawMode = state.drawMode;
	s_defaultDrawColor = state.defaultDrawColor;
	s_clipX = state.clipX;
	s_clipY = state.clipY;
	s_clipW = state.clipW;
	s_clipH = state.clipH;
	s_clipEnabled = state.clipEnabled;
	s_transformTX = state.transformTX;
	s_transformTY = state.transformTY;
	s_transformSX = state.transformSX;
	s_transformSY = state.transformSY;
	s_transformRot = state.transformRot;
	s_transformEnabled = state.transformEnabled;
	s_batching = state.batching;
	s_batchDepth = state.batchDepth;
	s_imageScaleMode = state.imageScaleMode;
	
	return 0;
}

// Lua function to set transform
int lua_settransform(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 5) {
		return luaL_error(L, "settransform(tx, ty, sx, sy, rot) requires 5 arguments");
	}
	
	float tx = (float)luaL_checknumber(L, 1);
	float ty = (float)luaL_checknumber(L, 2);
	float sx = (float)luaL_checknumber(L, 3);
	float sy = (float)luaL_checknumber(L, 4);
	float rot = (float)luaL_checknumber(L, 5);
	
	// Set transform values
	s_transformTX = tx;
	s_transformTY = ty;
	s_transformSX = sx;
	s_transformSY = sy;
	s_transformRot = rot;
	
	// Enable transform (even if values are identity, allow explicit control)
	s_transformEnabled = true;
	
	return 0;
}

// Lua function to reset transform
int lua_resettransform(lua_State *L) {
	// Reset to identity transform
	s_transformTX = 0.0f;
	s_transformTY = 0.0f;
	s_transformSX = 1.0f;
	s_transformSY = 1.0f;
	s_transformRot = 0.0f;
	s_transformEnabled = false;
	
	return 0;
}

// Lua function to begin batching draw calls
int lua_beginbatch(lua_State *L) {
	// Increment batch depth to support nested calls
	s_batchDepth++;
	s_batching = true;
	
	return 0;
}

// Lua function to end batching draw calls
int lua_endbatch(lua_State *L) {
	// Decrement batch depth
	if (s_batchDepth > 0) {
		s_batchDepth--;
	}
	
	// Only disable batching if we're back to depth 0
	if (s_batchDepth == 0) {
		s_batching = false;
	}
	
	return 0;
}

// Lua function to set image scaling mode
int lua_setimagescale(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "setimagescale(mode) requires 1 argument");
	}
	
	if (!lua_isstring(L, 1)) {
		return luaL_error(L, "setimagescale: mode must be a string");
	}
	
	const char* modeStr = lua_tostring(L, 1);
	
	if (strcmp(modeStr, "nearest") == 0) {
		s_imageScaleMode = IMAGE_SCALE_NEAREST;
	} else if (strcmp(modeStr, "linear") == 0) {
		s_imageScaleMode = IMAGE_SCALE_LINEAR;
	} else {
		return luaL_error(L, "setimagescale: invalid mode. Valid modes are: \"nearest\", \"linear\"");
	}
	
	return 0;
}

// Lua function to get current image scaling mode
int lua_getimagescale(lua_State *L) {
	const char* modeStr = (s_imageScaleMode == IMAGE_SCALE_NEAREST) ? "nearest" : "linear";
	lua_pushstring(L, modeStr);
	return 1;
}

// Lua function to create an offscreen canvas
int lua_createcanvas(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "createcanvas(w, h) requires 2 arguments");
	}
	
	int width = (int)luaL_checkinteger(L, 1);
	int height = (int)luaL_checkinteger(L, 2);
	
	// Validate dimensions
	if (width <= 0 || height <= 0) {
		return luaL_error(L, "createcanvas: width and height must be positive");
	}
	
	// Limit maximum size to prevent excessive memory usage
	const int MAX_CANVAS_SIZE = 1024;
	if (width > MAX_CANVAS_SIZE || height > MAX_CANVAS_SIZE) {
		return luaL_error(L, "createcanvas: maximum canvas size is %dx%d", MAX_CANVAS_SIZE, MAX_CANVAS_SIZE);
	}
	
	// Create new canvas
	Canvas* canvas = new Canvas();
	canvas->width = width;
	canvas->height = height;
	canvas->handle = s_nextCanvasHandle++;
	
	// Allocate buffer (same format as overlay: uint8 per pixel)
	int bufferSize = width * height;
	canvas->buffer = (uint8*)malloc(bufferSize);
	
	if (!canvas->buffer) {
		delete canvas;
		return luaL_error(L, "createcanvas: failed to allocate memory for canvas");
	}
	
	// Initialize buffer to transparent (0)
	memset(canvas->buffer, 0, bufferSize);
	
	// Store canvas in map
	s_canvases[canvas->handle] = canvas;
	
	// Return handle to Lua
	lua_pushinteger(L, canvas->handle);
	
	return 1;
}

// Lua function to set render target (canvas or screen)
int lua_setrendertarget(lua_State *L) {
	int n = lua_gettop(L);
	
	// If nil or no argument, reset to screen (overlay)
	if (n == 0 || lua_isnil(L, 1)) {
		// Restore to screen rendering (use overlay back buffer)
		// Only set if s_overlay_back is valid (it should always be, but be safe)
		if (s_overlay_back) {
			currentXBuf = s_overlay_back;
		} else {
			// If overlay not available, just reset state (currentXBuf will be set at callback start)
			// Don't crash - just reset the state
		}
		s_currentRenderTarget = NULL;
		s_renderTargetWidth = OVL_W;
		s_renderTargetHeight = OVL_H;
		return 0;
	}
	
	// Get canvas handle
	int canvasHandle = (int)luaL_checkinteger(L, 1);
	
	// Validate handle is positive (0 is invalid)
	if (canvasHandle <= 0) {
		return luaL_error(L, "setrendertarget: canvas handle must be positive (got %d)", canvasHandle);
	}
	
	// Look up canvas
	std::map<int, Canvas*>::iterator it = s_canvases.find(canvasHandle);
	if (it == s_canvases.end()) {
		return luaL_error(L, "setrendertarget: canvas handle %d not found", canvasHandle);
	}
	
	if (!it->second) {
		return luaL_error(L, "setrendertarget: canvas handle %d is NULL", canvasHandle);
	}
	
	Canvas* canvas = it->second;
	
	// Validate canvas structure
	if (!canvas) {
		return luaL_error(L, "setrendertarget: canvas is NULL");
	}
	
	if (!canvas->buffer) {
		return luaL_error(L, "setrendertarget: canvas buffer is NULL");
	}
	
	// Validate canvas dimensions
	if (canvas->width <= 0 || canvas->height <= 0) {
		return luaL_error(L, "setrendertarget: canvas has invalid dimensions (%dx%d)", canvas->width, canvas->height);
	}
	
	// Safety check: Ensure currentXBuf is initialized before switching
	// This prevents crashes if overlay isn't ready yet
	if (!currentXBuf) {
		return luaL_error(L, "setrendertarget: overlay buffer not initialized");
	}
	
	// Validate canvas buffer size matches dimensions
	int expectedSize = canvas->width * canvas->height;
	if (expectedSize <= 0) {
		return luaL_error(L, "setrendertarget: canvas buffer size calculation failed");
	}
	
	// CRITICAL SAFETY: Drawing functions currently use OVL_W/OVL_H (256x240) for buffer offsets
	// If we use a canvas smaller than screen size, drawing functions will calculate
	// buffer offsets using OVL_W but write to a smaller buffer, causing buffer overruns and crashes
	// For now, only allow screen-sized canvases (256x240) to prevent crashes
	// TODO: Update drawing functions to use s_renderTargetWidth/s_renderTargetHeight
	if (canvas->width != OVL_W || canvas->height != OVL_H) {
		return luaL_error(L, "setrendertarget: canvas must be screen-sized (256x240) for safety. Got %dx%d. Drawing functions need updates to support smaller canvases.", canvas->width, canvas->height);
	}
	
	// Set render target to canvas
	currentXBuf = canvas->buffer;
	s_currentRenderTarget = canvas;
	s_renderTargetWidth = canvas->width;
	s_renderTargetHeight = canvas->height;
	
	return 0;
}

// Lua function to blit (copy) a canvas to the screen
int lua_blit(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 3) {
		return luaL_error(L, "blit(canvas, x, y) requires 3 arguments");
	}
	
	// Get canvas handle
	int canvasHandle = (int)luaL_checkinteger(L, 1);
	int x = (int)luaL_checkinteger(L, 2);
	int y = (int)luaL_checkinteger(L, 3);
	
	// Validate handle is positive
	if (canvasHandle <= 0) {
		return luaL_error(L, "blit: canvas handle must be positive (got %d)", canvasHandle);
	}
	
	// Look up canvas
	std::map<int, Canvas*>::iterator it = s_canvases.find(canvasHandle);
	if (it == s_canvases.end() || !it->second) {
		return luaL_error(L, "blit: invalid canvas handle %d", canvasHandle);
	}
	
	Canvas* canvas = it->second;
	if (!canvas || !canvas->buffer) {
		return luaL_error(L, "blit: canvas buffer is invalid");
	}
	
	// Validate canvas dimensions
	if (canvas->width <= 0 || canvas->height <= 0) {
		return luaL_error(L, "blit: canvas has invalid dimensions (%dx%d)", canvas->width, canvas->height);
	}
	
	if (!currentXBuf) return 0;
	
	// Calculate source and destination regions with clipping
	int srcX = 0;
	int srcY = 0;
	int dstX = x;
	int dstY = y;
	int copyWidth = canvas->width;
	int copyHeight = canvas->height;
	
	// Clip left edge
	if (dstX < 0) {
		srcX = -dstX;
		copyWidth += dstX;
		dstX = 0;
	}
	
	// Clip top edge
	if (dstY < 0) {
		srcY = -dstY;
		copyHeight += dstY;
		dstY = 0;
	}
	
	// Clip right edge
	if (dstX + copyWidth > OVL_W) {
		copyWidth = OVL_W - dstX;
	}
	
	// Clip bottom edge
	if (dstY + copyHeight > OVL_H) {
		copyHeight = OVL_H - dstY;
	}
	
	// Check if anything is visible
	if (copyWidth <= 0 || copyHeight <= 0 || srcX >= canvas->width || srcY >= canvas->height) {
		return 0;  // Nothing to draw
	}
	
	bool drewSomething = false;
	
	// Copy canvas pixels to screen (row by row)
	for (int py = 0; py < copyHeight; ++py) {
		int srcYPos = srcY + py;
		int dstYPos = dstY + py;
		
		if (srcYPos >= canvas->height || dstYPos >= OVL_H) {
			break;  // Out of bounds
		}
		
		for (int px = 0; px < copyWidth; ++px) {
			int srcXPos = srcX + px;
			int dstXPos = dstX + px;
			
			if (srcXPos >= canvas->width || dstXPos >= OVL_W) {
				break;  // Out of bounds
			}
			
			// Check clipping
			if (!is_point_clipped(dstXPos, dstYPos)) {
				// Get source pixel from canvas
				int srcIndex = srcYPos * canvas->width + srcXPos;
				uint8 srcPixel = canvas->buffer[srcIndex];
				
				// Skip transparent pixels (0)
				if (srcPixel != 0) {
					// Get destination pixel
					uint8 *dest = currentXBuf + dstYPos * OVL_W + dstXPos;
					
					// Canvas pixels are already in overlay format (0x80-0xBF) since they
					// were drawn using the same drawing functions that use map_overlay_color()
					// Apply blending mode to composite the canvas onto the screen
					*dest = apply_blend_mode(*dest, srcPixel);
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

// Lua function to create a linear gradient
int lua_lineargradient(lua_State *L) {
	int n = lua_gettop(L);
	
	// Check minimum arguments: need at least 5 (x1, y1, x2, y2, and either a table or color)
	// Table mode: 5 args (x1, y1, x2, y2, stops_table)
	// Simple/variable mode: 6+ args (x1, y1, x2, y2, color1, color2, ...)
	if (n < 5) {
		return luaL_error(L, "lineargradient(x1, y1, x2, y2, ...) requires at least 5 arguments");
	}
	
	// Get start and end points
	float x1 = (float)luaL_checknumber(L, 1);
	float y1 = (float)luaL_checknumber(L, 2);
	float x2 = (float)luaL_checknumber(L, 3);
	float y2 = (float)luaL_checknumber(L, 4);
	
	// Check if points are the same (invalid gradient)
	if (x1 == x2 && y1 == y2) {
		return luaL_error(L, "lineargradient: start and end points cannot be the same");
	}
	
	// Create new gradient
	LinearGradient* gradient = new LinearGradient();
	gradient->x1 = x1;
	gradient->y1 = y1;
	gradient->x2 = x2;
	gradient->y2 = y2;
	gradient->handle = s_nextGradientHandle++;
	
	// Parse color stops
	// Three modes:
	// 1. Simple: lineargradient(x1, y1, x2, y2, color1, color2) - 6 args
	// 2. Table: lineargradient(x1, y1, x2, y2, stops) - 5 args, 5th is table
	// 3. Variable: lineargradient(x1, y1, x2, y2, color1, color2, color3, ...) - 6+ args
	
	if (n == 5 && lua_istable(L, 5)) {
		// Table of stops: { {pos1, color1}, {pos2, color2}, ... }
		int stopCount = 0;
		for (int i = 1; i <= 256; ++i) {
			lua_rawgeti(L, 5, i);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				break;  // End of table
			}
			
			if (!lua_istable(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip non-table entries
			}
			
			// Get position (first element)
			lua_rawgeti(L, -1, 1);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 2);
				continue;  // Skip invalid entries
			}
			float pos = (float)luaL_checknumber(L, -1);
			lua_pop(L, 1);
			
			// Get color (second element)
			lua_rawgeti(L, -1, 2);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip invalid entries
			}
			int color = (int)luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			
			// Clamp position to 0.0-1.0
			if (pos < 0.0f) pos = 0.0f;
			if (pos > 1.0f) pos = 1.0f;
			
			// Clamp color to valid range
			if (color < 0) color = 0;
			if (color > 0x3F) color = 0x3F;
			
			gradient->stops.push_back(GradientStop(pos, color));
			stopCount++;
			
			lua_pop(L, 1);  // Pop the stop table
		}
		
		if (stopCount < 2) {
			delete gradient;
			return luaL_error(L, "lineargradient: stops table must contain at least 2 color stops");
		}
		
		// Sort stops by position
		for (size_t i = 0; i < gradient->stops.size(); ++i) {
			for (size_t j = i + 1; j < gradient->stops.size(); ++j) {
				if (gradient->stops[i].position > gradient->stops[j].position) {
					GradientStop temp = gradient->stops[i];
					gradient->stops[i] = gradient->stops[j];
					gradient->stops[j] = temp;
				}
			}
		}
	} else if (n == 6) {
		// Simple two-color gradient
		int color1 = (int)luaL_checkinteger(L, 5);
		int color2 = (int)luaL_checkinteger(L, 6);
		
		// Clamp colors to valid range
		if (color1 < 0) color1 = 0;
		if (color1 > 0x3F) color1 = 0x3F;
		if (color2 < 0) color2 = 0;
		if (color2 > 0x3F) color2 = 0x3F;
		
		gradient->stops.push_back(GradientStop(0.0f, color1));
		gradient->stops.push_back(GradientStop(1.0f, color2));
	} else if (n == 5 && lua_istable(L, 5)) {
		// Table of stops: { {pos1, color1}, {pos2, color2}, ... }
		int stopCount = 0;
		for (int i = 1; i <= 256; ++i) {
			lua_rawgeti(L, 5, i);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				break;  // End of table
			}
			
			if (!lua_istable(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip non-table entries
			}
			
			// Get position (first element)
			lua_rawgeti(L, -1, 1);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 2);
				continue;  // Skip invalid entries
			}
			float pos = (float)luaL_checknumber(L, -1);
			lua_pop(L, 1);
			
			// Get color (second element)
			lua_rawgeti(L, -1, 2);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip invalid entries
			}
			int color = (int)luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			
			// Clamp position to 0.0-1.0
			if (pos < 0.0f) pos = 0.0f;
			if (pos > 1.0f) pos = 1.0f;
			
			// Clamp color to valid range
			if (color < 0) color = 0;
			if (color > 0x3F) color = 0x3F;
			
			gradient->stops.push_back(GradientStop(pos, color));
			stopCount++;
			
			lua_pop(L, 1);  // Pop the stop table
		}
		
		if (stopCount < 2) {
			delete gradient;
			return luaL_error(L, "lineargradient: stops table must contain at least 2 color stops");
		}
		
		// Sort stops by position
		for (size_t i = 0; i < gradient->stops.size(); ++i) {
			for (size_t j = i + 1; j < gradient->stops.size(); ++j) {
				if (gradient->stops[i].position > gradient->stops[j].position) {
					GradientStop temp = gradient->stops[i];
					gradient->stops[i] = gradient->stops[j];
					gradient->stops[j] = temp;
				}
			}
		}
	} else {
		// Variable arguments: lineargradient(x1, y1, x2, y2, color1, color2, color3, ...)
		// Treat as evenly spaced stops
		int colorCount = n - 4;
		if (colorCount < 2) {
			delete gradient;
			return luaL_error(L, "lineargradient: requires at least 2 colors");
		}
		
		for (int i = 0; i < colorCount; ++i) {
			int color = (int)luaL_checkinteger(L, 5 + i);
			
			// Clamp color to valid range
			if (color < 0) color = 0;
			if (color > 0x3F) color = 0x3F;
			
			float pos = (colorCount > 1) ? ((float)i / (float)(colorCount - 1)) : 0.0f;
			gradient->stops.push_back(GradientStop(pos, color));
		}
	}
	
	// Ensure we have at least 2 stops
	if (gradient->stops.size() < 2) {
		delete gradient;
		return luaL_error(L, "lineargradient: requires at least 2 color stops");
	}
	
	// Store gradient in map
	s_gradients[gradient->handle] = gradient;
	
	// Return handle to Lua
	lua_pushinteger(L, gradient->handle);
	
	return 1;
}

// Helper function to get color from gradient at a specific position along the gradient line
static int get_gradient_color(LinearGradient* gradient, float px, float py) {
	// Calculate position along gradient line (0.0 to 1.0)
	// Project point (px, py) onto the gradient line defined by (x1, y1) to (x2, y2)
	
	float dx = gradient->x2 - gradient->x1;
	float dy = gradient->y2 - gradient->y1;
	float lenSq = dx * dx + dy * dy;
	
	if (lenSq < 0.0001f) {
		// Degenerate gradient, return first color
		return gradient->stops[0].color;
	}
	
	// Vector from start point to pixel
	float vx = px - gradient->x1;
	float vy = py - gradient->y1;
	
	// Project onto gradient direction
	float t = (vx * dx + vy * dy) / lenSq;
	
	// Clamp t to 0.0-1.0
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	
	// Find the two stops that bracket this position
	if (gradient->stops.size() < 2) {
		return gradient->stops[0].color;
	}
	
	// Find the stop interval
	for (size_t i = 0; i < gradient->stops.size() - 1; ++i) {
		if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
			// Interpolate between these two stops
			float t0 = gradient->stops[i].position;
			float t1 = gradient->stops[i + 1].position;
			float localT = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
			
			int color0 = gradient->stops[i].color;
			int color1 = gradient->stops[i + 1].color;
			
			// For NES palette, colors are indices 0x00-0x3F
			// Simple linear interpolation between color indices
			// (Could be improved with proper color space interpolation)
			int result = (int)(color0 + (color1 - color0) * localT + 0.5f);
			
			// Clamp to valid range
			if (result < 0) result = 0;
			if (result > 0x3F) result = 0x3F;
			
			return result;
		}
	}
	
	// Outside range, return nearest stop
	if (t <= gradient->stops[0].position) {
		return gradient->stops[0].color;
	}
	return gradient->stops[gradient->stops.size() - 1].color;
}

// Helper function to get color from radial gradient at a specific position
static int get_radial_gradient_color(RadialGradient* gradient, float px, float py) {
	// Calculate distance from center
	float dx = px - gradient->cx;
	float dy = py - gradient->cy;
	float dist = sqrtf(dx * dx + dy * dy);
	
	// Normalize by radius (0.0 at center, 1.0 at radius, >1.0 beyond radius)
	float t = (gradient->radius > 0.0001f) ? (dist / gradient->radius) : 0.0f;
	
	// Clamp t to 0.0-1.0
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	
	// Find the two stops that bracket this position
	if (gradient->stops.size() < 2) {
		return gradient->stops[0].color;
	}
	
	// Find the stop interval
	for (size_t i = 0; i < gradient->stops.size() - 1; ++i) {
		if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
			// Interpolate between these two stops
			float t0 = gradient->stops[i].position;
			float t1 = gradient->stops[i + 1].position;
			float localT = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
			
			int color0 = gradient->stops[i].color;
			int color1 = gradient->stops[i + 1].color;
			
			// For NES palette, colors are indices 0x00-0x3F
			// Simple linear interpolation between color indices
			int result = (int)(color0 + (color1 - color0) * localT + 0.5f);
			
			// Clamp to valid range
			if (result < 0) result = 0;
			if (result > 0x3F) result = 0x3F;
			
			return result;
		}
	}
	
	// Outside range, return nearest stop
	if (t <= gradient->stops[0].position) {
		return gradient->stops[0].color;
	}
	return gradient->stops[gradient->stops.size() - 1].color;
}

// Lua function to fill a rectangle with a gradient
int lua_fillrectgradient(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 5) {
		return LuaArgCountError(L, "fillrectgradient", 5, 5, n);
	}
	
	int x = LuaCheckInt(L, 1, "fillrectgradient");
	int y = LuaCheckInt(L, 2, "fillrectgradient");
	int w = LuaCheckInt(L, 3, "fillrectgradient");
	int h = LuaCheckInt(L, 4, "fillrectgradient");
	int gradientHandle = LuaCheckInt(L, 5, "fillrectgradient");
	
	// Validate handle
	if (gradientHandle <= 0) {
		return luaL_error(L, "fillrectgradient: gradient handle must be positive (got %d)", gradientHandle);
	}
	
	// Look up gradient (will check both linear and radial in the loop)
	
	if (!currentXBuf) return 0;
	
	// Clamp coordinates to safe bounds
	if (x < 0) x = 0;
	if (x >= OVL_W) x = OVL_W - 1;
	if (y < 0) y = 0;
	if (y >= OVL_H) y = OVL_H - 1;
	
	// Clamp rectangle to valid bounds
	if (w <= 0 || h <= 0) return 0;
	if (y + h > OVL_H) h = OVL_H - y;
	if (h <= 0) return 0;
	
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
	
	// Fill the rectangle row by row with gradient
	for (int py = startY; py < endY; ++py) {
		for (int px = startX; px < endX; ++px) {
			// Check clipping
			if (!is_point_clipped(px, py)) {
				int color;
				
				// Check if it's a linear or radial gradient
				std::map<int, LinearGradient*>::iterator linearIt = s_gradients.find(gradientHandle);
				if (linearIt != s_gradients.end() && linearIt->second) {
					// Linear gradient
					color = get_gradient_color(linearIt->second, (float)px, (float)py);
				} else {
					// Check for radial gradient
					std::map<int, RadialGradient*>::iterator radialIt = s_radialGradients.find(gradientHandle);
					if (radialIt != s_radialGradients.end() && radialIt->second) {
						// Radial gradient
						color = get_radial_gradient_color(radialIt->second, (float)px, (float)py);
					} else {
						// Invalid gradient handle
						return luaL_error(L, "fillrectgradient: invalid gradient handle %d", gradientHandle);
					}
				}
				
				// Map color and apply blending
				uint8 *dest = currentXBuf + py * OVL_W + px;
				uint8 mappedColor = map_overlay_color(color);
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

// Lua function to create a radial gradient
int lua_radialgradient(lua_State *L) {
	int n = lua_gettop(L);
	
	// Check minimum arguments: need at least 4 (cx, cy, radius, and either a table or color)
	// Table mode: 4 args (cx, cy, radius, stops_table)
	// Simple/variable mode: 5+ args (cx, cy, radius, color1, color2, ...)
	if (n < 4) {
		return luaL_error(L, "radialgradient(cx, cy, radius, ...) requires at least 4 arguments");
	}
	
	// Get center and radius
	float cx = (float)luaL_checknumber(L, 1);
	float cy = (float)luaL_checknumber(L, 2);
	float radius = (float)luaL_checknumber(L, 3);
	
	// Check if radius is valid
	if (radius <= 0.0f) {
		return luaL_error(L, "radialgradient: radius must be positive (got %f)", radius);
	}
	
	// Create new gradient
	RadialGradient* gradient = new RadialGradient();
	gradient->cx = cx;
	gradient->cy = cy;
	gradient->radius = radius;
	gradient->handle = s_nextRadialGradientHandle++;
	
	// Parse color stops
	// Three modes:
	// 1. Simple: radialgradient(cx, cy, radius, color1, color2) - 5 args
	// 2. Table: radialgradient(cx, cy, radius, stops) - 4 args, 4th is table
	// 3. Variable: radialgradient(cx, cy, radius, color1, color2, color3, ...) - 5+ args
	
	if (n == 4 && lua_istable(L, 4)) {
		// Table of stops: { {pos1, color1}, {pos2, color2}, ... }
		int stopCount = 0;
		for (int i = 1; i <= 256; ++i) {
			lua_rawgeti(L, 4, i);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				break;  // End of table
			}
			
			if (!lua_istable(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip non-table entries
			}
			
			// Get position (first element)
			lua_rawgeti(L, -1, 1);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 2);
				continue;  // Skip invalid entries
			}
			float pos = (float)luaL_checknumber(L, -1);
			lua_pop(L, 1);
			
			// Get color (second element)
			lua_rawgeti(L, -1, 2);
			if (!lua_isnumber(L, -1)) {
				lua_pop(L, 1);
				continue;  // Skip invalid entries
			}
			int color = (int)luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			
			// Clamp position to 0.0-1.0
			if (pos < 0.0f) pos = 0.0f;
			if (pos > 1.0f) pos = 1.0f;
			
			// Clamp color to valid range
			if (color < 0) color = 0;
			if (color > 0x3F) color = 0x3F;
			
			gradient->stops.push_back(GradientStop(pos, color));
			stopCount++;
			
			lua_pop(L, 1);  // Pop the stop table
		}
		
		if (stopCount < 2) {
			delete gradient;
			return luaL_error(L, "radialgradient: stops table must contain at least 2 color stops");
		}
		
		// Sort stops by position
		for (size_t i = 0; i < gradient->stops.size(); ++i) {
			for (size_t j = i + 1; j < gradient->stops.size(); ++j) {
				if (gradient->stops[i].position > gradient->stops[j].position) {
					GradientStop temp = gradient->stops[i];
					gradient->stops[i] = gradient->stops[j];
					gradient->stops[j] = temp;
				}
			}
		}
	} else if (n == 5) {
		// Simple two-color gradient
		int color1 = (int)luaL_checkinteger(L, 4);
		int color2 = (int)luaL_checkinteger(L, 5);
		
		// Clamp colors to valid range
		if (color1 < 0) color1 = 0;
		if (color1 > 0x3F) color1 = 0x3F;
		if (color2 < 0) color2 = 0;
		if (color2 > 0x3F) color2 = 0x3F;
		
		gradient->stops.push_back(GradientStop(0.0f, color1));  // Center color
		gradient->stops.push_back(GradientStop(1.0f, color2));  // Edge color
	} else {
		// Variable arguments: radialgradient(cx, cy, radius, color1, color2, color3, ...)
		// Treat as evenly spaced stops
		int colorCount = n - 3;
		if (colorCount < 2) {
			delete gradient;
			return luaL_error(L, "radialgradient: requires at least 2 colors");
		}
		
		for (int i = 0; i < colorCount; ++i) {
			int color = (int)luaL_checkinteger(L, 4 + i);
			
			// Clamp color to valid range
			if (color < 0) color = 0;
			if (color > 0x3F) color = 0x3F;
			
			float pos = (colorCount > 1) ? ((float)i / (float)(colorCount - 1)) : 0.0f;
			gradient->stops.push_back(GradientStop(pos, color));
		}
	}
	
	// Ensure we have at least 2 stops
	if (gradient->stops.size() < 2) {
		delete gradient;
		return luaL_error(L, "radialgradient: requires at least 2 color stops");
	}
	
	// Store gradient in map
	s_radialGradients[gradient->handle] = gradient;
	
	// Return handle to Lua
	lua_pushinteger(L, gradient->handle);
	
	return 1;
}

// Lua function to set text rendering style options
int lua_textstyle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 1) {
		return luaL_error(L, "textstyle(options) requires 1 argument");
	}
	
	// Check if options is a table
	if (!lua_istable(L, 1)) {
		return luaL_error(L, "textstyle: options (1st argument) must be a table");
	}
	
	// Parse options table
	// font (optional)
	lua_getfield(L, 1, "font");
	if (lua_isnumber(L, -1)) {
		s_textStyle.font = (int)luaL_checkinteger(L, -1);
		if (s_textStyle.font < 0) s_textStyle.font = 0;
		// Note: Currently only font 0 is supported
	}
	lua_pop(L, 1);
	
	// size (optional)
	lua_getfield(L, 1, "size");
	if (lua_isnumber(L, -1)) {
		s_textStyle.size = (float)luaL_checknumber(L, -1);
		// Clamp to valid range (0.5-4.0)
		if (s_textStyle.size < 0.5f) s_textStyle.size = 0.5f;
		if (s_textStyle.size > 4.0f) s_textStyle.size = 4.0f;
	}
	lua_pop(L, 1);
	
	// wrap (optional)
	lua_getfield(L, 1, "wrap");
	if (lua_isboolean(L, -1)) {
		s_textStyle.wrap = lua_toboolean(L, -1) != 0;
	}
	lua_pop(L, 1);
	
	// align (optional)
	lua_getfield(L, 1, "align");
	if (lua_isstring(L, -1)) {
		const char* alignStr = luaL_checkstring(L, -1);
		if (strcmp(alignStr, "left") == 0 || strcmp(alignStr, "0") == 0) {
			s_textStyle.align = 0;
		} else if (strcmp(alignStr, "center") == 0 || strcmp(alignStr, "1") == 0) {
			s_textStyle.align = 1;
		} else if (strcmp(alignStr, "right") == 0 || strcmp(alignStr, "2") == 0) {
			s_textStyle.align = 2;
		} else {
			return luaL_error(L, "textstyle: align must be 'left', 'center', or 'right'");
		}
	} else if (lua_isnumber(L, -1)) {
		int alignVal = (int)luaL_checkinteger(L, -1);
		if (alignVal < 0) alignVal = 0;
		if (alignVal > 2) alignVal = 2;
		s_textStyle.align = alignVal;
	}
	lua_pop(L, 1);
	
	// outline (optional)
	lua_getfield(L, 1, "outline");
	if (lua_isnumber(L, -1)) {
		s_textStyle.outline = (int)luaL_checkinteger(L, -1);
		// Clamp to valid range (0-2)
		if (s_textStyle.outline < 0) s_textStyle.outline = 0;
		if (s_textStyle.outline > 2) s_textStyle.outline = 2;
	} else if (lua_isboolean(L, -1)) {
		s_textStyle.outline = lua_toboolean(L, -1) ? 1 : 0;
	}
	lua_pop(L, 1);
	
	// shadow (optional)
	lua_getfield(L, 1, "shadow");
	if (lua_isboolean(L, -1)) {
		s_textStyle.shadow = lua_toboolean(L, -1) ? 1 : 0;
	} else if (lua_isnumber(L, -1)) {
		s_textStyle.shadow = (int)luaL_checkinteger(L, -1);
		if (s_textStyle.shadow < 0) s_textStyle.shadow = 0;
		if (s_textStyle.shadow > 1) s_textStyle.shadow = 1;
	}
	lua_pop(L, 1);
	
	// spacing (optional)
	lua_getfield(L, 1, "spacing");
	if (lua_isnumber(L, -1)) {
		s_textStyle.spacing = (int)luaL_checkinteger(L, -1);
		// Clamp to reasonable range (-2 to 10)
		if (s_textStyle.spacing < -2) s_textStyle.spacing = -2;
		if (s_textStyle.spacing > 10) s_textStyle.spacing = 10;
	}
	lua_pop(L, 1);
	
	return 0;
}

// Lua function to measure wrapped text block dimensions
int lua_measuretextblock(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "measuretextblock(text, width) requires 2 arguments");
	}
	
	const char* text = luaL_checkstring(L, 1);
	int wrapWidth = (int)luaL_checkinteger(L, 2);
	
	if (!text || wrapWidth <= 0) {
		// Return empty result for invalid input
		lua_newtable(L);
		lua_pushstring(L, "width");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "height");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "lineCount");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		return 1;
	}
	
	// Account for text style size if set
	float sizeScale = s_textStyle.size;
	if (sizeScale < 0.5f) sizeScale = 0.5f;
	if (sizeScale > 4.0f) sizeScale = 4.0f;
	
	// Account for character spacing if set
	int spacing = s_textStyle.spacing;
	if (spacing < -2) spacing = -2;
	if (spacing > 10) spacing = 10;
	
	int lineCount = 0;
	int maxLineWidth = 0;
	int currentLineWidth = 0;
	int wordStart = 0;
	int wordWidth = 0;
	
	const char* p = text;
	bool inWord = false;
	
	// Process text character by character
	while (*p) {
		if (*p == '\n') {
			// Explicit newline - end current line
			if (currentLineWidth > maxLineWidth) {
				maxLineWidth = currentLineWidth;
			}
			lineCount++;
			currentLineWidth = 0;
			wordStart = 0;
			wordWidth = 0;
			inWord = false;
			p++;
			continue;
		}
		
		// Get character width
		int charWidth = JoedCharWidth((uint8)*p);
		if (charWidth <= 0) charWidth = 6; // fallback
		
		// Apply size scaling
		int scaledCharWidth = (int)(charWidth * sizeScale + 0.5f);
		if (scaledCharWidth < 1) scaledCharWidth = 1;
		
		// Add spacing (except for first character on line)
		if (currentLineWidth > 0 && spacing != 0) {
			scaledCharWidth += spacing;
		}
		
		// Check if this is a space character
		bool isSpace = (*p == ' ' || *p == '\t');
		
		if (isSpace) {
			// Space character - commit current word if any
			if (inWord) {
				currentLineWidth += wordWidth;
				wordWidth = 0;
				inWord = false;
			}
			
			// Add space to current line
			if (currentLineWidth + scaledCharWidth <= wrapWidth) {
				currentLineWidth += scaledCharWidth;
			} else {
				// Space doesn't fit - wrap to next line
				if (currentLineWidth > maxLineWidth) {
					maxLineWidth = currentLineWidth;
				}
				lineCount++;
				currentLineWidth = scaledCharWidth; // Start new line with space
			}
		} else {
			// Non-space character - part of a word
			if (!inWord) {
				// Start of new word
				wordStart = currentLineWidth;
				wordWidth = 0;
				inWord = true;
			}
			
			// Check if word fits on current line
			if (currentLineWidth + wordWidth + scaledCharWidth <= wrapWidth) {
				// Word continues to fit
				wordWidth += scaledCharWidth;
			} else {
				// Word doesn't fit on current line
				if (wordWidth > 0 && wordStart > 0) {
					// Word started mid-line - wrap it to next line
					if (currentLineWidth > maxLineWidth) {
						maxLineWidth = currentLineWidth;
					}
					lineCount++;
					currentLineWidth = wordWidth + scaledCharWidth;
					wordStart = 0;
				} else {
					// Word is at start of line or too long - break it
					if (currentLineWidth > maxLineWidth) {
						maxLineWidth = currentLineWidth;
					}
					if (currentLineWidth > 0) {
						lineCount++;
					}
					currentLineWidth = scaledCharWidth;
					wordStart = 0;
				}
				wordWidth = scaledCharWidth;
			}
		}
		
		p++;
	}
	
	// Handle final word and line
	if (inWord) {
		currentLineWidth += wordWidth;
	}
	if (currentLineWidth > 0) {
		lineCount++;
		if (currentLineWidth > maxLineWidth) {
			maxLineWidth = currentLineWidth;
		}
	}
	
	// Calculate total height (each line is 8 pixels tall, scaled)
	int lineHeight = (int)(GLYPH_H * sizeScale + 0.5f);
	if (lineHeight < 1) lineHeight = GLYPH_H;
	int totalHeight = lineCount * lineHeight;
	
	// Return result table
	lua_newtable(L);
	
	lua_pushstring(L, "width");
	lua_pushinteger(L, maxLineWidth);
	lua_settable(L, -3);
	
	lua_pushstring(L, "height");
	lua_pushinteger(L, totalHeight);
	lua_settable(L, -3);
	
	lua_pushstring(L, "lineCount");
	lua_pushinteger(L, lineCount);
	lua_settable(L, -3);
	
	return 1;
}

// Lua drawing function - allows scripts to draw a circle outline
int lua_drawcircle(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 4) {
		return LuaArgCountError(L, "drawcircle", 4, 4, n);
	}
	
	int cx = LuaCheckInt(L, 1, "drawcircle");
	int cy = LuaCheckInt(L, 2, "drawcircle");
	int radius = LuaCheckInt(L, 3, "drawcircle");
	int color = LuaCheckInt(L, 4, "drawcircle");
	
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
		return LuaArgCountError(L, "fillcircle", 4, 4, n);
	}
	
	int cx = LuaCheckInt(L, 1, "fillcircle");
	int cy = LuaCheckInt(L, 2, "fillcircle");
	int radius = LuaCheckInt(L, 3, "fillcircle");
	int color = LuaCheckInt(L, 4, "fillcircle");
	
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
		return LuaArgCountError(L, "drawtriangle", 7, 7, n);
	}
	
	int x1 = LuaCheckInt(L, 1, "drawtriangle");
	int y1 = LuaCheckInt(L, 2, "drawtriangle");
	int x2 = LuaCheckInt(L, 3, "drawtriangle");
	int y2 = LuaCheckInt(L, 4, "drawtriangle");
	int x3 = LuaCheckInt(L, 5, "drawtriangle");
	int y3 = LuaCheckInt(L, 6, "drawtriangle");
	int color = LuaCheckInt(L, 7, "drawtriangle");
	
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
		return LuaArgCountError(L, "filltriangle", 7, 7, n);
	}
	
	int x1 = LuaCheckInt(L, 1, "filltriangle");
	int y1 = LuaCheckInt(L, 2, "filltriangle");
	int x2 = LuaCheckInt(L, 3, "filltriangle");
	int y2 = LuaCheckInt(L, 4, "filltriangle");
	int x3 = LuaCheckInt(L, 5, "filltriangle");
	int y3 = LuaCheckInt(L, 6, "filltriangle");
	int color = LuaCheckInt(L, 7, "filltriangle");
	
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
		return LuaArgCountError(L, "drawellipse", 5, 5, n);
	}
	
	int cx = LuaCheckInt(L, 1, "drawellipse");
	int cy = LuaCheckInt(L, 2, "drawellipse");
	int rx = LuaCheckInt(L, 3, "drawellipse");
	int ry = LuaCheckInt(L, 4, "drawellipse");
	int color = LuaCheckInt(L, 5, "drawellipse");
	
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
		return LuaArgCountError(L, "fillellipse", 5, 5, n);
	}
	
	int cx = LuaCheckInt(L, 1, "fillellipse");
	int cy = LuaCheckInt(L, 2, "fillellipse");
	int rx = LuaCheckInt(L, 3, "fillellipse");
	int ry = LuaCheckInt(L, 4, "fillellipse");
	int color = LuaCheckInt(L, 5, "fillellipse");
	
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
		return LuaArgCountError(L, "drawarc", 6, 6, n);
	}
	
	int cx = LuaCheckInt(L, 1, "drawarc");
	int cy = LuaCheckInt(L, 2, "drawarc");
	int radius = LuaCheckInt(L, 3, "drawarc");
	int startAngle = LuaCheckInt(L, 4, "drawarc");
	int endAngle = LuaCheckInt(L, 5, "drawarc");
	int color = LuaCheckInt(L, 6, "drawarc");
	
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
		return LuaArgCountError(L, "fillarc", 6, 6, n);
	}
	
	int cx = LuaCheckInt(L, 1, "fillarc");
	int cy = LuaCheckInt(L, 2, "fillarc");
	int radius = LuaCheckInt(L, 3, "fillarc");
	int startAngle = LuaCheckInt(L, 4, "fillarc");
	int endAngle = LuaCheckInt(L, 5, "fillarc");
	int color = LuaCheckInt(L, 6, "fillarc");
	
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
		return LuaArgCountError(L, "fillroundrect", 6, 6, n);
	}
	
	int x = LuaCheckInt(L, 1, "fillroundrect");
	int y = LuaCheckInt(L, 2, "fillroundrect");
	int w = LuaCheckInt(L, 3, "fillroundrect");
	int h = LuaCheckInt(L, 4, "fillroundrect");
	int radius = LuaCheckInt(L, 5, "fillroundrect");
	int color = LuaCheckInt(L, 6, "fillroundrect");
	
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

// ============================================================================
// Module Registrar and Lifecycle Hooks
// ============================================================================

void Lua_RegisterVideo(lua_State* L) {
	if (!L) {
		return;
	}

	// Text drawing functions
	lua_register(L, "drawtext", lua_drawtext);
	lua_register(L, "drawtextwh", lua_drawtextwh);
	lua_register(L, "drawtextscaled", lua_drawtextscaled);
	lua_register(L, "drawtextrotated", lua_drawtextrotated);
	lua_pushcfunction(L, lua_gettextwidth);
	lua_setglobal(L, "gettextwidth");
	lua_pushcfunction(L, lua_gettextheight);
	lua_setglobal(L, "gettextheight");
	lua_register(L, "drawtextbox", lua_drawtextbox);
	lua_register(L, "setconsolespacing", lua_setconsolespacing);

	// Basic drawing primitives
	lua_register(L, "drawpixel", lua_drawpixel);
	lua_register(L, "drawline", lua_drawline);
	lua_register(L, "drawthickline", lua_drawthickline);
	lua_register(L, "drawrect", lua_drawrect);
	lua_register(L, "fillrect", lua_fillrect);
	lua_register(L, "clearrect", lua_clearrect);
	lua_register(L, "clearscreen", lua_clearscreen);
	lua_register(L, "fillscreen", lua_fillscreen);

	// Polygon drawing
	lua_register(L, "drawpolygon", lua_drawpolygon);
	lua_register(L, "drawpolyline", lua_drawpolyline);
	lua_register(L, "fillpolygon", lua_fillpolygon);

	// Circle and ellipse drawing
	lua_register(L, "drawcircle", lua_drawcircle);
	lua_register(L, "fillcircle", lua_fillcircle);
	lua_register(L, "drawellipse", lua_drawellipse);
	lua_register(L, "fillellipse", lua_fillellipse);
	lua_register(L, "drawarc", lua_drawarc);
	lua_register(L, "fillarc", lua_fillarc);

	// Rounded rectangles
	lua_register(L, "drawroundrect", lua_drawroundrect);
	lua_register(L, "fillroundrect", lua_fillroundrect);

	// Triangle drawing
	lua_register(L, "drawtriangle", lua_drawtriangle);
	lua_register(L, "filltriangle", lua_filltriangle);

	// Image drawing
	lua_register(L, "drawimage", lua_drawimage);
	lua_register(L, "drawimageindexed", lua_drawimageindexed);
	lua_register(L, "drawimageex", lua_drawimageex);
	lua_register(L, "drawtile", lua_drawtile);
	lua_register(L, "drawchrtile", lua_drawchrtile);

	// Screenshot functions
	lua_register(L, "screenshot", lua_screenshot);
	lua_register(L, "screenshotregion", lua_screenshotregion);

	// Drawing state management
	lua_register(L, "setdrawmode", lua_setdrawmode);
	lua_register(L, "setclipregion", lua_setclipregion);
	lua_register(L, "clearclipregion", lua_clearclipregion);
	lua_register(L, "setdrawcolor", lua_setdrawcolor);
	lua_register(L, "pushdrawstate", lua_pushdrawstate);
	lua_register(L, "popdrawstate", lua_popdrawstate);
	lua_register(L, "settransform", lua_settransform);
	lua_register(L, "resettransform", lua_resettransform);
	lua_register(L, "beginbatch", lua_beginbatch);
	lua_register(L, "endbatch", lua_endbatch);
	lua_register(L, "setimagescale", lua_setimagescale);
	lua_register(L, "getimagescale", lua_getimagescale);

	// Canvas management
	lua_register(L, "createcanvas", lua_createcanvas);
	lua_register(L, "setrendertarget", lua_setrendertarget);
	lua_register(L, "blit", lua_blit);

	// Gradients
	lua_register(L, "lineargradient", lua_lineargradient);
	lua_register(L, "fillrectgradient", lua_fillrectgradient);
	lua_register(L, "radialgradient", lua_radialgradient);

	// Text styling
	lua_register(L, "textstyle", lua_textstyle);
	lua_register(L, "measuretextblock", lua_measuretextblock);
}

void Lua_VideoReset(void) {
	// Clear overlay buffers
	ClearOverlaysIfAny();
	
	// Reset console state
	s_consoleVisible = false;
	s_consoleScrollOffset = 0;
	s_consoleScrollOffsetH = 0;
	s_luaConsoleCount = 0;
	
	// Reset drawing state
	s_drawStateStack.clear();
	DrawState defaultState;
	s_drawStateStack.push_back(defaultState);
	
	// Reset canvas handles
	s_nextCanvasHandle = 1;
	// Note: Canvas cleanup would happen here if needed
}

void Lua_VideoOnFrame(lua_State* L) {
	// Frame-based updates if needed
	// Currently no per-frame video logic needed
	(void)L;
}

void Lua_VideoSetRenderTarget(uint8* buffer) {
	currentXBuf = buffer;
}

// Getter functions for overlay buffers (needed by FCEU_LuaGui)
uint8* Lua_VideoGetOverlayBack(void) {
	return s_overlay_back;
}

uint8* Lua_VideoGetOverlayFront(void) {
	return s_overlay_front;
}

#endif // USE_LUA
