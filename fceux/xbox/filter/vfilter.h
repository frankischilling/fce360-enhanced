// prototypes
#include <stdint.h>

void filter_hq2x_32( unsigned char * srcPtr, DWORD srcPitch, unsigned char * dstPtr, int width, int height);
void filter_hq3x_32( unsigned char * srcPtr,  DWORD srcPitch, unsigned char * dstPtr, int width, int height);
void filter_Scale2x_ex8(unsigned char *srcPtr, DWORD srcPitch, unsigned char  *dstPtr, int width, int height);
void filter_Scale3x_ex8(unsigned char *srcPtr, DWORD srcPitch, unsigned char  *dstPtr, int width, int height);
void filter_Std2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch, unsigned char *dstBitmap, int width, int height);
void filter_Super2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch, unsigned char  *dstBitmap, int width, int height);
void filter_SuperEagle_ex8(unsigned char *srcPtr, DWORD srcPitch, unsigned char  *dstBitmap, int width, int height);