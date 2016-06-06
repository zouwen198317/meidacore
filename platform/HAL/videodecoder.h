#ifndef _VIDEO_DECODER_H
#define _VIDEO_DECODER_H

#include "imageblock.h"
#include "mediablock.h"
#include "mediaparms.h"
#include "vencblock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class CVideoDec
{
public:
	CVideoDec(videoParms * parms);
	virtual ~CVideoDec();
	virtual int config(videoParms * parms) { return -1; }
	virtual int decode(VencBlk * src, ImgBlk * dest) { return -1; }//blocking

	virtual bufZone* allocHwBuffer(u32 size) { return NULL; }
	virtual void freeHwBuffer(bufZone*) { };
};

#endif
