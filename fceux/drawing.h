#include <assert.h>

void DrawTextLineBG(uint8 *dest);
void DrawMessage(bool beforeMovie);
void FCEU_DrawRecordingStatus(uint8* XBuf);
void FCEU_DrawNumberRow(uint8 *XBuf, int *nstatus, int cur);
void DrawTextTrans(uint8 *dest, uint32 width, uint8 *textmsg, uint8 fgcolor);
void DrawTextTransWH(uint8 *dest, uint32 width, uint8 *textmsg, uint8 fgcolor, int max_w, int max_h, int border);
// Backwards-compatible entry point (legacy signature originated pre-refactor).
// It draws starting at (0,0) relative to the supplied buffer pointer.
void DrawTextTransScaled(uint8 *dest, uint32 width, uint8 *textmsg, uint8 fgcolor, float scaleX, float scaleY);
// New signature that takes an explicit origin relative to the start of the buffer.
void DrawTextTransScaled(uint8 *dest, uint32 width, int startX, int startY, uint8 *textmsg, uint8 fgcolor, float scaleX, float scaleY);
