#ifndef _IMAGE_PROCESS_UNIT_H_
#define _IMAGE_PROCESS_UNIT_H_
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>

//#include "mxc_ipudev_test.h"
#include "platform/HAL/imageprocess.h"

#include "ffmpeg/avcodec_common.h"


class X86Ipu : public CImageProcess
{
public:
	X86Ipu();//constructer
	~X86Ipu();
	int resize(ImgBlk* pSrc, ImgBlk* pDest); 
	int colorCovert(ImgBlk* pSrc, ImgBlk* pDest);
	//int rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 angle);
	
	int overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha);
	//using all the parameters to process, it can do resize overlay rotate colorcovert in this function according to the parameters
	//int process(ImgBlk* pSrc, ImgBlk* pDest); // alloc buffer itself

	ImgBlk* allocHwBuf(u32 width,u32 height,u32 format); // allocBuf to buff pool
	void freeHwBuf(ImgBlk* img); // free buff pool
private:
	unsigned int fmt_to_bpp(unsigned int pixelformat);
	int fmt_to_ipufmt(unsigned int pixelformat,u32 *format);
	//ipu 
	int mIpuFd; //IPU handle
	struct ipu_task mIpuTask;  //ipu task only operate in physical address
};

#endif
