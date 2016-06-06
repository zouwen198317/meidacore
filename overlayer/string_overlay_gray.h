#ifndef STRING_OVERLAY_GRAY_H
#define STRING_OVERLAY_GRAY_H
#include"common.h"
#include"dot_rgb.h"
int dot_overlay_gray(uchar *str[],
               const font_info f_info,
                 uchar *grayBuf,
                 uint width,
                 uint height,
                 uint X,
                 uint Y);

//this function will rewrite grayBg and grayAlpha
int overlayAlpha_Stroke(uchar *I420Buf,	//I420
						uchar *grayAlpha,					//gray8
						const uint width,const uint height,
						uint x0,uint y0,
						uint x1,uint y1,
						const font_info f_info,
						const rgb borderColor=rgb(255,255,255));

#endif
