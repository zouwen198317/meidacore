#ifndef _AUDIO_DECODER_H
#define _AUDIO_DECODER_H

#include "aencblock.h"
#include "mediaparms.h"
#include "audioblock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

class CAudioDec
{
public:
	CAudioDec(audioParms * parms) {}
	virtual ~CAudioDec() {}; 
	virtual int config(audioParms * parms) { return -1;}
	virtual int decode( AencBlk * src, AudBlk * dest)  { return -1;}
	//virtual int getFrames() { return 0;}
	//virtual int getFrameSize() { return 0;}

	virtual bufZone* allocHwBuffer(u32 size)  { return NULL;}
	virtual void freeHwBuffer(bufZone* zone)  {  }
	//virtual float compressRate(){ return 1;}
};

#endif