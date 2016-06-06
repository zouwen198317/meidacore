#ifndef _VIDEO_PREP_ENC_JOB_H_
#define _VIDEO_PREP_ENC_JOB_H_
#include "libthread/threadpp.h"
#include "platform/camcapture.h"
#include "platform/HAL/imageprocess.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "shmringqueue.h"
//#include "overlayer/string_overlay_yuyv.h"
#include "overlayer/string_overlay_I420.h"

class VideoPrepEncJob: public Thread
{
public:
    VideoPrepEncJob(vidPrepEncParms *pParm);
    ~VideoPrepEncJob();
    int setParm(vidPrepEncParms *pParm);
    bool isRunning() const {return mRunning;}
    void stop();
    void pause();
    void resume();
    void updateParm();

    int captureJPEG( );
    //int pauseDisplay(bool pause);
    //int enableDisplay(bool enable);

    //!!
    void setOverlay(bool);
    void setOverlayGPS(bool);
    void setLatitude(double latitude) { mLatitude = latitude;}
    void setLongtitude(double longtitude) { mLongtitude = longtitude;}
    int updateConfig(int width, int height, int capFps, int recFps);    
    int updateConfig( int capFps,int recFps);
    int getRecFps(){return mPreRecFps;}
private:
    void run();
    bool mRunning;
    bool mPause;
    bool mNeedCaputure;
    //!!
    bool mNeedOverlayGPS;
    bool mNeedOverlayTime;
    ImgBlk *mpOverlayBg;
    ImgBlk *mpOverlayAlpha;

    double mLongtitude;
    double mLatitude;
    
    bool overlayUpdateEn;
    int overlayTime(ImgBlk* input,bool needUpdate);
    int overlayGPS(ImgBlk* input,bool needUpdate);
    int overlayTime();
    int overlayGPS();
    bool needUpdateOverlay();
    struct  timeval mLastUpTime;
    OverlayerI420 *mTimeOverlay;
    OverlayerI420 *mGpsOverlay;
    
    int updateResolution(int width, int height);
    int updateFps (int capFps, int recFps);

    int notify(int msgopc);
    int notifyItself(int msgopc);
    //int display(ImgBlk *imgsrc);
    int status();
    int initHw();
    int unInitHw();
    int processImg(msgbuf_t *msgbuf);
    CImageProcess *mpImgProc; //main camera
    //CImageProcess *mpSubImgProc; //sub camera
    //consumer
    ShmRingQueue<ImgBlk> *mpCamRingQueue;

    //productor
    ShmRingQueue<ImgBlk> *mpVencRingQueue;
    ImgBlk *mpVencImgBlk[RING_BUFFER_VIN_TO_VENC_NUM];
    int mVencImgBlkId;
    ImgBlk* getVencImgBlk();
    
    vidPrepEncParms mVidPrepEncParms;
    //debug
    int saveResizeImg(ImgBlk* img);

    int mNewWidth;
    int mNewHeight;
    int mNewCapFps;
    int mNewRecFps;

    //change rec frame rate
    bool frameFilter(int flameIndex);
    float mFrameRate;
    int mFrameHasThrown;
    int mFrameId;
    int mFrameOpCnt;
    int mCaptureFps;
    int mPreRecFps;
};

#endif

