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

#include "audiocapturejob.h"
#include "mediafile/rawfile.h"
#include "platform/swcodec/adpcmencoder.h"
#include "rawfile.h"

#define DUMP_IMG_FILE 0
#if DUMP_IMG_FILE
    RawFile* gpAudFile = NULL;
#endif

extern mediaStatus gMediaStatus;
extern MsgQueue *gpAudCaptureMsgQueue;
extern MsgQueue *gpRecFileMsgQueue;

extern IpcClient * gpMediaCoreIpc;

AudioCaptureJob::AudioCaptureJob(audioParms *pParm)
{
    mRunning = false;
    memcpy(&mAudioParms, pParm, sizeof(audioParms));
    //productor
    mpMediaRingQueue = new ShmRingQueue<MediaBlk>((key_t)RING_BUFFER_AVENC_TO_MEDIA_BUF,RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM,sizeof(MediaBlk));

    mpMic = NULL;
    mpAudEnc = NULL;
    mBufferSize = 0;
    mpBuffer = NULL;
    mPause = false;
    initHw();
#if DUMP_IMG_FILE
     gpAudFile= new RawFile("./audioRawData");
#endif
}

AudioCaptureJob::~AudioCaptureJob()
{
    unInitHw();
    if(mpMediaRingQueue)
        delete mpMediaRingQueue;
#if DUMP_IMG_FILE
     if(gpAudFile)
        delete gpAudFile;
#endif
    
}

int AudioCaptureJob::initHw()
{
    if(mpAudEnc == NULL) {
        if(mAudioParms.codec == (CodecID)CODEC_ID_ADPCM_MS 
            || mAudioParms.codec == (CodecID)CODEC_ID_ADPCM_IMA_WAV) {
            mpAudEnc = new AdpcmEnc (&mAudioParms);
        } else {
            log_print(_ERROR,"AudioCaptureJob::initHw ERROR mAudioParms.codec 0x%x CODEC_ID_ADPCM_MS 0x%x\n",mAudioParms.codec ,CODEC_ID_ADPCM_MS);
        }
    }
    if(mpAudEnc) {
        int frames = mpAudEnc->getFrames();
        if(mpMic == NULL) {
            mpMic = new  MicroPhone(mAudioParms.sampleRate, mAudioParms.sampleSize, frames);
            mBufferSize = mpAudEnc->getFrameSize();
            mpBuffer = (char*)malloc(mBufferSize);
            log_print(_DEBUG,"AudioCaptureJob::initHw malloc buffer %d\n",mBufferSize);
        }
    }
    return 0;
}

int AudioCaptureJob::unInitHw()
{
    if(mpAudEnc)
        delete mpAudEnc;
    if(mpMic)
        delete mpMic;
    if(mpBuffer)
        free(mpBuffer);
    return 0;
}

int AudioCaptureJob::status()
{
    return gMediaStatus;
}

void AudioCaptureJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_DEBUG, "AudioCaptureJob::stop\n");
    mRunning = false;
    /*
    msgbuf_t msg;
    msg.type=MSGQUEU_TYPE_NORMAL;
    msg.len=0;
    msg.opcode=MSG_OP_STOP;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpAudCaptureMsgQueue) {
        log_print(_DEBUG,"AudioCaptureJob notify to stop \n");
        gpAudCaptureMsgQueue->send(&msg);
    }
    */
}

void AudioCaptureJob::pause()
{
    if(!mPause) {
        log_print(_INFO, "AudioCaptureJob::pause\n");
        mPause = true;
    }
}

void AudioCaptureJob::resume()
{
    if(mPause) {
        log_print(_INFO, "AudioCaptureJob::resume\n");
        mPause = false;
    }
}

int AudioCaptureJob::notify(AencBlk *pAenc)
{
    //log_print(_DEBUG,"AudioCaptureJob notify\n");
    int msgNum=0;
    MediaBlk mediablock;
    mediablock.blkType = AENC_BLK;
    memcpy(&(mediablock.blk.aud), pAenc, sizeof(AencBlk));
    mpMediaRingQueue->push(mediablock);
/*
    if(gMediaStatus != MEDIA_RECORD)
        return 0;
    
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_NORMAL;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpRecFileMsgQueue) {
        msgNum = gpRecFileMsgQueue->msgNum();
        //log_print(_INFO,"AudioCaptureJob notify queue num %d\n",msgNum);
        //if(msgNum < ( RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM>>2)) // write file may blocking too many frame
        if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) //if msgNum >61, msgsnd() may be block
            gpRecFileMsgQueue->send(&msg);
        //else 
            //log_print(_DEBUG,"AudioCaptureJob notify ignore, too more queue num %d\n", msgNum);
    }
    */
    return 0;
}

void AudioCaptureJob::run()
{
    mRunning = true;
    AudBlk audioBlk;
    AencBlk encBlk;
    int ret = -1;
    
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG, "AudioCaptureJob run()\n");
    memcpy(&(audioBlk.audParm), &mAudioParms, sizeof(audioParms));
    audioBlk.vaddr = mpBuffer;
    audioBlk.size = mBufferSize;
    audioBlk.paddr = 0;
    //printf("########mBufferSize %d\n",mBufferSize);
    while(mRunning) {
        if(mpMic){
            ret = mpMic->caputre(&audioBlk);
            if(ret <0) {
                sleep(1);
                continue;
            }
            //log_print(_INFO,"AudioCaptureJob::run() mpMic->caputre ret %d\n",ret);
            if(mPause) {
                usleep(200000);//200ms
                continue;
            }
#if DUMP_IMG_FILE
             if(gpAudFile) {
                gpAudFile->write((char*)audioBlk.vaddr, audioBlk.size);
             }
#endif
            
            if(mpAudEnc) {
                ret =mpAudEnc->encode(&audioBlk, &encBlk);
                if(ret < 0) 
                    continue;
                //printf("####encBlk.buf.vStartaddr %d   encBlk.offset %d  encBlk.frameSize %d\n",encBlk.buf.vStartaddr,encBlk.offset,encBlk.frameSize);
                //log_print(_DEBUG,"AudioCaptureJob::run() mpAudEnc->encode ret %d\n",ret);
                notify(&encBlk);
            } else {
                log_print(_ERROR, "AudioCaptureJob run() no Audio Encoder\n");
                sleep(1);
            }
        }else {
            log_print(_ERROR, "AudioCaptureJob run() no MicPhone\n");
            sleep(1);
        }
        
    }
    
    log_print(_INFO, "AudioCaptureJob run() exit\n");
}


