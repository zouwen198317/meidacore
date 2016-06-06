#include "imageprocess.h"

CImageProcess::CImageProcess()//constructer
{

}

CImageProcess::~CImageProcess()
{

}


ImgBlk* CImageProcess::allocHwBuf(u32 width,u32 height,u32 format) // allocBuf to buff pool
{
	return NULL;
}

ImgBlk* CImageProcess::freeHwBuf(ImgBlk* img) // free buff pool
{
	return NULL;
}

int CImageProcess::resize(ImgBlk* pSrc, ImgBlk* pDest)
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
	

ImgBlk* CImageProcess::overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap)
{
	return NULL;
}
	

ImgBlk* CImageProcess::overlay(ImgBlk* pSrc, ImgBlk* pOverlap)
{
	return NULL;
}
	

//using all the parameters to process, it can do resize overlay rotate colorcovert in this function according to the parameters
ImgBlk* CImageProcess::process(void* pSrcStruct, void* pDestStruct)
{
	return NULL;
}




