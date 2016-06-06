#ifndef _VIDEO_CAPTURE_JOB_H_
#define _VIDEO_CAPTURE_JOB_H_
#include "libthread/threadpp.h"
#include "platform/camcapture.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "shmringqueue.h"

//use for usb camera,
#define USING_UVC_VIDEO_DRIVER 1
#define CAM_POWER_ENABLE_FILE "/sys/devices/soc0/soc.1/2100000.aips-bus/21a8000.i2c/i2c-1/1-0030/cam_poweren"

#if USING_UVC_VIDEO_DRIVER
#include "platform/HAL/imageprocess.h"
#endif

class vidCaptureJob : public Thread
{
public:
    vidCaptureJob(vidCapParm *pParm);
    ~vidCaptureJob();
    bool isRunning() const {return mRunning;}
    void stop();
    void get_buffer_info(fbuffer_info*);
    int updateConfig(vidCapParm *pParm);
    time_t getCaptureTime();
    time_t getStartPts() { return mpCamera->getStartPts(); }
    int closeCamera();

#ifdef NO_CAM
    int getYuvData(char **yuvData);
#endif
private:
    void run();
    int notify();
    int status();
    CamCapture *mpCamera; //main camera
    //CamCapture *mpSubCamera; //sub camera
    //productor
    ShmRingQueue<ImgBlk> *mpRingQueue;
    ShmRingQueue<ImgBlk> *mpVidPrepEncRingQueue;
    ShmRingQueue<ImgBlk> *mpMotionDetectRingQueue;

    int m_camType;
    bool mRunning;
    int mFps;
    u64 mLastPts; //debug used
#ifdef NO_CAM
    struct timeval mNoCamTv;//start time. It's used to create the PTS when there is no camera.
#endif

#if USING_UVC_VIDEO_DRIVER
    CImageProcess *mpImgCap; //main camera
    ImgBlk *mpCaptureImgBlk[CAM_BUFFER_NUM];
    int mCaptureImgBlkId;
#endif  
};

#endif

