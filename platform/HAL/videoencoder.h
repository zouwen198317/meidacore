#ifndef _VIDEO_ENCODER_H
#define _VIDEO_ENCODER_H

#include "imageblock.h"
#include "mediablock.h"
#include "mediaparms.h"
#include "vencblock.h"

class CVideoEnc
{
public:
	CVideoEnc(videoParms * parms);
	virtual ~CVideoEnc();
	virtual int config(videoParms * parms);
	virtual int encode(ImgBlk * src, VencBlk * dest);//retuen -1, frame type 0, 1 for I frame
	virtual int encode(ImgBlk * src, VencBlk * dest, bool keyframe);//force to encode I frame
	//virtual int setSkipping(bool flag);

	virtual bufZone* allocHwBuffer(u32 size); 
	virtual void freeHwBuffer(bufZone* zone);
};

#endif
