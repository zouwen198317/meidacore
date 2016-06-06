#ifndef _ADPCM_DECODER_H_
#define _ADPCM_DECODER_H_

#include "platform/HAL/audiodecoder.h"
#include "ffmpeg/avcodec_common.h"
#include "ffmpeg/audiocodec/avcodec.h"

class AdpcmDec:public CAudioDec
{
public:
	AdpcmDec(audioParms * parms);
	~AdpcmDec();
	int decode(AencBlk * src, AudBlk * dest);
	bufZone* allocHwBuffer(u32 size);
	void freeHwBuffer(bufZone* zone);
private:
	AVCodecContext mAvCodecContext;
	struct AVCodec mCodec;
	int mSampleSize;
	bool isInit;
	audioParms mAudioParms;
};
#endif

