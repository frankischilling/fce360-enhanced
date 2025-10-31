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
// MSAA surface
//-------------------------------------------------------------------------------------
	D3DSURFACE_PARAMETERS params = {0};
	g_pd3dDevice->CreateRenderTarget( g_dwTileWidth, g_dwTileHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pBackBuffer, &params );
	params.Base = m_pBackBuffer->Size / GPU_EDRAM_TILE_SIZE;
	params.HierarchicalZBase = D3DHIZFUNC_GREATER_EQUAL;

	g_pd3dDevice->CreateDepthStencilSurface( g_dwTileWidth, g_dwTileHeight, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pDepthBuffer, &params );
	g_pd3dDevice->CreateTexture( g_dwFrameWidth, g_dwFrameHeight, 1, 0, D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer, NULL );
	if( FAILED( g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_BUFFER_2_FRAMES, &g_d3dpp, &g_pd3dDevice ) ) )
	{
		printf("CreateDevice failed\n");
        return E_FAIL;
	}
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
		D3DXSHADER_DEBUG , 
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
	if(FAILED( g_pXAudio2->CreateSourceVoice(&g_pSourceVoice,(WAVEFORMATEX*)&wfx, XAUDIO2_VOICE_NOSRC | XAUDIO2_VOICE_NOPITCH , 1.0f, &XAudio2_Notifier)	))
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
	if(sndsize==0)
		return;

//-------------------------------------------------------------------------------------
// Rebuild sound stream
//-------------------------------------------------------------------------------------	
	unsigned short sample;
	unsigned int *dst = (unsigned int *)g_sound_buffer;
	
	for(int i = 0;i<sndsize;i++)
	{
		sample = snd[i] & 0xffff;
		g_sound_buffer[snd_written++]=sample | ( sample << 16);
		if(snd_written==sndsize)
		{
			snd_written=0;

			//-------------------------------------------------------------------------------------
			// Submit sound - fait une copie par secu - free par la callback
			//-------------------------------------------------------------------------------------	
			unsigned int * nes_sound = (unsigned int *)malloc(sndsize * sizeof(int));
			XMemCpy(nes_sound,g_sound_buffer,sndsize* sizeof(int));
			g_SoundBuffer.AudioBytes = sndsize * sizeof(int);	//size of the audio buffer in bytes
			g_SoundBuffer.pAudioData = (BYTE*)nes_sound;//;		//buffer containing audio data
			g_SoundBuffer.pContext = (BYTE*)nes_sound;
			g_SoundBuffer.Flags = XAUDIO2_END_OF_STREAM;

			//-------------------------------------------------------------------------------------
			// Send sound stream
			//-------------------------------------------------------------------------------------	
			if( FAILED(g_pSourceVoice->SubmitSourceBuffer( &g_SoundBuffer ) ) )
			{
				printf("SubmitSourceBuffer failed\n");
				return ;
			}
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

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_DPAD_UP)
				pad[dwUser] |= m_Settings.gamepad_dpad_up;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN)
				pad[dwUser] |= m_Settings.gamepad_dpad_down;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT)
				pad[dwUser] |= m_Settings.gamepad_dpad_left;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
				pad[dwUser] |= m_Settings.gamepad_dpad_right;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_A)
				pad[dwUser] |= m_Settings.gamepad_a;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_B)
				pad[dwUser] |= m_Settings.gamepad_b;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_X)
				pad[dwUser] |= m_Settings.gamepad_x;
			
			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_Y)
				pad[dwUser] |= m_Settings.gamepad_y;
				
			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_LEFT_THUMB)
				pad[dwUser] |= m_Settings.gamepad_left_thumb;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
				pad[dwUser] |= m_Settings.gamepad_right_thumb;

			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				pad[dwUser] |= m_Settings.gamepad_left_shoulder;
			
			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				pad[dwUser] |= m_Settings.gamepad_right_shoulder;
				
			if(Gamepads[dwUser].bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				pad[dwUser] |= m_Settings.gamepad_left_trigger;
				
			if(Gamepads[dwUser].bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				pad[dwUser] |= m_Settings.gamepad_right_trigger;
				
			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_START)
				pad[dwUser] |= m_Settings.gamepad_start;
			
			if(Gamepads[dwUser].wLastButtons & XINPUT_GAMEPAD_BACK)
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
		FCEUI_SetInput(0, SI_GAMEPAD, (void*)&powerpadbuf, 0);
		FCEUI_SetInput(1, SI_GAMEPAD, (void*)&powerpadbuf, 0);

		//set to ntsc
		extern FCEUGI * GameInfo;
		GameInfo->vidsys=GIV_NTSC;

		if(restart)
			ResetNES();

		if(m_Settings.use_netplay)
			FCEUD_NetworkConnect();

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
		g_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, g_VerticesTextured, sizeof( TEXTURED ) );
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
		for( iPass = 0; iPass < cPasses; iPass++ )
		{
			g_effect->BeginPass( iPass );
			g_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, g_VerticesTextured, sizeof( TEXTURED ) );
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
    g_pd3dDevice->SynchronizeToPresentationInterval();
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
// Looop :D
//-------------------------------------------------------------------------------------	
	if(true)
	{
		int32 * snd;
		int32 sndsize;
		
		gfx_filter.SetTextureDimension(GetWidth(), GetHeight());
		//filter from configuration - can be updated by xui
		gfx_filter.UseFilter( m_Settings.SelectedGfxFilter );

		while(end==false)
		{
			if(RenderEmulation == true)
			{
				FCEUI_Emulate(&bitmap, &snd, &sndsize, 0);
				for(int i = 0;i<(256*240);i++)
				{
					//Make an ARGB bitmap
					nesBitmap[i] = ( (pcpalette[bitmap[i]].r) << 16) | ( (pcpalette[bitmap[i]].g) << 8 ) | ( pcpalette[bitmap[i]].b ) | ( 0xFF << 24 );
				}
				
				gfx_filter.UpdateFilter(nesBitmap);

				UpdateAudio(snd, sndsize);
				UpdateInput();
			}
			UpdateVideo();
			Render();
		}
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
	g_pMasteringVoice->DestroyVoice();
	g_pSourceVoice->DestroyVoice();
	free(&g_SoundBuffer);
	return S_OK;	
};

HRESULT Cemulator::CloseInput()
{
	return S_OK;	
};

HRESULT Cemulator::CloseSystem()
{
	return S_OK;	
};

