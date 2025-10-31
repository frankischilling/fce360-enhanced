#include "fceux\git.h"
#include "fceux\driver.h"
#include "fceux\video.h"
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <xuihtml.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <algorithm>
#include "..\Cemulator.h"
#include "..\config_reader.h"
#include "..\input.h"

extern Cemulator emul;//smsplus_pc.cpp

std::wstring strtowstr(std::string str)
{
	wchar_t buffer[1000];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 1000);
	std::wstring s = buffer;
	return s;
}
	
class CXuiEmulationScene: public CXuiSceneImpl{
protected:
	void EnableTab(bool enable){
		 // Get the parent tabbed scene.
		CXuiTabScene tabbedScene( m_hObj );
		if( FAILED( tabbedScene.GetParent( &tabbedScene ) ) )
		{
			return;
		}
		else
		{
			tabbedScene.EnableTabbing(enable);
		}
	}
	void GoToNext()
	{
		 // Get the parent tabbed scene.
		CXuiTabScene tabbedScene( m_hObj );
		if( FAILED( tabbedScene.GetParent( &tabbedScene ) ) )
		{
			return;
		}
		else
		{
			EnableTab(true);
			tabbedScene.GotoNext();
			EnableTab(false);
		}
	}
	void Goto(int n)
	{
		CXuiTabScene tabbedScene( m_hObj );
		if( FAILED( tabbedScene.GetParent( &tabbedScene ) ) )
		{
			return;
		}
		else
		{
			EnableTab(true);
			tabbedScene.Goto(n);
			EnableTab(false);
		}

	}
	void GotoPrev()
	{
		 // Get the parent tabbed scene.
		CXuiTabScene tabbedScene( m_hObj );
		if( FAILED( tabbedScene.GetParent( &tabbedScene ) ) )
		{
			return;
		}
		else
		{
			EnableTab(true);
			tabbedScene.GotoPrev();
			EnableTab(false);
		}
	}
};

// forward
class XuiRunner;
static XuiRunner* g_EmuSceneInstance = NULL;

class XuiRunner:public CXuiEmulationScene{
/*
	Display (nothing) when emulation is running
*/
	bool m_comboLatch;  // prevents re-trigger while still held
	
public:
	XUI_IMPLEMENT_CLASS( XuiRunner, L"XuiRunner", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_LEAVE_TAB ( OnLeaveTab )
		XUI_ON_XM_ENTER_TAB ( OnEnterTab )
    XUI_END_MSG_MAP()

    // Called once per frame from RenderXui()
    void UpdatePerFrame()
    {
        // Only act while we're actually "in game"
        if (!emul.RenderEmulation) return;

        Input::GetInput(NULL);
        GAMEPAD* pad = Input::GetMergedInput(0, NULL);
        if (!pad) return;

        const bool both =
            (pad->wButtons & XINPUT_GAMEPAD_START) &&
            (pad->wButtons & XINPUT_GAMEPAD_BACK); // "select" on 360

        if (both && !m_comboLatch) {
            m_comboLatch = true;
            // In your tab order, XuiRunner -> OSD is "next"
            GoToNext();                 // opens the OSD tab
            // OSD::OnEnterTab() pauses emulation already
        }
        if (!both) {
            m_comboLatch = false;       // reset latch when released
        }
    }
	
	HRESULT OnEnterTab( BOOL &bHandled){
		emul.RenderEmulation = true;//run emulation
		return S_OK;
	}

	HRESULT OnLeaveTab( BOOL &bHandled){
		emul.RenderEmulation = false;//stop emulation
		return S_OK;
	}

    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
		EnableTab(false);
		m_comboLatch = false;  // initialize latch
		g_EmuSceneInstance = this;              // expose instance
		
        return S_OK;
    }
};

typedef struct _LIST_ITEM_INFO {
	LPCWSTR pwszText;
	LPCWSTR pwszImage;
	BOOL fChecked;
	BOOL fEnabled;
} LIST_ITEM_INFO;

LIST_ITEM_INFO g_ListData[] = {
	{ L"gfx_normal",		L"gfx_normal.png",		FALSE, TRUE },
	{ L"gfx_hq2x",			L"gfx_hq2x.png",		FALSE, TRUE },
	{ L"gfx_hq3x",			L"gfx_hq3x.png",		FALSE, TRUE },
	{ L"gfx_2xsai",			L"gfx_2xsai.png",		FALSE, TRUE },
	{ L"gfx_super2sai",		L"gfx_super2sai.png",   FALSE, TRUE },
	{ L"gfx_superEagle",	L"gfx_superEagle.png",	FALSE, TRUE },
};

#define LIST_ITEM_COUNT (sizeof(g_ListData)/sizeof(LIST_ITEM_INFO))


class Osd: public CXuiEmulationScene
{
private:
	CXuiSlider XuiSaveStateSlot;

	CXuiControl	XuiSaveState;
	CXuiControl	XuiLoadState;
	CXuiControl XuiReset;
	CXuiControl XuiBack;
	CXuiControl XuiLoadGame;

	CXuiList XuiSwFilter;
	CXuiCheckbox XuiFullScreen;
	CXuiControl XuiSwSelFilter;

	typedef struct s_save_item{
		std::string filename;
		std::wstring affichage;
		std::wstring snapshot;
	} save_item;

	//list des roms
	std::vector<save_item> m_save_list;
public:
	XUI_IMPLEMENT_CLASS( Osd, L"Osd", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    		XUI_ON_XM_LEAVE_TAB ( OnLeaveTab )
		XUI_ON_XM_ENTER_TAB ( OnEnterTab )
    XUI_END_MSG_MAP()
	
	HRESULT OnEnterTab( BOOL &bHandled){
		emul.RenderEmulation = true;//run emulation
		FCEUI_SetEmulationPaused(1);
		return S_OK;
	}

	HRESULT OnLeaveTab( BOOL &bHandled){
		emul.RenderEmulation = false;//stop emulation
		FCEUI_SetEmulationPaused(0);

		emul.m_Settings.SelectedVertexFilter = XuiFullScreen.IsChecked()?0:1;

		extern Config fcecfg;
		fcecfg.Set("video","swfilter", emul.m_Settings.SelectedGfxFilter);
		fcecfg.Set("video","screenaspect", emul.m_Settings.SelectedVertexFilter);
		fcecfg.Save("game:\\fceui.ini");

		return S_OK;
	}

    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
		EnableTab(false);

		HRESULT hr = GetChildById( L"XuiSaveStateSlot", &XuiSaveStateSlot );
		hr = GetChildById( L"XuiSwSelFilter", &XuiSwSelFilter );
		hr = GetChildById( L"XuiSaveState", &XuiSaveState );
		hr = GetChildById( L"XuiBack", &XuiBack );
		hr = GetChildById( L"XuiLoadState", &XuiLoadState );
		hr = GetChildById( L"XuiReset", &XuiReset );
		hr = GetChildById( L"XuiLoadGame", &XuiLoadGame );
		hr = GetChildById( L"XuiFullScreen", &XuiFullScreen );
		hr = GetChildById( L"XuiSwFilter", &XuiSwFilter );

		if(!emul.m_Settings.SelectedVertexFilter)
			XuiFullScreen.SetCheck(TRUE);
		else
			XuiFullScreen.SetCheck(FALSE);

		XuiSwSelFilter.SetText(g_ListData[emul.m_Settings.SelectedVertexFilter].pwszText );
		XuiSwFilter.SetCurSel(emul.m_Settings.SelectedVertexFilter);

		XuiSwFilter.InsertItems(0,LIST_ITEM_COUNT);

		for(int i = 0;i<LIST_ITEM_COUNT;i++){
			XuiSwFilter.SetText(i, g_ListData[i].pwszText);
		};
		
        return S_OK;
    }

	bool FileExiste(char * filename){
		HANDLE hFile = CreateFileA(filename,               // file to open
           GENERIC_READ,          // open for reading
           FILE_SHARE_READ,       // share for reading
           NULL,                  // default security
           OPEN_EXISTING,         // existing file only
           FILE_ATTRIBUTE_NORMAL, // normal file
           NULL);                 // no attr. template

		bool ret = (hFile != NULL);

		CloseHandle(hFile);

		return ret;
	};

	void ScanSlot(){
#if 0
		//scan for 10 slot
		extern FCEUGI * GameInfo;
		//compute the md5 of file to a string
		char state_name[512];
		char rom_name[512];

		strcpy(rom_name,"");
		for(int x=0;x<16;x++)
			sprintf(rom_name, "%s%02x",rom_name,GameInfo->MD5[x]);
	
		int nbslot = 0;
		for(int i = 0;i<10;i++){
			sprintf(state_name, "game:\\states\\%s-%d.sav",rom_name,i);
			if(FileExiste(state_name)==false)
			{
				save_item item;
				item.affichage=L"";
				item.filename = state_name;
				//item.s
				m_save_list.push_back(item);
				nbslot = i;
				break;
			}
		}
			
		XuiSlotSelector.InsertItems(0, nbslot);
		for(int j=0;j<nbslot;j++)
		{
			XuiSlotSelector.SetText(j,L"test");
			XuiSlotSelector.SetImage(j,L"test");
		}
#endif
	};


	//----------------------------------------------------------------------------------
	// Name: OnNotifyPress
	// Desc: Handler for the button press message.
	//----------------------------------------------------------------------------------
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
	{
		HRESULT hr = S_OK;
		if(hObjPressed == XuiSwFilter){
			emul.m_Settings.SelectedGfxFilter = XuiSwFilter.GetCurSel();
			emul.gfx_filter.UseFilter( emul.m_Settings.SelectedGfxFilter );
			bHandled = TRUE;
		}

		if( hObjPressed == XuiSaveState )
		{
			int val = 0;
			char state_name[512];
			char rom_name[512];
	
			extern FCEUGI * GameInfo;

			XuiSaveStateSlot.GetValue(&val);

			strcpy(rom_name,"");
			for(int x=0;x<16;x++)
				sprintf(rom_name, "%s%02x",rom_name,GameInfo->MD5[x]);

			sprintf(state_name, "game:\\states\\%s-%d.sav",rom_name,val);
			
			FCEUI_SaveState(state_name);
			bHandled = TRUE;
		}
		if( hObjPressed == XuiLoadState )
		{
			int val = 0;
			char state_name[512];
			char rom_name[512];
	
			XuiSaveStateSlot.GetValue(&val);

			extern FCEUGI * GameInfo;

			strcpy(rom_name,"");
			for(int x=0;x<16;x++)
				sprintf(rom_name, "%s%02x",rom_name,GameInfo->MD5[x]);

			sprintf(state_name, "game:\\states\\%s-%d.sav",rom_name,val);
		
			FCEUI_LoadState(state_name);

			bHandled = TRUE;
		}
		if( hObjPressed == XuiReset )
		{
			FCEUI_ResetNES();
			bHandled = TRUE;
		}
		if( hObjPressed == XuiBack )
		{
			GotoPrev();
			bHandled = TRUE;
		}
		if( hObjPressed == XuiLoadGame)
		{
			Goto(0);
			bHandled = TRUE;
		}
		return S_OK;
	}

};

class LoadGame; // forward declaration
static LoadGame* g_LoadGameInstance = NULL;

class LoadGame: public CXuiEmulationScene
{
private:
	CXuiList XuiRomList;

	typedef struct s_rom_item{
		std::string path;
		std::string filename;
		std::wstring affichage;
	} rom_item;

	//list des roms
	std::vector<rom_item> m_rom_list;

    // Scroll state
    DWORD m_lastMoveTick;
    DWORD m_holdStartTick;
    int   m_lastDir; // -1, 0, +1
    
    // Tuning
    int   m_initialDelayMs;    // delay before first repeat when held
    int   m_repeatIntervalMs;  // cadence while held
    int   m_minDwellMs;        // hard minimum time between row changes
    float m_deadzone;          // analog deadzone
    int   m_pageSize;          // for LB/RB paging
    
    // RS time-based acceleration state
    int   m_rsHoldDir;        // -1, 0, +1
    DWORD m_rsHoldStartTick;  // when the current RS hold began
    DWORD m_rsLastInjectTick; // last time we injected a ScrollBy() from RS
    
    // LS/DPAD (left stick or dpad) time-based acceleration
    int   m_navHoldDir;        // -1, 0, +1
    DWORD m_navHoldStartTick;  // when current LS/DPAD hold began
    DWORD m_navLastInjectTick; // last injected ScrollBy() from LS/DPAD

public:
	XUI_IMPLEMENT_CLASS( LoadGame, L"RomChoose", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

	//----------------------------------------------------------------------------------
	// Name: OnNotifyPress
	// Desc: Handler for the button press message.
	//----------------------------------------------------------------------------------
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
	{
		HRESULT hr = S_OK;
		if( hObjPressed == XuiRomList )
		{
			std::string sRom = m_rom_list.at(XuiRomList.GetCurSel()).path;
			sRom += m_rom_list.at(XuiRomList.GetCurSel()).filename;
			emul.LoadGame( sRom ,true);

			GoToNext();
		}
		return S_OK;
	}
    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
		EnableTab(false);

        HRESULT hr = GetChildById( L"XuiRomList", &XuiRomList );
        if( FAILED( hr ) )
		{
			return hr;
		}
		else
		{
			XuiRomList.DeleteItems(0, XuiRomList.GetItemCount());
			//XuiRomList.addi
			if(!FAILED(ScanDir()))
			{
				std::vector<rom_item>::iterator it;
				XuiRomList.InsertItems(0, m_rom_list.size());
				int i = 0;
				for ( it=m_rom_list.begin() ; it < m_rom_list.end(); it++ )
				{
					XuiRomList.SetText(i, (*it).affichage.c_str());
					i++;
				}
			}
		}

        // init scroll config/state
        m_lastMoveTick = 0;
        m_holdStartTick = 0;
        m_lastDir = 0;

        // Tuned for "fast but precise"
        m_initialDelayMs = 180;   // was 220 — faster initial response
        m_repeatIntervalMs = 70;  // was 85 — quicker repeat cadence
        m_minDwellMs = 50;        // was 70 — faster but safe minimum
        m_deadzone = 0.25f;      // was ~0.15 — tiny deflections won't repeat
        m_pageSize = 12;          // was 8 — bigger page steps
        
        // Initialize RS time-based acceleration state
        m_rsHoldDir        = 0;
        m_rsHoldStartTick  = 0;
        m_rsLastInjectTick = 0;
        
        // Initialize LS/DPAD time-based acceleration state
        m_navHoldDir        = 0;
        m_navHoldStartTick  = 0;
        m_navLastInjectTick = 0;

        // expose instance for per-frame updates from RenderXui
        g_LoadGameInstance = this;

        return S_OK;
    }
    
    void Page(int dir /* +1 = up page, -1 = down page */)
    {
        int count = XuiRomList.GetItemCount();
        if (count <= 0) return;

        int cur = XuiRomList.GetCurSel();
        int next = cur + (dir > 0 ? -m_pageSize : +m_pageSize);
        if (next < 0) next = 0;
        if (next >= count) next = count - 1;

        if (next != cur)
        {
            XuiRomList.SetCurSel(next);

            // keep selection roughly centered
            int visible = 10;
            int top = next - (visible / 2);
            if (top < 0) top = 0;
            int maxTop = (count > visible) ? (count - visible) : 0;
            if (top > maxTop) top = maxTop;
            XuiRomList.SetTopItem(top);
        }
    }

    void UpdatePerFrame()
    {
        Input::GetInput(NULL);
        GAMEPAD* pad = Input::GetMergedInput(0, NULL);
        if (!pad) return;

        DWORD now = GetTickCount();

        // 1) PAGING (LB/RB) — repeats while held
        static DWORD lastPageTick = 0;
        const DWORD pageRepeatMs  = 100;
        if (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
        {
            if (now - lastPageTick >= pageRepeatMs) { Page(+1); lastPageTick = now; }
            return;
        }
        if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        {
            if (now - lastPageTick >= pageRepeatMs) { Page(-1); lastPageTick = now; }
            return;
        }

        // ---- RIGHT STICK acceleration (selection) ----
        const float RS_DEADZONE = 0.28f;
        float rsY = pad->fY2;              // up = +, down = -
        int   rsDir = 0;
        if      (rsY >  RS_DEADZONE) rsDir = +1;
        else if (rsY < -RS_DEADZONE) rsDir = -1;

        if (rsDir == 0)
        {
            // Reset RS state if neutral
            m_rsHoldDir        = 0;
            m_rsHoldStartTick  = 0;
            m_rsLastInjectTick = 0;
        }
        else
        {
            if (m_rsHoldDir != rsDir || m_rsHoldStartTick == 0)
            {
                m_rsHoldDir        = rsDir;
                m_rsHoldStartTick  = now;
                m_rsLastInjectTick = 0; // allow immediate inject
            }

            DWORD heldMs = now - m_rsHoldStartTick;

            DWORD intervalMs;
            int steps;

            // Time → speed tiers (friendly → fast, with hard cap)
            if      (heldMs < 200)  { intervalMs = 150; steps = 1; }
            else if (heldMs < 450)  { intervalMs = 115; steps = 1; }
            else if (heldMs < 800)  { intervalMs =  85; steps = 1; }
            else if (heldMs < 1200) { intervalMs =  65; steps = 2; }
            else if (heldMs < 1800) { intervalMs =  50; steps = 2; }
            else if (heldMs < 2600) { intervalMs =  40; steps = 3; }
            else                    { intervalMs =  35; steps = 3; } // cap

            // Fine control by deflection magnitude
            float mag = fabsf(rsY);
            if      (mag < 0.55f) steps = 1;
            else if (mag < 0.80f) steps = (steps > 1 ? 2 : 1);
            // >= 0.80f keeps computed steps

            if (m_rsLastInjectTick == 0 || (now - m_rsLastInjectTick) >= intervalMs)
            {
                ScrollBy(rsDir, steps);
                m_rsLastInjectTick = now;
            }

            // RS path owns movement this frame (prevents double-ramp when both sticks used)
            return;
        }

        // ---- LS/DPAD acceleration (adds speed on top of XUI's own repeat) ----
        const float LS_DEADZONE = 0.28f;
        bool upHeld   = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)   || (pad->fY1 >  LS_DEADZONE);
        bool downHeld = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || (pad->fY1 < -LS_DEADZONE);
        int  navDir   = upHeld ? +1 : (downHeld ? -1 : 0);

        if (navDir == 0)
        {
            m_navHoldDir        = 0;
            m_navHoldStartTick  = 0;
            m_navLastInjectTick = 0;
            return;
        }

        if (m_navHoldDir != navDir || m_navHoldStartTick == 0)
        {
            m_navHoldDir        = navDir;
            m_navHoldStartTick  = now;
            m_navLastInjectTick = 0;
            // Let XUI do the first few repeats before we start piling on.
            return;
        }

        DWORD heldMs = now - m_navHoldStartTick;

        // Wait a short moment so the first moves are precise,
        // then start injecting extra steps to "go faster the longer you hold".
        const DWORD accelStartMs = 280; // start adding extra after ~0.28s
        if (heldMs < accelStartMs) return;

        // Time → speed tiers for LS/DPAD (slightly gentler than RS)
        DWORD intervalMs;
        int steps;
        if      (heldMs < 700)   { intervalMs = 120; steps = 1; }
        else if (heldMs < 1200)  { intervalMs =  90; steps = 2; }
        else if (heldMs < 2000)  { intervalMs =  65; steps = 2; }
        else if (heldMs < 3000)  { intervalMs =  50; steps = 3; }
        else                     { intervalMs =  45; steps = 3; } // cap

        // Scale a bit by LS magnitude for analog feel
        float lsMag = fabsf(pad->fY1);
        if      (lsMag < 0.55f) steps = 1;
        else if (lsMag < 0.80f) steps = (steps > 1 ? 2 : 1);

        // Minimum dwell to avoid micro-jitter double steps
        const DWORD minDwell = 40;
        if (m_navLastInjectTick == 0 || (now - m_navLastInjectTick) >= (std::max)(minDwell, intervalMs))
        {
            ScrollBy(navDir, steps);   // extra steps on top of XUI's own repeats
            m_navLastInjectTick = now;
        }
    }

    void ScrollBy( int dir, int step )
    {
        if( step <= 0 ) return;
        int count = XuiRomList.GetItemCount();
        if( count <= 0 ) return;
        int cur = XuiRomList.GetCurSel();
        int next = cur + (dir > 0 ? -step : +step); // XUI list: up is smaller index
        if( next < 0 ) next = 0;
        if( next >= count ) next = count - 1;
        if( next != cur )
        {
            XuiRomList.SetCurSel( next );

            // Keep selection in view by adjusting the top item.
            int visible = 10; // conservative fallback; XUI doesn't expose a query here
            int top = next - (visible / 2);
            if( top < 0 ) top = 0;
            int maxTop = (count > visible) ? (count - visible) : 0;
            if( top > maxTop ) top = maxTop;
            XuiRomList.SetTopItem( top );
        }
    }
private:
	HRESULT ScanDir()
	{
		HANDLE				hFind;                   // Handle to file
		WIN32_FIND_DATA	FileInformation;         // File information

		m_rom_list.clear();

		hFind = FindFirstFile( "game:\\roms\\*", &FileInformation );
		if( hFind != INVALID_HANDLE_VALUE )
		{
			do
			{
				{
					if(!(FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ))
					{
						if(
							strstr(FileInformation.cFileName,".nes") ||
							//strstr(FileInformation.cFileName,".gg") ||
							strstr(FileInformation.cFileName,".zip")
						)
						{
							rom_item s;
							s.path="game:\\roms\\";
							s.filename= FileInformation.cFileName;
							s.affichage = strtowstr(FileInformation.cFileName);
							m_rom_list.push_back(s);
						}
					}
				}
			}
			while( FindNextFile( hFind, &FileInformation ) == TRUE );
			FindClose( hFind );
		}
		//Tri alphabetic
		return S_OK;
	}
};

//--------------------------------------------------------------------------------------
// Main xui 
//--------------------------------------------------------------------------------------
class XboxUI : public CXuiModule
{
	public:
		XboxUI()
        {
        }
        ~XboxUI()
        {
        }
	protected:
		HRESULT RegisterXuiClasses(){
			XuiVideoRegister();
			XuiSoundXACTRegister();
			XuiSoundXAudioRegister();
			XuiHtmlRegister();

			LoadGame::Register();
			Osd::Register();
			XuiRunner::Register();
			return S_OK;
		};
		HRESULT UnregisterXuiClasses(){
			XuiVideoUnregister();
			XuiSoundXACTUnregister();
			XuiSoundXAudioUnregister();
			XuiHtmlUnregister();

			LoadGame::Unregister();
			Osd::Unregister();
			XuiRunner::Unregister();
			return S_OK;
		};
};

XboxUI gUi;

HRESULT InitUi(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS d3dpp)
{
	HRESULT hr = gUi.InitShared( pDevice, &d3dpp, XuiPNGTextureLoader );
    if( FAILED( hr ) )
        return hr;

	 // Register a default typeface
    hr = gUi.RegisterDefaultTypeface( L"Arial", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        return hr;
    hr = gUi.LoadSkin( L"file://game:/media/ui.xzp#media\\xui\\skin_default.xur" );
    if( FAILED( hr ) )
        return hr;

    //hr = gUi.LoadFirstScene( L"file://game:/media/ui.xzp#media\\xui\\", L"main.xur" );
	hr = gUi.LoadFirstScene( L"file://game:/media/ui.xzp#media\\xui\\", L"LoadGame.xur" );
    if( FAILED( hr ) )
        return hr;

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Update the UI
//--------------------------------------------------------------------------------------

HRESULT RenderXui(IDirect3DDevice9* pDevice)
{
	//update ui
	gUi.RunFrame();

	// per-frame accelerated scrolling for ROM list, if the scene is active
	if( g_LoadGameInstance )
	{
		g_LoadGameInstance->UpdatePerFrame();
	}
	if( g_EmuSceneInstance )
	{
		g_EmuSceneInstance->UpdatePerFrame();
	}

	XuiTimersRun();

	XuiRenderBegin( gUi.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    D3DXMATRIX matOrigView;
    XuiRenderGetViewTransform( gUi.GetDC(), &matOrigView );

	XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender( &msg, &msgRender, gUi.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
    XuiSendMessage( gUi.GetRootObj(), &msg );

    XuiRenderSetViewTransform( gUi.GetDC(), &matOrigView );

    XuiRenderEnd( gUi.GetDC() );
	
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

	return S_OK;
}