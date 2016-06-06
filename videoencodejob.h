#ifndef _VIDEO_ENCODE_JOB_H_
#define _VIDEO_ENCODE_JOB_H_

#include <sys/time.h>
#include "mediablock.h"
#include "libthread/threadpp.h"
#include "shmringqueue.h"
#include "shmringbuffer.h"
#include "platform/HAL/videoencoder.h"
#include "mediacore.h"
#include "ipcopcode.h"

class VideoEncodeJob : public Thread
{
public:
    VideoEncodeJob(vencParms *pParm);
    ~VideoEncodeJob();
    bool isRunning() const {return mRunning;}
    void stop();
    int stopLiveStreaming();
    int startLiveStreaming();
    void pause();
    void resume();
    void GenKeyFrame() { mNeedKeyFrame = true;}
    int updateConfig(videoParms *pParm);
    //int setSkipping(bool flag);
    int setSkipping(bool flag);

private:
    int notify(VencBlk *pVenc);
    int notify(int msgopc); //need to add notify network later
    int notifyItself(int msgopc);
    void run();
    int status();
    int initHw();
    int unInitHw();
    int h264Encode(ImgBlk *imgsrc, VencBlk *pVencDest);
    int jpegEncode(ImgBlk *imgsrc, VencBlk *pVencDest);
    int updateEncoderParms(videoParms *pParm);
    bool mRunning;
    bool mPause;
    bool mNeedKeyFrame;
    bool mSkipping;
    bool mLiving;
    //consumer
    ShmRingQueue<ImgBlk> *mpVencRingQueue;
    //productor
    ShmRingQueue<MediaBlk> *mpMediaRingQueue;
    ShmRingQueue<VencBlk> *mpCaptureQueue;
    ShmRingQueue<RTSPBlk> *mpRTSPRingQueue;
    ShmRingBuffer *mpRTSPRingBuffer;
    CVideoEnc* mpH264Enc;
    videoParms mH264EncParm;
    CVideoEnc* mpJPEGEnc;
    videoParms mJPEGEncParm;


    u64 mLastPts; //debug used

    //debug
    int saveH264Data(VencBlk *pVencBlk);

    videoParms mNewVideoParms;
};

#endif

