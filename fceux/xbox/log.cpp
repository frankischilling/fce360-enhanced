#include <xtl.h>

void FCEUD_PrintError(const char *errormsg)
{
        //if(GuiLoaded())
        //      ErrorPrompt(errormsg);
	OutputDebugStringA(errormsg);
}

void FCEUD_Message(const char *text)
{
	OutputDebugStringA(text);
}