#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "videoprepencodejob.h"
#include "datetype.h"
#include "log/log.h"
#include "ipc/ipcclient.h"
#include "debug/errlog.h"
#include "mediacore.h"
#include"parms_dump.h"
#include "mediafile/rawfile.h"

#ifdef IMX6_PLATFORM
#include "platform/IMX6/imageprocessunit.h"
#endif

#include "overlayer/common.h"
#include "overlayer/createSingleColorBg.h"
#include "overlayer/dot_rgb.h"
#include "overlayer/string_overlay_gray.h"
#include "overlayer/string_overlay_I420.h"


#define DUMP_IMG_FILE 0
extern mediaStatus gMediaStatus;
extern MsgQueue *gpVidPrepVencMsgQueue;
extern MsgQueue *gpVidEncMsgQueue;
extern RecFileIniParm gRecIniParm;

extern IpcClient * gpMediaCoreIpc;


VideoPrepEncJob::VideoPrepEncJob(vidPrepEncParms *pParm)
{
    assert(pParm);
    log_print(_DEBUG,"VideoPrepEncJob\n");
    memcpy(&mVidPrepEncParms,pParm,sizeof(vidPrepEncParms));
    //IN
    mpCamRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_VINPREPENC,CAM_BUFFER_NUM,sizeof(ImgBlk));
    //OUT
    mpVencRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_VIN_TO_VENC,RING_BUFFER_VIN_TO_VENC_NUM,sizeof(ImgBlk));
    mpVencRingQueue->clear();
    
    mVencImgBlkId =0;
    mNeedOverlayTime = gRecIniParm.RecFile.overlay;
    //!!
    mNeedOverlayGPS = gRecIniParm.RecFile.overlay;
    mpOverlayBg = NULL; 
    mpOverlayAlpha = NULL;  
    mGpsOverlay = NULL;
    mTimeOverlay = NULL;
    overlayUpdateEn = false;
    mpImgProc = NULL;   
    mRunning = false;
    mPause = false;
    mNeedCaputure = false;
    //gettimeofday(&mLastUpTime, NULL);
    mLastUpTime.tv_sec = 0;
    mLongtitude = 0.0;
    mLatitude = 0.0;
    mNewWidth = 1264;
    mNewHeight = 720;
    mCaptureFps = gCapIniParm.VidCapParm.fps;
    mNewCapFps = mCaptureFps;
    mPreRecFps = pParm->vencParm.fps > mCaptureFps ? mCaptureFps : pParm->vencParm.fps;
    mNewRecFps = mPreRecFps;
    mFrameRate = ((float)mCaptureFps) / (mCaptureFps-mPreRecFps);
    mFrameOpCnt = 0;
    mFrameId =0;
    
#ifdef IMX6_PLATFORM
    mpImgProc = new CIpu;
#endif  
    
    initHw();
}

VideoPrepEncJob::~VideoPrepEncJob()
{
    mRunning = false;
    stop();
    sleep(1);
    unInitHw(); 
    if(mpImgProc)
        delete mpImgProc;
    
    if(mpVencRingQueue)
        delete mpVencRingQueue;

    if(mTimeOverlay)
        delete mTimeOverlay;
    mTimeOverlay = NULL;
    if(mGpsOverlay)
    mGpsOverlay = NULL;
    
}

int VideoPrepEncJob::updateResolution (int width, int height)
{
    int widthOrg =mVidPrepEncParms.vencParm.width;
    int heightOrg = mVidPrepEncParms.vencParm.height;
    if((width!=widthOrg) || (height!=heightOrg)) { 
        mVidPrepEncParms.vencParm.width = width;
        mVidPrepEncParms.vencParm.height = height;
        unInitHw();
        initHw();
        mpVencRingQueue->clear();   
        //mPreRecFps = gVideoIniParm.RecVideo.fps  > mCaptureFps ? mCaptureFps : gVideoIniParm.RecVideo.fps;
        //mFrameRate = ((float)mCaptureFps)/(mCaptureFps-mPreRecFps);
        log_print(_INFO,"VideoPrepEncJob updateResolution,resolution width:%d, height:%d\n", width, height);
    }
}

int VideoPrepEncJob::updateFps (int capFps, int recFps)
{
    if(capFps != mCaptureFps ||recFps != mPreRecFps) {
         //update CaptureFps
        if(capFps != mCaptureFps) {
            log_print(_INFO,"VideoPrepEncJob updateFps() CaptureFps:%d -> %d\n", mCaptureFps, capFps);
            mCaptureFps = capFps;   
        }
        //update FrameRate and RecFps
        log_print(_INFO,"VideoPrepEncJob updateFps() RecFps: %d -> %d\n", mPreRecFps, recFps);
        mPreRecFps = recFps > mCaptureFps ? mCaptureFps : recFps;
        mFrameRate = ((float)mCaptureFps) /(mCaptureFps - mPreRecFps);
        log_print(_INFO, "VideoPrepEncJob updateFps() captureFps:%d, RecordFps: %d, FrameRate:%f\n", mCaptureFps, mPreRecFps, mFrameRate);
    }
}

int VideoPrepEncJob::updateConfig(int width, int height, int capFps, int recFps)
{
    mNewWidth = width;
    mNewHeight = height;
    mNewCapFps = capFps;
    mNewRecFps = recFps;
    notifyItself(MSG_OP_UPDATE_CONFIG);
}

int VideoPrepEncJob::updateConfig( int capFps,int recFps)
{
    mNewCapFps = capFps;
    mNewRecFps = recFps;
    notifyItself(MSG_OP_UPDATE_CONFIG);
}

int VideoPrepEncJob::initHw()
{
    int i;
    ImgBlk *pImgblk = NULL;
    int width = mVidPrepEncParms.vencParm.width;
    int height = mVidPrepEncParms.vencParm.height;

    if(mpImgProc == NULL) {
        log_print(_ERROR,"VideoPrepEncJob Error, No ImgProcessUnit Created\n");
        return -1;
    }
    
    for(i=0; i<RING_BUFFER_VIN_TO_VENC_NUM; i++) {
        pImgblk = mpImgProc->allocHwBuf(width, height, mVidPrepEncParms.vencParm.fourcc);
        mpVencImgBlk[i] = pImgblk;
        //if(pImgblk)
        //  memcpy(&(mpVencImgBlk[i]), pImgblk, sizeof(ImgBlk));
    }
    log_print(_INFO, "VideoPrepEncJob ::initHw width %d height %d\n", width, height);

    mpOverlayBg = mpImgProc->allocHwBuf(width, height, PIX_FMT_YUV420P);
    if(mpOverlayBg == NULL)
        log_print(_ERROR, "VideoPrepEncJob ::initHw mpOverlayBg fail\n");
    createBg_i420(width, height, rgb(0, 0, 0), (unsigned char*)mpOverlayBg->vaddr);
    
    mpOverlayAlpha = mpImgProc->allocHwBuf(width, height, PIX_FMT_GRAY8);
    
    if(mpOverlayAlpha == NULL)
        log_print(_ERROR, "VideoPrepEncJob ::initHw mpOverlayAlpha fail\n");

    log_print(_DEBUG, "VideoPrepEncJob ::initHw finish\n");
    
    return 0;
}

int VideoPrepEncJob::unInitHw()
{
    int i;
    if(mpImgProc == NULL)
        return -1;
    
    for(i=0; i<RING_BUFFER_VIN_TO_VENC_NUM; i++) {
        mpImgProc->freeHwBuf(mpVencImgBlk[i]);
    }
    if(mpOverlayBg) {
        mpImgProc->freeHwBuf(mpOverlayBg);
        mpOverlayBg = NULL;
    }
    if(mpOverlayAlpha) {
        mpImgProc->freeHwBuf(mpOverlayAlpha);
        mpOverlayAlpha = NULL;
    }
    if(mGpsOverlay) {
        delete mGpsOverlay;
        mGpsOverlay = NULL;
    }
    if(mTimeOverlay) {
        delete mTimeOverlay;
        mTimeOverlay = NULL;
    }
    return 0;
}

ImgBlk* VideoPrepEncJob::getVencImgBlk()
{
    ImgBlk* pTmp = (mpVencImgBlk[mVencImgBlkId]);
    mVencImgBlkId++;
    if(mVencImgBlkId >= RING_BUFFER_VIN_TO_VENC_NUM)
        mVencImgBlkId = 0;
    return pTmp;
}

int VideoPrepEncJob::notify(int msgopc)
{
    //log_print(_DEBUG,"VideoPrepEncJob notify %d\n",msgopc);
    //if(status() == MEDIA_RECORD)
    ImgBlk* pTmp = getVencImgBlk();
    //printf("img.height %d img.width,%d img.size %d \n", pTmp->height,pTmp->width,pTmp->size);
    //$ img.height 720 img.width 1264 img.size 1365120
    //log_print(_DEBUG,"VideoPrepEncJob notify vaddr %x, paddr %x, size %d pts %llu\n",(u32)pTmp->vaddr,pTmp->paddr,pTmp->size,pTmp->pts);
    mpVencRingQueue->push(*pTmp);
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode= msgopc;//MSG_OP_NORMAL;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpVidEncMsgQueue) {
        //log_print(_DEBUG,"VideoPrepEncJob notify queue num %d\n",gpVidEncMsgQueue->msgNum());
        gpVidEncMsgQueue->send(&msg);
    }

    return 0;
}

int VideoPrepEncJob::notifyItself(int msgopc)
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpVidPrepVencMsgQueue) {
        log_print(_DEBUG, "VideoPrepEncJob notifyItself queue num %d\n", gpVidPrepVencMsgQueue->msgNum());
        gpVidPrepVencMsgQueue->send(&msg);
    }
}

int VideoPrepEncJob::status()
{
    return gMediaStatus;
}

int VideoPrepEncJob::saveResizeImg(ImgBlk* img)
{
    static int dumpImgId=0;
    char name[MAX_NAME_LEN];
    log_print(_DEBUG,"VideoPrepEncJob saveResizeImg vaddr %x, paddr %x, size %d\n",(u32)img->vaddr,img->paddr,img->size);
    sprintf(name,"PrepEncDump_%d.raw",dumpImgId++);
    RawFile * file = new RawFile(name);
    if(file) {
        file->write((char *)img->vaddr, img->size);
        delete file;
    }
    return 0;
}

bool VideoPrepEncJob::needUpdateOverlay()
{
    bool en = false;
    struct  timeval    tv;
    //log_print(_DEBUG," CamCapture:: capture gettimeofday to get PTS %lld\n",pts);
    gettimeofday(&tv, NULL);
    
    if( (tv.tv_sec -mLastUpTime.tv_sec ) != 0 ) {   
        en = true;
        gettimeofday(&mLastUpTime, NULL);
    }
    return en;
}
int VideoPrepEncJob::overlayTime(ImgBlk* input, bool needUpdate)
{
    static font_info f_info;
    static int w_offset, h_offset;
    if(mTimeOverlay == NULL)
    {
        f_info.width = 8;
        f_info.height = 16;
        f_info.path = "./dotfont/ASC16";
        f_info.hasBackground = false;
        f_info.background = rgb(0,0,0);
        f_info.font_color = rgb(255,255,255);
        w_offset = 180;
        h_offset = 30;
        mTimeOverlay = new OverlayerI420;
        mTimeOverlay->initialParms(input->width, input->height, input->width-w_offset, input->height-h_offset, true);
    }
    
    if(needUpdate)
    {
        char timestring[128] ={ 0 };
        struct tm *pTm=NULL;
        time_t capTime = time(NULL);
        pTm=gmtime(&capTime);
        if(gRecIniParm.RecFile.dateType == YMD)//y-m-d
            snprintf(timestring, 128, "%04d-%02d-%02d %02d:%02d:%02d",
                (1900+pTm->tm_year),(1+pTm->tm_mon),pTm->tm_mday,pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
        else if(gRecIniParm.RecFile.dateType == MDY)//m/d/y
            snprintf(timestring, 128, "%02d/%02d/%04d %02d:%02d:%02d",
                (1+pTm->tm_mon),pTm->tm_mday,(1900+pTm->tm_year),pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
        else if(gRecIniParm.RecFile.dateType == DMY)//d/m/y
            snprintf(timestring, 128, "%02d/%02d/%04d %02d:%02d:%02d",
                pTm->tm_mday,(1+pTm->tm_mon),(1900+pTm->tm_year),pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
        //f_info.str = "2014/11/6 15:00:00";
        f_info.str = std::string(timestring);
        mTimeOverlay->initialDotData(&f_info);
        mTimeOverlay->initialOverlay((uchar*)input->vaddr);
    } else {
        mTimeOverlay->overlay((uchar*)input->vaddr);
    }
}

int VideoPrepEncJob::overlayGPS(ImgBlk *input,bool needUpdate)
{
    static font_info f_info;
    static int w_offset,h_offset;

/**
    stringLen always equal 22
    
    left-bottom:
    W: 32.1234  N: 32.1345
    
*/  

    if(mGpsOverlay == NULL)
    {
        f_info.width = 8;
        f_info.height = 16;
        f_info.path = "./dotfont/ASC16";
        f_info.hasBackground = false;
        f_info.background = rgb(0,0,0);
        f_info.font_color = rgb(255,255,255);
        w_offset = 30;
        h_offset = 30;
        mGpsOverlay = new OverlayerI420;
        mGpsOverlay->initialParms(input->width,input->height,w_offset,input->height-h_offset,true);
    }

    if(needUpdate )
    {
        char gpsString[30] = { 0 };//longtitude and latitude
        double longti = mLongtitude;
        double lati = mLatitude;
        char W_E = longti > 0 ? 'E' : 'W';//request: if west longtitude, longtitude < 0
        char S_N = lati>0 ? 'N' : 'S';//request: if south latitude, latitude < 0

        if(longti > 180.0)longti = 180.0;
        else if(longti < -180.0)longti = -180.0;
        if(lati > 90.0)lati = 90.0;
        else if(lati < -90.0)lati = -90.0;
        
        snprintf(gpsString, 30, "%c:%8.4f  %c:%8.4f",W_E,longti,S_N,lati);
        
        f_info.str = std::string(gpsString);
        if(mGpsOverlay) {
            mGpsOverlay->initialDotData(&f_info);
            mGpsOverlay->initialOverlay((uchar*)input->vaddr);
        }
    }
    else
    {
        if(mGpsOverlay) {
            mGpsOverlay->overlay((uchar*)input->vaddr);
        }
    }
}

int VideoPrepEncJob::overlayTime()
{
    font_info f_info;
    /*
    f_info.width = 16;
    f_info.height = 32;
    f_info.path = "./dotfont/ASC32";
    int w_offset = 300;
    int h_offset = 60;
    */
    f_info.width = 8;
    f_info.height = 16;
    f_info.path = "./dotfont/ASC16";
    f_info.hasBackground = false;
    int w_offset = 180;
    int h_offset = 30;
    
    f_info.background = rgb(0,0,0);
    f_info.font_color = rgb(255,255,255);
    char timestring[128] = { 0 };
    struct tm *pTm = NULL;
    time_t nowTime = time(NULL);
    pTm=gmtime(&nowTime);
    snprintf(timestring, 128, "%04d-%02d-%02d %02d:%02d:%02d",
        (1900+pTm->tm_year), (1+pTm->tm_mon),pTm->tm_mday,pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    
    //f_info.str = "2014-11-6 15:00:00";
    f_info.str = std::string(timestring);
    
    int stringLen = f_info.str.size();
    uchar ** dotBuf = new uchar*[stringLen];
    uint font_bytes = f_info.width/8*f_info.height;
    for(int i=0;i < stringLen;++i)
        dotBuf[i] = new uchar[font_bytes];
    log_print(_DEBUG,"VideoPrepEncJob overlayTime %s\n",f_info.str.data());
    string_to_dot_matrix(f_info.path.data(),font_bytes,f_info.str.data(),dotBuf);
    int width = mVidPrepEncParms.vencParm.width;
    int height = mVidPrepEncParms.vencParm.height;
    
    uchar * grayBuf = (uchar * )mpOverlayAlpha->vaddr;
    uchar * bgBuf = (uchar * )mpOverlayBg->vaddr;


    dot_overlay_gray(dotBuf,f_info,grayBuf,width,height,width-w_offset,height-h_offset);

    overlayAlpha_Stroke(bgBuf,grayBuf,width,height,
                            width-w_offset,height-h_offset,     //start point(top-left)
                            width-w_offset+f_info.width*f_info.str.size(),//end point(bottom-right)
                            height-h_offset+f_info.height,//end point(bottom-right)
                            f_info,rgb(255,255,255));
    for(int i = 0;i < stringLen;++i)
            delete [] dotBuf[i];
    delete [] dotBuf;

}

int VideoPrepEncJob::overlayGPS()
{
    font_info f_info;
    
    f_info.width = 8;
    f_info.height = 16;
    f_info.path = "./dotfont/ASC16";
    f_info.hasBackground = false;
/**
    stringLen always equal 22
    
    left-bottom:
    W: 32.1234  N: 32.1345
    
*/
    int w_offset = 30;
    int h_offset = 30;

    
    f_info.background = rgb(0,0,0);
    f_info.font_color = rgb(255,255,255);
    char gpsString[30] = { 0 };//longtitude and latitude
    

    char W_E = mLongtitude>0?'E':'W';//request: if west longtitude, longtitude < 0
    char S_N = mLatitude>0?'N':'S';//request: if south latitude, latitude < 0

    if(mLongtitude > 180.0)mLongtitude = 180.0;
    else if(mLongtitude < -180.0)mLongtitude = -180.0;
    if(mLatitude > 90.0)mLatitude = 90.0;
    else if(mLatitude < -90.0)mLatitude = -90.0;
    
    snprintf(gpsString, 30, "%c:%8.4f  %c:%8.4f",W_E,mLongtitude,S_N,mLatitude);
    
    f_info.str = std::string(gpsString);
    
    int stringLen = f_info.str.size();

    uchar * grayBuf = (uchar * )mpOverlayAlpha->vaddr;
    uchar * bgBuf = (uchar * )mpOverlayBg->vaddr;

    uchar ** dotBuf = new uchar*[stringLen];
    uint font_bytes = f_info.width/8*f_info.height;
    for(int i=0;i < stringLen;++i)
        dotBuf[i] = new uchar[font_bytes];

    log_print(_DEBUG,"VideoPrepEncJob overlayGPS %s\n",f_info.str.data());
    string_to_dot_matrix(f_info.path.data(),font_bytes,f_info.str.data(),dotBuf);
    int width = mVidPrepEncParms.vencParm.width;
    int height = mVidPrepEncParms.vencParm.height;

    dot_overlay_gray(dotBuf,f_info,grayBuf,width,height,w_offset,height - h_offset);

    overlayAlpha_Stroke(bgBuf,grayBuf,width,height,
                                w_offset, height - h_offset,    //start point(top-left)
                                w_offset+f_info.width*f_info.str.size(), //x,end point(bottom-right)
                                height - h_offset+f_info.height,    //y,end point(bottom-right)
                                f_info,rgb(255,255,255));       //border-color

    for(int i = 0; i < stringLen; ++i)
            delete [] dotBuf[i];
    delete [] dotBuf;
}

int VideoPrepEncJob::processImg(msgbuf_t *msgbuf)
{
    int ret=-1;
    ImgBlk inputImgBlk;
    int ringNum = mpCamRingQueue->size();
    int  msgNum = gpVidPrepVencMsgQueue->msgNum();
    //log_print(_DEBUG,"VideoPrepEncJob processImg status %x,MsgQueue size %d, RingQueue size %d\n",status(),msgNum,ringNum);
    if(ringNum >= CAM_BUFFER_NUM || msgNum >=10){
        log_print(_INFO,"VideoPrepEncJob processImg status %x,MsgQueue size %d, RingQueue size %d\n",status(),msgNum,ringNum);
        mpCamRingQueue->clear();
        return -1;
    }
    switch(status()) {
        case MEDIA_PLAYBACK:
        case MEDIA_RECORD:
        case MEDIA_IDLE_MAIN:
        case MEDIA_IDLE:
        case MEDIA_CAMERA_CALIBRATION:
            ret = mpCamRingQueue->pop();        
            if(ret < 0) {
                log_print(_ERROR,"VideoPrepEncJob processImg mpCamRingQueue no data input %d\n",ret);
                return ret;
            }
            if(mPause == true && mNeedCaputure == false)
                break;

            inputImgBlk = mpCamRingQueue->front();

            //log_print(_INFO,"VideoPrepEncJob::processImg mPause %d,mNeedCapture %d\n",mPause,mNeedCaputure);
            if(mpImgProc) {
                
                //Venc
                bool nofityCapture = mNeedCaputure;
                if(mFrameId >= mCaptureFps) {
                    //log_print(_DEBUG,"VideoPrepEncJob::processImg fps %d(ini:%d); CaptureFps %d\n",mFrameOpCnt,mPreRecFps,mCaptureFps);
                    mFrameId = 0;
                    mFrameOpCnt = 0;
                }
                
                if(!nofityCapture) {
                    if(frameFilter(mFrameId++) == false){
                        //log_print(_INFO,"VideoPrepEncJob::processImg ignore frameId %d  fps %d(ini:%d); CaptureFps %d\n",mFrameId,mFrameOpCnt,mPreRecFps,mCaptureFps);
                        break;
                    }
                    mFrameOpCnt++;
                } else {
                    mFrameId++;
                }
                
                if(mNeedOverlayTime || mNeedOverlayGPS) {
                    //log_print(_DEBUG,"VideoPrepEncJob processImg overlay() \n");
                    mpImgProc->resize(&inputImgBlk,(mpVencImgBlk[mVencImgBlkId]));
                    overlayUpdateEn = needUpdateOverlay();
                    if(mNeedOverlayTime)
                        overlayTime((mpVencImgBlk[mVencImgBlkId]),overlayUpdateEn);
                    if(mNeedOverlayGPS)
                        overlayGPS((mpVencImgBlk[mVencImgBlkId]),overlayUpdateEn);
                } else {
                    log_print(_DEBUG,"VideoPrepEncJob processImg resize() \n");
                    mpImgProc->resize(&inputImgBlk, (mpVencImgBlk[mVencImgBlkId]));
                }
                #if DUMP_IMG_FILE
                //saveResizeImg(&inputImgBlk);          
                saveResizeImg((mpVencImgBlk[mVencImgBlkId]));           
                #endif
                if(nofityCapture) {
                    notify(MSG_OP_CAPTURE);
                    mNeedCaputure = false;
                } else {
                    notify(MSG_OP_NORMAL);
                }
                
            }
        //case MEDIA_IDLE:
            break;
            
    }
    
    return 0;
}

void VideoPrepEncJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_INFO,"VideoPrepEncJob stop() \n");

    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_STOP;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpVidPrepVencMsgQueue) {
        log_print(_DEBUG,"vidCaptureJob notify to stop \n");
        gpVidPrepVencMsgQueue->send(&msg);
    }
    
}


void VideoPrepEncJob::pause()
{
    if(mPause == false)
        notifyItself(MSG_OP_PAUSE); 
}

void VideoPrepEncJob::resume()
{
    if(mPause) {
        log_print(_INFO, "VideoPrepEncJob resume()\n");
        mPause = false;
        // need to notify next job
        mpVencRingQueue->clear();
        notify(MSG_OP_RESUME);
    }
}

//!!
void VideoPrepEncJob::setOverlay(bool timeOnOff)
{
    if(mNeedOverlayTime != timeOnOff) {
        mNeedOverlayTime = timeOnOff;
    }
    
}
void VideoPrepEncJob::setOverlayGPS(bool gpsOnOff)
{
    if(mNeedOverlayGPS != gpsOnOff) {
        mNeedOverlayGPS = gpsOnOff;
    }
}

int VideoPrepEncJob::setParm(vidPrepEncParms *pParm)
{
    memcpy(&mVidPrepEncParms,pParm,sizeof(imgProcParms));
    return 0;
}

void VideoPrepEncJob::run()
{
    mRunning = true;
    msgbuf_t msg;

    mpVencRingQueue->clear();
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG,"VideoPrepEncJob run()\n");
    while( mRunning ) {
        int kk = gpVidPrepVencMsgQueue->recv(&msg);
        switch(msg.opcode) {
            case MSG_OP_UPDATE_CONFIG:
                printf("mNewWidth%d,mNewHeight%d,mNewCapFps%d,mNewRecFps%d\n",mNewWidth,mNewHeight,mNewCapFps,mNewRecFps);
                updateResolution(mNewWidth,mNewHeight);
                updateFps(mNewCapFps,mNewRecFps);
                break;
            case MSG_OP_NORMAL:
                processImg(&msg);
                break;
            case MSG_OP_RESUME:
                resume();
                break;
            case MSG_OP_START:
                
                break;
            case MSG_OP_PAUSE:
                mPause = true;
                log_print(_INFO, "VideoPrepEncJob pause()\n");
                // need to notify next job
                mpVencRingQueue->clear();
                notify(MSG_OP_PAUSE);   
                break;
            case MSG_OP_STOP:
                mRunning = false;
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                break;
            default:
                break;
        }       
    }
    log_print(_ERROR,"VideoPrepEncJob run() exit\n");
    
}

//opccommand use it to offer pic to next job to capture JPEG pic
int VideoPrepEncJob::captureJPEG( )
{
    log_print(_INFO,"VideoPrepEncJob captureJPEG() \n");
    mNeedCaputure = true;
    return 0;
}


static bool notLessThan(float a, float b)
{
    float c = a - b;
    if((c>-0.000001 && c<0.000001) || a>b){
        //log_print(_INFO,"notLessThan: a %f,b %f true\n",a,b);
        return true;
    } else {
        //log_print(_INFO,"notLessThan: a %f,b %f false\n",a,b);
        return false;
    }
}

bool VideoPrepEncJob::frameFilter(int flameIndex)
{
    if(flameIndex == 0)
        mFrameHasThrown = 0;
    if( notLessThan((float)(flameIndex+1),mFrameRate*(mFrameHasThrown+1))) {
        ++mFrameHasThrown;
        return false;
    }
    return true;
}

