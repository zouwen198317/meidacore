#ifndef _IMAGE_PROCESS_JOB_H_
#define _IMAGE_PROCESS_JOB_H_
#include "libthread/threadpp.h"
#include "platform/camcapture.h"
#include "platform/HAL/imageprocess.h"
#include "platform/videodisp.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "shmringqueue.h"
#include "algopcode.h"

#define DISPLAY_BUFFER_NUM 3
#define FREEZE_FRAMES_NUM 50 //freeze 60 frames after capture
class ImgProcessJob: public Thread
{
public:
    ImgProcessJob(imgProcParms *pParm);
    ~ImgProcessJob();
    int setParm(imgProcParms *pParm);
    bool isRunning() const {return mRunning;}
    void stop();
    void pause();
    void resume();
    //int captureJPEG(char* path);
    int pauseDisplay(bool pause);
    //int enableDisplay(bool enable);
    int adjustDisplay(int width,int height,int top,int left);
    void videoDisplayOnOff(int onOff);
    int screenFreeze();
    bool IsCapturing(){return mFreezeNum;}
private:
    void run();
    bool mRunning;
    bool mPause;
    bool mDispPause;
    bool mDispEnable;
    int mDispAdjust;
    
    bool mSwitchIndisplay;
    bool mNeedFreeze;
    int mFreezeNum;

    
    int notify();
    int notifyItself(int msgopc);
    int switchVidDispBuffer(vidDispParm * pParm);
    int InitDisplay(ImgBlk *imgsrc,mediaStatus pStatus);
    int display(ImgBlk *img);
    int status();
    int initHw();
    int unInitHw();
    int processImg(msgbuf_t *msgbuf,mediaStatus pStatus);
    VidDisp *mVidDisp;
    CImageProcess *mpImgProc; //main camera
    //CImageProcess *mpSubImgProc; //sub camera
    //consumer
    ShmRingQueue<ImgBlk> *mpCamRingQueue;
    ShmRingQueue<ImgBlk> *mpPlayRingQueue;
    //productor
    //ShmRingQueue<ImgBlk> *mpVencRingQueue;
    //ImgBlk mVencImgBlk[RING_BUFFER_VIN_TO_VENC_NUM];
    int mDispImgBlkId;
    ImgBlk *mDispImgBlk[DISPLAY_BUFFER_NUM];
    ImgBlk* getDispImgBlk();
    int saveBmpFile(char *name, ImgBlk* img);
    
    mediaStatus mMediaStatus;
    vidDispParm mVidDispParm;

    //debug
    int saveResizeImg(ImgBlk* img);

};

#endif

