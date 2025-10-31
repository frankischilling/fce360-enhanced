#pragma once

#include <Xaudio2.h>
#include "filter/vfilter.h"
#ifdef _XBOX
HRESULT InitUi(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS d3dpp);
HRESULT RenderXui(IDirect3DDevice9* pDevice);
void UpdateUI();
#endif

//-----------------------------------------------------------------------------
// Path
//-----------------------------------------------------------------------------
#ifdef _XBOX
#	define EFFECT_FILE "game:\\media\\effect.fx"
#	define DEFAULT_GAME "game:\\roms\\sonic.ss"
#	define X_FILE "game:\\media\\screen.x"
#	define BG_FILE "game:\\media\\background.png"
#else
#	define EFFECT_FILE "media\\effect.fx"
#	define DEFAULT_GAME "roms\\sonic.sms"
#	define X_FILE "media\\test.x"
#	define BG_FILE "media\\bg.jpg"
#endif

#define g_dwTileWidth   1280
#define g_dwTileHeight  256
#define g_dwFrameWidth  1280
#define g_dwFrameHeight 720

enum VIDEO_VERTEX_FILTER{
	FullScreen,//Default
	TvScreen
};

class Cemulator
{
private:
	std::string defaut_rom;
	int mWidth;
	int mHeight;

	//Sound
	int snd_written;

	void SetSystemWidth(int w)
	{
		mWidth=w;
	};

	void SetSystemHeight(int h)
	{
		mHeight=h;
	};

	int GetSystemWidth()
	{
		return mWidth;
	};

	int GetSystemHeight()
	{
		return mHeight;
	};

	int GetWidth()
	{
		return 256;
	};
	int GetHeight()
	{
		return 240;
	};

	bool end;

	void SendExitSignal()
	{
		end=true;
	}
public:
//-------------------------------------------------------------------------------------
// 	Store Settings here
//-------------------------------------------------------------------------------------
	struct Settings{
		//Sound
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

		//Netplay
		int use_netplay;
		
		//video
		DWORD SelectedVertexFilter;
		DWORD SelectedGfxFilter;
		
		//controller
		DWORD gamepad_dpad_up, gamepad_dpad_down, gamepad_dpad_left, gamepad_dpad_right, 
			gamepad_start, gamepad_back, 
			gamepad_left_thumb, gamepad_right_thumb, 
			gamepad_left_shoulder, gamepad_right_shoulder, 
			gamepad_a, gamepad_b, gamepad_x, gamepad_y,
			gamepad_left_trigger, gamepad_right_trigger;
			
	} m_Settings;
private:
//-------------------------------------------------------------------------------------
// 	Audio Synchronization
//-------------------------------------------------------------------------------------
	class XAudio2_BufferNotify : public IXAudio2VoiceCallback
	{
	public:
		HANDLE hBufferEndEvent;

		XAudio2_BufferNotify() {
			hBufferEndEvent = NULL;
			hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		}

		~XAudio2_BufferNotify() {
			CloseHandle( hBufferEndEvent );
			hBufferEndEvent = NULL;
		}

		STDMETHOD_( void, OnBufferEnd ) ( void *pBufferContext ) {
			SetEvent( hBufferEndEvent );
			free(pBufferContext);
		}

		// dummies:
		STDMETHOD_( void, OnVoiceProcessingPassStart ) ( UINT32 BytesRequired ) {}
		STDMETHOD_( void, OnVoiceProcessingPassEnd ) () {}
		STDMETHOD_( void, OnStreamEnd ) () {}
		STDMETHOD_( void, OnBufferStart ) ( void *pBufferContext ) {}
		STDMETHOD_( void, OnLoopEnd ) ( void *pBufferContext ) {}
		STDMETHOD_( void, OnVoiceError ) ( void *pBufferContext, HRESULT Error ) {};
	};
	XAudio2_BufferNotify XAudio2_Notifier; //XAudio2 event notifier

		
//-------------------------------------------------------------------------------------
// 	Framelimit Synchronization
//-------------------------------------------------------------------------------------
	class FrameSkip
	{
	public:
		LARGE_INTEGER ts_old;
		LARGE_INTEGER ts_new;
		DOUBLE ms_sec;
		int target_fps;

		FrameSkip(){
			ts_old;
			ts_new;
			ms_sec;
			LARGE_INTEGER ts_frequency;

			target_fps = 16666; //16.66 ms
			
			QueryPerformanceFrequency(&ts_frequency);
			QueryPerformanceCounter(&ts_old);
			QueryPerformanceCounter(&ts_new);
			ms_sec = ts_frequency.QuadPart* 0.000001;
		};

		void Wait()
		{
			QueryPerformanceCounter(&ts_new);
			while(((ts_new.QuadPart - ts_old.QuadPart)/ms_sec)<target_fps)
			{
				QueryPerformanceCounter(&ts_new);
			}
			ts_old = ts_new;
		};
	};
	FrameSkip fskip;

	
//-------------------------------------------------------------------------------------
// 	Gfx filter
//-------------------------------------------------------------------------------------	
	
	enum __gfxfilter{
		gfx_normal,
		gfx_hq2x,
		gfx_hq3x,
		gfx_2xsai,
		gfx_super2sai,
		gfx_superEagle
	};

	class GfxFilter
	{
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
		unsigned int * data;
		DWORD pitch;

		// filtre utilisé en cours
		DWORD32 selFilter;

		HRESULT CreateTexture()
		{
			extern LPDIRECT3DDEVICE9 g_pd3dDevice;
			extern IDirect3DTexture9 * g_texture;
			HRESULT hr = D3DXCreateTexture(
				g_pd3dDevice, CurrW,
				CurrH, D3DX_DEFAULT, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED,
				&g_texture
			);

			if(FAILED(hr))
				return hr;

			RECT d3dr;

			d3dr.left=0;
			d3dr.top=0;
			d3dr.right=CurrW;
			d3dr.bottom=CurrH;

			D3DLOCKED_RECT texture_info;
			
			g_texture->LockRect( 0,  &texture_info, &d3dr, NULL );

			pitch = (DWORD) texture_info.Pitch;
			data = ((unsigned int*) texture_info.pBits);

			g_texture->UnlockRect(0);

			return S_OK;
		}

	public:
		GfxFilter(){
			selFilter = -1;//Will be updated later
		};

		void SetTextureDimension(DWORD32 width, DWORD32 height)
		{
			BaseW = width;
			BaseH = height;
		}
		
		void UpdateFilter(unsigned int * bitmap){
			switch (selFilter)
			{	
				case gfx_hq2x:
					filter_hq2x_32((unsigned char *)bitmap, BaseW<<2, (unsigned char *)data, BaseW, BaseH);
					break;
				case gfx_hq3x:
					filter_hq3x_32((unsigned char *)bitmap, BaseW<<2, (unsigned char *)data, BaseW, BaseH);
					break;
				case gfx_2xsai:
					filter_Std2xSaI_ex8((unsigned char *)bitmap, BaseW<<2, (unsigned char *)data, BaseW, BaseH);
					break;
				case gfx_super2sai:
					filter_Super2xSaI_ex8((unsigned char *)bitmap, BaseW<<2, (unsigned char *)data, BaseW, BaseH);
					break;
				case gfx_superEagle:
					filter_SuperEagle_ex8((unsigned char *)bitmap, BaseW<<2, (unsigned char *)data, BaseW, BaseH);
					break;
				default:
					memcpy(data,bitmap, (BaseW * BaseH * 4) );
					break;
			}	
			
		}
		
		void UseFilter(unsigned int filter){
			if(filter != selFilter){
				selFilter = filter;
				switch(filter){
					case gfx_normal:
					{
						CurrH = BaseH;
						CurrW = BaseW;
						break;
					}
					case gfx_hq2x:
					case gfx_2xsai:
					case gfx_super2sai:
					case gfx_superEagle:
					{
						CurrH = BaseH*2;
						CurrW = BaseW*2;
						break;
					}
					case gfx_hq3x:
					{
						CurrH = BaseH*3;
						CurrW = BaseW*3;
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

	//render emu or ui
	bool RenderEmulation;

	//PC SIDE
	HRESULT InitVideo();
	HRESULT InitAudio();
	HRESULT InitInput();
	HRESULT CloseVideo();
	HRESULT CloseAudio();
	HRESULT CloseInput();
	HRESULT Finish(){
		SendExitSignal();
		return S_OK;
	};

	HRESULT LoadGame(std::string name, bool restart);

	//SYSTEM SIDE
	HRESULT InitSystem();
	HRESULT CloseSystem();

	void Render();
	void UpdateVideo();
	void UpdateAudio(int * snd, int sndsize);
	void UpdateInput();

	void SetStartGame(std::string gamename)
	{
		defaut_rom=gamename;
	}
	//Run
	HRESULT Run();

	//Used on ui
	void SetVertexFilter(int i){
		m_Settings.SelectedVertexFilter = i;
	}

	DWORD GetVertexFilter(){
		return m_Settings.SelectedVertexFilter;
	}
};
