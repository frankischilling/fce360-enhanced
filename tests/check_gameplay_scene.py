"""Compile the production gameplay scene against a small XUI navigation shim."""
from pathlib import Path
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
tabs = ET.parse(root / 'fceux/media/xui/LoadGame.xui').findall('./XuiTabScene/XuiScene')
assert tabs[1].findtext('Properties/ClassOverride') == 'XuiRunner'
assert tabs[2].findtext('Properties/ClassOverride') == 'Osd'
source = (root / 'fceux/xbox/ui/mainui.cpp').read_text(encoding='utf-8')
scene = source[source.index(' class CXuiEmulationScene:'):source.index(' typedef struct _LIST_ITEM_INFO')]
shim = r'''
#include <cassert>
#include <cstddef>
#include "gameplay_shortcuts.h"
#define XUI_IMPLEMENT_CLASS(a,b,c)
#define XUI_BEGIN_MSG_MAP()
#define XUI_ON_XM_INIT(a)
#define XUI_ON_XM_LEAVE_TAB(a)
#define XUI_ON_XM_ENTER_TAB(a)
#define XUI_END_MSG_MAP()
#define FAILED(hr) ((hr) < 0)
#define S_OK 0
typedef int HRESULT;
typedef bool BOOL;
struct XUIMessageInit {};
struct CXuiSceneImpl { int m_hObj; };
static int selected = -1, navigations = 0, captures = 0, toggles = 0;
static bool enabled = false;
struct CXuiTabScene {
    CXuiTabScene(int) {}
    int GetParent(CXuiTabScene*) { return 0; }
    void EnableTabbing(bool b) { enabled = b; }
    void Goto(int n) { assert(enabled); selected = n; ++navigations; }
    void GotoNext() { assert(false); }
    void GotoPrev() { assert(false); }
};
struct Cemulator {
    bool RenderEmulation;
    struct Settings { enum { LUA_AUTO_ALL }; };
} emul;
struct GAMEPAD { unsigned short wButtons; bool bConnected; };
struct Input {
    static GAMEPAD m_Gamepads[4];
    static void GetInput(void*) {}
};
GAMEPAD Input::m_Gamepads[4] = {};
void FCEUI_SaveSnapshot() { ++captures; }
void FCEU_ToggleLuaConsole() { ++toggles; }
void FCEU_LuaSetDisabled(int n) { assert(n == 0); }
void FCEU_ApplyLuaMode(int, const char*) {}
'''
test = r'''
int main() {
    XuiRunner scene;
    BOOL handled = false;
    scene.OnInit(NULL, handled);
    scene.OnEnterTab(handled);
    Input::m_Gamepads[0].bConnected = true;
    Input::m_Gamepads[0].wButtons = 0xf0;
    scene.UpdatePerFrame();
    assert(selected == 2 && navigations == 1 && !enabled);
    assert(captures == 0 && toggles == 0);
    scene.OnLeaveTab(handled);
    // OSD draws the game too. This must not reactivate gameplay shortcuts.
    emul.RenderEmulation = true;
    scene.UpdatePerFrame();
    Input::m_Gamepads[0].wButtons = 0;
    scene.UpdatePerFrame();
    Input::m_Gamepads[0].wButtons = 0xc0;
    scene.UpdatePerFrame();
    assert(captures == 0 && toggles == 0 && navigations == 1);
    scene.OnEnterTab(handled);
    scene.UpdatePerFrame();
    assert(toggles == 0);
    Input::m_Gamepads[0].wButtons = 0;
    scene.UpdatePerFrame();
    Input::m_Gamepads[0].wButtons = 0xc0;
    scene.UpdatePerFrame();
    assert(toggles == 1 && captures == 0);
    Input::m_Gamepads[0].wButtons = 0;
    scene.UpdatePerFrame();
    Input::m_Gamepads[0].wButtons = 0x80;
    scene.UpdatePerFrame();
    scene.UpdatePerFrame();
    assert(captures == 1);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'scene.cpp'
    exe = Path(tmp) / ('scene.exe' if os.name == 'nt' else 'scene')
    cpp.write_text(shim + scene + test)
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++98', '-DUSE_LUA',
                    '-I' + str(root / 'fceux/xbox'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production gameplay scene and OSD tab tests passed')
