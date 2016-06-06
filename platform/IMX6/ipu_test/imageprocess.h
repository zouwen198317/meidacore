#ifndef _IMAGE_PROCESS_H_
#define _IMAGE_PROCESS_H_
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

//抽象各平台图像处理单元功能，由具体平台继承
/*
typedef struct ImgBlk_
{
	void *vaddr; //virtual address
	u32 paddr; //physical address
	u32 size;
	
	u32 height;
	u32 width;
	u32 format; //color space
}ImgBlk;
*/


class CImageProcess 
{
public:
	CImageProcess();//constructer
	~CImageProcess();
	ImgBlk* allocHwBuf(u32 width,u32 height,u32 format); // allocBuf to buff pool
	ImgBlk* freeHwBuf(ImgBlk* img); // free buff pool
	int resize(ImgBlk* pSrc, ImgBlk* pDest); 
	int colorCovert(ImgBlk* pSrc, ImgBlk* pDest);
	int rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 angle);
	
	ImgBlk* overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap);
	ImgBlk* overlay(ImgBlk* pSrc, ImgBlk* pOverlap);
	//using all the parameters to process, it can do resize overlay rotate colorcovert in this function according to the parameters
	ImgBlk* process(void* pSrcStruct, void* pDestStruct);

};

#endif
