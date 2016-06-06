
#include "videoencoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CVideoEnc::CVideoEnc(videoParms * parms)
{

}

CVideoEnc::~CVideoEnc()
{

}

int CVideoEnc::config(videoParms * parms)
{
	return -1;
}

int CVideoEnc::encode(ImgBlk * src, VencBlk * dest)//retuen -1, frame type 0, 1 for I frame
{
	return -1;
}

int CVideoEnc::encode(ImgBlk * src, VencBlk * dest, bool keyframe)//force to encode I frame
{
	return -1;
}

bufZone* CVideoEnc::allocHwBuffer(u32 size)
{
	return NULL;
}

void CVideoEnc::freeHwBuffer(bufZone* zone)
{
	
}

