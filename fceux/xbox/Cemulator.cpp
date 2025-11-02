#include "xbox/fceusupport.h"
#include "stdafx.h"
#include <xtl.h>
#include <fxl.h>
#include <xui.h>
#include <string>
#include <vector>
#include <xaudio2.h>

#include "Cemulator.h"
#include "audio.h"
#include "input.h"
#include "xconfig.h"
#include "config_reader.h"
#include "net360.h"
#include "fceux/emufile.h"
#include "fceux/drawing.h"
#ifdef USE_LUA
#include "fceux/fceulua.h"
#endif
#include "zlib.h"

//-----------------------------------------------------------------------------
// Performance: Disable printf spam in retail builds
//-----------------------------------------------------------------------------
#if !defined(DEBUG) && !defined(_DEBUG)
#undef printf
#define printf(...) ((void)0)
#endif

// Log budget system to prevent excessive debug output
static int g_log_budget = 200; // print at most 200 lines total per run
#define LOGF(...) do { if (g_log_budget > 0) { --g_log_budget; printf(__VA_ARGS__); } } while(0)

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
IDirect3DTexture9 * g_texture = NULL;
IDirect3DTexture9 * g_bg_texture = NULL;
D3DPRESENT_PARAMETERS g_d3dpp;
LPD3DXEFFECT g_effect = NULL; //handle to D3DXEffect

// Rendering surfaces and textures
#ifdef _XBOX
D3DSurface*                 m_pBackBuffer;
D3DSurface*                 m_pDepthBuffer;
D3DTexture*                 m_pFrontBuffer;
#endif

//-------------------------------------------------------------------------------------
// Shader
//-------------------------------------------------------------------------------------
IDirect3DVertexDeclaration9* g_pVertexDecl;   // Vertex format decl

D3DXMATRIX g_matWorld;
D3DXMATRIX g_matProj;
D3DXMATRIX g_matView;
D3DXMATRIX g_matWorldViewProjection;

//-------------------------------------------------------------------------------------
// Audio
//-------------------------------------------------------------------------------------
#define SOUND_BUFFER_SIZE 5000
#define BLOCK_FRAMES 512    // ~10.7ms @48k, lower latency for snappier controls
#define MAX_BLOCKS_QUEUED 2  // Cap queue depth (still stable on 360)

//fceux bitmap
uint8 * bitmap;
//bitmap with good color ARGB
unsigned int * nesBitmap;
//sound buffer
unsigned int * g_sound_buffer;

IXAudio2* g_pXAudio2 = NULL;
IXAudio2MasteringVoice* g_pMasteringVoice = NULL;
IXAudio2SourceVoice* g_pSourceVoice = NULL;
WAVEFORMATEXTENSIBLE wfx;
XAUDIO2_BUFFER g_SoundBuffer;

//

float ftime=0.f;

//-------------------------------------------------------------------------------------
// Input
//-------------------------------------------------------------------------------------
GAMEPAD Gamepads[XUSER_MAX_COUNT];
uint32 powerpadbuf=0;

//-------------------------------------------------------------------------------------
// TEXTURE
//-------------------------------------------------------------------------------------
struct TEXTURED
{
    FLOAT x, y, z;      // The untransformed, 3D position for the vertex
    FLOAT u,v;			// The texture coordonate
};

#define D3DFVF_TEXTURED (D3DFVF_XYZ|D3DFVF_TEX1)

static const D3DVERTEXELEMENT9 g_ElementsTextured[4] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

struct TEXTURED g_VerticesTextured[] =
{
	//square
	{ -1.0f, -1.0f, 0.0f,  0.0f,  1.0f },//1
	{ -1.0f,  1.0f, 0.0f,  0.0f,  0.0f },//2
	{  1.0f,  1.0f, 0.0f,  1.0f,  0.0f },//4
	{  1.0f, -1.0f, 0.0f,  1.0f,  1.0f }//3
};

static const D3DRECT g_tiles[3] = 
{
    {             0,              0,  g_dwTileWidth,  g_dwTileHeight },
    {             0, g_dwTileHeight,  g_dwTileWidth, g_dwTileHeight * 2 },
    {             0, g_dwTileHeight * 2,  g_dwTileWidth, g_dwFrameHeight },
};

//-------------------------------------------------------------------------------------
// TEXTURE
//-------------------------------------------------------------------------------------
D3DXHANDLE  g_MaterialAmbientColor;
D3DXHANDLE  g_MaterialDiffuseColor;
D3DXHANDLE  g_mWorldViewProjection;
D3DXHANDLE  g_MeshTexture;
D3DXHANDLE  g_bgTexture;
D3DXHANDLE  g_technique_bg;
D3DXHANDLE  g_technique_model;
D3DXHANDLE  g_technique_model_tv;
D3DXHANDLE  g_technique_model_fullscreen;

D3DXHANDLE  g_TexelSize;
D3DXHANDLE  g_fTime;
LPD3DXBUFFER materialBuffer;
DWORD numMaterials;            // Note: DWORD is a typedef for unsigned long
LPD3DXMESH mesh;

float g_pTexelSize[2];

//-------------------------------------------------------------------------------------
// Cemulator
//-------------------------------------------------------------------------------------
Cemulator::Cemulator(void)
{
	end=false;
	RenderEmulation = false;//Display xui at first
	m_Settings.SelectedVertexFilter = FullScreen;
	snd_written = 0;
	ftime = 0.0f;
	m_screenshotLatch = false;
	InitRewindBuffer();
}

HRESULT Cemulator::InitVideo(){
//-------------------------------------------------------------------------------------
// Create d3d device
//-------------------------------------------------------------------------------------
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !g_pD3D )
        return E_FAIL;

//-------------------------------------------------------------------------------------
// Set the system width 
//-------------------------------------------------------------------------------------
    ZeroMemory( &g_d3dpp, sizeof(g_d3dpp) );
	// Set up the structure used to create the D3DDevice.
	XVIDEO_MODE VideoMode;
	ZeroMemory( &VideoMode, sizeof( VideoMode ) );
	XGetVideoMode( &VideoMode );
	BOOL bEnable720p = ( VideoMode.dwDisplayHeight >= 720 ) ? TRUE : FALSE;
	SetSystemWidth(( bEnable720p ) ? 1280 : 640);
	SetSystemHeight(( bEnable720p ) ?  720 : 480);
	
//-------------------------------------------------------------------------------------
// MSAA surface
//-------------------------------------------------------------------------------------
	g_d3dpp.BackBufferWidth = GetSystemWidth();
	g_d3dpp.BackBufferHeight = GetSystemHeight();
    g_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    g_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    g_d3dpp.MultiSampleQuality = 0;
    g_d3dpp.BackBufferCount = 0;
    g_d3dpp.EnableAutoDepthStencil = FALSE;
    g_d3dpp.DisableAutoBackBuffer = TRUE;
    g_d3dpp.DisableAutoFrontBuffer = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	//g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

//-------------------------------------------------------------------------------------
// Create device FIRST (before creating surfaces - correct order)
// Note: D3DCREATE_BUFFER_3_FRAMES not available on Xbox 360 - command buffer depth is managed internally
// Using standard flags for Xbox 360
//-------------------------------------------------------------------------------------
	if( FAILED( g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice ) ) )
	{
		printf("CreateDevice failed\n");
        return E_FAIL;
	}

//-------------------------------------------------------------------------------------
// Create render surfaces AFTER device creation
//-------------------------------------------------------------------------------------
	D3DSURFACE_PARAMETERS params = {0};
	g_pd3dDevice->CreateRenderTarget( g_dwTileWidth, g_dwTileHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, 0, &m_pBackBuffer, &params );
	params.Base = m_pBackBuffer->Size / GPU_EDRAM_TILE_SIZE;
	params.HierarchicalZBase = D3DHIZFUNC_GREATER_EQUAL;

	g_pd3dDevice->CreateDepthStencilSurface( g_dwTileWidth, g_dwTileHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, 0, &m_pDepthBuffer, &params );
	g_pd3dDevice->CreateTexture( g_dwFrameWidth, g_dwFrameHeight, 1, 0, D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer, NULL );
//-------------------------------------------------------------------------------------
// Create the buffer, and load the effect from the file.
//-------------------------------------------------------------------------------------
	HRESULT Result;
	ID3DXEffectCompiler* pCompiler = NULL;
	ID3DXBuffer* pCompiledData = NULL;

//-------------------------------------------------------------------------------------
// Create a ID3DXEffectCompiler interface for the effect that was just loaded.
//-------------------------------------------------------------------------------------
	Result = D3DXCreateEffectCompilerFromFileA(
         EFFECT_FILE,
         NULL,
         NULL,
         0,
         &pCompiler,
         NULL
	);
	if (FAILED(Result))
    {
		printf( "D3DXCreateEffectCompiler FAILED\n" );
        return Result;
    }

    // Compile the effect by using the ID3DXEffectCompiler interface, and then release the compiler.
    Result = pCompiler->CompileEffect(
		0,  // No debug flags in retail (was D3DXSHADER_DEBUG)
		&pCompiledData, 
		NULL
	);

    pCompiler->Release();
    if (FAILED(Result))
    {
		printf( "CompileEffect FAILED\n" );
        return Result;
    }

    // Create the effect that was just compiled.
    Result = D3DXCreateEffect(g_pd3dDevice, (DWORD*)pCompiledData->GetBufferPointer(), pCompiledData->GetBufferSize(),
                                        NULL, NULL, 0, NULL, &g_effect, NULL);

//-------------------------------------------------------------------------------------
// Create the model
//-------------------------------------------------------------------------------------
	HRESULT hr=D3DXLoadMeshFromXA(X_FILE, D3DXMESH_SYSTEMMEM, 
                             g_pd3dDevice, NULL, 
                             &materialBuffer,NULL, &numMaterials, 
                             &mesh );
//-------------------------------------------------------------------------------------
// Load the bg
//-------------------------------------------------------------------------------------
	D3DXCreateTextureFromFileA(g_pd3dDevice,BG_FILE,&g_bg_texture);

//-------------------------------------------------------------------------------------
// Create SMS RenderTexture
//-------------------------------------------------------------------------------------
	D3DXCreateTexture(
		g_pd3dDevice, GetWidth(),
		GetHeight(), D3DX_DEFAULT, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED,
		&g_texture
	);

//-------------------------------------------------------------------------------------
// Create VB
//-------------------------------------------------------------------------------------
	g_pd3dDevice->CreateVertexDeclaration( g_ElementsTextured, &g_pVertexDecl );

    if( FAILED( g_pd3dDevice->CreateVertexBuffer( 4*sizeof(TEXTURED),
                                                  D3DUSAGE_WRITEONLY, 
                                                  NULL,
                                                  D3DPOOL_MANAGED, 
                                                  &g_pVB, 
                                                  NULL ) ) )
	{
		printf("CreateVertexBuffer failed\n");
        return E_FAIL;
	}

	TEXTURED* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_VerticesTextured, 4*sizeof(TEXTURED) );
    g_pVB->Unlock();

//-------------------------------------------------------------------------------------
// Param from effectfile
//-------------------------------------------------------------------------------------	
	g_technique_bg = g_effect->GetTechniqueByName("RenderFullScreen");
	g_technique_model = g_effect->GetTechniqueByName("RenderModel");
	g_technique_model_tv = g_effect->GetTechniqueByName("RenderModelTv");
	g_technique_model_fullscreen = g_effect->GetTechniqueByName("RenderModelFullScreen");

	g_mWorldViewProjection = g_effect->GetParameterByName(NULL,  "g_mWorldViewProjection" );
	g_MaterialAmbientColor = g_effect->GetParameterByName(NULL,  "g_MaterialAmbientColor" );
	g_MaterialDiffuseColor = g_effect->GetParameterByName(NULL,  "g_MaterialDiffuseColor" );
	g_mWorldViewProjection = g_effect->GetParameterByName(NULL,  "g_mWorldViewProjection" );
	g_MeshTexture = g_effect->GetParameterByName(NULL,  "g_MeshTexture" );
	g_bgTexture  = g_effect->GetParameterByName(NULL,  "g_bgTexture" );
	g_TexelSize = g_effect->GetParameterByName(NULL,  "g_TexelSize" );
	g_fTime = g_effect->GetParameterByName(NULL,  "g_fTime" );

//-------------------------------------------------------------------------------------
// Default Renderstates
//-------------------------------------------------------------------------------------
	g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	return S_OK;
};


void Cemulator::UpdateVideo(){
	/*
//-------------------------------------------------------------------------------------
// Refresh texture cache
//-------------------------------------------------------------------------------------
	RECT d3dr;

	d3dr.left=0;
	d3dr.top=0;
	d3dr.right=GetWidth();
	d3dr.bottom=GetHeight();

	D3DLOCKED_RECT texture_info;
	
	g_texture->LockRect( 0,  &texture_info, &d3dr, NULL );
	g_texture->UnlockRect(0);

//-------------------------------------------------------------------------------------
// Initialise view
//-------------------------------------------------------------------------------------
	// Initialize the world matrix
	D3DXMatrixIdentity(&g_matWorld);
	D3DXMATRIX scale;

    // Initialize the projection matrix
    FLOAT fAspect = 16.0f / 9.0f; 
	//FLOAT fAspect = 1.0f;
	D3DXMatrixPerspectiveFovLH(&g_matProj, D3DX_PI/4, fAspect, 1.0f, 200.0f );

    // Initialize the view matrix
	D3DXVECTOR3 vEyePt    = D3DXVECTOR3( 0.f, 0.f, -8.f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.f, 0.f, 0.f );
    D3DXVECTOR3 vUp       = D3DXVECTOR3( 0.f, 1.f, 1.f );
	D3DXMatrixLookAtLH(&g_matView,&vEyePt, &vLookatPt, &vUp );

	g_matWorldViewProjection = g_matWorld * g_matView * g_matProj;
	*/
};

HRESULT Cemulator::InitAudio()
{
//-------------------------------------------------------------------------------------
// Initialise Audio
//-------------------------------------------------------------------------------------	
	HRESULT hr;
	if( FAILED( hr = XAudio2Create( &g_pXAudio2, 0 ) ) )
    {
        printf( "Failed to init XAudio2 engine: %#X\n", hr );
        return E_FAIL;
    }

//-------------------------------------------------------------------------------------
// Create a mastering voice
//-------------------------------------------------------------------------------------	
    if( FAILED( hr = g_pXAudio2->CreateMasteringVoice( &g_pMasteringVoice ) ) )
    {
		printf( "Failed creating mastering voice: %#X\n", hr );
        return E_FAIL;
    }

//-------------------------------------------------------------------------------------
// Create source voice
//-------------------------------------------------------------------------------------	
	WAVEFORMATEXTENSIBLE wfx;
	memset(&wfx, 0, sizeof(WAVEFORMATEXTENSIBLE));
	
	wfx.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE ;
	wfx.Format.nSamplesPerSec       = m_Settings.soundrate;//48000 by default
	wfx.Format.nChannels            = 2;
	wfx.Format.wBitsPerSample       = 16;
	wfx.Format.nBlockAlign          = wfx.Format.nChannels*wfx.Format.wBitsPerSample/8;
	wfx.Format.nAvgBytesPerSec      = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
	wfx.Format.cbSize               = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
	wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
	wfx.dwChannelMask               = SPEAKER_STEREO;
	wfx.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

//-------------------------------------------------------------------------------------
//	Source voice
//-------------------------------------------------------------------------------------
	// Only use NOSRC if we're at 48k (mastering voice default)
	// Otherwise, let XAudio2 resample to avoid issues
	UINT32 voiceFlags = (m_Settings.soundrate == 48000) ? (XAUDIO2_VOICE_NOSRC | XAUDIO2_VOICE_NOPITCH)
	                                                    : (XAUDIO2_VOICE_NOPITCH);
	
	if(FAILED( g_pXAudio2->CreateSourceVoice(&g_pSourceVoice,(WAVEFORMATEX*)&wfx, voiceFlags, 1.0f, &XAudio2_Notifier)	))
	{
		printf("CreateSourceVoice failed\n");
		return E_FAIL;
	}

//-------------------------------------------------------------------------------------
// Start sound
//-------------------------------------------------------------------------------------	
	if ( FAILED(g_pSourceVoice->Start( 0 ) ) )
	{
		printf("g_pSourceVoice failed\n");
		return E_FAIL;
	}

	return S_OK;
};

void Cemulator::UpdateAudio(int * snd, int sndsize)
{
	if(sndsize <= 0 || !g_pSourceVoice)
		return;

//-------------------------------------------------------------------------------------
// Audio: Convert mono 16-bit (signed) -> interleaved stereo 16-bit (L=R)
// Use signed samples to avoid DC bias and XAudio2 limiter overhead
// Accumulate samples across frames since NES produces ~735-800 samples/frame
//-------------------------------------------------------------------------------------	
	static std::vector<short> accumulator;  // Accumulate samples across frames
	static int accCount = 0;
	
	// Accumulate incoming samples
	for (int i = 0; i < sndsize; ++i) {
		short sample = (short)(snd[i] & 0xFFFF);    // signed 16-bit!
		
		// Grow accumulator if needed
		if (accCount * 2 + 1 >= (int)accumulator.size()) {
			accumulator.resize((accCount + sndsize) * 2);  // Ensure room for stereo
		}
		
		accumulator[accCount * 2 + 0] = sample;  // Left channel
		accumulator[accCount * 2 + 1] = sample;  // Right channel (same as left)
		++accCount;
	}
	
	// Submit when we have a full block
	// Use a pool of reusable buffers to avoid malloc/free churn
	static const int POOL_SIZE = 8;
	static BYTE* pool[POOL_SIZE] = {0};
	static int poolHead = 0;
	
	while (accCount >= BLOCK_FRAMES) {
		// Check queue state - flush only if strictly greater than threshold
		XAUDIO2_VOICE_STATE st = {0};
		g_pSourceVoice->GetState(&st);
		if (st.BuffersQueued > MAX_BLOCKS_QUEUED) {  // Strictly greater, not >=
			// Drop backlog to keep A/V in sync (prefer a tiny click over seconds of lag)
			g_pSourceVoice->FlushSourceBuffers();
		}

		// Get buffer from pool (reuse instead of malloc/free per block)
		const size_t bytes = BLOCK_FRAMES * 2 /*stereo*/ * sizeof(short);
		if (!pool[poolHead]) {
			pool[poolHead] = (BYTE*)malloc(bytes);
			if (!pool[poolHead])
				break;  // Can't allocate, skip this block
		}
		BYTE* buf = pool[poolHead];
		
		// Copy BLOCK_FRAMES worth of stereo samples
		memcpy(buf, &accumulator[0], bytes);

		XAUDIO2_BUFFER xb = {0};
		xb.AudioBytes = (UINT32)bytes;
		xb.pAudioData = buf;
		xb.pContext   = (void*)(intptr_t)poolHead;  // Store pool index instead of pointer
		xb.Flags      = 0;     // streaming, NOT end-of-stream

		if (SUCCEEDED(g_pSourceVoice->SubmitSourceBuffer(&xb))) {
			// Advance pool head (buffer will be reused when callback fires)
			poolHead = (poolHead + 1) % POOL_SIZE;
			
			// Remove submitted samples from accumulator
			int remaining = accCount - BLOCK_FRAMES;
			if (remaining > 0) {
				// Shift remaining samples to front
				memmove(&accumulator[0], &accumulator[BLOCK_FRAMES * 2], remaining * 2 * sizeof(short));
			}
			accCount = remaining;
		} else {
			break;  // Submission failed, stop trying (buffer stays in pool)
		}
	}
	return;
};

HRESULT Cemulator::InitInput()
{
//-------------------------------------------------------------------------------------
// Init input
//-------------------------------------------------------------------------------------	
	return S_OK;
}

void Cemulator::UpdateInput()
{
//-------------------------------------------------------------------------------------
// Get input from all the gamepads
//-------------------------------------------------------------------------------------	
    Input::GetInput( Gamepads );

	unsigned char pad[4];
    memset(pad, 0, sizeof(char) * 4);

	for( DWORD dwUser = 0; dwUser < 2; dwUser++ )
	{
		if(!FCEUI_EmulationPaused())
		{
			if(Gamepads[dwUser].fY1 > 0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_up;

			if(Gamepads[dwUser].fY1 < -0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_down;

			if(Gamepads[dwUser].fX1 > 0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_right;

			if(Gamepads[dwUser].fX1 < -0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_left;

		// Use wButtons instead of wLastButtons for better frame cadence (avoids frame-timing weirdness)
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_UP)
			pad[dwUser] |= m_Settings.gamepad_dpad_up;
			
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
			pad[dwUser] |= m_Settings.gamepad_dpad_down;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			pad[dwUser] |= m_Settings.gamepad_dpad_left;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			pad[dwUser] |= m_Settings.gamepad_dpad_right;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_A)
			pad[dwUser] |= m_Settings.gamepad_a;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_B)
			pad[dwUser] |= m_Settings.gamepad_b;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_X)
			pad[dwUser] |= m_Settings.gamepad_x;
		
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_Y)
			pad[dwUser] |= m_Settings.gamepad_y;
			
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
			pad[dwUser] |= m_Settings.gamepad_left_thumb;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
			pad[dwUser] |= m_Settings.gamepad_right_thumb;

		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
			pad[dwUser] |= m_Settings.gamepad_left_shoulder;
		
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
			pad[dwUser] |= m_Settings.gamepad_right_shoulder;
				
		// When LT is serving rewind, don't pass it through to the NES pad
		if(!m_isRewinding && Gamepads[dwUser].bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			pad[dwUser] |= m_Settings.gamepad_left_trigger;
			
		if(Gamepads[dwUser].bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			pad[dwUser] |= m_Settings.gamepad_right_trigger;
			
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_START)
			pad[dwUser] |= m_Settings.gamepad_start;
		
		if(Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_BACK)
			pad[dwUser] |= m_Settings.gamepad_back;
		}
	}
//-------------------------------------------------------------------------------------
// Set input from all the gamepads
//-------------------------------------------------------------------------------------	
	powerpadbuf = pad[0] | pad[1] << 8 ;//| pad[2] << 16 | pad[3] << 24;;
};

HRESULT Cemulator::InitSystem()
{
//-------------------------------------------------------------------------------------
// Set up rendering texture
//-------------------------------------------------------------------------------------
	nesBitmap = (unsigned int *)malloc(256 * 240 * sizeof(unsigned int));
//-------------------------------------------------------------------------------------
// Set up sound
//-------------------------------------------------------------------------------------
	g_sound_buffer = (unsigned int *)malloc(SOUND_BUFFER_SIZE * sizeof(unsigned int));
	memset(g_sound_buffer,0,SOUND_BUFFER_SIZE);

//-------------------------------------------------------------------------------------
// Read config
//-------------------------------------------------------------------------------------
	extern Config fcecfg;

	//Load and save configuration
	ReadConfig();

	//Fetch configuration
	fcecfg.Find("sound","enabled", m_Settings.sound);
	fcecfg.Find("sound","rate", m_Settings.soundrate);
	fcecfg.Find("sound","bufsize", m_Settings.soundbufsize);
	fcecfg.Find("sound","volume", m_Settings.soundvolume);
	fcecfg.Find("sound","trianglevolume", m_Settings.soundtrianglevolume);
	fcecfg.Find("sound","square1volume", m_Settings.soundsquare1volume);
	fcecfg.Find("sound","square2volume", m_Settings.soundsquare2volume);
	fcecfg.Find("sound","noisevolume", m_Settings.soundnoisevolume);
	fcecfg.Find("sound","pcmvolume", m_Settings.soundpcmvolume);

	//fcecfg.Find("video","region","NTSC"); //not used
	fcecfg.Find("video","swfilter", m_Settings.SelectedGfxFilter);
	fcecfg.Find("video","screenaspect", m_Settings.SelectedVertexFilter);
	
	// Default to cheap filter for performance (override expensive CPU filters)
	// User can still change this in settings, but baseline should be fast
	if (m_Settings.SelectedGfxFilter != gfx_normal && 
	    m_Settings.SelectedGfxFilter != gfx_hq2x &&
	    m_Settings.SelectedGfxFilter != gfx_hq3x &&
	    m_Settings.SelectedGfxFilter != gfx_2xsai &&
	    m_Settings.SelectedGfxFilter != gfx_super2sai &&
	    m_Settings.SelectedGfxFilter != gfx_superEagle) {
		m_Settings.SelectedGfxFilter = gfx_normal;  // Force baseline if invalid
	}
	
	// Lock to gfx_normal for performance - fancy CPU filters are expensive and not worth it on 360
	m_Settings.SelectedGfxFilter = gfx_normal;
	
	//network
	fcecfg.Find("network","enable", m_Settings.use_netplay);

	fcecfg.Find("controller","XINPUT_GAMEPAD_DPAD_UP", m_Settings.gamepad_dpad_up );
	fcecfg.Find("controller","XINPUT_GAMEPAD_DPAD_DOWN", m_Settings.gamepad_dpad_down );
	fcecfg.Find("controller","XINPUT_GAMEPAD_DPAD_LEFT", m_Settings.gamepad_dpad_left );
	fcecfg.Find("controller","XINPUT_GAMEPAD_DPAD_RIGHT", m_Settings.gamepad_dpad_right );
	fcecfg.Find("controller","XINPUT_GAMEPAD_START", m_Settings.gamepad_start );
	fcecfg.Find("controller","XINPUT_GAMEPAD_BACK", m_Settings.gamepad_back );
	fcecfg.Find("controller","XINPUT_GAMEPAD_A", m_Settings.gamepad_a );
	fcecfg.Find("controller","XINPUT_GAMEPAD_B", m_Settings.gamepad_b );
	fcecfg.Find("controller","XINPUT_GAMEPAD_X", m_Settings.gamepad_x );
	fcecfg.Find("controller","XINPUT_GAMEPAD_Y", m_Settings.gamepad_y);
	fcecfg.Find("controller","XINPUT_GAMEPAD_LEFT_THUMB", m_Settings.gamepad_left_thumb );
	fcecfg.Find("controller","XINPUT_GAMEPAD_RIGHT_THUMB", m_Settings.gamepad_right_thumb );
	fcecfg.Find("controller","XINPUT_GAMEPAD_LEFT_SHOULDER", m_Settings.gamepad_left_shoulder);
	fcecfg.Find("controller","XINPUT_GAMEPAD_RIGHT_SHOULDER", m_Settings.gamepad_right_shoulder);
	fcecfg.Find("controller","XINPUT_LEFT_TRIGGER", m_Settings.gamepad_left_trigger);
	fcecfg.Find("controller","XINPUT_RIGHT_TRIGGER", m_Settings.gamepad_right_trigger);

	return S_OK;
};

HRESULT Cemulator::LoadGame(std::string name, bool restart)
{
//-------------------------------------------------------------------------------------
// Initilise emu
//-------------------------------------------------------------------------------------
	FCEUI_Initialize();

//-------------------------------------------------------------------------------------
// Set some setting
//-------------------------------------------------------------------------------------
	FCEUI_SetBaseDirectory("game:");
	
	// Set snapshot directory to user-writable location on hdd1
	// This works even when game: is read-only (XZP/STFS packages)
	static char snapDir[] = "hdd1:\\fce360-enhanced\\snaps";
	FCEUI_SetDirOverride(FCEUIOD_SNAPS, snapDir);
	
	// Create user directories on hdd1 (always writable)
	CreateDirectoryA("hdd1:\\fce360-enhanced", NULL);
	CreateDirectoryA("hdd1:\\fce360-enhanced\\snaps", NULL);
	CreateDirectoryA("hdd1:\\fce360-enhanced\\lua", NULL);
	
	// Also try to create lua directory in game: (works if not read-only)
	CreateDirectoryA("game:\\lua", NULL);
	CreateDirectoryA("game:\\Lua", NULL);
	
	// Auto-load all Lua scripts from lua directories (in addition to FCEUD_LuaRunFrom())
#ifdef USE_LUA
	extern void FCEU_AutoLoadLuaScripts(void);
	FCEU_AutoLoadLuaScripts();
#endif
	
	FCEUI_SetVidSystem(0);

	//Apply settings
	FCEUI_Sound(m_Settings.soundrate);
	FCEUI_SetSoundVolume(m_Settings.soundvolume);
	FCEUI_SetLowPass(0);
	//FCEUI_SetSoundQuality(m_Settings.soundq);
    FCEUI_SetTriangleVolume(m_Settings.soundtrianglevolume);
    FCEUI_SetSquare1Volume(m_Settings.soundsquare1volume);
    FCEUI_SetSquare2Volume(m_Settings.soundsquare2volume);
    FCEUI_SetNoiseVolume(m_Settings.soundnoisevolume);
    FCEUI_SetPCMVolume(m_Settings.soundpcmvolume);
	
//-------------------------------------------------------------------------------------
// Load rom
//-------------------------------------------------------------------------------------	
	if(FCEUI_LoadGame(name.c_str() ,0)!=NULL)
	{
		// Clear rewind buffer when loading a new game
		ClearRewindBuffer();
		
		// Ensure FileBase is set correctly from the ROM filename for proper snapshot naming
		extern char FileBase[];
		
		// Manually extract the filename from the path
		std::string romPath = name;
		std::string romFilename;
		
		// Handle zip archive format: "path.zip|internal.nes"
		size_t pipePos = romPath.find('|');
		if(pipePos != std::string::npos)
		{
			romFilename = romPath.substr(pipePos + 1);
		}
		else
		{
			romFilename = romPath;
		}
		
		// Extract just the filename without path
		size_t lastSlash = romFilename.find_last_of("\\/");
		if(lastSlash != std::string::npos)
		{
			romFilename = romFilename.substr(lastSlash + 1);
		}
		
		// Remove extension (.nes, .zip, etc.)
		size_t lastDot = romFilename.find_last_of(".");
		if(lastDot != std::string::npos)
		{
			romFilename = romFilename.substr(0, lastDot);
		}
		
		// Set FileBase to the extracted filename (without extension)
		if(romFilename.length() > 0)
		{
			strncpy(FileBase, romFilename.c_str(), 2047);
			FileBase[2047] = '\0';
		}
		
		FCEUI_SetInput(0, SI_GAMEPAD, (void*)&powerpadbuf, 0);
		FCEUI_SetInput(1, SI_GAMEPAD, (void*)&powerpadbuf, 0);

		//set to ntsc
		extern FCEUGI * GameInfo;
		GameInfo->vidsys=GIV_NTSC;

		if(restart)
			ResetNES();

		if(m_Settings.use_netplay)
			FCEUD_NetworkConnect();

		// Load Lua scripts from game:/lua/ folder
		FCEUD_LuaRunFrom();

		return S_OK;
	}
	
	return E_FAIL;
};

void Cemulator::Render()
{
	g_pd3dDevice->BeginScene();

	// Set effect variables as needed
    D3DXCOLOR colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXCOLOR colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );

//-------------------------------------------------------------------------------------
// Clear screen
//-------------------------------------------------------------------------------------	
	g_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
	g_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );

	const D3DVECTOR4 clearColor = { 0.f, 0.f, 0.f, 1.f };
    g_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

	g_pd3dDevice->SetPredication( D3DPRED_TILE( 0 ) );
	//g_pd3dDevice->Clear( D3DCLEAR_TARGET1 | D3DCLEAR_TARGET2 );
	g_pd3dDevice->Clear(0L, NULL, D3DCLEAR_ZBUFFER |D3DCLEAR_TARGET0| D3DCLEAR_TARGET,D3DCOLOR_XRGB(70,140,255), 1.0f, 0L);
	g_pd3dDevice->SetPredication( 0 );

//-------------------------------------------------------------------------------------
// Setup technique
//-------------------------------------------------------------------------------------	
	ftime+=0.1f;

	g_pTexelSize[0]=1.f/float(GetWidth());
	g_pTexelSize[1]=1.f/float(GetHeight());
	
	g_effect->SetValue( g_MaterialAmbientColor, &colorMtrlAmbient, sizeof( D3DXCOLOR ) );
    g_effect->SetValue( g_MaterialDiffuseColor, &colorMtrlDiffuse, sizeof( D3DXCOLOR ) );
	g_effect->SetValue ( g_fTime, &ftime, sizeof(float));
	g_effect->SetMatrix( g_mWorldViewProjection, &g_matWorldViewProjection );
	g_effect->SetTexture( g_MeshTexture, g_texture );
	g_effect->SetTexture( g_bgTexture, g_bg_texture );
	g_effect->SetTechnique( g_technique_model );
    g_effect->SetFloatArray(g_TexelSize,g_pTexelSize,2);

	g_pd3dDevice->SetFVF( D3DFVF_XYZ|D3DFVF_TEX1 );

//-------------------------------------------------------------------------------------
// Render
//-------------------------------------------------------------------------------------	
	unsigned int iPass, cPasses;

//-------------------------------------------------------------------------------------
// Draw Bg
//-------------------------------------------------------------------------------------	
	g_effect->SetTechnique( g_technique_bg );
		g_effect->Begin( &cPasses, 0 );
		g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl );
		g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof(TEXTURED) );
		for( iPass = 0; iPass < cPasses; iPass++ )
		{
			g_effect->BeginPass( iPass );
			// Use VB draw instead of DrawPrimitiveUP for better performance (doesn't stall pipeline)
			g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
			g_effect->EndPass();
		}
		g_effect->End();

//-------------------------------------------------------------------------------------
// Draw Game
//-------------------------------------------------------------------------------------	
	if(RenderEmulation==true)
	{
		switch(m_Settings.SelectedVertexFilter)
		{
			case FullScreen:
				g_effect->SetTechnique( g_technique_model_fullscreen );
				break;
			case TvScreen:
				g_effect->SetTechnique( g_technique_model_tv );
				break;
			default: 
				g_effect->SetTechnique( g_technique_model_tv );
				break;
		}
		
		g_effect->SetTexture( g_MeshTexture, g_texture );
		g_effect->Begin( &cPasses, 0 );
		g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl );
		g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof(TEXTURED) );
		for( iPass = 0; iPass < cPasses; iPass++ )
		{
			g_effect->BeginPass( iPass );
			// Use VB draw instead of DrawPrimitiveUP for better performance (doesn't stall pipeline)
			g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
			//mesh->DrawSubset(0);
			g_effect->EndPass();
		}
		g_effect->End();
	}

//-------------------------------------------------------------------------------------
// Ui
//-------------------------------------------------------------------------------------	
	//toujours
	RenderXui(g_pd3dDevice);

//-------------------------------------------------------------------------------------
// Affiche
//-------------------------------------------------------------------------------------	
	g_pd3dDevice->EndScene();
	g_pd3dDevice->SetPredication( 0 );
	g_pd3dDevice->EndTiling( 0, NULL, m_pFrontBuffer, NULL, 1, 0, NULL );
    
    // Present the backbuffer contents to the display
    // Note: PresentationInterval already set, no need to double-block with SynchronizeToPresentationInterval
	g_pd3dDevice->Swap( m_pFrontBuffer, NULL );
};


HRESULT Cemulator::Run()
{
//-------------------------------------------------------------------------------------
// Lance l'application
//-------------------------------------------------------------------------------------	
	if(FAILED( InitVideo()	))
	{
		printf("InitVideo failed\n");
		return E_FAIL;
	}
	//Load Configuration
	if(FAILED( InitSystem()	))
	{
		printf("InitSystem failed\n");
		return E_FAIL;
	}
	if(FAILED( InitAudio()	))
	{
		printf("InitAudio failed\n");
		return E_FAIL;
	}
	if(FAILED( InitInput()	))
	{
		printf("InitInput failed\n");
		return E_FAIL;
	}
	
	InitUi(g_pd3dDevice, g_d3dpp);

//-------------------------------------------------------------------------------------
// Fixed-timestep emulation loop (60.0 Hz, matched to vsync to prevent drift-induced stutter)
//-------------------------------------------------------------------------------------	
	// Use QueryPerformanceCounter for high-resolution timing on Xbox 360
	// Frequency is typically 3.2MHz on Xbox 360, so we convert to 100ns units (10MHz)
	LARGE_INTEGER perfFreq, lastCounter, currentCounter;
	QueryPerformanceFrequency(&perfFreq);
	QueryPerformanceCounter(&lastCounter);
	
	static const double NTSC_HZ = 60.0;  // Match display refresh to remove drift-induced stutter
	static const double TICKS_PER_FRAME_D = (10000000.0 / NTSC_HZ);  // 100ns units per frame
	ULONGLONG acc = 0;

	gfx_filter.SetTextureDimension(GetWidth(), GetHeight());
	//filter from configuration - can be updated by xui
	gfx_filter.UseFilter( m_Settings.SelectedGfxFilter );

	// Fixed-timestep loop: run emulation at 60.0988 Hz, present at vsync (60Hz)
	// Always render to maintain responsive UI and smooth presentation
	int32 * snd;
	int32 sndsize;
	
	// Pre-calculate conversion factor for better performance
	double perfFreqDouble = (double)perfFreq.QuadPart;
	double conversionFactor = 10000000.0 / perfFreqDouble;

	while (end == false) {
		QueryPerformanceCounter(&currentCounter);
		// Convert performance counter delta to 100ns units (optimized calculation)
		ULONGLONG delta = (ULONGLONG)((double)(currentCounter.QuadPart - lastCounter.QuadPart) * conversionFactor);
		acc += delta;
		
		// Clamp runaway catch-up to prevent blasting multiple frames and causing hitches
		const ULONGLONG MAX_CATCHUP = (ULONGLONG)(TICKS_PER_FRAME_D * 2);
		if (acc > MAX_CATCHUP) acc = MAX_CATCHUP;
		
		lastCounter = currentCounter;

		// run emu at 60.0 Hz (matches vsync); allow a small catch-up to avoid spiraling
		int safety = 0;
		
		while (acc >= (ULONGLONG)TICKS_PER_FRAME_D && safety < 4) {
			if (RenderEmulation) {
				// Input
				UpdateInput();

				// Screenshot combo
				bool screenshotCombo =
					(Gamepads[0].wButtons & XINPUT_GAMEPAD_LEFT_THUMB) &&
					(Gamepads[0].bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
				if (screenshotCombo && !m_screenshotLatch) { m_screenshotLatch = true; FCEUI_SaveSnapshot(); }
				else if (!screenshotCombo) { m_screenshotLatch = false; }

				// Rewind (LT) with tap=single-step semantics + delayed key-repeat
				static bool prevRewindPressed = false;
				static int  rewindRepeatFrames = 0;

				const bool rewindPressed = (Gamepads[0].bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) && !screenshotCombo;
				const bool justPressed   = rewindPressed && !prevRewindPressed;
				const bool justReleased  = !rewindPressed && prevRewindPressed;

				// Count how long LT is held (in emu frames)
				m_rewindHeldFrames = rewindPressed ? (m_rewindHeldFrames + 1) : 0;

				if (rewindPressed) {
					if (!m_isRewinding && m_rewindCount > 0) {
						// IMPORTANT: Do NOT save a state here; we want a tap to jump exactly one saved interval
						m_isRewinding      = true;
						m_rewindFrameSkip  = 0;
						m_rewindStartPos   = m_rewindWritePos;
						rewindRepeatFrames = 0;
						// m_rewindHeldFrames is already 1 on first pressed frame
					}

					if (m_isRewinding) {
						int steps = 0;

						if (justPressed) {
							// Single step on tap
							steps = 1;
						} else {
							// After a short delay, start key-repeat;
							// accelerate the longer LT is held
							++rewindRepeatFrames;

							int repeatRate = 4; // one step every 4 frames (~66ms)
							if (m_rewindHeldFrames >= 90)      repeatRate = 1; // ~16ms per step
							else if (m_rewindHeldFrames >= 45) repeatRate = 2; // ~33ms per step

							if (rewindRepeatFrames >= REWIND_INITIAL_DELAY_FRAMES &&
							   ((rewindRepeatFrames - REWIND_INITIAL_DELAY_FRAMES) % repeatRate) == 0) {
								steps = 1;
							}
						}

						for (int s = 0; s < steps; ++s) {
							if (!LoadRewindState()) { m_isRewinding = false; break; }
						}
						// (Audio intentionally skipped while rewinding; Render() refreshes visuals)
					}
					// Skip normal emu frame while rewinding
				} else if (m_isRewinding) {
					// Release: exit rewind cleanly and reset repeat timers
					m_isRewinding      = false;
					m_rewindFrameSkip  = 0;
					m_frameCounter     = 0;
					rewindRepeatFrames = 0;
				} else {
					// Normal emulation
					// Fast-forward (RT) = 2× emu steps for this render
					int framesToRun = (Gamepads[0].bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 2 : 1;

					// Cache input state before fast-forward loop to prevent double-processing
					// When multiple buttons are pressed during fast-forward, we want both frames
					// to use the same input state (not re-read multiple times)
					uint32 cachedPowerpadbuf = powerpadbuf;

					for (int frame = 0; frame < framesToRun; ++frame) {
						// Restore cached input before each frame to ensure consistent input across fast-forward frames
						powerpadbuf = cachedPowerpadbuf;
						FCEUI_Emulate(&bitmap, &snd, &sndsize, 0);

#ifdef USE_LUA
						FCEU_LuaFrameBoundary();
#endif
						// Save rewind state periodically (less frequently for performance)
						if (++m_frameCounter >= REWIND_SAVE_INTERVAL) {
							m_frameCounter = 0;
							SaveRewindState();
						}

						if (frame == framesToRun - 1) {
#ifndef USE_LUA
							DrawTextTrans(bitmap + 4*256 + 4, 256, (uint8*)"LUA OFF", 0x2E | 0x80);
#else
							DrawTextTrans(bitmap + 4*256 + 4, 256, (uint8*)"LUA ON",  0x2E | 0x80);
							// Throttled Lua GUI (runs at ~30Hz for performance), draws onto XBuf
							FCEU_LuaGui(bitmap);
#endif
							
						// Optimized ARGB conversion with precomputed LUT
						// NOTE: LUT uses full 8-bit palette index (0-255) - no masking
						// This allows overlay palette ranges (0x80-0xBF normal, 0xC0-0xFF dimmed) to work correctly
						static unsigned int lut[256];
						static bool lutInit = false;
						if (!lutInit) {
							extern pcpal pcpalette[256];
							for (int i = 0; i < 256; ++i) {
								lut[i] = (0xFF << 24) | 
								         (pcpalette[i].r << 16) | 
								         (pcpalette[i].g << 8) | 
								         pcpalette[i].b;
							}
							lutInit = true;
						}
						
						// Fast lookup table conversion (no branching per pixel)
						// Uses full 8-bit index from bitmap - overlay palette ranges (0x80-0xFF) are properly handled
						for (int i = 0; i < (256 * 240); ++i) {
							nesBitmap[i] = lut[bitmap[i]];  // Full 8-bit index, no masking
						}
							
							gfx_filter.UpdateFilter(nesBitmap);
							UpdateAudio(snd, sndsize);
						}
					}
				}
			}
			acc -= (ULONGLONG)TICKS_PER_FRAME_D;
			++safety;
		}

		// Always render every loop iteration - Swap() will block on vsync for smooth pacing
		// This creates perfectly regular present intervals and absorbs timing differences
		UpdateVideo();
		Render();  // Always present once per loop; Swap() syncs to vsync
	}
//-------------------------------------------------------------------------------------
// End
//-------------------------------------------------------------------------------------	
	CloseSystem();
	CloseVideo();
	CloseAudio();
	CloseInput();
	return S_OK;
};


HRESULT Cemulator::CloseVideo()
{
	g_texture->Release();
	g_pd3dDevice->Release();
	g_pD3D->Release();

	return S_OK;	
};
HRESULT Cemulator::CloseAudio()
{
	if (g_pSourceVoice) {
		g_pSourceVoice->Stop(0);
		g_pSourceVoice->DestroyVoice();
		g_pSourceVoice = NULL;
	}
	if (g_pMasteringVoice) {
		g_pMasteringVoice->DestroyVoice();
		g_pMasteringVoice = NULL;
	}

	if (g_sound_buffer) {
		free(g_sound_buffer);
		g_sound_buffer = NULL;
	}

	return S_OK;	
};

HRESULT Cemulator::CloseInput()
{
	return S_OK;	
};

HRESULT Cemulator::CloseSystem()
{
	ClearRewindBuffer();
	return S_OK;	
};

//-------------------------------------------------------------------------------------
// Rewind System Implementation
//-------------------------------------------------------------------------------------
void Cemulator::InitRewindBuffer()
{
	m_rewindWritePos = 0;
	m_rewindCount = 0;
	m_frameCounter = 0;
	m_isRewinding = false;
	m_rewindFrameSkip = 0;
	m_rewindStartPos = 0;
	m_rewindHeldFrames = 0;
	
	for(int i = 0; i < REWIND_BUFFER_SIZE; i++)
	{
		m_rewindBuffer[i].isValid = false;
		m_rewindBuffer[i].stateData.clear();
	}
}

void Cemulator::ClearRewindBuffer()
{
	for(int i = 0; i < REWIND_BUFFER_SIZE; i++)
	{
		m_rewindBuffer[i].isValid = false;
		m_rewindBuffer[i].stateData.clear();
	}
	m_rewindWritePos = 0;
	m_rewindCount = 0;
	m_frameCounter = 0;
	m_isRewinding = false;
	m_rewindStartPos = 0;
	m_rewindHeldFrames = 0;
}

void Cemulator::SaveRewindState()
{
	extern FCEUGI *GameInfo;
	if(!GameInfo || FCEUI_EmulationPaused())
		return;
	
	// Don't save states while rewinding - we're going backwards through saved states
	if(m_isRewinding)
		return;
	
	// Create a memory stream to save the state
	EMUFILE_MEMORY ms;
	if(!FCEUSS_SaveMS(&ms, Z_NO_COMPRESSION))
		return;
	
	// Copy the state data to the buffer
	RewindState& state = m_rewindBuffer[m_rewindWritePos];
	state.stateData.assign((uint8*)ms.buf(), (uint8*)ms.buf() + ms.size());
	state.isValid = true;
	
	// Update buffer position (circular)
	m_rewindWritePos = (m_rewindWritePos + 1) % REWIND_BUFFER_SIZE;
	
	// Update count (don't exceed buffer size)
	if(m_rewindCount < REWIND_BUFFER_SIZE)
		m_rewindCount++;
	// If buffer is full, it wraps around automatically
}

bool Cemulator::LoadRewindState()
{
	if(!m_isRewinding)
		return false;
	
	// Calculate position of the previous state (one step back)
	int readPos = (m_rewindWritePos - 1 + REWIND_BUFFER_SIZE) % REWIND_BUFFER_SIZE;
	
	// Make sure we have a valid state to read
	if(!m_rewindBuffer[readPos].isValid)
		return false;
	
	// Calculate how many states we've rewound from the start
	// When we started, m_rewindStartPos was the position AFTER saving current state
	// So the state we're currently at (before rewinding this step) is at m_rewindWritePos
	// We want to check if we can go back one more
	int stepsBack;
	if(readPos <= m_rewindStartPos)
	{
		// No wrap - simple subtraction
		stepsBack = m_rewindStartPos - readPos;
	}
	else
	{
		// Wrapped around - readPos is near start of buffer, m_rewindStartPos is later
		stepsBack = (REWIND_BUFFER_SIZE - readPos) + m_rewindStartPos;
	}
	
	// We can rewind up to m_rewindCount states (the number we had before saving at start)
	// But we saved one state at the start, so we subtract 1
	if(stepsBack >= m_rewindCount)
	{
		// Can't rewind further - we've reached the oldest saved state
		return false;
	}
	
	// Create a memory stream from the saved state
	std::vector<uint8>& stateData = m_rewindBuffer[readPos].stateData;
	EMUFILE_MEMORY ms(&stateData);
	
	// Reset the memory stream to the beginning
	ms.fseek(0, SEEK_SET);
	
	// Load the state
	if(!FCEUSS_LoadFP(&ms, SSLOADPARAM_NOBACKUP))
		return false;
	
	// Update write position to the state we just loaded
	// This way the next rewind will go back one more step
	m_rewindWritePos = readPos;
	
	return true;
}

