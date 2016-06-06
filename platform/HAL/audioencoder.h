#ifndef _AUDIO_ENCODER_H
#define _AUDIO_ENCODER_H

#include "aencblock.h"
#include "mediaparms.h"
#include "audioblock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

class CAudioEnc
{
public:
	CAudioEnc(audioParms * parms) {}
	virtual ~CAudioEnc() {}; 
	virtual int config(audioParms * parms) { return -1;}
	virtual int encode(AudBlk * src, AencBlk * dest)  { return -1;}
	virtual int getFrames() { return 0;}
	virtual int getFrameSize() { return 0;}

	virtual bufZone* allocHwBuffer(u32 size)  { return NULL;}
	virtual void freeHwBuffer(bufZone* zone)  {  }
	virtual float compressRate(){ return 1;}
};

#endif

