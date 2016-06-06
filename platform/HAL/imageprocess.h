#ifndef _IMAGE_PROCESS_H_
#define _IMAGE_PROCESS_H_

#include "imageblock.h"
//抽象各平台图像处理单元功能，由具体平台继承


class CImageProcess 
{
public:
	CImageProcess();//constructer
	virtual ~CImageProcess();
	virtual ImgBlk* allocHwBuf(u32 width,u32 height,u32 format); // allocBuf to buff pool
	virtual void freeHwBuf(ImgBlk* img); // free buff pool
	virtual int resize(ImgBlk* pSrc, ImgBlk* pDest); 
	virtual int colorCovert(ImgBlk* pSrc, ImgBlk* pDest);
	virtual int rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 angle);
	virtual int crop(ImgBlk* pSrc, ImgBlk* pDest);
	virtual int overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha);
	virtual ImgBlk* overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap);
	virtual ImgBlk* overlay(ImgBlk* pSrc, ImgBlk* pOverlap);
	//using all the parameters to process, it can do resize overlay rotate colorcovert in this function according to the parameters
	virtual int process(ImgBlk* pSrc, ImgBlk* pDest);

};

#endif
