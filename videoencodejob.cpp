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

#include "log/log.h"
#include "ipc/ipcclient.h"
#include "debug/errlog.h"
#include "mediacore.h"

#include "recfilejob.h"
#include "videoencodejob.h"
#include "videocapturejob.h"

#ifdef IMX6_PLATFORM
#include "platform/IMX6/vpuenc.h"
#endif

#include "parms_dump.h"

#include "mediafile/rawfile.h"

#define DUMP_IMG_FILE 0

#define DEBUG_ENC 0
#if DUMP_IMG_FILE
#define VIDEO_BUF_LEN 512000
RawFile *h264File = NULL;
u8 * gpVideoBuf = NULL;
int gVideoBufLen = 0;
#endif

extern mediaStatus gMediaStatus;
extern MsgQueue *gpVidEncMsgQueue;
extern MsgQueue *gpRecFileMsgQueue;

extern IpcClient * gpMediaCoreIpc;
extern vidCaptureJob *gpVidCaptureJob;
extern RecFileJob *gpRecFileJob;

VideoEncodeJob::VideoEncodeJob(vencParms *pParm)
{
    assert(pParm);
    log_print(_DEBUG,"VideoEncodeJob\n");
    memcpy(&mH264EncParm, &(pParm->h264Parm), sizeof(videoParms));
    memcpy(&mJPEGEncParm, &(pParm->jpegParm), sizeof(videoParms));
    memcpy(&mNewVideoParms, &(pParm->h264Parm), sizeof(videoParms));

    mpVencRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_VIN_TO_VENC, RING_BUFFER_VIN_TO_VENC_NUM, sizeof(ImgBlk));
    mpMediaRingQueue = new ShmRingQueue<MediaBlk>((key_t)RING_BUFFER_AVENC_TO_MEDIA_BUF, RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM, sizeof(MediaBlk));
    mpCaptureQueue = new ShmRingQueue<VencBlk>((key_t)RING_BUFFER_AVENC_TO_CAPTURE_BUF, RING_BUFFER_AVENC_TO_CAPTURE_BUF_NUM, sizeof(MediaBlk));
    mpRTSPRingQueue = new ShmRingQueue<RTSPBlk>((key_t)RING_BUFFER_AVENC_TO_RTSP_BUF, RING_BUFFER_AVENC_TO_RTSP_NUM, sizeof(RTSPBlk));
    mpRTSPRingBuffer = new ShmRingBuffer((key_t)RING_BUFFER_RTSP_BUF,RINGBUFFER_SIZE);
    mpMediaRingQueue->clear();
    mpCaptureQueue->clear();
    mpRTSPRingQueue->clear();
    mpRTSPRingBuffer->clear();

    mLiving = false;
    mRunning = false;
    mPause = false;
    mNeedKeyFrame = false;
    mSkipping = false;
    mLastPts = 0;
    mpH264Enc = NULL;
    mpJPEGEnc = NULL;
#ifdef IMX6_PLATFORM
    VpuEnc::VpuInit();
#endif
    initHw();
}

VideoEncodeJob::~VideoEncodeJob()
{
    mRunning = false;
    stop();
    sleep(1);
    
    unInitHw(); 
#ifdef IMX6_PLATFORM
    VpuEnc::VpuUnInit();
#endif
    
    if(mpVencRingQueue)
        delete mpVencRingQueue;
    if(mpMediaRingQueue)
        delete mpMediaRingQueue;
    if(mpRTSPRingQueue)
        delete mpRTSPRingQueue;
}

int VideoEncodeJob::initHw()
{
    log_print(_INFO,"VideoEncodeJob initHw() %d x %d\n",mH264EncParm.width,mH264EncParm.height);

#ifdef IMX6_PLATFORM
    mpH264Enc = new VpuEnc(&mH264EncParm);
    mpJPEGEnc = new VpuEnc(&mJPEGEncParm);
#endif  

    if(mpH264Enc == NULL) {
        log_print(_ERROR,"VideoEncodeJob H264Enc Error, No VideoEncode Created\n");
        return -1;
    }
    if(mpJPEGEnc == NULL) {
        log_print(_ERROR,"VideoEncodeJob JPEGEnc Error, No VideoEncode Created\n");
        //return -1;
    }
    
#if DUMP_IMG_FILE
    h264File = new RawFile((char*)"./vpuencode.h264");
    gpVideoBuf =(u8*)malloc(VIDEO_BUF_LEN);
    gVideoBufLen =0;
#endif

    return 0;
}

int VideoEncodeJob::unInitHw()
{
    log_print(_INFO,"VideoEncodeJob unInitHw()\n");
    if(mpH264Enc) {
        delete mpH264Enc;
        mpH264Enc= NULL;
    }
    
    if(mpJPEGEnc){//?? corrupted double-linked list
        delete mpJPEGEnc;
        mpJPEGEnc= NULL;
    }
#if DUMP_IMG_FILE
    if(h264File) {
        delete h264File;
        h264File = NULL;
    }
#endif
    return 0;
}

int VideoEncodeJob::updateEncoderParms(videoParms *pParm)
{   
    if(memcmp(pParm,&mH264EncParm, sizeof(videoParms)) != 0) { //different
        memcpy(&mH264EncParm,pParm,sizeof(videoParms));
        memcpy(&mJPEGEncParm,pParm,sizeof(videoParms));
        mJPEGEncParm.fourcc = gCapIniParm.PicCapParm.fourcc;
        mJPEGEncParm.codec = gCapIniParm.PicCapParm.codec;//JPEG ?
    
        unInitHw();
        initHw();
        mpMediaRingQueue->clear();
        
    }
}

int VideoEncodeJob::status()
{
    return gMediaStatus;
}

void VideoEncodeJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_DEBUG,"VideoEncodeJob stop()\n");
    mRunning = false;

    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_EXIT;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpVidEncMsgQueue) {
        //log_print(_DEBUG,"VideoEncodeJob notify to exit \n");
        gpVidEncMsgQueue->send(&msg);
    }

}

int VideoEncodeJob::jpegEncode(ImgBlk *imgsrc, VencBlk *pVencDest)
{
    int ret = 0;
    log_print(_DEBUG,"VideoEncodeJob jpegEncode\n");
    if(mpJPEGEnc) {
        ret = mpJPEGEnc->encode(imgsrc, pVencDest);
    }
    return ret;
}

int VideoEncodeJob::h264Encode(ImgBlk *imgsrc, VencBlk *pVencDest)
{
    int ret = 0;
    //log_print(_DEBUG,"VideoEncodeJob h264Encode vaddr %x, paddr %x, size %d, pts %llu\n",(u32)imgsrc->vaddr,imgsrc->paddr,imgsrc->size,imgsrc->pts);
    //log_print(_DEBUG,"VideoEncodeJob h264Encode \n");
    if(mpH264Enc) {
        if(mNeedKeyFrame) {
            ret = mpH264Enc->encode(imgsrc, pVencDest, true);
            mNeedKeyFrame = false;
        } else {
            ret = mpH264Enc->encode(imgsrc, pVencDest);
        }
    }
    return ret;
}

int VideoEncodeJob::notify(int msgopc)
{
    if(msgopc != MSG_OP_CAPTURE) {
        if(gMediaStatus != MEDIA_RECORD)
            return 0;
    }
    int msgNum = 0;
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;//MSG_OP_NORMAL;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpRecFileMsgQueue) {
        //msgNum = gpRecFileMsgQueue->msgNum();
        //log_print(_INFO,"VideoEncodeJob notify queue num %d\n",msgNum);
        //if(msgNum < ( RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM>>2)) // write file may blocking too many frame
       // if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) {//if msgNum >61, msgsnd() may be block
            //gpRecFileMsgQueue->send(&msg);
       // } else
        if(msgopc == MSG_OP_CAPTURE) {
            gpRecFileMsgQueue->send(&msg);
        //} else {
            //log_print(_DEBUG, "VideoEncodeJob notify ignore, too more queue num %d\n", msgNum);
        }
    }
    return 0;
}

int VideoEncodeJob::notify(VencBlk *pVenc)
{
    //log_print(_DEBUG,"VideoEncodeJob notify\n");
    int size = 0;
    MediaBlk mediablock;
    mediablock.blkType = VENC_BLK;
    memcpy(&(mediablock.blk.vid), pVenc, sizeof(VencBlk));
    if(pVenc->codec == CODEC_ID_MJPEG) {
        mpCaptureQueue->push(mediablock.blk.vid);
        notify(MSG_OP_CAPTURE);
    } else {
        mpMediaRingQueue->push(mediablock);
        notify(MSG_OP_NORMAL);
    }
            /*
            struct timeval tv;
            gettimeofday(&tv, NULL);
            unsigned long long pts_sec = tv.tv_sec*1000 + tv.tv_usec/1000;
            unsigned long long captureTime = gpVidCaptureJob->getCaptureTime()*1000 + pVenc->pts;
            //printf("captureTime:%llu encodeTime:%llu \n", captureTime, pts_sec, pts_sec-captureTime);
            log_print(_INFO, "VideoEncodeJob delay:%llu ms \n", pts_sec-captureTime);
            */

#if 1
    if(mLiving == true) {//TODO
        RTSPBlk rtspblock;
        rtspblock.mediaSize = pVenc->frameSize;
        rtspblock.frameType = pVenc->frameType;
        size = mpRTSPRingQueue->size();
        if(size < 3)
            log_print(_ERROR, "VideoEncodeJob living mpRTSPRingQueue size %d \n", size);
        if(pVenc->frameSize < MAX_FRAME_SIZE) {
            memcpy(rtspblock.mediaData, (unsigned char* )pVenc->buf.vStartaddr+pVenc->offset, pVenc->frameSize);
            if(mpRTSPRingQueue != NULL)
                mpRTSPRingQueue->push(rtspblock);            
        } else {
            log_print(_ERROR, "VideoEncodeJob::notify() frame size %u, ignore\n", pVenc->frameSize);
        }
    }
#else 
    if(!mLiving) {
        RTSPBlk rtspblock;
        rtspblock.mediaSize = pVenc->frameSize;
        rtspblock.frameType = pVenc->frameType;
        rtspblock.mediaData = mpRTSPRingBuffer->putData(pVenc->buf.vStartaddr+pVenc->offset, pVenc->frameSize);
        mpRTSPRingQueue->push(rtspblock);            
    }


#endif
    //printf("img.height %d img.width,%d img.size %d bufsize%d\n", pVenc->height,pVenc->width,pVenc->frameSize,pVenc->buf.bufSize);
    return 0;
}

int VideoEncodeJob::notifyItself(int msgopc)
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpVidEncMsgQueue) {
        log_print(_DEBUG, "VideoPrepEncJob notifyItself queue num %d\n", gpVidEncMsgQueue->msgNum());
        gpVidEncMsgQueue->send(&msg);
    }
}

int VideoEncodeJob::stopLiveStreaming()
{
    if(mLiving == true) {
        printf("VideoEncodeJob::stopLiveStreaming\n");
        mLiving = false;
        mpRTSPRingQueue->clear();
    }
    return 0;
}

int VideoEncodeJob::startLiveStreaming()
{
    mpRTSPRingQueue->clear();
    if(mLiving == false) {
        printf("VideoEncodeJob::startLiveStreaming\n");
        mLiving = true;
    }
    return 0;
}

void VideoEncodeJob::pause()
{
    if(mPause == false) {
        mPause = true;
        log_print(_INFO, "VideoEncodeJob pause() mpMediaRingQueue clear %d buf\n", mpMediaRingQueue->size());
        mpMediaRingQueue->clear();
        mpVencRingQueue->clear();
        // need to notify next job
        //notify(MSG_OP_PAUSE);
    }
}

void VideoEncodeJob::resume()
{
    if(mPause) {
        mPause = false;
        log_print(_INFO, "VideoEncodeJob resume()\n");
        // need to notify next job
        mpMediaRingQueue->clear();
        mpVencRingQueue->clear();
        //notify(MSG_OP_RESUME);
    }
}

void VideoEncodeJob::run()
{
    mRunning = true;
    msgbuf_t msg;
    int ret = -1;
    int ringNum = 0, msgNum = 0;
    ImgBlk inputImgBlk;
    VencBlk outputVencBlk;
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG, "VideoEncodeJob run()\n");
    int frameNum = 0;

    while( mRunning ) {
        gpVidEncMsgQueue->recv(&msg);
        ringNum = mpVencRingQueue->size();
        msgNum = gpVidEncMsgQueue->msgNum();
        if(ringNum >= RING_BUFFER_VIN_TO_VENC_NUM || msgNum >= 10){
            log_print(_INFO,"VideoEncodeJob run() MsgQueue size %d RingQueue size %d\n",msgNum,ringNum);
        }
    
        ret = -1;
        switch(msg.opcode) {
            case MSG_OP_UPDATE_CONFIG:
                updateEncoderParms(&mNewVideoParms);

                break;
            case MSG_OP_CAPTURE:
                ret = mpVencRingQueue->pop();
                if(ret < 0) {
                    log_print(_ERROR,"VideoEncodeJob run() MSG_OP_CAPTURE no ImgBlk input\n");
                    continue;
                }
                inputImgBlk = mpVencRingQueue->front();
                //captureJPEG(msg.data, &inputImgBlk);
                ret = jpegEncode(&inputImgBlk, &outputVencBlk);
                if(ret >=0){
                    notify(&outputVencBlk);
                    //gpRecFileJob->captureJPEG(&outputVencBlk);
                    log_print(_INFO,"VideoEncodeJob::run() notify capture.\n");
                }
                //break;
            case MSG_OP_NORMAL:
                if(ret < 0) {
                    if(mSkipping) {
                        if(ringNum < RING_BUFFER_VIN_TO_VENC_NUM - 5) //delay some frames
                            break;
                    }
                    
                    ret = mpVencRingQueue->pop();
                    if(ret < 0) {
                        log_print(_ERROR,"VideoEncodeJob run() MSG_OP_NORMAL no ImgBlk input\n");
                        continue;
                    }
                    
                    if(mSkipping) {
                        if(++frameNum < mNewVideoParms.fps)
                            break;
                        frameNum = 0;
                    }
                    inputImgBlk = mpVencRingQueue->front();
                }
                
                #if DEBUG_ENC                   
                if(mLastPts != 0) {
                    int diff = inputImgBlk.pts - mLastPts;
                    if(diff >50)//50ms
                        log_print(_DEBUG,"VideoEncodeJob interval too big %d, msgNum %d, RingQueue size %d,ImgBlk.pts %llu, mLastPts %llu\n",
                            diff,gpVidEncMsgQueue->msgNum(),mpVencRingQueue->size(),inputImgBlk.pts,mLastPts);
                }
                mLastPts = inputImgBlk.pts;
                #endif
                
                if(mPause)break;//encodejob will generate a video flame if not break when paused
                
                ret = h264Encode(&inputImgBlk, &outputVencBlk);
                if(ret >= 0) {
                    #if DUMP_IMG_FILE
                    saveH264Data(&outputVencBlk);
                    #endif          
                    //log_print(_INFO,"VideoEncodeJob::run() notify normal.\n");
                    notify(&outputVencBlk);
                }
                break;
            case MSG_OP_PAUSE:
                pause();
                break;
            case MSG_OP_RESUME:
                resume();
                mNeedKeyFrame = true;
                frameNum = 0;
                mSkipping = false;
                break;
            default:
                break;
        }
        
    }
    log_print(_ERROR,"VideoEncodeJob run() exit\n");
    
}

#if DUMP_IMG_FILE
int VideoEncodeJob::saveH264Data(VencBlk *pVencBlk)
{
    int ret = -1;
    u8 *data = NULL;
    data = (u8*)(pVencBlk->buf.vStartaddr) + pVencBlk->offset;
    int size = pVencBlk->frameSize;
    log_print(_DEBUG, "VideoEncodeJob saveH264Data data %x ,size %d\n", (u32)data, size );
    if((pVencBlk->offset+pVencBlk->frameSize) >= pVencBlk->buf.bufSize) {
        int len = size > VIDEO_BUF_LEN ? VIDEO_BUF_LEN : size;
        int offset = pVencBlk->buf.bufSize - pVencBlk->offset;
        log_print(_DEBUG, "VideoEncodeJob writeVencBlk using Software Video Buf %d \n", size);
        memcpy(gpVideoBuf, data, offset);
        memcpy(gpVideoBuf + offset, pVencBlk->buf.vStartaddr, len - offset);
        data = gpVideoBuf;
        size = len;
    }
    if(h264File){
        ret = h264File->write((char*)data, size);
    }
    return ret;
}
#endif


int VideoEncodeJob::updateConfig(videoParms *pParm)
{
    memcpy(&mNewVideoParms, pParm, sizeof(videoParms));
    notifyItself(MSG_OP_UPDATE_CONFIG);
    return 0;
}

//int VideoEncodeJob::setSkipping(bool flag)
//{
//  return mpH264Enc->setSkipping(flag);
//}

int  VideoEncodeJob::setSkipping(bool flag)
{
    mSkipping = flag;
    return 0;
}


