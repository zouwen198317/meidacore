#include "imageprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CImageProcess::CImageProcess()//constructer
{

}

CImageProcess::~CImageProcess()
{

}


ImgBlk* CImageProcess::allocHwBuf(u32 width,u32 height,u32 format) // allocBuf to buff pool
{
	printf("CImageProcess allocHwBuf NULL\n");
	return NULL;
}

void CImageProcess::freeHwBuf(ImgBlk* img) // free buff pool
{

}

int CImageProcess::resize(ImgBlk* pSrc, ImgBlk* pDest)
{
	return -1;
}

int CImageProcess::crop(ImgBlk* pSrc, ImgBlk* pDest)
{
	return -1;
}

	
int CImageProcess::colorCovert(ImgBlk* pSrc, ImgBlk* pDest)
{
	return -1;
}
	
int CImageProcess::rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 angle)
{
	return -1;
}

int CImageProcess::overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha)
{
	return -1;
}
	

ImgBlk* CImageProcess::overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap)
{
	return NULL;
}
	

ImgBlk* CImageProcess::overlay(ImgBlk* pSrc, ImgBlk* pOverlap)
{
	return NULL;
}
	

//using all the parameters to process, it can do resize overlay rotate colorcovert in this function according to the parameters
int CImageProcess::process(ImgBlk* pSrc, ImgBlk* pDest)
{
	return -1;
}




