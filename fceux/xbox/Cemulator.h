/* FCE Ultra - NES/Famicom Emulator
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#pragma once

#include "filter/vfilter.h"
#include "fceux/types.h"
#include <Xaudio2.h>
#ifdef _XBOX
HRESULT InitUi(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS d3dpp);
HRESULT RenderXui(IDirect3DDevice9 *pDevice);
void UpdateUI();
#endif

//-----------------------------------------------------------------------------
// Path
//-----------------------------------------------------------------------------
#ifdef _XBOX
#define EFFECT_FILE "game:\\media\\effect.fx"
#define DEFAULT_GAME "game:\\roms\\sonic.ss"
#define X_FILE "game:\\media\\screen.x"
#define BG_FILE "game:\\media\\background.png"
#else
#define EFFECT_FILE "media\\effect.fx"
#define DEFAULT_GAME "roms\\sonic.sms"
#define X_FILE "media\\test.x"
#define BG_FILE "media\\bg.jpg"
#endif

#define g_dwTileWidth 1280
#define g_dwTileHeight 256
#define g_dwFrameWidth 1280
#define g_dwFrameHeight 720

enum VIDEO_VERTEX_FILTER {
	FullScreen, // Default
	TvScreen
};

class Cemulator {
  private:
	std::string defaut_rom;
	int mWidth;
	int mHeight;

	// Sound
	int snd_written;

	// --- A/V sync (dynamic audio rate control) ---
	float m_targetQueued;   // legacy: target queued blocks (kept for reference)
	float m_freqRatio;      // current XAudio2 frequency ratio
	float m_syncI;          // legacy: integral term for queued-block PI
	// New sample-accurate drift tracking
	double m_expectedSamples; // per-channel samples submitted
	double m_errI;            // integral term (seconds)
	
	// --- Audio stream state (per-ROM) ---
	std::vector<short> m_audioAccumulator; // interleaved L/R (L=R)
	int m_accCount;
	static const int AUDIO_POOL_SIZE = 4;   // Reduced from 8 to prevent latency buildup
	BYTE* m_audioPool[AUDIO_POOL_SIZE];
	int m_audioPoolHead;
	
	// SamplesPlayed baseline so PI uses a relative counter
	uint64_t m_samplesPlayedBase;
	
	// FDS audio delay - wait until first audio buffer is submitted before setting baseline
	bool m_fdsWaitingForFirstBuffer;
	
	// --- Audio latency control ---
	static const int kBlockSamples = 512;      // ~10.7ms @48k, crisp latency
	static const int kTargetBlocks = 2;        // target ~21ms total queued
	static const int kMaxBlocks    = 6;        // safety cap ~64ms
	static const int kSilenceThresh = 16;      // peak sample threshold for "silent" (conservative to avoid false positives)
	static const int kSilenceMsLatch = 300;    // consider "long silence" after 300ms (increased to avoid accidental latching on quick pauses)
	
	bool     m_inLongSilence;
	int      m_silenceSamplesAcc;
	
	// --- Pause-aware audio flags ---
	bool     m_prevPaused;                      // previous pause state (for edge detection)
	bool     m_pauseSilenceBypass;              // when true, do not trigger long-silence reset logic
	
	// --- ROM change tracking ---
	bool     m_inRomChange;
	int      m_resetCooldownMs;                // don't spam resets during transitions
	LARGE_INTEGER m_lastResetQPC;              // last time we hard-reset audio
	LARGE_INTEGER m_perfFreq;                  // performance counter frequency
	int      m_warmupBlocksToDrop;             // drop 1-2 blocks right after new ROM
	bool     m_isFDS;                          // true if current game is FDS
	
	void SyncAudioQueue();
	void ResetAudioStream(); // Reset audio stream on ROM switch (hard reset - frees pool)
	void SoftResetAudioStream(); // Soft reset - keeps pool allocations
	void HardRecreateSourceVoice(); // Recreate source voice completely
	void BeginRomChange(); // Called at start of ROM load
	void EndRomChange(); // Called after ROM load completes
	void PrimeAudioQueue(int blocks); // Prime audio queue with silent blocks
	void OnPauseStateChanged(bool paused); // Handle pause/unpause state transitions

	void SetSystemWidth(int w) { mWidth = w; };

	void SetSystemHeight(int h) { mHeight = h; };

	int GetSystemWidth() { return mWidth; };

	int GetSystemHeight() { return mHeight; };

	int GetWidth() { return 256; };
	int GetHeight() { return 240; };

	bool end;
	bool m_screenshotLatch; // prevents multiple screenshots per button press

	// Rewind system
	static const int REWIND_BUFFER_SIZE =
		300; // Store up to 300 states (~5 seconds at 60fps)
#ifndef REWIND_SAVE_INTERVAL
	// Save a rewind state ~every 100ms @60fps (fine-grained steps)
	static const int REWIND_SAVE_INTERVAL = 6;
#endif
#ifndef REWIND_INITIAL_DELAY_FRAMES
	// How long you must hold before auto-repeat kicks in (~166ms @60fps)
	static const int REWIND_INITIAL_DELAY_FRAMES = 10;
#endif

	struct RewindState {
		std::vector<uint8> stateData;
		bool isValid;
	};

	RewindState m_rewindBuffer[REWIND_BUFFER_SIZE];
	int m_rewindWritePos;	// Current write position in circular buffer
	int m_rewindCount;		// Number of valid states in buffer
	int m_frameCounter;		// Frame counter for periodic saves
	bool m_isRewinding;		// Whether we're currently rewinding
	int m_rewindFrameSkip;	// Counter to skip frames during rewind for
							// performance
	int m_rewindStartPos;	// Position where we started rewinding from
	int m_rewindHeldFrames; // Counter for how long LT has been held (for speed
							// ramping)
	bool m_isFastForwarding; // Whether we're currently fast-forwarding

	void SendExitSignal() { end = true; }

	void InitRewindBuffer();
	void ClearRewindBuffer();
	void SaveRewindState();
	bool LoadRewindState();
	void BlitARGBToTexture(const uint32_t *pixels);

  public:
	//-------------------------------------------------------------------------------------
	// 	Store Settings here
	//-------------------------------------------------------------------------------------
	struct Settings {
		// Sound
		int sound;
		int soundrate;
		int soundbufsize;
		int soundvolume;
		int soundtrianglevolume;
		int soundsquare1volume;
		int soundsquare2volume;
		int soundnoisevolume;
		int soundpcmvolume;
		int soundq;

		// Netplay
		int use_netplay;

		// video
		DWORD SelectedVertexFilter;
		DWORD SelectedGfxFilter;

		// controller
		DWORD gamepad_dpad_up, gamepad_dpad_down, gamepad_dpad_left,
			gamepad_dpad_right, gamepad_start, gamepad_back, gamepad_left_thumb,
			gamepad_right_thumb, gamepad_left_shoulder, gamepad_right_shoulder,
			gamepad_a, gamepad_b, gamepad_x, gamepad_y, gamepad_left_trigger,
			gamepad_right_trigger;

		// lua scripting
		enum LuaAutoloadMode {
			LUA_AUTO_ALL = 0,
			LUA_AUTO_ONE = 1,
			LUA_AUTO_NONE = 2
		};
		int luaAutoloadMode;		 // 0=All, 1=One specific script, 2=None
		char selectedLuaScript[256]; // UTF-8 filename (no path), 0-terminated

	} m_Settings;

  private:
	//-------------------------------------------------------------------------------------
	// 	Audio Synchronization
	//-------------------------------------------------------------------------------------
	class XAudio2_BufferNotify : public IXAudio2VoiceCallback {
	  public:
		HANDLE hBufferEndEvent;

		XAudio2_BufferNotify() {
			hBufferEndEvent = NULL;
			hBufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		}

		~XAudio2_BufferNotify() {
			CloseHandle(hBufferEndEvent);
			hBufferEndEvent = NULL;
		}

		STDMETHOD_(void, OnBufferEnd)(void *pBufferContext) {
			// Buffer is from pool - don't free, keep it hot for reuse
			// pBufferContext is actually a pool index cast to void*
			SetEvent(hBufferEndEvent);
		}

		// dummies:
		STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32 BytesRequired) {}
		STDMETHOD_(void, OnVoiceProcessingPassEnd)() {}
		STDMETHOD_(void, OnStreamEnd)() {}
		STDMETHOD_(void, OnBufferStart)(void *pBufferContext) {}
		STDMETHOD_(void, OnLoopEnd)(void *pBufferContext) {}
		STDMETHOD_(void, OnVoiceError)(void *pBufferContext, HRESULT Error) {};
	};
	XAudio2_BufferNotify XAudio2_Notifier; //XAudio2 event notifier

		
	//-------------------------------------------------------------------------------------
	// 	Framelimit Synchronization
	//-------------------------------------------------------------------------------------
	class FrameSkip {
	  public:
		LARGE_INTEGER ts_old;
		LARGE_INTEGER ts_new;
		DOUBLE ms_sec;
		int target_fps;

		FrameSkip() {
			ts_old;
			ts_new;
			ms_sec;
			LARGE_INTEGER ts_frequency;

			target_fps = 16666; // 16.66 ms

			QueryPerformanceFrequency(&ts_frequency);
			QueryPerformanceCounter(&ts_old);
			QueryPerformanceCounter(&ts_new);
			ms_sec = ts_frequency.QuadPart * 0.000001;
		};

		void Wait() {
			QueryPerformanceCounter(&ts_new);
			while (((ts_new.QuadPart - ts_old.QuadPart) / ms_sec) <
				   target_fps) {
				QueryPerformanceCounter(&ts_new);
			}
			ts_old = ts_new;
		};
	};
	FrameSkip fskip;

	//-------------------------------------------------------------------------------------
	// 	Gfx filter
	//-------------------------------------------------------------------------------------

	enum __gfxfilter {
		gfx_normal,
		gfx_hq2x,
		gfx_hq3x,
		gfx_2xsai,
		gfx_super2sai,
		gfx_superEagle
	};

	class GfxFilter {
	  private:
		// base w/h
		DWORD32 BaseW;
		DWORD32 BaseH;

		// current w/h
		DWORD32 CurrW;
		DWORD32 CurrH;

		// texture a applique le filtre
		LPDIRECT3DTEXTURE9 mtext;
		// bit de la texture
		unsigned int *data;
		DWORD pitch;

		// filtre utilis� en cours
		DWORD32 selFilter;

		// backing buffer for filtered output
		unsigned int *filteredBuffer;

		HRESULT CreateTexture() {
			// Textures are now managed by ring buffer, no need to create here
			// Allocate temporary buffer for filtered output if needed
			if (filteredBuffer) {
				free(filteredBuffer);
				filteredBuffer = NULL;
			}
			if (CurrW > 0 && CurrH > 0) {
				filteredBuffer = (unsigned int*)malloc(CurrW * CurrH * sizeof(unsigned int));
				if (!filteredBuffer)
					return E_OUTOFMEMORY;
			}
			return S_OK;
		}

	  public:
		GfxFilter() : BaseW(0), BaseH(0), CurrW(0), CurrH(0),
					 mtext(NULL), data(NULL), pitch(0),
					 selFilter((DWORD32)-1), filteredBuffer(NULL) {}
		
		~GfxFilter() {
			if (filteredBuffer) {
				free(filteredBuffer);
				filteredBuffer = NULL;
			}
		}

		void SetTextureDimension(DWORD32 width, DWORD32 height) {
			BaseW = width;
			BaseH = height;
		}

		// Get actual filtered texture dimensions (for texel size calculation)
		DWORD32 GetCurrentWidth() const { return CurrW; }
		DWORD32 GetCurrentHeight() const { return CurrH; }

		void UpdateFilter(unsigned int *bitmap) {
			// Apply filter to temporary buffer (not directly to texture)
			// The ring buffer system will upload via UploadNESFrame()
			if (!filteredBuffer)
				return;

			// Apply filter to buffer
			switch (selFilter) {
			case gfx_hq2x:
				filter_hq2x_32((unsigned char *)bitmap, BaseW << 2,
							   (unsigned char *)filteredBuffer, BaseW, BaseH);
				break;
			case gfx_hq3x:
				filter_hq3x_32((unsigned char *)bitmap, BaseW << 2,
							   (unsigned char *)filteredBuffer, BaseW, BaseH);
				break;
			case gfx_2xsai:
				filter_Std2xSaI_ex8((unsigned char *)bitmap, BaseW << 2,
									(unsigned char *)filteredBuffer, BaseW, BaseH);
				break;
			case gfx_super2sai:
				filter_Super2xSaI_ex8((unsigned char *)bitmap, BaseW << 2,
									  (unsigned char *)filteredBuffer, BaseW, BaseH);
				break;
			case gfx_superEagle:
				filter_SuperEagle_ex8((unsigned char *)bitmap, BaseW << 2,
									  (unsigned char *)filteredBuffer, BaseW, BaseH);
				break;
			default:
				// Copy for normal filter (no scaling)
				memcpy(filteredBuffer, bitmap, BaseW * BaseH * sizeof(unsigned int));
				break;
			}
		}
		
		// Get the filtered output buffer for uploading
		unsigned int* GetFilteredBuffer() const { return filteredBuffer; }

		void UseFilter(unsigned int filter) {
			if (filter != selFilter) {
				selFilter = filter;
				switch (filter) {
				case gfx_normal: {
					CurrH = BaseH;
					CurrW = BaseW;
					break;
				}
				case gfx_hq2x:
				case gfx_2xsai:
				case gfx_super2sai:
				case gfx_superEagle: {
					CurrH = BaseH * 2;
					CurrW = BaseW * 2;
					break;
				}
				case gfx_hq3x: {
					CurrH = BaseH * 3;
					CurrW = BaseW * 3;
					break;
				}
				};

				CreateTexture();
			}
		};
	};

  public:
	GfxFilter gfx_filter;
	Cemulator(void);

	// render emu or ui
	bool RenderEmulation;

	// PC SIDE
	HRESULT InitVideo();
	HRESULT InitAudio();
	HRESULT InitInput();
	HRESULT CloseVideo();
	HRESULT CloseAudio();
	HRESULT CloseInput();
	HRESULT Finish() {
		SendExitSignal();
		return S_OK;
	};

	HRESULT LoadGame(std::string name, bool restart);

	// SYSTEM SIDE
	HRESULT InitSystem();
	HRESULT CloseSystem();

	void Render();
	void UpdateVideo();
	void UpdateAudio(int *snd, int sndsize);
	void UpdateInput();

	void SetStartGame(std::string gamename) { defaut_rom = gamename; }
	// Run
	HRESULT Run();

	// Used on ui
	void SetVertexFilter(int i) { m_Settings.SelectedVertexFilter = i; }

	DWORD GetVertexFilter() { return m_Settings.SelectedVertexFilter; }

	bool IsRewinding() { return m_isRewinding; };
	bool IsFastForwarding() { return m_isFastForwarding; };
};
