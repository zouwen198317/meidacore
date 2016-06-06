#ifndef _ADPCM_ENCODER_H_
#define _ADPCM_ENCODER_H_

#include "platform/HAL/audioencoder.h"
#include "ffmpeg/avcodec_common.h"
#include "ffmpeg/audiocodec/avcodec.h"

class AdpcmEnc:public CAudioEnc
{
public:
	AdpcmEnc(audioParms * parms);
	~AdpcmEnc();
	int encode(AudBlk * src, AencBlk * dest);
	int getFrames();
	int getFrameSize();
	bufZone* allocHwBuffer(u32 size);
	void freeHwBuffer(bufZone* zone);
	float compressRate() { return 0.25; }
private:
	AVCodecContext mAvCodecContext;
	struct AVCodec mCodec;
	int mSampleSize;
	bool isInit;
	bufZone *mpBufZone;
	audioParms mAudioParms;
	int mOffset;
};
#endif
