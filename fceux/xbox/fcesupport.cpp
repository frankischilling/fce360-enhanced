#include "fceusupport.h"


bool turbo = false;


FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord& asr, std::string &fname, int innerIndex) { return 0; }
FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord& asr, std::string& fname, std::string* innerFilename) { return 0; }
ArchiveScanRecord FCEUD_ScanArchive(std::string fname) { return ArchiveScanRecord(); }

// Need something to hold the PC palette
pcpal pcpalette[256];

void FCEUD_SetPalette(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
    pcpalette[index].r = r;
    pcpalette[index].g = g;
    pcpalette[index].b = b;
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