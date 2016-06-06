#ifndef _AUDIO_PLAY_H_
#define _AUDIO_PLAY_H_
//#define u32 unsigned int
//#define u64 unsigned long long
/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API


#define MEDIA_CORE_CONTROL_VOL 1

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mediaparms.h"
#include "audioblock.h"

class AudioPlay
{
public:
    AudioPlay(int sampleRate, int sampleSize, int frames);
    ~AudioPlay();
    int write_pcm(char *buf, int size);
    int getFrameSize() const { return mFrameSize; }
    int getSamplesPerFrames() const { return mFrames; }
    int getSampleSize() const { return mSampleSize; }
    int getSampleRate() const { return mSampleRate; }
    int getChannel() const { return mChannel;}
    u32 getFrameDuration(); // per frame using xx us
    int getDelay();
    int getSpace(void);
   int setVolume(int value);
    void clearBuf(void);
    
private:
    int setHwParams();
    int mFrameSize; //mFrameSize =mSampleSize*mFrames
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int mSampleRate;
    int mSampleSize;
    
    int mFrames;
    int mChannel;
#if  MEDIA_CORE_CONTROL_VOL
	snd_mixer_t *mixer;
	snd_mixer_elem_t *master_element;
#endif
   unsigned char * mpBuf;
   int mBufLen;
   int mAlsaBufSpace;
};

#endif // _AUDIO_PLAY_H_