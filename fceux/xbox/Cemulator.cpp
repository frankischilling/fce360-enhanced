#include "stdafx.h"
#include "xbox/fceusupport.h"
#include <fxl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <xaudio2.h>
#include <xtl.h>
#include <xui.h>
#include <math.h>

#include "Cemulator.h"
#include "audio.h"
#include "config_reader.h"
#include "fceux/drawing.h"
#include "fceux/emufile.h"
#include "fceux/video.h"
#include "input.h"
#include "net360.h"
#include "xconfig.h"
#ifdef USE_LUA
#include "fceux/fceulua.h"
#endif
#include "zlib.h"

//-----------------------------------------------------------------------------
// Screenshot warmup helpers (eliminate first-use stall)
//-----------------------------------------------------------------------------
static bool g_zlibWarm = false;

static void WarmupZlibOnce() {
	if (g_zlibWarm) return;
	z_stream s = {};
	if (deflateInit(&s, Z_DEFAULT_COMPRESSION) == Z_OK) {
		unsigned char in[256] = {0}, out[256] = {0};
		s.next_in = in;
		s.avail_in = sizeof(in);
		s.next_out = out;
		s.avail_out = sizeof(out);
		deflate(&s, Z_FINISH);
		deflateEnd(&s);
		g_zlibWarm = true;
	}
}

static void WarmupSnapshotFilesystemOnce(const char* dir) {
	// Touch file system in the exact target directory to populate caches
	// This runs on every ROM load to ensure filesystem cache is warm for this directory
	char path[MAX_PATH];
	snprintf(path, sizeof(path), "%s\\~snap_warmup.tmp", dir);
	HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		DWORD w = 0;
		char b[64] = {0};
		WriteFile(h, b, sizeof(b), &w, NULL);
		CloseHandle(h); // DELETE_ON_CLOSE releases on close
	}
}

static void WarmupSnapshotPathAfterRomLoad() {
	// Warmup FCEU_MakeFName and actual snapshot path generation
	// This must be called AFTER ROM is loaded (FileBase is set)
	extern std::string FCEU_MakeFName(int type, int id1, const char *cd1);
	extern FILE* FCEUD_UTF8fopen(const char* fn, const char* mode);
	
	// Generate a real snapshot path to warm up FCEU_MakeFName
	std::string testPath = FCEU_MakeFName(2, 99999, "png"); // 2 = FCEUMKF_SNAP
	
	// Open and immediately close a file to warm up the full I/O path
	FILE* f = FCEUD_UTF8fopen(testPath.c_str(), "wb");
	if (f) {
		// Write PNG header and minimal valid PNG to warm up compression path
		static uint8_t pngHeader[8] = {137, 80, 78, 71, 13, 10, 26, 10};
		fwrite(pngHeader, 8, 1, f);
		
		// Write IHDR chunk (1x1 image)
		uint8_t ihdr[25] = {
			0, 0, 0, 13,           // chunk length = 13
			73, 72, 68, 82,        // "IHDR"
			0, 0, 0, 1,            // width = 1
			0, 0, 0, 1,            // height = 1
			8, 3, 0, 0, 0,         // 8-bit indexed, no interlace
			0xb1, 0x8e, 0x7c, 0xfb // CRC
		};
		fwrite(ihdr, 25, 1, f);
		
		// Write minimal PLTE chunk (1 color palette)
		uint8_t plte[15] = {
			0, 0, 0, 3,      // chunk length = 3
			80, 76, 84, 69,  // "PLTE"
			0, 0, 0,         // RGB for palette entry 0
			0xa7, 0x7a, 0x71, 0x1d // CRC
		};
		fwrite(plte, 15, 1, f);
		
		// Write minimal IDAT chunk with compressed 1x1 data
		uint8_t idat[19] = {
			0, 0, 0, 10,     // chunk length = 10
			73, 68, 65, 84,  // "IDAT"
			0x78, 0x9c,      // zlib header
			0x63, 0x00, 0x01, // compressed data (1 pixel)
			0x00, 0x00, 0x00, 0x02, 0x00, 0x01
		};
		fwrite(idat, 19, 1, f);
		
		// Write IEND chunk
		uint8_t iend[12] = {
			0, 0, 0, 0,            // chunk length = 0
			73, 69, 78, 68,        // "IEND"
			0xae, 0x42, 0x60, 0x82 // CRC
		};
		fwrite(iend, 12, 1, f);
		
		fclose(f);
		
		// Delete the warmup file immediately
		DeleteFileA(testPath.c_str());
	}
}

//-----------------------------------------------------------------------------
// Performance: Disable printf spam in retail builds
//-----------------------------------------------------------------------------
#if !defined(DEBUG) && !defined(_DEBUG)
#undef printf
#define printf(...) ((void)0)
#endif

// Log budget system to prevent excessive debug output
static int g_log_budget = 200; // print at most 200 lines total per run
#define LOGF(...)                                                              \
	do {                                                                       \
		if (g_log_budget > 0) {                                                \
			--g_log_budget;                                                    \
			printf(__VA_ARGS__);                                               \
		}                                                                      \
	} while (0)

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9 g_pD3D = NULL;			   // Used to create the D3DDevice
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;  // Buffer to hold vertices
// --- globals ---
static IDirect3DTexture9* g_nesTex[3] = { NULL, NULL, NULL };
static int g_texWrite = 0;   // index we upload into this frame
static int g_texDraw  = 0;   // index we bind for drawing this frame
static DWORD g_texFence[3] = { 0,0,0 }; // GPU fences protecting each tex
IDirect3DTexture9 *g_bg_texture = NULL;
D3DPRESENT_PARAMETERS g_d3dpp;
LPD3DXEFFECT g_effect = NULL; // handle to D3DXEffect
static volatile bool g_hasNewFrame = false;
static volatile int g_frameDrift = 0; // Track how many frames ahead emulation is (accessible to Render)
static int g_framesProduced = 0; // Track complete NES frames produced
static int g_framesDisplayed = 0; // Track frames actually displayed (advanced display buffer)

// NES texture present latching
static int g_texLatched   = -1;   // last latched (displayed) NES texture
static int g_pendingTex   = -1;   // a complete frame uploaded this emu step
static int g_maxLead      = 0;    // keep drift at 0 (most aggressive - eliminates all artifacts)

// Rendering surfaces and textures
#ifdef _XBOX
D3DSurface *m_pBackBuffer;
D3DSurface *m_pDepthBuffer;
IDirect3DTexture9* m_front[3] = { NULL, NULL, NULL };
int m_idxRender  = 0;   // resolve current frame into this
int m_idxQueued  = -1;  // last completed frame, ready to display next vblank
int m_idxDisplay = -1;  // the one we'll pass to Swap() this frame
#endif

//-------------------------------------------------------------------------------------
// Shader
//-------------------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *g_pVertexDecl; // Vertex format decl

D3DXMATRIX g_matWorld;
D3DXMATRIX g_matProj;
D3DXMATRIX g_matView;
D3DXMATRIX g_matWorldViewProjection;

//-------------------------------------------------------------------------------------
// Audio
//-------------------------------------------------------------------------------------
#define SOUND_BUFFER_SIZE 5000
// Block size and queue limits now defined in Cemulator class constants

// Helper: Get current voice state
struct VoiceState { 
    uint64_t played; 
    uint32_t queued; 
};

static VoiceState GetVoiceState(IXAudio2SourceVoice* v) {
    VoiceState vs;
    XAUDIO2_VOICE_STATE st;
    ZeroMemory(&st, sizeof(st));
    v->GetState(&st);  // Xbox 360 XAudio2 only takes 1 argument
    vs.played = (uint64_t)st.SamplesPlayed;
    vs.queued = st.BuffersQueued;
    return vs;
}

// fceux bitmap
uint8 *bitmap;
// bitmap with good color ARGB
unsigned int *nesBitmap;
// sound buffer
unsigned int *g_sound_buffer;

IXAudio2 *g_pXAudio2 = NULL;
IXAudio2MasteringVoice *g_pMasteringVoice = NULL;
IXAudio2SourceVoice *g_pSourceVoice = NULL;
XAUDIO2_BUFFER g_SoundBuffer;

//

float ftime = 0.f;

//-------------------------------------------------------------------------------------
// Input
//-------------------------------------------------------------------------------------
GAMEPAD Gamepads[XUSER_MAX_COUNT];
uint32 powerpadbuf = 0;

//-------------------------------------------------------------------------------------
// TEXTURE
//-------------------------------------------------------------------------------------
struct TEXTURED {
	FLOAT x, y, z; // The untransformed, 3D position for the vertex
	FLOAT u, v;	   // The texture coordonate
};

#define D3DFVF_TEXTURED (D3DFVF_XYZ | D3DFVF_TEX1)

static const D3DVERTEXELEMENT9 g_ElementsTextured[4] = {
	{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,
	 0},
	D3DDECL_END()};

struct TEXTURED g_VerticesTextured[] = {
	// square
	{-1.0f, -1.0f, 0.0f, 0.0f, 1.0f}, // 1
	{-1.0f, 1.0f, 0.0f, 0.0f, 0.0f},  // 2
	{1.0f, 1.0f, 0.0f, 1.0f, 0.0f},	  // 4
	{1.0f, -1.0f, 0.0f, 1.0f, 1.0f}	  // 3
};

static D3DRECT g_tiles[3];

static void BuildTiles(UINT tileW, UINT tileH, UINT frameH) {
	g_tiles[0].x1 = 0;
	g_tiles[0].y1 = 0;
	g_tiles[0].x2 = (LONG)tileW;
	g_tiles[0].y2 = (LONG)tileH;
	
	g_tiles[1].x1 = 0;
	g_tiles[1].y1 = (LONG)tileH;
	g_tiles[1].x2 = (LONG)tileW;
	g_tiles[1].y2 = (LONG)(tileH * 2);
	
	g_tiles[2].x1 = 0;
	g_tiles[2].y1 = (LONG)(tileH * 2);
	g_tiles[2].x2 = (LONG)tileW;
	g_tiles[2].y2 = (LONG)frameH;
}

//-------------------------------------------------------------------------------------
// TEXTURE
//-------------------------------------------------------------------------------------
D3DXHANDLE g_MaterialAmbientColor;
D3DXHANDLE g_MaterialDiffuseColor;
D3DXHANDLE g_mWorldViewProjection;
D3DXHANDLE g_MeshTexture;
D3DXHANDLE g_bgTexture;
D3DXHANDLE g_technique_bg;
D3DXHANDLE g_technique_model;
D3DXHANDLE g_technique_model_tv;
D3DXHANDLE g_technique_model_fullscreen;

D3DXHANDLE g_TexelSize;
D3DXHANDLE g_fTime;
LPD3DXBUFFER materialBuffer;
DWORD numMaterials; // Note: DWORD is a typedef for unsigned long
LPD3DXMESH mesh;

float g_pTexelSize[2];

//-------------------------------------------------------------------------------------
// Cemulator
//-------------------------------------------------------------------------------------
Cemulator::Cemulator(void) {
	end = false;
	RenderEmulation = false; // Display xui at first
	m_Settings.SelectedVertexFilter = FullScreen;
	snd_written = 0;
	// A/V sync init
	m_targetQueued = 1.0f;   // legacy target (unused by new loop)
	m_freqRatio = 1.0f;
	m_syncI = 0.0f;          // legacy integrator (unused by new loop)
	m_expectedSamples = 0.0; // per-channel samples submitted
	m_errI = 0.0;            // seconds integral
	m_accCount = 0;
	m_audioPoolHead = 0;
	m_samplesPlayedBase = 0;
	m_fdsWaitingForFirstBuffer = false;
	m_inLongSilence = false;
	m_silenceSamplesAcc = 0;
	m_prevPaused = false;
	m_pauseSilenceBypass = false;
	m_inRomChange = false;
	m_resetCooldownMs = 250; // don't spam resets during transitions
	m_warmupBlocksToDrop = 0;
	m_isFDS = false;
	QueryPerformanceFrequency(&m_perfFreq);
	QueryPerformanceCounter(&m_lastResetQPC);
	for (int i = 0; i < AUDIO_POOL_SIZE; ++i) {
		m_audioPool[i] = NULL;
	}
	ftime = 0.0f;
	m_screenshotLatch = false;
	InitRewindBuffer();
}

HRESULT Cemulator::InitVideo() {
	//-------------------------------------------------------------------------------------
	// Create d3d device
	//-------------------------------------------------------------------------------------
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_pD3D)
		return E_FAIL;

	//-------------------------------------------------------------------------------------
	// Set the system width
	//-------------------------------------------------------------------------------------
	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	// Set up the structure used to create the D3DDevice.
	XVIDEO_MODE VideoMode;
	ZeroMemory(&VideoMode, sizeof(VideoMode));
	XGetVideoMode(&VideoMode);
	BOOL bEnable720p = (VideoMode.dwDisplayHeight >= 720) ? TRUE : FALSE;
	SetSystemWidth((bEnable720p) ? 1280 : 640);
	SetSystemHeight((bEnable720p) ? 720 : 480);

	// Build tiles at runtime using actual dimensions
	// Make sure these three are initialized before calling BuildTiles(...):
	// g_dwTileWidth, g_dwTileHeight, g_dwFrameHeight
	BuildTiles(g_dwTileWidth, g_dwTileHeight, g_dwFrameHeight);

	//-------------------------------------------------------------------------------------
	// MSAA surface
	//-------------------------------------------------------------------------------------
	g_d3dpp.BackBufferWidth = GetSystemWidth();
	g_d3dpp.BackBufferHeight = GetSystemHeight();
	g_d3dpp.BackBufferFormat = (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8);
	g_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	g_d3dpp.MultiSampleQuality = 0;
	g_d3dpp.BackBufferCount = 0;
	g_d3dpp.EnableAutoDepthStencil = FALSE;
	g_d3dpp.DisableAutoBackBuffer = TRUE;
	g_d3dpp.DisableAutoFrontBuffer = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	// g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	//-------------------------------------------------------------------------------------
	// Create device FIRST (before creating surfaces - correct order)
	// Note: D3DCREATE_BUFFER_3_FRAMES not available on Xbox 360 - command
	// buffer depth is managed internally Using standard flags for Xbox 360
	//-------------------------------------------------------------------------------------
	if (FAILED(g_pD3D->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
									D3DCREATE_HARDWARE_VERTEXPROCESSING,
									&g_d3dpp, &g_pd3dDevice))) {
		printf("CreateDevice failed\n");
		return E_FAIL;
	}

	//-------------------------------------------------------------------------------------
	// Create render surfaces AFTER device creation
	//-------------------------------------------------------------------------------------
	D3DSURFACE_PARAMETERS params = {0};
	g_pd3dDevice->CreateRenderTarget(g_dwTileWidth, g_dwTileHeight,
									 D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, 0,
									 &m_pBackBuffer, &params);
	params.Base = m_pBackBuffer->Size / GPU_EDRAM_TILE_SIZE;
	params.HierarchicalZBase = D3DHIZFUNC_GREATER_EQUAL;

	g_pd3dDevice->CreateDepthStencilSurface(g_dwTileWidth, g_dwTileHeight,
											D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
											0, 0, &m_pDepthBuffer, &params);
    g_pd3dDevice->CreateTexture(g_dwFrameWidth, g_dwFrameHeight, 1, 0,
                                D3DFMT_LE_X8R8G8B8, 0, &m_front[0], NULL);
    g_pd3dDevice->CreateTexture(g_dwFrameWidth, g_dwFrameHeight, 1, 0,
                                D3DFMT_LE_X8R8G8B8, 0, &m_front[1], NULL);
    g_pd3dDevice->CreateTexture(g_dwFrameWidth, g_dwFrameHeight, 1, 0,
                                D3DFMT_LE_X8R8G8B8, 0, &m_front[2], NULL);

    // initialize triple-buffer indices
    m_idxRender = 0;
    m_idxQueued = -1;
    m_idxDisplay = -1;
	//-------------------------------------------------------------------------------------
	// Create the buffer, and load the effect from the file.
	//-------------------------------------------------------------------------------------
	HRESULT Result;
	ID3DXEffectCompiler *pCompiler = NULL;
	ID3DXBuffer *pCompiledData = NULL;

	//-------------------------------------------------------------------------------------
	// Create a ID3DXEffectCompiler interface for the effect that was just
	// loaded.
	//-------------------------------------------------------------------------------------
	Result = D3DXCreateEffectCompilerFromFileA(EFFECT_FILE, NULL, NULL, 0,
											   &pCompiler, NULL);
	if (FAILED(Result)) {
		printf("D3DXCreateEffectCompiler FAILED\n");
		return Result;
	}

	// Compile the effect by using the ID3DXEffectCompiler interface, and then
	// release the compiler.
	Result = pCompiler->CompileEffect(
		0, // No debug flags in retail (was D3DXSHADER_DEBUG)
		&pCompiledData, NULL);

	pCompiler->Release();
	if (FAILED(Result)) {
		printf("CompileEffect FAILED\n");
		return Result;
	}

	// Create the effect that was just compiled.
	Result = D3DXCreateEffect(
		g_pd3dDevice, (DWORD *)pCompiledData->GetBufferPointer(),
		pCompiledData->GetBufferSize(), NULL, NULL, 0, NULL, &g_effect, NULL);
	
	// Release the compiled blob after creating the effect
	if (pCompiledData) { pCompiledData->Release(); pCompiledData = NULL; }

	//-------------------------------------------------------------------------------------
	// Create the model
	//-------------------------------------------------------------------------------------
	HRESULT hr =
		D3DXLoadMeshFromXA(X_FILE, D3DXMESH_SYSTEMMEM, g_pd3dDevice, NULL,
						   &materialBuffer, NULL, &numMaterials, &mesh);
	
	// Release material buffer if not used
	if (materialBuffer) { materialBuffer->Release(); materialBuffer = NULL; }
	//-------------------------------------------------------------------------------------
	// Load the bg
	//-------------------------------------------------------------------------------------
	D3DXCreateTextureFromFileA(g_pd3dDevice, BG_FILE, &g_bg_texture);

	//-------------------------------------------------------------------------------------
	// Create NES texture ring buffer (3 textures for triple buffering)
	//-------------------------------------------------------------------------------------
	for (int i = 0; i < 3; ++i) {
		D3DXCreateTexture(g_pd3dDevice, GetWidth(), GetHeight(),
						  1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED,
						  &g_nesTex[i]);
	}

	//-------------------------------------------------------------------------------------
	// Create VB
	//-------------------------------------------------------------------------------------
	g_pd3dDevice->CreateVertexDeclaration(g_ElementsTextured, &g_pVertexDecl);

	if (FAILED(g_pd3dDevice->CreateVertexBuffer(
			4 * sizeof(TEXTURED), D3DUSAGE_WRITEONLY, NULL, D3DPOOL_MANAGED,
			&g_pVB, NULL))) {
		printf("CreateVertexBuffer failed\n");
		return E_FAIL;
	}

	TEXTURED *pVertices;
	if (FAILED(g_pVB->Lock(0, 0, (void **)&pVertices, 0)))
		return E_FAIL;
	memcpy(pVertices, g_VerticesTextured, 4 * sizeof(TEXTURED));
	g_pVB->Unlock();

	//-------------------------------------------------------------------------------------
	// Param from effectfile
	//-------------------------------------------------------------------------------------
	g_technique_bg = g_effect->GetTechniqueByName("RenderFullScreen");
	g_technique_model = g_effect->GetTechniqueByName("RenderModel");
	g_technique_model_tv = g_effect->GetTechniqueByName("RenderModelTv");
	g_technique_model_fullscreen =
		g_effect->GetTechniqueByName("RenderModelFullScreen");

	g_mWorldViewProjection =
		g_effect->GetParameterByName(NULL, "g_mWorldViewProjection");
	g_MaterialAmbientColor =
		g_effect->GetParameterByName(NULL, "g_MaterialAmbientColor");
	g_MaterialDiffuseColor =
		g_effect->GetParameterByName(NULL, "g_MaterialDiffuseColor");
	g_mWorldViewProjection =
		g_effect->GetParameterByName(NULL, "g_mWorldViewProjection");
	g_MeshTexture = g_effect->GetParameterByName(NULL, "g_MeshTexture");
	g_bgTexture = g_effect->GetParameterByName(NULL, "g_bgTexture");
	g_TexelSize = g_effect->GetParameterByName(NULL, "g_TexelSize");
	g_fTime = g_effect->GetParameterByName(NULL, "g_fTime");

	//-------------------------------------------------------------------------------------
	// Default Renderstates
	//-------------------------------------------------------------------------------------
	g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	//-------------------------------------------------------------------------------------
	// NES layer must be pixel-perfect - force point sampling and clamp
	// addressing
	//-------------------------------------------------------------------------------------
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	// Don't blend the base NES layer
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	return S_OK;
};

void Cemulator::UpdateVideo() {
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

HRESULT Cemulator::InitAudio() {
	//-------------------------------------------------------------------------------------
	// Initialise Audio
	//-------------------------------------------------------------------------------------
	HRESULT hr;
	if (FAILED(hr = XAudio2Create(&g_pXAudio2, 0))) {
		printf("Failed to init XAudio2 engine: %#X\n", hr);
		return E_FAIL;
	}

	//-------------------------------------------------------------------------------------
	// Create a mastering voice
	//-------------------------------------------------------------------------------------
	if (FAILED(hr = g_pXAudio2->CreateMasteringVoice(&g_pMasteringVoice))) {
		printf("Failed creating mastering voice: %#X\n", hr);
		return E_FAIL;
	}

	//-------------------------------------------------------------------------------------
	// Create source voice
	//-------------------------------------------------------------------------------------
	WAVEFORMATEXTENSIBLE wfx;
	memset(&wfx, 0, sizeof(WAVEFORMATEXTENSIBLE));

	wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfx.Format.nSamplesPerSec = m_Settings.soundrate; // 48000 by default
	wfx.Format.nChannels = 2;
	wfx.Format.wBitsPerSample = 16;
	wfx.Format.nBlockAlign =
		wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
	wfx.Format.nAvgBytesPerSec =
		wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
	wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
	wfx.dwChannelMask = SPEAKER_STEREO;
	wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

	//-------------------------------------------------------------------------------------
	//	Source voice
	//-------------------------------------------------------------------------------------
	// DO allow resampling + pitch control for dynamic A/V sync.
	// XAudio2's high-quality resampler easily handles the tiny ±0.2% corrections we need.
	UINT32 voiceFlags = 0;  // <- not XAUDIO2_VOICE_NOSRC

	// Give yourself a little headroom for 59.94↔60.10
	const float kMaxRatio = 1.02f;

	if (FAILED(g_pXAudio2->CreateSourceVoice(&g_pSourceVoice,
											 (WAVEFORMATEX *)&wfx, voiceFlags,
										 kMaxRatio, &XAudio2_Notifier))) {
		printf("CreateSourceVoice failed\n");
		return E_FAIL;
	}

	//-------------------------------------------------------------------------------------
	// Start sound
	//-------------------------------------------------------------------------------------
	if (FAILED(g_pSourceVoice->Start(0))) {
		printf("g_pSourceVoice failed\n");
		return E_FAIL;
	}

	// Initialize frequency ratio to 1.0
	if (g_pSourceVoice) {
		g_pSourceVoice->SetFrequencyRatio(1.0f);
		m_freqRatio = 1.0f;
		m_expectedSamples = 0.0; // reset drift tracker on (re)start
		m_errI = 0.0;
	}

	return S_OK;
};

// Helper: Calculate milliseconds since a performance counter timestamp
static inline int msSince(const LARGE_INTEGER& since, const LARGE_INTEGER& freq) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double ms = (double)(now.QuadPart - since.QuadPart) * 1000.0 / (double)freq.QuadPart;
    return (int)ms;
}

void Cemulator::UpdateAudio(int *snd, int sndsize) {
	if (sndsize <= 0 || !g_pSourceVoice)
		return;

	//-------------------------------------------------------------------------------------
	// Audio: Convert mono 16-bit (signed) -> interleaved stereo 16-bit (L=R)
	// Use signed samples to avoid DC bias and XAudio2 limiter overhead
	// Accumulate samples across frames since NES produces ~735-800
	// samples/frame
	//-------------------------------------------------------------------------------------
	
	// Accumulate incoming samples
	for (int i = 0; i < sndsize; ++i) {
		short sample = (short)(snd[i] & 0xFFFF); // signed 16-bit!

		// Grow accumulator if needed
		if (m_accCount * 2 + 1 >= (int)m_audioAccumulator.size()) {
			m_audioAccumulator.resize((m_accCount + sndsize) * 2); // Ensure room for stereo
		}

		m_audioAccumulator[m_accCount * 2 + 0] = sample; // Left channel
		m_audioAccumulator[m_accCount * 2 + 1] = sample; // Right channel (same as left)
		++m_accCount;
	}

	// Submit when we have a full block
	while (m_accCount >= kBlockSamples) {
		// Do not hard-flush the queue; the PI loop will gently correct drift

		// Get buffer from pool (reuse instead of malloc/free per block)
		const size_t bytes = kBlockSamples * 2 /*stereo*/ * sizeof(short);
		if (!m_audioPool[m_audioPoolHead]) {
			m_audioPool[m_audioPoolHead] = (BYTE *)malloc(bytes);
			if (!m_audioPool[m_audioPoolHead])
				break; // Can't allocate, skip this block
		}
		BYTE *buf = m_audioPool[m_audioPoolHead];

		// Copy kBlockSamples worth of stereo samples
		memcpy(buf, &m_audioAccumulator[0], bytes);

		XAUDIO2_BUFFER xb = {0};
		xb.AudioBytes = (UINT32)bytes;
		xb.pAudioData = buf;
		xb.pContext = (void *)(intptr_t)m_audioPoolHead; // Store pool index instead of pointer
		xb.Flags = 0;					// streaming, NOT end-of-stream

		if (SUCCEEDED(g_pSourceVoice->SubmitSourceBuffer(&xb))) {
			// Track submitted per-channel samples (stereo frames -> per-channel N)
			m_expectedSamples += (double)kBlockSamples;
			// Advance pool head (buffer will be reused when callback fires)
			m_audioPoolHead = (m_audioPoolHead + 1) % AUDIO_POOL_SIZE;

			// Remove submitted samples from accumulator
			int remaining = m_accCount - kBlockSamples;
			if (remaining > 0) {
				// Shift remaining samples to front
				memmove(&m_audioAccumulator[0], &m_audioAccumulator[kBlockSamples * 2],
						remaining * 2 * sizeof(short));
			}
			m_accCount = remaining;
		} else {
			break; // Submission failed, stop trying (buffer stays in pool)
		}
	}
	return;
};

// --- A/V sync (dynamic audio rate control) ---
void Cemulator::SyncAudioQueue() {
    if (!g_pSourceVoice) return;
    XAUDIO2_VOICE_STATE st = {};
    g_pSourceVoice->GetState(&st /*, 0*/); // use SamplesPlayed

    // Error in seconds using sample-accurate drift
    const double sr = (double)m_Settings.soundrate;
    double errSec = (m_expectedSamples - (double)st.SamplesPlayed) / sr;

    // Gentle PI tuned for ~20ms queue
    m_errI += errSec * 0.02; // integral term
    double ratioD = 1.0 + errSec * 0.40 + m_errI;

    // Keep corrections inaudible
    if (ratioD < 0.995) ratioD = 0.995;
    if (ratioD > 1.005) ratioD = 1.005;

    float ratio = (float)ratioD;
    if (fabsf(ratio - m_freqRatio) > 0.0001f) {
        m_freqRatio = ratio;
        g_pSourceVoice->SetFrequencyRatio(m_freqRatio);
    }
}

// Soft reset audio stream - keeps pool allocations to avoid malloc storm
void Cemulator::SoftResetAudioStream() {
    if (!g_pSourceVoice) return;

    g_pSourceVoice->Stop(0);
    g_pSourceVoice->FlushSourceBuffers();
    g_pSourceVoice->SetFrequencyRatio(1.0f);
    g_pSourceVoice->Start(0);

    m_expectedSamples = 0.0;
    m_errI = 0.0;
    m_freqRatio = 1.0f;
    m_inLongSilence = false;
    m_silenceSamplesAcc = 0;
    m_samplesPlayedBase = 0;
    m_fdsWaitingForFirstBuffer = true;
    m_audioAccumulator.clear();
    m_accCount = 0;
    // KEEP pool allocations; they'll be reused (no malloc storm on resume)
    m_audioPoolHead = 0;

    // video latch clean
    g_hasNewFrame = false;
    g_pendingTex  = -1;
    g_texLatched  = -1;
    g_framesProduced  = 0;
    g_framesDisplayed = 0;
}

// Hard reset audio stream - frees pool (use only when necessary)
void Cemulator::ResetAudioStream() {
    if (!g_pSourceVoice) return;

    // Stop and flush any queued buffers from the old game
    g_pSourceVoice->Stop(0);
    g_pSourceVoice->FlushSourceBuffers();
    g_pSourceVoice->SetFrequencyRatio(1.0f);
    g_pSourceVoice->Start(0);

    // Reset all state
    m_expectedSamples = 0.0;
    m_errI = 0.0;
    m_freqRatio = 1.0f;
    m_inLongSilence = false;
    m_silenceSamplesAcc = 0;
    m_samplesPlayedBase = 0;
    m_fdsWaitingForFirstBuffer = true;

    // Clear per-frame audio accumulation and buffer pool
    m_audioAccumulator.clear();
    m_accCount = 0;
    for (int i = 0; i < AUDIO_POOL_SIZE; ++i) {
        if (m_audioPool[i]) { 
            free(m_audioPool[i]); 
            m_audioPool[i] = NULL; 
        }
    }
    m_audioPoolHead = 0;

    // (Optional but recommended) reset video latch so first frame of new game is clean
    g_hasNewFrame = false;
    g_pendingTex  = -1;
    g_texLatched  = -1;
    g_framesProduced  = 0;
    g_framesDisplayed = 0;
}

// Hard recreate source voice (use when sample rate changes or voice is corrupted)
void Cemulator::HardRecreateSourceVoice() {
    if (g_pSourceVoice) { 
        g_pSourceVoice->Stop(0); 
        g_pSourceVoice->DestroyVoice(); 
        g_pSourceVoice = NULL; 
    }

    // Build WAVEFORMATEXTENSIBLE from current m_Settings.soundrate
    WAVEFORMATEXTENSIBLE wfx;
    ZeroMemory(&wfx, sizeof(wfx));
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nSamplesPerSec = m_Settings.soundrate; // ensure this was set via FCEUI_Sound() first
    wfx.Format.nChannels = 2;
    wfx.Format.wBitsPerSample = 16;
    wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample = 16;
    wfx.dwChannelMask = SPEAKER_STEREO;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    const float kMaxRatio = 1.02f;
    UINT32 flags = 0; // allow SRC for tiny drift
    if (FAILED(g_pXAudio2->CreateSourceVoice(&g_pSourceVoice, (WAVEFORMATEX*)&wfx, flags, kMaxRatio, &XAudio2_Notifier))) {
        // fall back defensively; in practice this path won't hit
        return;
    }
    g_pSourceVoice->Start(0);
    SoftResetAudioStream();
}

// Prime audio queue with silent blocks to avoid first-second underrun
void Cemulator::PrimeAudioQueue(int blocks) {
    if (!g_pSourceVoice) return;

    const size_t bytes = kBlockSamples * 2 /*stereo*/ * sizeof(short);
    static std::vector<short> zeros(kBlockSamples * 2, 0);

    for (int i = 0; i < blocks; ++i) {
        if (!m_audioPool[m_audioPoolHead]) {
            m_audioPool[m_audioPoolHead] = (BYTE*)malloc(bytes);
            if (!m_audioPool[m_audioPoolHead]) break;
        }
        memcpy(m_audioPool[m_audioPoolHead], &zeros[0], bytes);

        XAUDIO2_BUFFER xb;
        ZeroMemory(&xb, sizeof(xb));
        xb.AudioBytes = (UINT32)bytes;
        xb.pAudioData = m_audioPool[m_audioPoolHead];
        xb.pContext   = (void*)(intptr_t)m_audioPoolHead;

        if (SUCCEEDED(g_pSourceVoice->SubmitSourceBuffer(&xb))) {
            m_expectedSamples += (double)kBlockSamples;
            m_audioPoolHead = (m_audioPoolHead + 1) % AUDIO_POOL_SIZE;
        }
    }

    // We just primed the queue – treat as baseline
    m_samplesPlayedBase = 0;
    m_fdsWaitingForFirstBuffer = false;
}

// Handle pause/unpause state transitions
void Cemulator::OnPauseStateChanged(bool paused) {
    if (!g_pSourceVoice) { 
        m_prevPaused = paused; 
        return; 
    }

    if (paused) {
        // Entering pause: keep the voice running with minimal priming
        // to avoid underrun, but don't add excessive latency.
        m_pauseSilenceBypass = true;       // tell UpdateAudio() not to latch/soft-reset
        
        // Check current queue depth - only prime if queue is low
        VoiceState st = GetVoiceState(g_pSourceVoice);
        if (st.queued < 2) {
            PrimeAudioQueue(1);  // minimal priming - just 1 block to prevent underrun
        }
        
        // Freeze resampler at 1.0 while paused (optional: avoids tiny drift)
        m_freqRatio = 1.0f;
        g_pSourceVoice->SetFrequencyRatio(1.0f);
    } else {
        // Leaving pause: DO NOT SoftReset. Check queue depth and only prime if needed.
        // The queue should still have buffers from when we paused, so minimal/no priming.
        VoiceState st = GetVoiceState(g_pSourceVoice);
        if (st.queued < kTargetBlocks) {
            // Only prime if queue is actually low - minimal priming to avoid latency
            PrimeAudioQueue(1);
        }
        // Shorter "ramp-in" after pause so recovery is snappy but stable.
        QueryPerformanceCounter(&m_lastResetQPC);
    }

    m_prevPaused = paused;
}

// Begin ROM change - called at start of ROM load
void Cemulator::BeginRomChange() {
    m_inRomChange = true;
    if (g_pSourceVoice) { 
        g_pSourceVoice->Stop(0); 
        g_pSourceVoice->FlushSourceBuffers(); 
    }
    m_expectedSamples = 0.0; 
    m_errI = 0.0; 
    m_silenceSamplesAcc = 0;
    m_inLongSilence = false; 
    m_fdsWaitingForFirstBuffer = true;
    // Warmup will be set in LoadGame after detecting FDS
    m_warmupBlocksToDrop = 0;
}

// End ROM change - called after ROM load completes
void Cemulator::EndRomChange() {
    // Voice may need a full recreate if sample rate changed or the resampler glitched
    HardRecreateSourceVoice();

    // Prime 2-3 blocks so we never start "dry"
    PrimeAudioQueue(m_isFDS ? 3 : 2);

    m_inRomChange = false;
    QueryPerformanceCounter(&m_lastResetQPC);
}

HRESULT Cemulator::InitInput() {
	//-------------------------------------------------------------------------------------
	// Init input
	//-------------------------------------------------------------------------------------
	return S_OK;
}

void Cemulator::UpdateInput() {
	//-------------------------------------------------------------------------------------
	// Get input from all the gamepads
	//-------------------------------------------------------------------------------------
	Input::GetInput(Gamepads);

	unsigned char pad[4];
	memset(pad, 0, sizeof(char) * 4);

	for (DWORD dwUser = 0; dwUser < 2; dwUser++) {
		if (!FCEUI_EmulationPaused()) {
			if (Gamepads[dwUser].fY1 > 0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_up;

			if (Gamepads[dwUser].fY1 < -0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_down;

			if (Gamepads[dwUser].fX1 > 0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_right;

			if (Gamepads[dwUser].fX1 < -0.3f)
				pad[dwUser] |= m_Settings.gamepad_dpad_left;

			// Use wButtons instead of wLastButtons for better frame cadence
			// (avoids frame-timing weirdness)
			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_UP)
				pad[dwUser] |= m_Settings.gamepad_dpad_up;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
				pad[dwUser] |= m_Settings.gamepad_dpad_down;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
				pad[dwUser] |= m_Settings.gamepad_dpad_left;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
				pad[dwUser] |= m_Settings.gamepad_dpad_right;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_A)
				pad[dwUser] |= m_Settings.gamepad_a;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_B)
				pad[dwUser] |= m_Settings.gamepad_b;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_X)
				pad[dwUser] |= m_Settings.gamepad_x;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_Y)
				pad[dwUser] |= m_Settings.gamepad_y;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
				pad[dwUser] |= m_Settings.gamepad_left_thumb;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
				pad[dwUser] |= m_Settings.gamepad_right_thumb;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				pad[dwUser] |= m_Settings.gamepad_left_shoulder;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				pad[dwUser] |= m_Settings.gamepad_right_shoulder;

			// When LT is serving rewind, don't pass it through to the NES pad
			if (!m_isRewinding && Gamepads[dwUser].bLeftTrigger >
									  XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				pad[dwUser] |= m_Settings.gamepad_left_trigger;

			if (Gamepads[dwUser].bRightTrigger >
				XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				pad[dwUser] |= m_Settings.gamepad_right_trigger;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_START)
				pad[dwUser] |= m_Settings.gamepad_start;

			if (Gamepads[dwUser].wButtons & XINPUT_GAMEPAD_BACK)
				pad[dwUser] |= m_Settings.gamepad_back;
		}
	}
	//-------------------------------------------------------------------------------------
	// Set input from all the gamepads
	//-------------------------------------------------------------------------------------
	// Check if Lua has overridden the input - if so, use Lua's value instead of hardware
	#ifdef USE_LUA
	extern void FCEU_LuaJoypadApply(void);
	// Store hardware input temporarily
	uint32 hardwarePowerpadbuf = pad[0] | pad[1] << 8;
	powerpadbuf = hardwarePowerpadbuf;
	// Apply Lua override (this will modify powerpadbuf if Lua has set joypad values)
	FCEU_LuaJoypadApply();
	#else
	powerpadbuf = pad[0] | pad[1] << 8; //| pad[2] << 16 | pad[3] << 24;;
	#endif
};

HRESULT Cemulator::InitSystem() {
	//-------------------------------------------------------------------------------------
	// Set up rendering texture
	//-------------------------------------------------------------------------------------
	nesBitmap = (unsigned int *)malloc(256 * 240 * sizeof(unsigned int));
	//-------------------------------------------------------------------------------------
	// Set up sound
	//-------------------------------------------------------------------------------------
	g_sound_buffer =
		(unsigned int *)malloc(SOUND_BUFFER_SIZE * sizeof(unsigned int));
	memset(g_sound_buffer, 0, SOUND_BUFFER_SIZE);

	//-------------------------------------------------------------------------------------
	// Read config
	//-------------------------------------------------------------------------------------
	extern Config fcecfg;

	// Load and save configuration
	ReadConfig();

	// Fetch configuration
	fcecfg.Find("sound", "enabled", m_Settings.sound);
	fcecfg.Find("sound", "rate", m_Settings.soundrate);
	fcecfg.Find("sound", "bufsize", m_Settings.soundbufsize);
	fcecfg.Find("sound", "volume", m_Settings.soundvolume);
	fcecfg.Find("sound", "trianglevolume", m_Settings.soundtrianglevolume);
	fcecfg.Find("sound", "square1volume", m_Settings.soundsquare1volume);
	fcecfg.Find("sound", "square2volume", m_Settings.soundsquare2volume);
	fcecfg.Find("sound", "noisevolume", m_Settings.soundnoisevolume);
	fcecfg.Find("sound", "pcmvolume", m_Settings.soundpcmvolume);

	// fcecfg.Find("video","region","NTSC"); //not used
	fcecfg.Find("video", "swfilter", m_Settings.SelectedGfxFilter);
	fcecfg.Find("video", "screenaspect", m_Settings.SelectedVertexFilter);

	// Default to cheap filter for performance (override expensive CPU filters)
	// User can still change this in settings, but baseline should be fast
	if (m_Settings.SelectedGfxFilter != gfx_normal &&
		m_Settings.SelectedGfxFilter != gfx_hq2x &&
		m_Settings.SelectedGfxFilter != gfx_hq3x &&
		m_Settings.SelectedGfxFilter != gfx_2xsai &&
		m_Settings.SelectedGfxFilter != gfx_super2sai &&
		m_Settings.SelectedGfxFilter != gfx_superEagle) {
		m_Settings.SelectedGfxFilter = gfx_normal; // Force baseline if invalid
	}

	// Lock to gfx_normal for performance - fancy CPU filters are expensive and
	// not worth it on 360
	m_Settings.SelectedGfxFilter = gfx_normal;

	// network
	fcecfg.Find("network", "enable", m_Settings.use_netplay);

	fcecfg.Find("controller", "XINPUT_GAMEPAD_DPAD_UP",
				m_Settings.gamepad_dpad_up);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_DPAD_DOWN",
				m_Settings.gamepad_dpad_down);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_DPAD_LEFT",
				m_Settings.gamepad_dpad_left);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_DPAD_RIGHT",
				m_Settings.gamepad_dpad_right);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_START", m_Settings.gamepad_start);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_BACK", m_Settings.gamepad_back);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_A", m_Settings.gamepad_a);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_B", m_Settings.gamepad_b);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_X", m_Settings.gamepad_x);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_Y", m_Settings.gamepad_y);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_LEFT_THUMB",
				m_Settings.gamepad_left_thumb);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_RIGHT_THUMB",
				m_Settings.gamepad_right_thumb);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_LEFT_SHOULDER",
				m_Settings.gamepad_left_shoulder);
	fcecfg.Find("controller", "XINPUT_GAMEPAD_RIGHT_SHOULDER",
				m_Settings.gamepad_right_shoulder);
	fcecfg.Find("controller", "XINPUT_LEFT_TRIGGER",
				m_Settings.gamepad_left_trigger);
	fcecfg.Find("controller", "XINPUT_RIGHT_TRIGGER",
				m_Settings.gamepad_right_trigger);

	// Lua scripting - auto-load all scripts by default
#ifdef USE_LUA
	extern void FCEU_LuaSetDisabled(int disabled);
	extern void FCEU_LuaKillAll(void);
	// Start with Lua enabled - scripts will auto-load when ROM is loaded
	FCEU_LuaSetDisabled(0);
	FCEU_LuaKillAll();
	printf("InitSystem: Lua enabled for auto-loading\n");
#endif

	return S_OK;
};

HRESULT Cemulator::LoadGame(std::string name, bool restart) {
	//-------------------------------------------------------------------------------------
	// Initilise emu
	//-------------------------------------------------------------------------------------
	FCEUI_Initialize();

	//-------------------------------------------------------------------------------------
	// Set some setting
	//-------------------------------------------------------------------------------------
	FCEUI_SetBaseDirectory("game:");

	// Set snapshot directory to game: drive
	static char snapDir[] = "game:\\snaps";
	FCEUI_SetDirOverride(FCEUIOD_SNAPS, snapDir);

	// Create snaps directory if it doesn't exist
	DWORD snapAttrib = GetFileAttributesA("game:\\snaps");
	if (snapAttrib == 0xFFFFFFFF || !(snapAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
		CreateDirectoryA("game:\\snaps", NULL);
	}
	
	// Warmup screenshot infrastructure to eliminate first-use stall
	WarmupZlibOnce();
	WarmupSnapshotFilesystemOnce("game:\\snaps");

	// Set save state directory to game: drive
	static char stateDir[] = "game:\\states";
	FCEUI_SetDirOverride(FCEUIOD_STATES, stateDir);
	
	// Create states directory if it doesn't exist
	DWORD stateAttrib = GetFileAttributesA("game:\\states");
	if (stateAttrib == 0xFFFFFFFF || !(stateAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
	CreateDirectoryA("game:\\states", NULL);
	}

	// Create user directories on hdd1 (always writable) for lua
	CreateDirectoryA("hdd1:\\fce360-enhanced", NULL);
	CreateDirectoryA("hdd1:\\fce360-enhanced\\lua", NULL);

	// Also try to create lua directory in game: (works if not read-only)
	CreateDirectoryA("game:\\lua", NULL);
	CreateDirectoryA("game:\\Lua", NULL);

	// CRITICAL: Disable Lua BEFORE ROM load to prevent any autoload during file
	// open
#ifdef USE_LUA
	extern void FCEU_LuaSetDisabled(int disabled);
	extern void FCEU_LuaKillAll(void);
	FCEU_LuaSetDisabled(1); // Hard disable before ROM load
	FCEU_LuaKillAll();		// Kill any running scripts
	printf("LoadGame: Lua disabled before ROM load to prevent autoload\n");
#endif

	FCEUI_SetVidSystem(0);

	// Apply settings
	FCEUI_Sound(m_Settings.soundrate);
	FCEUI_SetSoundVolume(m_Settings.soundvolume);
	FCEUI_SetLowPass(0);
	// FCEUI_SetSoundQuality(m_Settings.soundq);
	FCEUI_SetTriangleVolume(m_Settings.soundtrianglevolume);
	FCEUI_SetSquare1Volume(m_Settings.soundsquare1volume);
	FCEUI_SetSquare2Volume(m_Settings.soundsquare2volume);
	FCEUI_SetNoiseVolume(m_Settings.soundnoisevolume);
	FCEUI_SetPCMVolume(m_Settings.soundpcmvolume);

	//-------------------------------------------------------------------------------------
	// Load rom
	//-------------------------------------------------------------------------------------
	if (FCEUI_LoadGame(name.c_str(), 0) != NULL) {
		// Clear rewind buffer when loading a new game
		ClearRewindBuffer();

		// Ensure FileBase is set correctly from the ROM filename for proper
		// snapshot naming
		extern char FileBase[];

		// Manually extract the filename from the path
		std::string romPath = name;
		std::string romFilename;

		// Handle zip archive format: "path.zip|internal.nes"
		size_t pipePos = romPath.find('|');
		if (pipePos != std::string::npos) {
			romFilename = romPath.substr(pipePos + 1);
		} else {
			romFilename = romPath;
		}

		// Extract just the filename without path
		size_t lastSlash = romFilename.find_last_of("\\/");
		if (lastSlash != std::string::npos) {
			romFilename = romFilename.substr(lastSlash + 1);
		}

		// Remove extension (.nes, .zip, etc.)
		size_t lastDot = romFilename.find_last_of(".");
		if (lastDot != std::string::npos) {
			romFilename = romFilename.substr(0, lastDot);
		}

		// Set FileBase to the extracted filename (without extension)
		if (romFilename.length() > 0) {
			strncpy(FileBase, romFilename.c_str(), 2047);
			FileBase[2047] = '\0';
		}

		FCEUI_SetInput(0, SI_GAMEPAD, (void *)&powerpadbuf, 0);
		FCEUI_SetInput(1, SI_GAMEPAD, (void *)&powerpadbuf, 0);

		// set to ntsc
		extern FCEUGI *GameInfo;
		GameInfo->vidsys = GIV_NTSC;

		if (restart)
			ResetNES();

		if (m_Settings.use_netplay)
			FCEUD_NetworkConnect();

		// Auto-load all Lua scripts after ROM is loaded
#ifdef USE_LUA
		extern void FCEU_ApplyLuaMode(int mode, const char *scriptName);
		// Auto-load all scripts from known directories
		FCEU_ApplyLuaMode(Settings::LUA_AUTO_ALL, NULL);
		printf("LoadGame: Auto-loaded all Lua scripts\n");
#endif

		// Warmup screenshot path now that ROM is loaded and FileBase is set
		WarmupSnapshotPathAfterRomLoad();

		return S_OK;
	}

	return E_FAIL;
};

static void UploadNESFrame(const unsigned int* srcARGB, int width, int height)
{
    int next = (g_texWrite + 1) % 3;
    
    // If the next slot is still in use by the GPU, try the third one.
    // If that one is also busy, fall back to the current write index.
    if (g_texFence[next] && g_pd3dDevice->IsFencePending(g_texFence[next])) {
        int alt = (next + 1) % 3;
        if (!(g_texFence[alt] && g_pd3dDevice->IsFencePending(g_texFence[alt]))) {
            next = alt;
        } else {
            next = g_texWrite; // everything is busy; reuse current to avoid stall
        }
    }

    D3DLOCKED_RECT lr; ZeroMemory(&lr, sizeof(lr));
    if (SUCCEEDED(g_nesTex[next]->LockRect(0, &lr, NULL, 0))) {
        const unsigned char* src = (const unsigned char*)srcARGB;
        unsigned char*       dst = (unsigned char*)lr.pBits;
        const int srcStride = width * 4;
        for (int y = 0; y < height; ++y) {
            memcpy(dst + y * lr.Pitch, src + y * srcStride, srcStride);
        }
        g_nesTex[next]->UnlockRect(0);
    }
    g_texWrite = next;
}

void Cemulator::Render() {
	g_pd3dDevice->BeginScene();

	// Set effect variables as needed
	D3DXCOLOR colorMtrlDiffuse(1.0f, 1.0f, 1.0f, 1.0f);
	D3DXCOLOR colorMtrlAmbient(0.35f, 0.35f, 0.35f, 0);

	//-------------------------------------------------------------------------------------
	// Clear screen
	//-------------------------------------------------------------------------------------
	g_pd3dDevice->SetRenderTarget(0, m_pBackBuffer);
	g_pd3dDevice->SetDepthStencilSurface(m_pDepthBuffer);

	const D3DVECTOR4 clearColor = {0.f, 0.f, 0.f, 1.f};
	g_pd3dDevice->BeginTiling(0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1,
							  0);

	g_pd3dDevice->SetPredication(D3DPRED_TILE(0));
	// g_pd3dDevice->Clear( D3DCLEAR_TARGET1 | D3DCLEAR_TARGET2 );
	g_pd3dDevice->Clear(0L, NULL,
						D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET0 | D3DCLEAR_TARGET,
						D3DCOLOR_XRGB(70, 140, 255), 1.0f, 0L);
	g_pd3dDevice->SetPredication(0);

	//-------------------------------------------------------------------------------------
	// Setup technique
	//-------------------------------------------------------------------------------------
	ftime += 0.1f;

	// Use actual filtered texture dimensions for texel size (not base 256x240)
	// This ensures correct half-texel correction regardless of filter applied
	UINT texW = gfx_filter.GetCurrentWidth();
	UINT texH = gfx_filter.GetCurrentHeight();
	if (texW == 0)
		texW = GetWidth(); // Fallback if not initialized
	if (texH == 0)
		texH = GetHeight();
	g_pTexelSize[0] = 1.f / float(texW);
	g_pTexelSize[1] = 1.f / float(texH);

	// Half-texel correction on D3D9-style quads
	float uo = 0.5f / float(texW);
	float vo = 0.5f / float(texH);

	// Update vertex buffer with half-texel corrected coordinates
	TEXTURED *pVertices;
	if (SUCCEEDED(g_pVB->Lock(0, 0, (void **)&pVertices, 0))) {
		pVertices[0].u = 0.0f + uo; pVertices[0].v = 1.0f - vo;
		pVertices[1].u = 0.0f + uo; pVertices[1].v = 0.0f + vo;
		pVertices[2].u = 1.0f - uo; pVertices[2].v = 0.0f + vo;
		pVertices[3].u = 1.0f - uo; pVertices[3].v = 1.0f - vo;
		g_pVB->Unlock();
	}

	g_effect->SetValue(g_MaterialAmbientColor, &colorMtrlAmbient,
					   sizeof(D3DXCOLOR));
	g_effect->SetValue(g_MaterialDiffuseColor, &colorMtrlDiffuse,
					   sizeof(D3DXCOLOR));
	g_effect->SetValue(g_fTime, &ftime, sizeof(float));
	g_effect->SetMatrix(g_mWorldViewProjection, &g_matWorldViewProjection);
	g_effect->SetTexture(g_MeshTexture, g_nesTex[g_texDraw]);
	g_effect->SetTexture(g_bgTexture, g_bg_texture);
	g_effect->SetTechnique(g_technique_model);
	g_effect->SetFloatArray(g_TexelSize, g_pTexelSize, 2);

	//-------------------------------------------------------------------------------------
	// Render
	//-------------------------------------------------------------------------------------
	unsigned int iPass, cPasses;

	//-------------------------------------------------------------------------------------
	// Draw Bg
	//-------------------------------------------------------------------------------------
	g_effect->SetTechnique(g_technique_bg);
	g_effect->Begin(&cPasses, 0);
	g_pd3dDevice->SetVertexDeclaration(g_pVertexDecl);
	g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(TEXTURED));
	for (iPass = 0; iPass < cPasses; iPass++) {
		g_effect->BeginPass(iPass);
		// Use VB draw instead of DrawPrimitiveUP for better performance
		// (doesn't stall pipeline)
		g_pd3dDevice->DrawPrimitive(D3DPT_QUADLIST, 0, 1);
		g_effect->EndPass();
	}
	g_effect->End();

	//-------------------------------------------------------------------------------------
	// Draw Game
	//-------------------------------------------------------------------------------------
	if (RenderEmulation == true) {
		switch (m_Settings.SelectedVertexFilter) {
		case FullScreen:
			g_effect->SetTechnique(g_technique_model_fullscreen);
			break;
		case TvScreen:
			g_effect->SetTechnique(g_technique_model_tv);
			break;
		default:
			g_effect->SetTechnique(g_technique_model_tv);
			break;
		}

		// Always use latched texture if available, otherwise fall back to current draw texture
		// During rewind, we don't upload frames so just show the last latched frame
		const int texToUse = (g_texLatched >= 0 && g_texLatched < 3) ? g_texLatched : 
		                     ((g_texDraw >= 0 && g_texDraw < 3) ? g_texDraw : 0);
		g_effect->SetTexture(g_MeshTexture, g_nesTex[texToUse]);
		g_effect->Begin(&cPasses, 0);
		g_pd3dDevice->SetVertexDeclaration(g_pVertexDecl);
		g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(TEXTURED));
		for (iPass = 0; iPass < cPasses; iPass++) {
			g_effect->BeginPass(iPass);

			// NES layer must be pixel-perfect - re-assert after BeginPass to
			// override effect state
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,
										  D3DTADDRESS_CLAMP);
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,
										  D3DTADDRESS_CLAMP);

			// Don't blend the base NES layer
			g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

			// Use VB draw instead of DrawPrimitiveUP for better performance
			// (doesn't stall pipeline)
			g_pd3dDevice->DrawPrimitive(D3DPT_QUADLIST, 0, 1);
			// mesh->DrawSubset(0);
			g_effect->EndPass();
		}
		g_effect->End();
		
		// Fence after you submit the draw that samples it
		g_texFence[texToUse] = g_pd3dDevice->InsertFence();
	}

	//-------------------------------------------------------------------------------------
	// Ui
	//-------------------------------------------------------------------------------------
	// toujours
	RenderXui(g_pd3dDevice);

	//-------------------------------------------------------------------------------------
	// Affiche
	//-------------------------------------------------------------------------------------
	g_pd3dDevice->EndScene();

	// 1) Finish the composed scene into the current render slot (N)
	g_pd3dDevice->SetPredication(0);
	g_pd3dDevice->EndTiling(0, NULL, m_front[m_idxRender], NULL, 1, 0, NULL);

	// 2) Present the previous completed frame (N-1) at vsync for pacing
	if (m_idxDisplay >= 0) {
		// Synchronize to presentation interval before swapping (critical for proper VSync)
		g_pd3dDevice->SynchronizeToPresentationInterval();
		g_pd3dDevice->Swap(m_front[m_idxDisplay], NULL);
	}

	// We are at vblank boundary now (Swap returned). Decide if we latch a new frame.
	// This is the critical sync point - we only change the displayed frame at vblank.
	if (RenderEmulation) {
		if (m_isRewinding) {
			// During rewind, bypass drift logic entirely - we don't upload frames during rewind
			// Just keep showing the last latched frame until rewind ends
			g_pendingTex = -1;
		} else if (g_pendingTex >= 0) {
			// Normal playback: existing drift-controlled latch.
			// Track producer/consumer to keep cadence smooth
			g_frameDrift = g_framesProduced - g_framesDisplayed;

			// Periodic drift correction: if drift gets too large, reset to prevent overflow
			// and keep tracking accurate
			if (g_frameDrift > 100) {
				// Reset drift tracking (shouldn't happen in normal operation)
				g_framesProduced = g_framesDisplayed;
				g_frameDrift = 0;
			}

			// With maxLead = 0, we keep drift at exactly 0, eliminating all artifacts.
			// This means we'll skip a frame as soon as we produce one ahead of display.
			// With 60.0988 Hz vs 60 Hz, we accumulate ~1 frame every 10 seconds,
			// so we'll duplicate a frame roughly every 10 seconds to keep drift at 0.
			// Always latch if we don't have a latched texture yet (first frame or initialization)
			// OR if drift is exactly 0 (we're in sync)
			if (g_texLatched < 0 || g_frameDrift == 0) {
				// Normal case: latch the new frame at vblank (drift is 0, we're in sync)
				g_texLatched = g_pendingTex;
				g_pendingTex = -1;
				g_framesDisplayed++;
			} else {
				// We're ahead (drift > 0) - MUST skip this frame to prevent artifacts
				// Keep showing the previous g_texLatched for this vblank
				// Clear g_pendingTex since we're skipping it (new frame will set it if needed)
				// Don't increment g_framesDisplayed - we're showing the same frame again
				// This reduces drift: framesProduced stays same, framesDisplayed doesn't increase
				// Next vblank, drift will be reduced by 1
				g_pendingTex = -1;
			}
		} else if (g_texLatched < 0 && g_texDraw >= 0) {
			// First frame fallback: if no pending texture but we have a draw texture, use it
			g_texLatched = g_texDraw;
		}
	}

	// 3) Always rotate render/queued; update display only on new NES frame
	m_idxQueued  = m_idxRender;
	m_idxRender  = (m_idxRender + 1) % 3;
	
	// Update display buffer index for next frame (used for Swap)
	if (g_hasNewFrame) {
		if (m_idxDisplay < 0) {
			m_idxDisplay = m_idxQueued;
		} else {
			// Advance display buffer when we have a new complete NES frame
			m_idxDisplay = m_idxQueued;
		}
		g_hasNewFrame = false;
	} else if (m_idxDisplay < 0 && RenderEmulation) {
		// During emulation, only show frames when we have complete NES frames
		// (m_idxDisplay stays -1 until first frame)
	} else if (m_idxDisplay < 0) {
		// UI-only mode: allow first frame to display immediately
		m_idxDisplay = m_idxQueued;
	}
	
	// A/V sync: nudge audio rate to track vsync cadence (call once per vblank)
	SyncAudioQueue();
};

HRESULT Cemulator::Run() {
	//-------------------------------------------------------------------------------------
	// Lance l'application
	//-------------------------------------------------------------------------------------
	if (FAILED(InitVideo())) {
		printf("InitVideo failed\n");
		return E_FAIL;
	}
	// Load Configuration
	if (FAILED(InitSystem())) {
		printf("InitSystem failed\n");
		return E_FAIL;
	}
	if (FAILED(InitAudio())) {
		printf("InitAudio failed\n");
		return E_FAIL;
	}
	if (FAILED(InitInput())) {
		printf("InitInput failed\n");
		return E_FAIL;
	}

	InitUi(g_pd3dDevice, g_d3dpp);

	//-------------------------------------------------------------------------------------
	// Fixed-timestep emulation loop: run at true NTSC rate (60.0988 Hz),
	// present at vsync (60 Hz) with frame pacing to handle the mismatch
	//-------------------------------------------------------------------------------------
	// Use QueryPerformanceCounter for high-resolution timing on Xbox 360
	// Frequency is typically 3.2MHz on Xbox 360, so we convert to 100ns units
	// (10MHz)
	LARGE_INTEGER perfFreq, lastCounter, currentCounter;
	QueryPerformanceFrequency(&perfFreq);
	QueryPerformanceCounter(&lastCounter);

	// True NTSC frame rate: 60.0988118623484 Hz
	// Xbox display refresh: 60.0 Hz
	// The difference causes drift that becomes visible during motion
	static const double NTSC_HZ = 60.0988118623484;
	static const double DISPLAY_HZ = 60.0;
	static const double TICKS_PER_EMU_FRAME_D = (10000000.0 / NTSC_HZ); // 100ns units per emu frame
	static const double TICKS_PER_DISPLAY_FRAME_D = (10000000.0 / DISPLAY_HZ); // 100ns units per vsync
	
	ULONGLONG acc = 0;
	ULONGLONG displayAcc = 0; // Accumulator for display timing (vsync)
	g_framesProduced = 0; // Reset frame counters
	g_framesDisplayed = 0;

	gfx_filter.SetTextureDimension(GetWidth(), GetHeight());
	// filter from configuration - can be updated by xui
	gfx_filter.UseFilter(m_Settings.SelectedGfxFilter);

	// Fixed-timestep loop: run emulation at 60.0988 Hz, present at vsync (60Hz)
	// Always render to maintain responsive UI and smooth presentation
	int32 *snd;
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
        const ULONGLONG MAX_CATCHUP = (ULONGLONG)(TICKS_PER_EMU_FRAME_D * 2);
        if (acc > MAX_CATCHUP) acc = MAX_CATCHUP;

        lastCounter = currentCounter;

        // Run emu at true NTSC rate (60.0988 Hz)
        int safety = 0;

		while (acc >= (ULONGLONG)TICKS_PER_EMU_FRAME_D && safety < 4) {
			if (RenderEmulation) {
				// Input
				UpdateInput();

				// Check for console combo and screenshot buttons
				bool leftStickPressed = (Gamepads[0].wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
				bool rightStickPressed = (Gamepads[0].wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
				bool consoleCombo = leftStickPressed && rightStickPressed;

#ifdef USE_LUA
				// Lua Console toggle (LS + RS combo - both thumbsticks clicked)
				static WORD prevButtons = 0;
				WORD b = Gamepads[0].wButtons;
				bool prevConsoleCombo = (prevButtons & XINPUT_GAMEPAD_LEFT_THUMB) && (prevButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
				
				// Rising edge: toggle console
				if (consoleCombo && !prevConsoleCombo) {
					extern void FCEU_ToggleLuaConsole(void);
					extern void FCEU_LuaSetDisabled(int disabled);
					// Ensure Lua is enabled when toggling console
					FCEU_LuaSetDisabled(0);
					FCEU_ToggleLuaConsole();
					printf("Lua console toggled via LS+RS\n");
				}
				prevButtons = b;
#endif

				// Screenshot (RIGHT_THUMB button only - right stick click)
				// But ignore if both sticks are pressed (console combo)
				// Only take screenshot if right stick is pressed AND console combo is NOT active
				bool screenshotPressed = rightStickPressed && !consoleCombo;
				
				if(screenshotPressed && !m_screenshotLatch)
				{
					m_screenshotLatch = true;
					FCEUI_SaveSnapshot();
				}
				else if(!screenshotPressed)
				{
					m_screenshotLatch = false;
				}

				// Rewind (LT) with tap=single-step semantics + delayed
				// key-repeat
				static bool prevRewindPressed = false;
				static int rewindRepeatFrames = 0;

				const bool rewindPressed = (Gamepads[0].bLeftTrigger >
											XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
				const bool justPressed = rewindPressed && !prevRewindPressed;
				const bool justReleased = !rewindPressed && prevRewindPressed;

				// Count how long LT is held (in emu frames)
				m_rewindHeldFrames =
					rewindPressed ? (m_rewindHeldFrames + 1) : 0;

				if (rewindPressed) {
					if (!m_isRewinding && m_rewindCount > 0) {
						// IMPORTANT: Do NOT save a state here; we want a tap to
						// jump exactly one saved interval
						m_isRewinding = true;
						m_rewindFrameSkip = 0;
						m_rewindStartPos = m_rewindWritePos;
						rewindRepeatFrames = 0;
						// m_rewindHeldFrames is already 1 on first pressed
						// frame
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

							int repeatRate =
								4; // one step every 4 frames (~66ms)
							if (m_rewindHeldFrames >= 90)
								repeatRate = 1; // ~16ms per step
							else if (m_rewindHeldFrames >= 45)
								repeatRate = 2; // ~33ms per step

							if (rewindRepeatFrames >=
									REWIND_INITIAL_DELAY_FRAMES &&
								((rewindRepeatFrames -
								  REWIND_INITIAL_DELAY_FRAMES) %
								 repeatRate) == 0) {
								steps = 1;
							}
						}

						// Step back 'steps' times through the circular buffer
						for (int s = 0; s < steps; ++s) {
							if (!LoadRewindState()) {
								m_isRewinding = false;
								break;
							}
						}
						
						// Call Lua during rewind so scripts can detect rewind state
						// This runs every frame while rewinding (not just when steps occur)
#ifdef USE_LUA
						extern int FCEU_LuaIsDisabled(void);
						if (!FCEU_LuaIsDisabled()) {
							FCEU_LuaFrameBoundary();
							// Also call Lua GUI so scripts can draw and detect rewind state
							extern void FCEU_LuaGui(uint8* XBuf);
							FCEU_LuaGui(bitmap);
						}
#endif
						// (Audio intentionally skipped while rewinding;
						// Render() refreshes visuals - no frame upload during rewind for performance)
					}
					// Skip normal emu frame while rewinding
				} else if (m_isRewinding) {
					// Release: exit rewind cleanly and reset repeat timers
					m_isRewinding = false;
					m_rewindFrameSkip = 0;
					m_frameCounter = 0;
					rewindRepeatFrames = 0;
				} else {
					// Normal emulation
					// Fast-forward (RT) = 2× emu steps for this render
					bool fastForwardPressed = (Gamepads[0].bRightTrigger >
											   XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
					m_isFastForwarding = fastForwardPressed;
					int framesToRun = fastForwardPressed ? 2 : 1;

					// Cache input state before fast-forward loop to prevent
					// double-processing When multiple buttons are pressed
					// during fast-forward, we want both frames to use the same
					// input state (not re-read multiple times)
					uint32 cachedPowerpadbuf = powerpadbuf;

					for (int frame = 0; frame < framesToRun; ++frame) {
						// Restore cached input before each frame to ensure
						// consistent input across fast-forward frames
						powerpadbuf = cachedPowerpadbuf;
						FCEUI_Emulate(&bitmap, &snd, &sndsize, 0);

#ifdef USE_LUA
						// Only tick Lua if it's not disabled (auto-load is
						// always enabled)
						extern int FCEU_LuaIsDisabled(void);
						if (!FCEU_LuaIsDisabled()) {
							FCEU_LuaFrameBoundary();
						}
#endif
						// Save rewind state periodically (less frequently for
						// performance)
						if (++m_frameCounter >= REWIND_SAVE_INTERVAL) {
							m_frameCounter = 0;
							SaveRewindState();
						}

						if (frame == framesToRun - 1) {
#ifndef USE_LUA
							DrawTextTrans(bitmap + 4 * 256 + 4, 256,
										  (uint8 *)"LUA: N/A", 0x2E | 0x80);
#else
							extern int FCEU_LuaIsDisabled(void);
							extern const char *FCEU_LuaGetStatusMsg(void);
							const char *luaMsg = FCEU_LuaIsDisabled()
													 ? "LUA: OFF"
													 : FCEU_LuaGetStatusMsg();
							if (!luaMsg || !luaMsg[0])
								luaMsg = "LUA: ON";
							DrawTextTrans(bitmap + 4 * 256 + 4, 256,
										  (uint8 *)luaMsg, 0x2E | 0x80);

							// Run Lua GUI every frame (handles disabled state internally)
							// This ensures console and overlays are drawn properly
							// FCEU_LuaGui handles disabled checks and console visibility internally
							extern void FCEU_LuaGui(uint8* XBuf);
							FCEU_LuaGui(bitmap);
#endif

							// Set XBuf to bitmap for FCEU_PutImage (matches old build logic)
							// Call this AFTER Lua GUI so screenshots include overlays
							extern uint8 *XBuf;
							uint8 *oldXBuf = XBuf;
							XBuf = bitmap;
							
							// Call FCEU_PutImage to handle screenshot saving (matches old build)
							// This processes dosnapsave flag set by FCEUI_SaveSnapshot()
							// Note: FCEU_PutImage also calls FCEU_LuaGui internally, but we call it
							// separately above to ensure proper ordering and avoid double-drawing
							extern void FCEU_PutImage(void);
							FCEU_PutImage();
							
							XBuf = oldXBuf;  // Restore original XBuf pointer


							// Optimized ARGB conversion with precomputed LUT
							// NOTE: LUT uses full 8-bit palette index (0-255) -
							// no masking This allows overlay palette ranges
							// (0x80-0xBF normal, 0xC0-0xFF dimmed) to work
							// correctly
							static unsigned int lut[256];
							static bool lutInit = false;
							if (!lutInit) {
								extern pcpal pcpalette[256];
								for (int i = 0; i < 256; ++i) {
									lut[i] =
										(0xFF << 24) | (pcpalette[i].r << 16) |
										(pcpalette[i].g << 8) | pcpalette[i].b;
								}
								lutInit = true;
							}

							// Fast lookup table conversion (no branching per
							// pixel) Uses full 8-bit index from bitmap -
							// overlay palette ranges (0x80-0xFF) are properly
							// handled
							for (int i = 0; i < (256 * 240); ++i) {
								nesBitmap[i] =
									lut[bitmap[i]]; // Full 8-bit index, no
													// masking
							}

							gfx_filter.UpdateFilter(nesBitmap);
							// Upload the filtered frame to ring buffer
						unsigned int* filtered = gfx_filter.GetFilteredBuffer();
						if (filtered) {
							UploadNESFrame(filtered, gfx_filter.GetCurrentWidth(), gfx_filter.GetCurrentHeight());
							g_texDraw = g_texWrite;  // draw the one we just finished uploading
							g_hasNewFrame = true;    // mark that a complete NES frame is ready
							g_pendingTex = g_texDraw; // advertise "this is the next complete frame"
							g_framesProduced++; // Track that we produced a complete frame
						} else {
								// Fallback: upload base bitmap if filter buffer not available
							UploadNESFrame(nesBitmap, GetWidth(), GetHeight());
							g_texDraw = g_texWrite;
							g_hasNewFrame = true;
							g_pendingTex = g_texDraw; // advertise "this is the next complete frame"
							g_framesProduced++; // Track that we produced a complete frame
							}
							UpdateAudio(snd, sndsize);
						}
					}
				}
				
				// Update rewind edge tracker once per emu step (inside RenderEmulation block)
				prevRewindPressed = rewindPressed;
			}
			acc -= (ULONGLONG)TICKS_PER_EMU_FRAME_D;
			++safety;
			// g_framesProduced is incremented when g_pendingTex is set (frame uploaded)
		}

		// Swap() already blocks on vsync (D3DPRESENT_INTERVAL_ONE), so it naturally paces to 60 Hz
		// Track display timing - framesDisplayed is incremented in Render() when we actually advance
		
		UpdateVideo();
		Render(); // Swap() syncs to vsync - this naturally paces to 60 Hz
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

HRESULT Cemulator::CloseVideo() {
    if (m_front[0]) { m_front[0]->Release(); m_front[0] = NULL; }
    if (m_front[1]) { m_front[1]->Release(); m_front[1] = NULL; }
    if (m_front[2]) { m_front[2]->Release(); m_front[2] = NULL; }
	for (int i = 0; i < 3; ++i) {
		if (g_nesTex[i]) {
			g_nesTex[i]->Release();
			g_nesTex[i] = NULL;
		}
	}
	if (g_effect)         { g_effect->Release(); g_effect = NULL; }
	if (g_bg_texture)     { g_bg_texture->Release(); g_bg_texture = NULL; }
	if (g_pVB)            { g_pVB->Release(); g_pVB = NULL; }
	if (g_pVertexDecl)    { g_pVertexDecl->Release(); g_pVertexDecl = NULL; }
	if (m_pDepthBuffer)   { m_pDepthBuffer->Release(); m_pDepthBuffer = NULL; }
	if (m_pBackBuffer)    { m_pBackBuffer->Release(); m_pBackBuffer = NULL; }
	if (mesh)             { mesh->Release(); mesh = NULL; }
	if (g_pd3dDevice)     { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	if (g_pD3D)           { g_pD3D->Release(); g_pD3D = NULL; }

	return S_OK;
};
HRESULT Cemulator::CloseAudio() {
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

HRESULT Cemulator::CloseInput() { return S_OK; };

HRESULT Cemulator::CloseSystem() {
	ClearRewindBuffer();
	return S_OK;
};

//-------------------------------------------------------------------------------------
// Rewind System Implementation
//-------------------------------------------------------------------------------------
void Cemulator::InitRewindBuffer() {
	m_rewindWritePos = 0;
	m_rewindCount = 0;
	m_frameCounter = 0;
	m_isRewinding = false;
	m_rewindFrameSkip = 0;
	m_rewindStartPos = 0;
	m_rewindHeldFrames = 0;
	m_isFastForwarding = false;

	for (int i = 0; i < REWIND_BUFFER_SIZE; i++) {
		m_rewindBuffer[i].isValid = false;
		m_rewindBuffer[i].stateData.clear();
	}
}

void Cemulator::ClearRewindBuffer() {
	for (int i = 0; i < REWIND_BUFFER_SIZE; i++) {
		m_rewindBuffer[i].isValid = false;
		m_rewindBuffer[i].stateData.clear();
	}
	m_rewindWritePos = 0;
	m_rewindCount = 0;
	m_frameCounter = 0;
	m_isRewinding = false;
	m_rewindStartPos = 0;
	m_rewindHeldFrames = 0;
	m_isFastForwarding = false;
}

void Cemulator::SaveRewindState() {
	extern FCEUGI *GameInfo;
	if (!GameInfo || FCEUI_EmulationPaused())
		return;

	// Don't save states while rewinding - we're going backwards through saved
	// states
	if (m_isRewinding)
		return;

	// Create a memory stream to save the state
	EMUFILE_MEMORY ms;
	if (!FCEUSS_SaveMS(&ms, Z_NO_COMPRESSION))
		return;

	// Copy the state data to the buffer
	RewindState &state = m_rewindBuffer[m_rewindWritePos];
	state.stateData.assign((uint8 *)ms.buf(), (uint8 *)ms.buf() + ms.size());
	state.isValid = true;

	// Update buffer position (circular)
	m_rewindWritePos = (m_rewindWritePos + 1) % REWIND_BUFFER_SIZE;

	// Update count (don't exceed buffer size)
	if (m_rewindCount < REWIND_BUFFER_SIZE)
		m_rewindCount++;
	// If buffer is full, it wraps around automatically
}

bool Cemulator::LoadRewindState() {
	if (!m_isRewinding)
		return false;

	// Calculate position of the previous state (one step back)
	int readPos =
		(m_rewindWritePos - 1 + REWIND_BUFFER_SIZE) % REWIND_BUFFER_SIZE;

	// Make sure we have a valid state to read
	if (!m_rewindBuffer[readPos].isValid)
		return false;

	// Calculate how many states we've rewound from the start
	// When we started, m_rewindStartPos was the position AFTER saving current
	// state So the state we're currently at (before rewinding this step) is at	
	// m_rewindWritePos We want to check if we can go back one more
	int stepsBack;
	if (readPos <= m_rewindStartPos) {
		// No wrap - simple subtraction
		stepsBack = m_rewindStartPos - readPos;
	} else {
		// Wrapped around - readPos is near start of buffer, m_rewindStartPos is
		// later
		stepsBack = (REWIND_BUFFER_SIZE - readPos) + m_rewindStartPos;
	}

	// We can rewind up to m_rewindCount states (the number we had before saving
	// at start) But we saved one state at the start, so we subtract 1
	if (stepsBack >= m_rewindCount) {
		// Can't rewind further - we've reached the oldest saved state
		return false;
	}

	// Create a memory stream from the saved state
	std::vector<uint8> &stateData = m_rewindBuffer[readPos].stateData;
	EMUFILE_MEMORY ms(&stateData);

	// Reset the memory stream to the beginning
	ms.fseek(0, SEEK_SET);

	// Load the state
	if (!FCEUSS_LoadFP(&ms, SSLOADPARAM_NOBACKUP))
		return false;

	// Update write position to the state we just loaded
	// This way the next rewind will go back one more step
	m_rewindWritePos = readPos;

	return true;
}
