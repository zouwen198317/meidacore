#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include <sys/types.h>
#include <dirent.h>

#include "videocapturejob.h"
#include "log/log.h"
#include "debug/errlog.h"
#include "mediacore.h"
#include "motion_detection_technology.h"
#ifdef IMX6_PLATFORM
#include "platform/IMX6/imageprocessunit.h"
#endif


#define DEBUG_CAPTURE 1
#define IMG_DIR_PATH "./nocamPic"

extern mediaStatus gMediaStatus;

//extern MsgQueue *gpVidCaptMsgQueue;
extern MyMotionDetectionTechnology* gpMotionDetectJob;
extern MsgQueue *gpImgProcMsgQueue;
extern MsgQueue *gpVidPrepVencMsgQueue;
extern MsgQueue *gpMotionDetectMsgQueue;


vidCaptureJob::vidCaptureJob(vidCapParm *pParm)
{
    mpRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_VIN, CAM_BUFFER_NUM, sizeof(ImgBlk));
    mpVidPrepEncRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_VINPREPENC, CAM_BUFFER_NUM, sizeof(ImgBlk));
    mpMotionDetectRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_MOTIONDETECT, CAM_BUFFER_NUM, sizeof(ImgBlk));
    
    mpRingQueue->clear();
    mpVidPrepEncRingQueue->clear();
    mpMotionDetectRingQueue->clear();
    
    mRunning = false;
    mLastPts = 0;
    
#ifdef NO_CAM
    m_camType = CAM_NULL;
    gettimeofday(&mNoCamTv, NULL);
#else
    m_camType = CAM_OV10635;

    mpCamera = new CamCapture;
    if(mpCamera)
        mpCamera->configure(pParm);
    mFps = pParm->fps;
    m_camType = mpCamera->getCamType();
#endif//NO_CAM

    if(m_camType != CAM_OV10635){
        mCaptureImgBlkId = 0;
        ImgBlk *pImgblk = NULL;
        
    #ifdef IMX6_PLATFORM
        mpImgCap = new CIpu;
    #endif //IMX6_PLATFORM

        if(mpImgCap == NULL) {
            log_print(_ERROR, "vidCaptureJob Error, No ImgProcess Created\n");
        } else {
            for(int i=0; i<CAM_BUFFER_NUM; i++) {
                pImgblk = mpImgCap->allocHwBuf(pParm->width, pParm->height, pParm->fourcc);
                mpCaptureImgBlk[i] = pImgblk;
                log_print(_INFO,"vidCaptureJob IMG alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);
            }
        }
    } else {
        log_print(_INFO, "vidCaptureJob m_camType is CAM_OV10635\n");
    }   
}

vidCaptureJob::~vidCaptureJob()
{
    stop();
    
#ifndef NO_CAM
    if(mpCamera)
        delete mpCamera;
#endif
    
    if(mpRingQueue)
        delete mpRingQueue;
    if(m_camType != CAM_OV10635) {
        for(int i=0; i<CAM_BUFFER_NUM; i++) {
            if(mpCaptureImgBlk[i])
                mpImgCap->freeHwBuf(mpCaptureImgBlk[i]);
        }

        if(mpImgCap)
            delete mpImgCap;
    }   
}

int vidCaptureJob::notify()
{
    //log_print(_DEBUG,"vidCaptureJob notify\n");
    int msgNum = 0;
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_NORMAL;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    
    if(gpVidPrepVencMsgQueue) {
        msgNum = gpVidPrepVencMsgQueue->msgNum();
        if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) {
            gpVidPrepVencMsgQueue->send(&msg);
        } else {
            log_print(_ERROR, "vidCaptureJob notify VidPrepVenc queue num %d\n", msgNum);
        }
    }
    
    if(gpImgProcMsgQueue && (status() > MEDIA_PLAYBACK)) {
        msgNum = gpImgProcMsgQueue->msgNum();
        if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) {
            gpImgProcMsgQueue->send(&msg);
        } else {
            log_print(_ERROR, "vidCaptureJob notify ImgProc queue num %d\n", msgNum);
        }
    }
    #ifndef NO_CAM
    if(gpMotionDetectJob->isRunning()) {
        if(gpMotionDetectMsgQueue) {
            msgNum = gpMotionDetectMsgQueue->msgNum();
            if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) {
                gpMotionDetectMsgQueue->send(&msg);
            } else {
                log_print(_ERROR, "vidCaptureJob notify MotionDetection queue num %d\n", msgNum);
            }
        }
    }
    #endif
    
    return 0;
}

int vidCaptureJob::status()
{
    return gMediaStatus;
}

void vidCaptureJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_INFO,"vidCaptureJob stop\n");
    mRunning = false;
}

// returns the startup time as the number of seconds since the Epoch, 1970-01-01 00:00:00
time_t vidCaptureJob::getCaptureTime()
{
    time_t startPts;
#ifdef NO_CAM
    startPts = mNoCamTv.tv_sec;
#else
    startPts = mpCamera->getStartPts();
#endif
    //pts = startPts*1000 + time_t(pts_msec);
    return startPts;
}

int vidCaptureJob::closeCamera()
{
    FILE* fp = fopen(CAM_POWER_ENABLE_FILE,"w");
    if(fp == NULL) {
        log_print(_ERROR,"vidCaptureJob closeCamera() fopen failed\n");
        return -1;
    }
    char enable = '0';
    fputc(enable, fp);  
    fclose(fp);
    return 0;
}

int vidCaptureJob::updateConfig(vidCapParm *pParm)
{
    if(pParm->fps != mFps)
    {
#ifndef NO_CAM
        mpCamera->stopCapture();
        if(mpCamera) {
            mpCamera->configure(pParm);
        }
        mpCamera->startCapture();
#endif
        log_print(_INFO,"---vidCaptureJob updateConfig() captureFps %d -> %d\n", mFps, pParm->fps);
        mFps = pParm->fps;
        return 0;
    }
    return -1;
}

void vidCaptureJob::get_buffer_info(fbuffer_info* bufInfo)
{
    if(bufInfo) {
        if(m_camType == CAM_OV10635) {
            if(mpCamera)
                mpCamera->get_buffer_info(bufInfo);
        } else {
            int i;
            for(i=0; i<CAM_BUFFER_NUM; i++) {
                bufInfo->addr[i] = (u32)mpCaptureImgBlk[i]->vaddr;
                bufInfo->length = mpCaptureImgBlk[i]->size;
            }
            bufInfo->num = i;
        }
    }
}

#ifndef NO_CAM
void vidCaptureJob::run()
{
    fd_set fdset;
    struct timeval timeout;
    int maxfd = 0;
    int ret = mpCamera->startCapture();
    if(ret < 0) {
        log_print(_ERROR, "vidCaptureJob startCapture failed exit\n");
    }
    ImgBlk img;
    mRunning = true;

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG,"vidCaptureJob run()\n");
    
    while( mRunning ) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        FD_ZERO(&fdset);

        if(mpCamera->getFd() >= 0) {
            FD_SET(mpCamera->getFd(), &fdset);
        } else {
            sleep(1);
            log_print(_INFO, "vidCaptureJob Camera fd %d\n", mpCamera->getFd());
            continue;
        }
        maxfd = mpCamera->getFd();
        ret = select(maxfd+1, &fdset, NULL, NULL, &timeout);
        
        if(ret == -1) {
            log_print(_ERROR, "vidCaptureJob EvetMgr select() failed\n");
        }  else if (ret) {   
            if (FD_ISSET(mpCamera->getFd(), &fdset)) {
                //log_print(_DEBUG,"vidCaptureJob run() capture\n");
                ret = mpCamera->capture(&img);
                if((ret >= 0) /*&& (status() > MEDIA_PLAYBACK)*/) {
                    if(m_camType != CAM_OV10635) {
                    
                        log_print(_DEBUG,"vidCaptureJob run() copy data to ipu physical memory,vaddr %x <-- %x, size %u\n",mpCaptureImgBlk[mCaptureImgBlkId]->vaddr, img.vaddr, img.size);
                        memcpy(mpCaptureImgBlk[mCaptureImgBlkId]->vaddr, img.vaddr, img.size);
                        mpCaptureImgBlk[mCaptureImgBlkId]->pts = img.pts;
                        
                        log_print(_DEBUG,"vidCaptureJob run() copy data OK\n");
                        mpRingQueue->push(*mpCaptureImgBlk[mCaptureImgBlkId]);
                        mpVidPrepEncRingQueue->push(*mpCaptureImgBlk[mCaptureImgBlkId]);
                        
                        mCaptureImgBlkId++;
                        if(mCaptureImgBlkId >= CAM_BUFFER_NUM)
                            mCaptureImgBlkId = 0;
                        
                    } else {
                        mpRingQueue->push(img);
                        mpVidPrepEncRingQueue->push(img);
                        if(gpMotionDetectJob->isRunning()) {
                            mpMotionDetectRingQueue->push(img);
                        }
                        //printf("img.height %d img.width %d img.size %d \n", img.height,img.width,img.size);
                        //$ img.height 720 img.width 1280 img.size 1843200
                    }
    #if DEBUG_CAPTURE                   
                    if(mLastPts != 0) {
                        int diff = img.pts - mLastPts;
                        if(diff > 50) //50ms
                            log_print(_ERROR,"vidCaptureJob interval too big %d, imgpro %d, prepenc %d, img.pts %llu ,mLastPts %llu\n",diff,mpRingQueue->size(),mpVidPrepEncRingQueue->size(),img.pts,mLastPts);
                    }
                    mLastPts = img.pts;
    #endif //DEBUG_CAPTURE
                    notify();
                }
            } 
        }       
    }
    log_print(_ERROR,"vidCaptureJob run() exit\n");
}

#else

void vidCaptureJob::run()
{
    int flen,ret;
    char *yuvData = NULL;
    struct timeval cap_freq;
    u64 pts = 0,pts_sec = 0; //ms
    struct timeval tv;
    flen = getYuvData(&yuvData);
    log_print(_INFO,"vidCaptureJob getYuvData length %d\n", flen);
    if(yuvData != NULL) {
        for(int i=0; i<CAM_BUFFER_NUM; i++)
            memcpy(mpCaptureImgBlk[i]->vaddr, yuvData, flen);
    }

    mRunning = true;

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG,"vidCaptureJob run()\n");
    
    while( mRunning ) {
        if(yuvData == NULL) {
            log_print(_ERROR,"vidCaptureJob run() yuvData is NULL, Ensure that existing YUV images in ./nocamPic\n");
            sleep(1);
            continue;
        }
        cap_freq.tv_sec = 0;
        cap_freq.tv_usec = 40000;
        ret = select(0, NULL, NULL, NULL, &cap_freq);
        if (ret == -1) {
            log_print(_ERROR, "vidCaptureJob EvetMgr select() failed\n");
           } else {
            gettimeofday(&tv, NULL);
            pts_sec = tv.tv_sec;
            pts = ((pts_sec - mNoCamTv.tv_sec) * 1000) + tv.tv_usec/1000; //ms
            //log_print(_DEBUG," CamCapture:: capture gettimeofday to get PTS %llu ms\n",pts);
            mpCaptureImgBlk[mCaptureImgBlkId]->pts = pts;

            mpRingQueue->push(*mpCaptureImgBlk[mCaptureImgBlkId]);
            mpVidPrepEncRingQueue->push(*mpCaptureImgBlk[mCaptureImgBlkId]);
            
            mCaptureImgBlkId++;
            if(mCaptureImgBlkId >= CAM_BUFFER_NUM)
                mCaptureImgBlkId = 0;

        #if DEBUG_CAPTURE                   
            if(mLastPts != 0) {
                int diff = pts - mLastPts;
                if(diff >50) //50ms
                    log_print(_ERROR, "vidCaptureJob interval too big %d, imgpro %d, prepenc %d, img.pts %llu, mLastPts %llu\n", diff, mpRingQueue->size(), mpVidPrepEncRingQueue->size(), pts, mLastPts);
            }
            mLastPts = pts;
        #endif //DEBUG_CAPTURE

            notify();
           }
    }
    free(yuvData);
    yuvData = NULL;
    
    log_print(_ERROR,"vidCaptureJob run() exit\n");
}

int vidCaptureJob::getYuvData(char **yuvData)
{
    DIR *dp;
    struct dirent* entry;
    struct stat statbuf;
    int flen = 0, ret;
    FILE *fp = NULL;
    char file_path[128];
    memset(file_path, 0, 128);
    
    if((dp = opendir(IMG_DIR_PATH)) == NULL) {
        log_print(_ERROR, "Videocapturejob getYuvData() Cannot open directory %s\n", IMG_DIR_PATH);
        return -1;
    }
    while((entry = readdir(dp)) != NULL) {
        char file_path[128];
        sprintf(file_path, "%s/%s", IMG_DIR_PATH, entry->d_name);
        log_print(_DEBUG, "file path %s\n", file_path);
        lstat(file_path, &statbuf);
        if(S_IFDIR & statbuf.st_mode)//directory
            continue;
        fp = fopen(file_path, "rb");
        if(fp == NULL) {
            log_print(_DEBUG, "Videocapturejob getYuvData() Cannot fopen path %s\n", IMG_DIR_PATH);
            closedir(dp);
            return -2;
        } else {
            fseek(fp, 0L, SEEK_END); 
            flen = ftell(fp);
            fseek(fp, 0L, SEEK_SET);
            log_print(_DEBUG, "file size %d\n", flen);
            
            *yuvData = (char *)malloc(flen);
            if(*yuvData == NULL) {
                log_print(_ERROR, "Videocapturejob getYuvData() yuvData malloc fail!\n");
                fclose(fp);
                closedir(dp);
                return -3;
            }
            if((ret = fread(*yuvData, 1, flen, fp)) != flen)
                log_print(_ERROR, "fread error, fread %d byte\n", ret);
        }
        break;
    }//while
    if(fp == NULL) {
        log_print(_ERROR, "Videocapturejob getYuvData() Did not find YUV images in the ./nocamPic!\n");
    } else {
        fclose(fp);
    }
    closedir(dp);
    return flen;
}
#endif
