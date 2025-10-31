#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <xtl.h>
#include "interp.h"

int finalw,finalh;


void * pSaISmallBuff=NULL;
void * pSaIBigBuff=NULL;

#define GET_RESULT(A, B, C, D) ((A != C || A != D) - (B != C || B != D))

static __inline int GetResult1(DWORD A, DWORD B, DWORD C, DWORD D, DWORD E)
{
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r+=1; 
	if (y <= 1) r-=1;
	return r;
}

static __inline int GetResult2(DWORD A, DWORD B, DWORD C, DWORD D, DWORD E) 
{
	int x = 0; 
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r-=1; 
	if (y <= 1) r+=1;
	return r;
}

#define colorMask8     0x00FEFEFE
#define lowPixelMask8  0x00010101
#define qcolorMask8    0x00FCFCFC
#define qlowpixelMask8 0x00030303

#define INTERPOLATE8(A, B) ((((A & colorMask8) >> 1) + ((B & colorMask8) >> 1) + (A & B & lowPixelMask8)))
#define Q_INTERPOLATE8(A, B, C, D) (((((A & qcolorMask8) >> 2) + ((B & qcolorMask8) >> 2) + ((C & qcolorMask8) >> 2) + ((D & qcolorMask8) >> 2) \
	+ ((((A & qlowpixelMask8) + (B & qlowpixelMask8) + (C & qlowpixelMask8) + (D & qlowpixelMask8)) >> 2) & qlowpixelMask8))))


void filter_Super2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch,
					unsigned char  *dstBitmap, int width, int height)
{
	DWORD dstPitch        = srcPitch<<1;
	DWORD srcPitchHalf    = srcPitch>>1;
	int   finWidth        = srcPitch>>2;
	DWORD line;
	DWORD *dP;
	DWORD *bP;
	int iXA,iXB,iXC,iYA,iYB,iYC,finish;
	DWORD color4, color5, color6;
	DWORD color1, color2, color3;
	DWORD colorA0, colorA1, colorA2, colorA3,
		colorB0, colorB1, colorB2, colorB3,
		colorS1, colorS2;
	DWORD product1a, product1b,
		product2a, product2b;

	finalw=width<<1;
	finalh=height<<1;

	line = 0;

	{
		for (; height; height-=1)
		{
			bP = (DWORD *)srcPtr;
			dP = (DWORD *)(dstBitmap + line*dstPitch);
			for (finish = width; finish; finish -= 1 )
			{
				//---------------------------------------    B1 B2
				//                                         4  5  6 S2
				//                                         1  2  3 S1
				//                                           A1 A2
				if(finish==finWidth) iXA=0;
				else                 iXA=1;
				if(finish>4) {iXB=1;iXC=2;}
				else
					if(finish>3) {iXB=1;iXC=1;}
					else         {iXB=0;iXC=0;}
					if(line==0)  {iYA=0;}
					else         {iYA=finWidth;}
					if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
					else
						if(height>3) {iYB=finWidth;iYC=finWidth;}
						else         {iYB=0;iYC=0;}

						colorB0 = *(bP- iYA - iXA);
						colorB1 = *(bP- iYA);
						colorB2 = *(bP- iYA + iXB);
						colorB3 = *(bP- iYA + iXC);

						color4 = *(bP  - iXA);
						color5 = *(bP);
						color6 = *(bP  + iXB);
						colorS2 = *(bP + iXC);

						color1 = *(bP  + iYB  - iXA);
						color2 = *(bP  + iYB);
						color3 = *(bP  + iYB  + iXB);
						colorS1= *(bP  + iYB  + iXC);

						colorA0 = *(bP + iYC - iXA);
						colorA1 = *(bP + iYC);
						colorA2 = *(bP + iYC + iXB);
						colorA3 = *(bP + iYC + iXC);

						if (color2 == color6 && color5 != color3)
						{
							product2b = product1b = color2;
						}
						else
							if (color5 == color3 && color2 != color6)
							{
								product2b = product1b = color5;
							}
							else
								if (color5 == color3 && color2 == color6)
								{
									register int r = 0;

									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));

									if (r > 0)
										product2b = product1b = color6;
									else
										if (r < 0)
											product2b = product1b = color5;
										else
										{
											product2b = product1b = INTERPOLATE8(color5, color6);
										}
								}
								else
								{
									if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
										product2b = Q_INTERPOLATE8 (color3, color3, color3, color2);
									else
										if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
											product2b = Q_INTERPOLATE8 (color2, color2, color2, color3);
										else
											product2b = INTERPOLATE8 (color2, color3);

									if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
										product1b = Q_INTERPOLATE8 (color6, color6, color6, color5);
									else
										if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
											product1b = Q_INTERPOLATE8 (color6, color5, color5, color5);
										else
											product1b = INTERPOLATE8 (color5, color6);
								}

								if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
									product2a = INTERPOLATE8(color2, color5);
								else
									if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
										product2a = INTERPOLATE8(color2, color5);
									else
										product2a = color2;

								if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
									product1a = INTERPOLATE8(color2, color5);
								else
									if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
										product1a = INTERPOLATE8(color2, color5);
									else
										product1a = color5;

								*dP=product1a;
								*(dP+1)=product1b;
								*(dP+(srcPitchHalf))=product2a;
								*(dP+1+(srcPitchHalf))=product2b;

								bP += 1;
								dP += 2;
			}//end of for ( finish= width etc..)

			line += 2;
			srcPtr += srcPitch;
		}; //endof: for (; height; height--)
	}
}

////////////////////////////////////////////////////////////////////////

void filter_Std2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch,
				  unsigned char *dstBitmap, int width, int height)
{
	DWORD dstPitch        = srcPitch<<1;
	DWORD srcPitchHalf    = srcPitch>>1;
	int   finWidth        = srcPitch>>2;
	DWORD line;
	DWORD *dP;
	DWORD *bP;
	int iXA,iXB,iXC,iYA,iYB,iYC,finish;

	DWORD colorA, colorB;
	DWORD colorC, colorD,
		colorE, colorF, colorG, colorH,
		colorI, colorJ, colorK, colorL,
		colorM, colorN, colorO, colorP;
	DWORD product, product1, product2;

	finalw=width<<1;
	finalh=height<<1;

	line = 0;

	{
		for (; height; height-=1)
		{
			bP = (DWORD *)srcPtr;
			dP = (DWORD *)(dstBitmap + line*dstPitch);
			for (finish = width; finish; finish -= 1 )
			{
				//---------------------------------------
				// Map of the pixels:                    I|E F|J
				//                                       G|A B|K
				//                                       H|C D|L
				//                                       M|N O|P
				if(finish==finWidth) iXA=0;
				else                 iXA=1;
				if(finish>4) {iXB=1;iXC=2;}
				else
					if(finish>3) {iXB=1;iXC=1;}
					else         {iXB=0;iXC=0;}
					if(line==0)  {iYA=0;}
					else         {iYA=finWidth;}
					if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
					else
						if(height>3) {iYB=finWidth;iYC=finWidth;}
						else         {iYB=0;iYC=0;}

						colorI = *(bP- iYA - iXA);
						colorE = *(bP- iYA);
						colorF = *(bP- iYA + iXB);
						colorJ = *(bP- iYA + iXC);

						colorG = *(bP  - iXA);
						colorA = *(bP);
						colorB = *(bP  + iXB);
						colorK = *(bP + iXC);

						colorH = *(bP  + iYB  - iXA);
						colorC = *(bP  + iYB);
						colorD = *(bP  + iYB  + iXB);
						colorL = *(bP  + iYB  + iXC);

						colorM = *(bP + iYC - iXA);
						colorN = *(bP + iYC);
						colorO = *(bP + iYC + iXB);
						colorP = *(bP + iYC + iXC);


						if((colorA == colorD) && (colorB != colorC))
						{
							if(((colorA == colorE) && (colorB == colorL)) ||
								((colorA == colorC) && (colorA == colorF) && 
								(colorB != colorE) && (colorB == colorJ)))
							{
								product = colorA;
							}
							else
							{
								product = INTERPOLATE8(colorA, colorB);
							}

							if(((colorA == colorG) && (colorC == colorO)) ||
								((colorA == colorB) && (colorA == colorH) && 
								(colorG != colorC) && (colorC == colorM)))
							{
								product1 = colorA;
							}
							else
							{
								product1 = INTERPOLATE8(colorA, colorC);
							}
							product2 = colorA;
						}
						else
							if((colorB == colorC) && (colorA != colorD))
							{
								if(((colorB == colorF) && (colorA == colorH)) ||
									((colorB == colorE) && (colorB == colorD) && 
									(colorA != colorF) && (colorA == colorI)))
								{
									product = colorB;
								}
								else
								{
									product = INTERPOLATE8(colorA, colorB);
								}

								if(((colorC == colorH) && (colorA == colorF)) ||
									((colorC == colorG) && (colorC == colorD) && 
									(colorA != colorH) && (colorA == colorI)))
								{
									product1 = colorC;
								}
								else
								{
									product1=INTERPOLATE8(colorA, colorC);
								}
								product2 = colorB;
							}
							else
								if((colorA == colorD) && (colorB == colorC))
								{
									if (colorA == colorB)
									{
										product = colorA;
										product1 = colorA;
										product2 = colorA;
									}
									else
									{
										register int r = 0;
										product1 = INTERPOLATE8(colorA, colorC);
										product = INTERPOLATE8(colorA, colorB);

										r += GetResult1 (colorA&0x00FFFFFF, colorB&0x00FFFFFF, colorG&0x00FFFFFF, colorE&0x00FFFFFF, colorI&0x00FFFFFF);
										r += GetResult2 (colorB&0x00FFFFFF, colorA&0x00FFFFFF, colorK&0x00FFFFFF, colorF&0x00FFFFFF, colorJ&0x00FFFFFF);
										r += GetResult2 (colorB&0x00FFFFFF, colorA&0x00FFFFFF, colorH&0x00FFFFFF, colorN&0x00FFFFFF, colorM&0x00FFFFFF);
										r += GetResult1 (colorA&0x00FFFFFF, colorB&0x00FFFFFF, colorL&0x00FFFFFF, colorO&0x00FFFFFF, colorP&0x00FFFFFF);

										if (r > 0)
											product2 = colorA;
										else
											if (r < 0)
												product2 = colorB;
											else
											{
												product2 = Q_INTERPOLATE8(colorA, colorB, colorC, colorD);
											}
									}
								}
								else
								{
									product2 = Q_INTERPOLATE8(colorA, colorB, colorC, colorD);

									if ((colorA == colorC) && (colorA == colorF) && 
										(colorB != colorE) && (colorB == colorJ))
									{
										product = colorA;
									}
									else
										if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
										{
											product = colorB;
										}
										else
										{
											product = INTERPOLATE8(colorA, colorB);
										}

										if ((colorA == colorB) && (colorA == colorH) && 
											(colorG != colorC) && (colorC == colorM))
										{
											product1 = colorA;
										}
										else
											if ((colorC == colorG) && (colorC == colorD) && 
												(colorA != colorH) && (colorA == colorI))
											{
												product1 = colorC;
											}
											else
											{
												product1 = INTERPOLATE8(colorA, colorC);
											}
								}

								//////////////////////////

								*dP=colorA;
								*(dP+1)=product;
								*(dP+(srcPitchHalf))=product1;
								*(dP+1+(srcPitchHalf))=product2;

								bP += 1;
								dP += 2;
			}//end of for ( finish= width etc..)

			line += 2;
			srcPtr += srcPitch;
		}; //endof: for (; height; height--)
	}
}

////////////////////////////////////////////////////////////////////////

void filter_SuperEagle_ex8(unsigned char *srcPtr, DWORD srcPitch,
					unsigned char  *dstBitmap, int width, int height)
{
	DWORD dstPitch        = srcPitch<<1;
	DWORD srcPitchHalf    = srcPitch>>1;
	int   finWidth        = srcPitch>>2;
	DWORD line;
	DWORD *dP;
	DWORD *bP;
	int iXA,iXB,iXC,iYA,iYB,iYC,finish;
	DWORD color4, color5, color6;
	DWORD color1, color2, color3;
	DWORD colorA1, colorA2, 
		colorB1, colorB2,
		colorS1, colorS2;
	DWORD product1a, product1b,
		product2a, product2b;

	finalw=width<<1;
	finalh=height<<1;

	line = 0;

	{
		for (; height; height-=1)
		{
			bP = (DWORD *)srcPtr;
			dP = (DWORD *)(dstBitmap + line*dstPitch);
			for (finish = width; finish; finish -= 1 )
			{
				if(finish==finWidth) iXA=0;
				else                 iXA=1;
				if(finish>4) {iXB=1;iXC=2;}
				else
					if(finish>3) {iXB=1;iXC=1;}
					else         {iXB=0;iXC=0;}
					if(line==0)  {iYA=0;}
					else         {iYA=finWidth;}
					if(height>4) {iYB=finWidth;iYC=srcPitchHalf;}
					else
						if(height>3) {iYB=finWidth;iYC=finWidth;}
						else         {iYB=0;iYC=0;}

						colorB1 = *(bP- iYA);
						colorB2 = *(bP- iYA + iXB);

						color4 = *(bP  - iXA);
						color5 = *(bP);
						color6 = *(bP  + iXB);
						colorS2 = *(bP + iXC);

						color1 = *(bP  + iYB  - iXA);
						color2 = *(bP  + iYB);
						color3 = *(bP  + iYB  + iXB);
						colorS1= *(bP  + iYB  + iXC);

						colorA1 = *(bP + iYC);
						colorA2 = *(bP + iYC + iXB);

						if(color2 == color6 && color5 != color3)
						{
							product1b = product2a = color2;
							if((color1 == color2) ||
								(color6 == colorB2))
							{
								product1a = INTERPOLATE8(color2, color5);
								product1a = INTERPOLATE8(color2, product1a);
							}
							else
							{
								product1a = INTERPOLATE8(color5, color6);
							}

							if((color6 == colorS2) ||
								(color2 == colorA1))
							{
								product2b = INTERPOLATE8(color2, color3);
								product2b = INTERPOLATE8(color2, product2b);
							}
							else
							{
								product2b = INTERPOLATE8(color2, color3);
							}
						}
						else
							if (color5 == color3 && color2 != color6)
							{
								product2b = product1a = color5;

								if ((colorB1 == color5) ||
									(color3 == colorS1))
								{
									product1b = INTERPOLATE8(color5, color6);
									product1b = INTERPOLATE8(color5, product1b);
								}
								else
								{
									product1b = INTERPOLATE8(color5, color6);
								}

								if ((color3 == colorA2) ||
									(color4 == color5))
								{
									product2a = INTERPOLATE8(color5, color2);
									product2a = INTERPOLATE8(color5, product2a);
								}
								else
								{
									product2a = INTERPOLATE8(color2, color3);
								}
							}
							else
								if (color5 == color3 && color2 == color6)
								{
									register int r = 0;

									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
									r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));

									if (r > 0)
									{
										product1b = product2a = color2;
										product1a = product2b = INTERPOLATE8(color5, color6);
									}
									else
										if (r < 0)
										{
											product2b = product1a = color5;
											product1b = product2a = INTERPOLATE8(color5, color6);
										}
										else
										{
											product2b = product1a = color5;
											product1b = product2a = color2;
										}
								}
								else
								{
									product2b = product1a = INTERPOLATE8(color2, color6);
									product2b = Q_INTERPOLATE8(color3, color3, color3, product2b);
									product1a = Q_INTERPOLATE8(color5, color5, color5, product1a);

									product2a = product1b = INTERPOLATE8(color5, color3);
									product2a = Q_INTERPOLATE8(color2, color2, color2, product2a);
									product1b = Q_INTERPOLATE8(color6, color6, color6, product1b);
								}

								////////////////////////////////

								*dP=product1a;
								*(dP+1)=product1b;
								*(dP+(srcPitchHalf))=product2a;
								*(dP+1+(srcPitchHalf))=product2b;

								bP += 1;
								dP += 2;
			}//end of for ( finish= width etc..)

			line += 2;
			srcPtr += srcPitch;
		}; //endof: for (; height; height--)
	}
}

/////////////////////////

//#include <assert.h>

static __inline void scale2x_32_def_whole(uint32_t*  dst0, uint32_t* dst1, const uint32_t* src0, const uint32_t* src1, const uint32_t* src2, unsigned count)
{

	//assert(count >= 2);

	// first pixel
	if (src0[0] != src2[0] && src1[0] != src1[1]) {
		dst0[0] = src1[0] == src0[0] ? src0[0] : src1[0];
		dst0[1] = src1[1] == src0[0] ? src0[0] : src1[0];
		dst1[0] = src1[0] == src2[0] ? src2[0] : src1[0];
		dst1[1] = src1[1] == src2[0] ? src2[0] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
	}
	++src0;
	++src1;
	++src2;
	dst0 += 2;
	dst1 += 2;

	// central pixels
	count -= 2;
	while (count) {
		if (src0[0] != src2[0] && src1[-1] != src1[1]) {
			dst0[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
			dst0[1] = src1[1] == src0[0] ? src0[0] : src1[0];
			dst1[0] = src1[-1] == src2[0] ? src2[0] : src1[0];
			dst1[1] = src1[1] == src2[0] ? src2[0] : src1[0];
		} else {
			dst0[0] = src1[0];
			dst0[1] = src1[0];
			dst1[0] = src1[0];
			dst1[1] = src1[0];
		}

		++src0;
		++src1;
		++src2;
		dst0 += 2;
		dst1 += 2;
		--count;
	}

	// last pixel
	if (src0[0] != src2[0] && src1[-1] != src1[0]) {
		dst0[0] = src1[-1] == src0[0] ? src0[0] : src1[0];
		dst0[1] = src1[0] == src0[0] ? src0[0] : src1[0];
		dst1[0] = src1[-1] == src2[0] ? src2[0] : src1[0];
		dst1[1] = src1[0] == src2[0] ? src2[0] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
	}
}

void filter_Scale2x_ex8(unsigned char *srcPtr, DWORD srcPitch,
				 unsigned char  *dstPtr, int width, int height)
{
	//const int srcpitch = srcPitch;
	const int dstPitch = srcPitch<<1;

	int count = height;

	uint32_t  *dst0 = (uint32_t  *)dstPtr;
	uint32_t  *dst1 = dst0 + (dstPitch >> 2);

	uint32_t  *src0 = (uint32_t  *)srcPtr;
	uint32_t  *src1 = src0 + (srcPitch >> 2);
	uint32_t  *src2 = src1 + (srcPitch >> 2);

	finalw=width<<1;
	finalh=height<<1;

	scale2x_32_def_whole(dst0, dst1, src0, src0, src1, width);

	count -= 2;
	while(count) {
		dst0 += dstPitch >> 1;
		dst1 += dstPitch >> 1;
		scale2x_32_def_whole(dst0, dst1, src0, src0, src1, width);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}
	dst0 += dstPitch >> 1;
	dst1 += dstPitch >> 1;
	scale2x_32_def_whole(dst0, dst1, src0, src1, src1, width);

}

////////////////////////////////////////////////////////////////////////

static __inline void scale3x_32_def_whole(uint32_t* dst0, uint32_t* dst1, uint32_t* dst2, const uint32_t* src0, const uint32_t* src1, const uint32_t* src2, unsigned count)
{
	//assert(count >= 2);

	//first pixel
	if (src0[0] != src2[0] && src1[0] != src1[1]) {
		dst0[0] = src1[0];
		dst0[1] = (src1[0] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[0]) ? src0[0] : src1[0];
		dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
		dst1[0] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
		dst2[0] = src1[0];
		dst2[1] = (src1[0] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[0]) ? src2[0] : src1[0];
		dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
	++src0;
	++src1;
	++src2;
	dst0 += 3;
	dst1 += 3;
	dst2 += 3;

	//central pixels
	count -= 2;
	while (count) {
		if (src0[0] != src2[0] && src1[-1] != src1[1]) {
			dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
			dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[1]) || (src1[1] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
			dst0[2] = src1[1] == src0[0] ? src1[1] : src1[0];
			dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
			dst1[1] = src1[0];
			dst1[2] = (src1[1] == src0[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src0[1]) ? src1[1] : src1[0];
			dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
			dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[1]) || (src1[1] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
			dst2[2] = src1[1] == src2[0] ? src1[1] : src1[0];
		} else {
			dst0[0] = src1[0];
			dst0[1] = src1[0];
			dst0[2] = src1[0];
			dst1[0] = src1[0];
			dst1[1] = src1[0];
			dst1[2] = src1[0];
			dst2[0] = src1[0];
			dst2[1] = src1[0];
			dst2[2] = src1[0];
		}

		++src0;
		++src1;
		++src2;
		dst0 += 3;
		dst1 += 3;
		dst2 += 3;
		--count;
	}

	// last pixel
	if (src0[0] != src2[0] && src1[-1] != src1[0]) {
		dst0[0] = src1[-1] == src0[0] ? src1[-1] : src1[0];
		dst0[1] = (src1[-1] == src0[0] && src1[0] != src0[0]) || (src1[0] == src0[0] && src1[0] != src0[-1]) ? src0[0] : src1[0];
		dst0[2] = src1[0];
		dst1[0] = (src1[-1] == src0[0] && src1[0] != src2[-1]) || (src1[-1] == src2[0] && src1[0] != src0[-1]) ? src1[-1] : src1[0];
		dst1[1] = src1[0];
		dst1[2] = (src1[0] == src0[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src0[0]) ? src1[0] : src1[0];
		dst2[0] = src1[-1] == src2[0] ? src1[-1] : src1[0];
		dst2[1] = (src1[-1] == src2[0] && src1[0] != src2[0]) || (src1[0] == src2[0] && src1[0] != src2[-1]) ? src2[0] : src1[0];
		dst2[2] = src1[0];
	} else {
		dst0[0] = src1[0];
		dst0[1] = src1[0];
		dst0[2] = src1[0];
		dst1[0] = src1[0];
		dst1[1] = src1[0];
		dst1[2] = src1[0];
		dst2[0] = src1[0];
		dst2[1] = src1[0];
		dst2[2] = src1[0];
	}
}


void filter_Scale3x_ex8(unsigned char *srcPtr, DWORD srcPitch,
				 unsigned char  *dstPtr, int width, int height)
{
	int count = height;

	int dstPitch = srcPitch*3;
	int dstRowPixels = dstPitch>>2;	

	uint32_t  *dst0 = (uint32_t  *)dstPtr;
	uint32_t  *dst1 = dst0 + dstRowPixels;
	uint32_t  *dst2 = dst1 + dstRowPixels;

	uint32_t  *src0 = (uint32_t  *)srcPtr;
	uint32_t  *src1 = src0 + (srcPitch >> 2);
	uint32_t  *src2 = src1 + (srcPitch >> 2);
	scale3x_32_def_whole(dst0, dst1, dst2, src0, src0, src2, width);

	finalw=width*3;
	finalh=height*3;


	count -= 2;
	while(count) {
		dst0 += dstRowPixels*3;
		dst1 += dstRowPixels*3;
		dst2 += dstRowPixels*3;

		scale3x_32_def_whole(dst0, dst1, dst2, src0, src1, src2, width);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}

	dst0 += dstRowPixels*3;
	dst1 += dstRowPixels*3;
	dst2 += dstRowPixels*3;

	scale3x_32_def_whole(dst0, dst1, dst2, src0, src1, src1, width);
}


////////////////////////////////////////////////////////////////////////


static void hq2x_32_def(uint32_t * dst0, uint32_t * dst1, const uint32_t * src0, const uint32_t * src1, const uint32_t * src2, unsigned count)
{
	static unsigned char cache_vert_mask[640];
	unsigned char cache_horiz_mask = 0;

	unsigned i;
	unsigned char mask;
	uint32_t  c[9];

	if (src0 == src1)	//processing first row
		memset(cache_vert_mask, 0, count);

	for(i=0;i<count;++i) {
		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];

		if (i>0) {
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		} else {
			c[0] = c[1];
			c[3] = c[4];
			c[6] = c[7];
		}

		if (i<count-1) {
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		} else {
			c[2] = c[1];
			c[5] = c[4];
			c[8] = c[7];
		}

		mask = 0;

		mask |= interp_32_diff(c[0], c[4]) << 0;
		mask |= cache_vert_mask[i];
		mask |= interp_32_diff(c[2], c[4]) << 2;
		mask |= cache_horiz_mask;
		cache_horiz_mask = interp_32_diff(c[5], c[4]) << 3;
		mask |= cache_horiz_mask << 1;	// << 3 << 1 == << 4
		mask |= interp_32_diff(c[6], c[4]) << 5;
		cache_vert_mask[i] = interp_32_diff(c[7], c[4]) << 1;
		mask |= cache_vert_mask[i] << 5; // << 1 << 5 == << 6
		mask |= interp_32_diff(c[8], c[4]) << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst1[0]
#define P3 dst1[1]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_32_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_32_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_32_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_32_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_32_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_32_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_32_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_32_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_32_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_32_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_32_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_32_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_32_151(c[p0], c[p1])

		switch (mask) {
			#include "hq2x.h"
		}

#undef P0
#undef P1
#undef P2
#undef P3
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 2;
		dst1 += 2;
	}
}

void filter_hq2x_32( unsigned char * srcPtr,  DWORD srcPitch, unsigned char * dstPtr, int width, int height)
{
	const int dstPitch = srcPitch<<1;

	int count = height;

	uint32_t  *dst0 = (uint32_t  *)dstPtr;
	uint32_t  *dst1 = dst0 + (dstPitch >> 2);

	uint32_t  *src0 = (uint32_t  *)srcPtr;
	uint32_t  *src1 = src0 + (srcPitch >> 2);
	uint32_t  *src2 = src1 + (srcPitch >> 2);

	finalw=width*2;
	finalh=height*2;

	hq2x_32_def(dst0, dst1, src0, src0, src1, width);

	count -= 2;
	while(count) {
		dst0 += dstPitch >> 1;		//next 2 lines (dstPitch / 4 char per int * 2)
		dst1 += dstPitch >> 1;
		hq2x_32_def(dst0, dst1, src0, src1, src2, width);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}
	dst0 += dstPitch >> 1;
	dst1 += dstPitch >> 1;
	hq2x_32_def(dst0, dst1, src0, src1, src1, width);
}

static void hq3x_32_def(uint32_t*  dst0, uint32_t*  dst1, uint32_t*  dst2, const uint32_t* src0, const uint32_t* src1, const uint32_t* src2, unsigned count)
{
	static unsigned char cache_vert_mask[640];
	unsigned char cache_horiz_mask = 0;

	unsigned i;
	unsigned char mask;
	uint32_t  c[9];

	if (src0 == src1)	//processing first row
		memset(cache_vert_mask, 0, count);

	for(i=0;i<count;++i) {
		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];

		if (i>0) {
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		} else {
			c[0] = c[1];
			c[3] = c[4];
			c[6] = c[7];
		}

		if (i<count-1) {
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		} else {
			c[2] = c[1];
			c[5] = c[4];
			c[8] = c[7];
		}

		mask = 0;

		mask |= interp_32_diff(c[0], c[4]) << 0;
		mask |= cache_vert_mask[i];
		mask |= interp_32_diff(c[2], c[4]) << 2;
		mask |= cache_horiz_mask;
		cache_horiz_mask = interp_32_diff(c[5], c[4]) << 3;
		mask |= cache_horiz_mask << 1;	// << 3 << 1 == << 4
		mask |= interp_32_diff(c[6], c[4]) << 5;
		cache_vert_mask[i] = interp_32_diff(c[7], c[4]) << 1;
		mask |= cache_vert_mask[i] << 5; // << 1 << 5 == << 6
		mask |= interp_32_diff(c[8], c[4]) << 7;

#define P0 dst0[0]
#define P1 dst0[1]
#define P2 dst0[2]
#define P3 dst1[0]
#define P4 dst1[1]
#define P5 dst1[2]
#define P6 dst2[0]
#define P7 dst2[1]
#define P8 dst2[2]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define IC(p0) c[p0]
#define I11(p0,p1) interp_32_11(c[p0], c[p1])
#define I211(p0,p1,p2) interp_32_211(c[p0], c[p1], c[p2])
#define I31(p0,p1) interp_32_31(c[p0], c[p1])
#define I332(p0,p1,p2) interp_32_332(c[p0], c[p1], c[p2])
#define I431(p0,p1,p2) interp_32_431(c[p0], c[p1], c[p2])
#define I521(p0,p1,p2) interp_32_521(c[p0], c[p1], c[p2])
#define I53(p0,p1) interp_32_53(c[p0], c[p1])
#define I611(p0,p1,p2) interp_32_611(c[p0], c[p1], c[p2])
#define I71(p0,p1) interp_32_71(c[p0], c[p1])
#define I772(p0,p1,p2) interp_32_772(c[p0], c[p1], c[p2])
#define I97(p0,p1) interp_32_97(c[p0], c[p1])
#define I1411(p0,p1,p2) interp_32_1411(c[p0], c[p1], c[p2])
#define I151(p0,p1) interp_32_151(c[p0], c[p1])

		switch (mask) {
			#include "hq3x.h"
		}

#undef P0
#undef P1
#undef P2
#undef P3
#undef P4
#undef P5
#undef P6
#undef P7
#undef P8
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 3;
		dst1 += 3;
		dst2 += 3;
	}
}

void filter_hq3x_32( unsigned char * srcPtr,  DWORD srcPitch, unsigned char * dstPtr, int width, int height)
{
	int count = height;

	int dstPitch = srcPitch*3;
	int dstRowPixels = dstPitch>>2;

	uint32_t  *dst0 = (uint32_t  *)dstPtr;
	uint32_t  *dst1 = dst0 + dstRowPixels;
	uint32_t  *dst2 = dst1 + dstRowPixels;

	uint32_t  *src0 = (uint32_t  *)srcPtr;
	uint32_t  *src1 = src0 + (srcPitch >> 2);
	uint32_t  *src2 = src1 + (srcPitch >> 2);

	finalw=width*3;
	finalh=height*3;

	hq3x_32_def(dst0, dst1, dst2, src0, src0, src2, width);

	count -= 2;
	while(count) {
		dst0 += dstRowPixels * 3;
		dst1 += dstRowPixels * 3;
		dst2 += dstRowPixels * 3;

		hq3x_32_def(dst0, dst1, dst2, src0, src1, src2, width);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}
	dst0 += dstRowPixels * 3;
	dst1 += dstRowPixels * 3;
	dst2 += dstRowPixels * 3;

	hq3x_32_def(dst0, dst1, dst2, src0, src1, src1, width);

}
