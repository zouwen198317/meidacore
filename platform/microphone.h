#ifndef _MICRO_PHONE_H_
#define _MICRO_PHONE_H_

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mediaparms.h"
#include "audioblock.h"
#include "types.h"
#include<sys/time.h>

class MicroPhone
{
public:
	MicroPhone(int sampleRate, int sampleSize, int frames);
	~MicroPhone();
	int read(char *buf,  u32 size); //using getFrameSize to fill the size
	int caputre(AudBlk* pAudBlk);
	int getFrameSize() const { return mFrameSize; }
	int getSamplesPerFrames() const { return mFrames; }
	int getSampleSize() const { return mSampleSize; }
	int getSampleRate() const { return mSampleRate; }
	int getChannel() const { return mChannel;}
	u32 getFrameDuration(); // per frame using xx us
	
private:
	u64 getPts();
	int setHwParams();
	struct  timeval    mStarTv;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	
	int mSampleRate;
	int mSampleSize;
	int mFrameSize; //mFrameSize =mSampleSize*mFrames
	int mFrames;
	int mChannel;
};

#endif
