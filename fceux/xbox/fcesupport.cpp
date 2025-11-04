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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "fceusupport.h"


bool turbo = false;


FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord& asr, std::string &fname, int innerIndex) { return 0; }
FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord& asr, std::string& fname, std::string* innerFilename) { return 0; }
ArchiveScanRecord FCEUD_ScanArchive(std::string fname) { return ArchiveScanRecord(); }

// Need something to hold the PC palette
pcpal pcpalette[256];

// Dimming function: reduces color intensity to ~60% for overlay backgrounds
static inline void dim_color(uint8 r, uint8 g, uint8 b, uint8* out_r, uint8* out_g, uint8* out_b) {
    // gentle dim: ~60% (multiply by 3, divide by 5)
    *out_r = (uint8)((r * 3) / 5);
    *out_g = (uint8)((g * 3) / 5);
    *out_b = (uint8)((b * 3) / 5);
}

void FCEUD_SetPalette(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
    pcpalette[index].r = r;
    pcpalette[index].g = g;
    pcpalette[index].b = b;
    
    // Mirror color into overlay range (0x80-0xBF) so 0x80|color shows up
    // This makes overlay pixels (used by DrawTextTrans) visible
    if (index < 128) {
        pcpalette[index | 0x80].r = r;
        pcpalette[index | 0x80].g = g;
        pcpalette[index | 0x80].b = b;
        
        // Also populate dimmed overlay range (0xC0-0xFF) for bordered text backgrounds
        // This is used by DrawTextTransWH when border > 0 (0xC1, 0xD1, 0xCF palette indices)
        uint8 dim_r, dim_g, dim_b;
        dim_color(r, g, b, &dim_r, &dim_g, &dim_b);
        pcpalette[index | 0xC0].r = dim_r;
        pcpalette[index | 0xC0].g = dim_g;
        pcpalette[index | 0xC0].b = dim_b;
    }
}

void FCEUD_GetPalette(unsigned char i, unsigned char *r, unsigned char *g, unsigned char *b) {
    *r = pcpalette[i].r;
    *g = pcpalette[i].g;
    *b = pcpalette[i].b;
}


/**
 * Closes a game.  Frees memory, and deinitializes the drivers.
 */
int CloseGame()
{
    FCEUI_CloseGame();
    GameInfo = 0;
    return(1);
}

// File Control
FILE *FCEUD_UTF8fopen(const char *n, const char *m)
{
    return(fopen(n,m));
}

EMUFILE_FILE* FCEUD_UTF8_fstream(const char *n, const char *m)
{
        std::ios_base::openmode mode = std::ios_base::binary;
	if(!strcmp(m,"r") || !strcmp(m,"rb"))
		mode |= std::ios_base::in;
	else if(!strcmp(m,"w") || !strcmp(m,"wb"))
		mode |= std::ios_base::out | std::ios_base::trunc;
	else if(!strcmp(m,"a") || !strcmp(m,"ab"))
		mode |= std::ios_base::out | std::ios_base::app;
	else if(!strcmp(m,"r+") || !strcmp(m,"r+b"))
		mode |= std::ios_base::in | std::ios_base::out;
	else if(!strcmp(m,"w+") || !strcmp(m,"w+b"))
		mode |= std::ios_base::in | std::ios_base::out | std::ios_base::trunc;
	else if(!strcmp(m,"a+") || !strcmp(m,"a+b"))
		mode |= std::ios_base::in | std::ios_base::out | std::ios_base::app;
    return new EMUFILE_FILE(n, m);
}

bool FCEUD_ShouldDrawInputAids()
{
	return false;
}


void FCEUD_VideoChanged()
{
}
/*
// Netplay
int FCEUD_SendData(void *data, unsigned int len)
{
    return 1;
}

int FCEUD_RecvData(void *data, unsigned int len)
{
    return 0;
}

void FCEUD_NetworkClose(void)
{
}
*/
void FCEUD_NetplayText(unsigned char *text)
{
	printf("%s",text);
}
#undef DUMMY
#define DUMMY(f) void f(void) { };
DUMMY(FCEUD_HideMenuToggle)
DUMMY(FCEUD_TurboOn)
DUMMY(FCEUD_TurboOff)
DUMMY(FCEUD_TurboToggle)
DUMMY(FCEUD_SaveStateAs)
DUMMY(FCEUD_LoadStateFrom)
DUMMY(FCEUD_MovieRecordTo)
DUMMY(FCEUD_MovieReplayFrom)
void FCEUD_LuaRunFrom(void) {
#ifdef USE_LUA
	// Respect UI: autoloading is handled in Cemulator::LoadGame()
	// Leave this stub empty so legacy calls from the core do nothing.
	printf("FCEUD_LuaRunFrom: disabled (UI controls autoload mode)\n");
#endif
}
DUMMY(FCEUD_ToggleStatusIcon)
DUMMY(FCEUD_DebugBreakpoint)
DUMMY(FCEUD_SoundToggle)
DUMMY(FCEUD_AviRecordTo)
DUMMY(FCEUD_AviStop)
void FCEUI_AviVideoUpdate(const unsigned char* buffer) { }
int FCEUD_ShowStatusIcon(void) { return 0; }
bool FCEUI_AviIsRecording(void) { return 0; }
bool FCEUI_AviDisableMovieMessages() { return true; }
const char *FCEUD_GetCompilerString() { return NULL; }
void FCEUI_UseInputPreset(int preset) { }
void FCEUD_SoundVolumeAdjust(int n) { }
void FCEUD_SetEmulationSpeed(int cmd) { }