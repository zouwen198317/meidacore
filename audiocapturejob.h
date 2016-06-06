#ifndef _AUDIO_CAPTURE_JOB_H_
#define _AUDIO_CAPTURE_JOB_H_

#include "mediablock.h"
#include "mediacore.h"
#include "libthread/threadpp.h"
#include "shmringqueue.h"
#include "platform/HAL/audioencoder.h"
#include "platform/microphone.h"

class AudioCaptureJob : public Thread
{
public:
    AudioCaptureJob(audioParms *pParm);
    ~AudioCaptureJob();
    bool isRunning() const {return mRunning;}
    void stop();
    void pause();
    void resume();
private:
    int notify(AencBlk *pAenc);
    int status();
    int initHw();
    int unInitHw();
    void run();
    bool mRunning;
    bool mPause;
    
    //productor
    ShmRingQueue<MediaBlk> *mpMediaRingQueue;

    MicroPhone *mpMic;
    CAudioEnc *mpAudEnc;
    audioParms mAudioParms;
    char* mpBuffer;
    int mBufferSize;
    //debug
    //int saveAencData(AencBlk *pAencBlk);
};


#endif

