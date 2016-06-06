#ifndef _GPU_2D_BLIT_H_
#define _GPU_2D_BLIT_H_


#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "ffmpeg/avcodec_common.h"
#include "platform/HAL/imageprocess.h"
#include <linux/g2d.h>

#define BUF_MAX 20

class CGpu2D : public CImageProcess
{
public:
	CGpu2D();//constructer
	~CGpu2D();
	int resize(ImgBlk* pSrc, ImgBlk* pDest); 
	int colorCovert(ImgBlk* pSrc, ImgBlk* pDest);
	int rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 rotation);//g2d_rotation 0~5
	
	int overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha);

	ImgBlk* allocHwBuf(u32 width,u32 height,u32 format); // allocBuf to buff pool
	void freeHwBuf(ImgBlk* img); // free buff pool
private:
	int fmt_to_g2dfmt(unsigned int pixelformat,u32 *format);
	unsigned int fmt_to_bpp(unsigned int pixelformat);
	int set_surface_planes(g2d_surface* surface,ImgBlk* iBlk);
	void *mHandle;
	int mBuf_num;
	struct g2d_buf* mBufs[BUF_MAX];
};



	





#endif
