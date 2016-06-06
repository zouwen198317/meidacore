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

#include "playbackjob.h"
#include "mediacore.h"
#include "debug/errlog.h"
#include "log/log.h"
#include "ipc/ipcclient.h"

#include "ffmpeg/avcodec_common.h"
#include "ipc/msgqueue.h"

#include "imageprocessjob.h"
#include "videoprepencodejob.h"
#include "readPacketjob.h"

#include "ipcopcode.h"
#include "avidemuxer.h"

#ifdef IMX6_PLATFORM
#include "platform/IMX6/vpudec.h"
#endif
#include "platform/swcodec/adpcmdecoder.h"
#include "ffmpeg/avformat/utils.h"
#include "mediafile/rawfile.h"

#define DUMP_IMG_FILE 0
#define PB_PKG_BUF_LEN 307200
#define AUDIO_BUF_LEN 5000
#define AUDIO_VIDEO_ALLOWABLE_DELAY 2000

extern MsgQueue *gpPlayBackMsgQueue;
extern MsgQueue *gpImgProcMsgQueue;

extern mediaStatus gMediaStatus;
mediaStatus gMediaStatus_backup = MEDIA_IDLE;
extern IpcClient * gpMediaCoreIpc;
extern ImgProcessJob *gpImgProcJob;
static u32 gmsec = 0;
int gpNetChannel = -1;

int  sendEndFileEvent(int endFlag)
{
    log_print(_DEBUG, "PlayBackJob sendEndFileEvent\n");
    MsgPacket packet;
    OpcEvent_t event;
    event.value = endFlag;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_GUI_EVENT);
    packet.header.opcode = OPC_PLAYBACK_END_EVENT;
    packet.header.len = sizeof(OpcEvent_t);
    memcpy(packet.args, &event, sizeof(OpcEvent_t));    
    return gpMediaCoreIpc->sendPacket(&packet);
}

int  sendCurrentTimeEvent(int curtime, int duration)
{
    log_print(_INFO,"PlayBackJob sendCurrentTimeEvent() curtime:%d duration:%d\n", curtime, duration);
    MsgPacket packet;
    OpcPlayCurPos_t playcurpos;
    playcurpos.curPos = curtime;
    playcurpos.fileDuration = duration;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_GUI_EVENT);
    packet.header.opcode = OPC_PLAYBACK_CUR_POS_EVENT;
    packet.header.len = sizeof(OpcPlayCurPos_t);
    memcpy(packet.args, &playcurpos, sizeof(OpcPlayCurPos_t));
    return gpMediaCoreIpc->sendPacket(&packet);
}

int findFileType(char *filename)
{
    char suffix[64] = {0};
    //int len = strlen(filename);
    char *dot = strrchr(filename, '.');
    strcpy(suffix, dot);
    int suflen = strlen(suffix);
    for(int i =0; i <suflen; i++) {
        suffix[i] = toupper(suffix[i]);
    }
    
    if(dot) {
        if(strcmp(suffix, ".AVI") == 0) {
            return FILE_AVI;
        } else if(strcmp(suffix, ".MOV") == 0) {
            return FILE_MOV;
        } else if(strcmp(suffix, ".MP4") == 0) {
            return FILE_MP4;
        } else {
            return -1;
        }
    }
    return -1;
}

PlayBackJob::PlayBackJob()
{
    mRunning = false;
    mPlaying = false;
    mPause = false;
    mNet = false;
    mpReadPacketJob = NULL;
    mpReadPacketJob_net = NULL;
    mpFileDemux = NULL;
    mpAudDec = NULL;
    mpVidDec = NULL;

    memset(mPlayFileName, 0, MAX_FILE_NAME_LEN);;
    mpPkgBuf = NULL;
    mpAudBuf = NULL;
    mpAudioPlay = NULL;
    mStartTime =0;
    //st_time = 0;
    //en_time = 0;
    mCurtime = -1;
    mDuration = -1; 
    //acc_AudioTime = 0;
    clearAVSync();
    mAudioPacketDur = 0;    
    mpRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_PLAY_TO_VIN, RING_BUFFER_PLAY_TO_VIN_NUM, sizeof(ImgBlk));
    fetchPktRingQueue = new ShmRingQueue<AVPacket>((key_t)RING_BUFFER_SDCARD_TO_PLAY, RING_BUFFER_SDCARD_TO_PLAY_NUM, sizeof(AVPacket));

}

PlayBackJob::~PlayBackJob()
{
    endFile();
    endFile_net();
    if(mpRingQueue)
        delete mpRingQueue;
}


int PlayBackJob::notifyDishStop()
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_DISHSTOP;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpImgProcMsgQueue) {
        log_print(_DEBUG, "PlayBackJob notify imageprocess to change buffer.\n");
        gpImgProcMsgQueue->send(&msg);
    }
    return 0;
}

int PlayBackJob::notify()
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_PLAY;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpImgProcMsgQueue) {
        //log_print(_DEBUG, "PlayBackJob notify to display \n");
        gpImgProcMsgQueue->send(&msg);
    }
    return 0;
}

int PlayBackJob::notifyItself(int msgopc)
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpPlayBackMsgQueue) {
        //log_print(_DEBUG, "PlayBackJob notifyItself %d \n", msgopc);
        gpPlayBackMsgQueue->send(&msg);
    }
    return 0;
}


int PlayBackJob::pause(bool isNet)
{
    int ret = -1;
    //if(mPause == false && mPlaying == true) {
    if(isNet) {
        //printf("PlayBackJob::pause()\n");
        if(mpReadPacketJob_net)
            mpReadPacketJob_net->pause();
        ret = 0;
    } else if(mPlaying == true) {
        log_print(_DEBUG, "PlayBackJob pause()\n");
        notifyItself(MSG_OP_PAUSE);
        ret =0;
    }
    return ret;
}

int PlayBackJob::resume(bool isNet)
{
    int ret = -1;
    //if(mPause && mPlaying == true) {
    if(isNet) {
        if(mpReadPacketJob_net)
            mpReadPacketJob_net->resume();
        ret = 0;
    } else if(mPlaying == true) {
        log_print(_DEBUG, "PlayBackJob resume()\n");     
        notifyItself(MSG_OP_RESUME);
        ret = 0;
    }
    return ret;
}

int PlayBackJob::startPlay(char *fileName, bool net)
{
    int ret = -1;
    log_print(_DEBUG, "PlayBackJob startPlay()\n");
    strcpy(mPlayFileName, fileName);
    mNet = net;
    if(net) {
        notifyItself(MSG_OP_START_NET);
    } else {
        if(mPlaying == false) 
            notifyItself(MSG_OP_START);
    }
    ret = 0;
    return ret;
}

int PlayBackJob::stopPlay(bool isNet)
{
    int ret = -1;
    log_print(_DEBUG,"PlayBackJob stopPlay()\n");
    if(isNet) {
        notifyItself(MSG_OP_STOP_NET);
    } else {
        if(mPlaying ) {
            gpImgProcJob->videoDisplayOnOff(false);
            notifyItself(MSG_OP_STOP);
        }
    }
    ret = 0;
    return ret;
}

//it seems not elegance.Perhaps,send event to Gui by playbackjob will better
int PlayBackJob::seek(unsigned int msec, bool isNet)
{
    int ret = -1;
    //printf("### pause seek() %d\n", mPause);
    log_print(_INFO, "PlayBackJob seek() %d\n", msec);
    gmsec = msec;
    //st_time = 0;
    //en_time = 0;
    //acc_AudioTime = 0;
    if(isNet) {
        notifyItself(MSG_OP_SEEK_NET);
    } else {
        if(mPlaying)
            notifyItself(MSG_OP_SEEK);
    }
    ret = 0;
    return ret;
}

void PlayBackJob::run()
{
    mRunning = true;
    msgbuf_t msg;
    int ret = 0;

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG, "PlayBackJob run()\n");
    
    while( mRunning ) {
        gpPlayBackMsgQueue->recv(&msg);
        switch(msg.opcode) {
            case MSG_OP_PLAY:
            case MSG_OP_NORMAL:
                if(!mPause) {
                    playFilePacket();
                }
                if(mpReadPacketJob!=NULL && mpReadPacketJob->isTheEnd() && fetchPktRingQueue->empty()) {
                    //notifyItself(MSG_OP_STOP);//gui send it
                    sendEndFileEvent(PLAYBACK_NORMAN_END);
                }
                break;
            case MSG_OP_RESUME:
                if(mPause == true) {
                    printf("PlayBackJob::run() MSG_OP_RESUME\n");
                    mPause = false;
                    notifyItself(MSG_OP_NORMAL);
                    clearAVSync();
                }
                break;
            case MSG_OP_START:
                mPlaying = true;
                //mPause = true;
                //sendCurrentTimeEvent(0, 0);
                gMediaStatus_backup = gMediaStatus;
                gMediaStatus = MEDIA_PLAYBACK;
                log_print(_INFO, "PlayBackJob run() MSG_OP_START change to MEDIA_PLAYBACK(%d,from %d)\n", MEDIA_PLAYBACK, gMediaStatus_backup);
                notifyDishStop();
                ret = playFile(mPlayFileName);
                if(ret >= 0) {
                    gpImgProcJob->videoDisplayOnOff(true);
                    //gMediaStatus_backup = gMediaStatus;
                    //gMediaStatus = MEDIA_PLAYBACK;
                    //log_print(_INFO,"PlayBackJob run() MSG_OP_START changet to MEDIA_PLAYBACK(%d,from %d)\n",MEDIA_PLAYBACK,gMediaStatus_backup);
                    //notifyDishStop();
                } else {
                    mPlaying = false;
                    gMediaStatus = gMediaStatus_backup;
                    log_print(_ERROR, "PlayBackJob run() MSG_OP_START playFile fail! MediaStatus(%d)\n", gMediaStatus);
                    notifyItself(MSG_OP_STOP);
                    sleep(1);
                    sendEndFileEvent(PLAYBACK_NORMAN_END);
                }
                break;
            case MSG_OP_PAUSE:
                printf("PlayBackJob::run() MSG_OP_PAUSE\n");
                mPause = true;
                break;
            case MSG_OP_STOP:
                printf("PlayBackJob::run() MSG_OP_STOP\n");
                if(mpReadPacketJob != NULL) {
                    mpReadPacketJob->exit();
                    delete mpReadPacketJob;
                    mpReadPacketJob = NULL;
                }
                printf("--endFile()--\n");
                endFile();
                if(gMediaStatus != MEDIA_IDLE)
                    gMediaStatus = MEDIA_IDLE;
                mPlaying = false;
                mPause = false;
                notifyDishStop();
                log_print(_INFO, "PlayBackJob run() MSG_OP_STOP MediaStatus change to %d\n", gMediaStatus);
                
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                break;
            case MSG_OP_SEEK: 
                //printf("PlayBackJob::run() MSG_OP_SEEK %d\n", gmsec);
                seekNextPacket(gmsec, mpFileDemux);
                fetchPktRingQueue->clear();
                //if(mpReadPacketJob)
                    //mpReadPacketJob->clearRingQueue_net();
                clearAVSync();
                usleep(40000);
                break;
                //
            case MSG_OP_START_NET:
                printf("PlayBackJob run() MSG_OP_START_NET\n");
                ret = playFile_net(mPlayFileName);
                if(ret < 0) {
                    notifyItself(MSG_OP_STOP_NET);
                }
                break;
            case MSG_OP_STOP_NET:
                printf("PlayBackJob run() MSG_OP_STOP_NET\n");
                endFile_net();
                break;
            case MSG_OP_SEEK_NET:
                printf("PlayBackJob run() MSG_OP_SEEK_NET\n");
                seekNextPacket(gmsec, mpFileDemux_net);
                mpReadPacketJob_net->clearRingQueue_net();
                usleep(40000);
                break;
                
            default:
                break;
        }       
    }
    log_print(_ERROR,"PlayBackJob run() exit\n");
    
}

unsigned int PlayBackJob::getFileDuration(bool isNet)
{
    Demuxer * fileDemux = isNet ? mpFileDemux_net : mpFileDemux;
    if(fileDemux == NULL)
        return 0;
    if(isNet)
        return fileDemux->getFileDuration();
    else
        return fileDemux->getFileDuration();
}

int PlayBackJob::seekNextPacket(unsigned int msec, Demuxer *fileDemux)
{    
    //if(fileDemux==NULL || mpPkgBuf==NULL) {
    if(fileDemux==NULL) {
        log_print(_WARN,"PlayBackJob playFilePacket  mpFileDemux == %x || mpPkgBuf == %x\n",(unsigned int)fileDemux,(unsigned int)mpPkgBuf);  
        return -1;
    }
    printf("-------seekNextPacket(%d)-----------------\n", msec);
    int seektime = msec/1000;
    int duration = fileDemux->getFileDuration();
    if(duration< seektime) {
        log_print(_ERROR, "PlayBackJob::seekNextPacket failed, Duration%d seektime%d\n", duration, seektime);
        return -1;
    }
    /*
    if(mPlaying == false) {
        log_print(_ERROR,"PlayBackJob is not playing but get seek command.\n");
        return -1;
    }*/
    int res = fileDemux->seek(msec);

    if(res < 0) return -1;
    
    if(mPause && !mNet) { //if pause,display new image after demuxer->seek()
        int ret= -1;
        AVPacket pkt = {0};
        av_init_packet(&pkt);
        pkt.data = (uint8_t *)mpPkgBuf;
        pkt.maxsize = PB_PKG_BUF_LEN;
        while(true) {
            if(false == fileDemux->isVideoStream(pkt.stream_id))continue;
            ret = fileDemux->read(&pkt);
            if(ret == 1) {
                log_print(_DEBUG, "PlayBackJob playFilePacket  read reach end %d\n", ret);
                return ret;
            }
            if(ret < 0) {
                log_print(_ERROR, "PlayBackJob seek():playFilePacket read error %d\n", ret);    
                return -1;
            }
            VencBlk vBlk = {0};
            ImgBlk vdecImg;
            log_print(_DEBUG, "PlayBackJob seekNextPacket video stream_id %d, size %d \n", pkt.stream_id, pkt.size);
            vBlk.buf.vStartaddr = (void*)pkt.data;
            vBlk.buf.bufSize = pkt.maxsize;
            vBlk.buf.isRingBuf = 0;
            vBlk.offset = 0;
            vBlk.codec = fileDemux->getVideoCodedId();
            vBlk.frameSize = pkt.size;
            vBlk.frameType = (pkt.flags == PKT_FLAG_KEY) ? VIDEO_I_FRAME : VIDEO_P_FRAME;
            vBlk.height = mMuxParm.video.height;
            vBlk.width = mMuxParm.video.width;
            vBlk.pts = pkt.pts;
            if(mpVidDec) {
                ret = mpVidDec->decode(&vBlk, &vdecImg);
                //notity other job to display
                log_print(_DEBUG, "PlayBackJob playFilePacket video dec size %d \n", vdecImg.size);
                #if DUMP_IMG_FILE
                saveImg(&vdecImg);
                #endif
                //u64 duration = mpFileDemux->getFileDuration();
                //u32 duration = mpFileDemux->getFileDuration();
                //sendCurrentTimeEvent((int)(pkt.pts/1000000),(int)(duration/1000000));
                //sendCurrentTimeEvent((int)(pkt.pts/1000),(int)(duration/1000));
                sendCurrentTimeEvent((int)(pkt.pts/1000), mDuration);
                if(ret > 0) {
                    mpRingQueue->push(vdecImg);
                    notify();
                }
            }//if
            break;
        }//while(true)
    }
    return 0;

}

int PlayBackJob::playFile(char *filename)
{
    printf("--------%s--------\n", filename);

    int ret = -1;
    log_print(_DEBUG,"PlayBackJob playFile %s \n", filename);
    int fileType = findFileType(filename);
    unsigned int videoCodecId = 0;
    unsigned int audioCodecId = 0;
    mStartTime = 0;
    //acc_AudioTime = 0;
    mpRingQueue->clear();
    clearAVSync();
    mAudioPacketDur = 0;
    mHasDecIFrame = false;
    switch(fileType) {
        case FILE_AVI:
            //printf("------------AVI---------\n");
            mpFileDemux = new AviDemuxer(filename);
            mpFileDemux->getFileMuxParms(&mMuxParm);
            mDuration= mpFileDemux->getFileDuration();
            videoCodecId = mpFileDemux->getVideoCodedId();
            audioCodecId = mpFileDemux->getAudioCodedId();
            if(videoCodecId == (unsigned int)CODEC_ID_H264) {

                if(mpPkgBuf == NULL) //used in seekNextPacket()
                    mpPkgBuf = (char *)malloc(PB_PKG_BUF_LEN);
                
                mMuxParm.video.codec = (unsigned int)CODEC_ID_H264;
                
                #ifdef IMX6_PLATFORM
                mpVidDec = new VpuDec(&mMuxParm.video);  
                #endif
                
                if(mpVidDec)
                    ret = 0;
            }else {
                log_print(_ERROR,"PlayBackJob playFile %s video Codec %x, not support yet\n",filename,videoCodecId);
            }

            if(audioCodecId == (unsigned int)CODEC_ID_ADPCM_MS 
                || audioCodecId == (unsigned int)CODEC_ID_ADPCM_IMA_WAV) {
                mpAudBuf = (char *)malloc(AUDIO_BUF_LEN);
                #ifdef IMX6_PLATFORM
                mpAudDec = new AdpcmDec(&mMuxParm.audio);
                #endif
            } else if(audioCodecId == (unsigned int)CODEC_ID_NONE) {
                log_print(_WARN, "PlayBackJob playFile %s is no audio stream\n", filename);
            } else {
                log_print(_ERROR, "PlayBackJob playFile %s audio Codec %x, not support yet\n", filename, audioCodecId);
            }
            
            mpReadPacketJob = new ReadPacketJob(mpFileDemux, mNet);
            if(mpReadPacketJob != NULL)
                mpReadPacketJob->start();

            if(ret >= 0 && !mNet)
                notifyItself(MSG_OP_NORMAL);
            break;
        case FILE_MOV:
            log_print(_ERROR, "PlayBackJob playFile %s FILE_MOV, not support yet\n", filename);
            break;
        case FILE_MP4:
            log_print(_ERROR, "PlayBackJob playFile %s FILE_MP4, not support yet\n", filename);
            break;
        default:
            log_print(_ERROR, "PlayBackJob playFile %s , not support yet\n", filename);
            sendEndFileEvent(PLAYBACK_ERROR_FILE_TYPE);
            break;
    }
    return ret;
}

int PlayBackJob::playFile_net(char *filename)
{
    log_print(_INFO, "PlayBackJob playFile_net %s \n", filename);
    endFile_net();
    mpFileDemux_net = new AviDemuxer(filename);
    mpReadPacketJob_net = new ReadPacketJob(mpFileDemux_net, mNet);
    if(mpReadPacketJob_net != NULL)
        mpReadPacketJob_net->start();
}
void PlayBackJob::endFile_net()
{
    if(mpReadPacketJob_net != NULL) {
        mpReadPacketJob_net->exit();
        delete mpReadPacketJob_net;
        mpReadPacketJob_net = NULL;
    }
    if(mpFileDemux_net != NULL) {
        delete mpFileDemux_net;
        mpFileDemux_net = NULL;
    }
}

int PlayBackJob::playFilePacket()
{
    //static long int audioPacketTime = 0;
    //static long int sum_AudioTime = 0;
    //static long int acc_AudioTime = 0;
    //static long int delay_time = 0;
    //static struct timeval audioPlayTime;
    //static u64 current_time_timer=9999;
    //struct timeval timeVal;
    
    int ret= -1;
    if(mpFileDemux == NULL || mpPkgBuf == NULL) {
        log_print(_ERROR, "PlayBackJob playFilePacket  mpFileDemux == %x || mpPkgBuf == %x\n", (unsigned int)mpFileDemux, (unsigned int)mpPkgBuf);      
        return -1;
    }
    
    AVPacket pkt = {0};
    //u64 tmp_duration = (u64)(mpFileDemux->getFileDuration()*1000+1);
    //int duration = tmp_duration/1000;
    int curtime = 0;

    //AVPacket pkt = {0};
    //av_init_packet(&pkt);
    //pkt.data = (uint8_t *)mpPkgBuf;
    //pkt.maxsize = PB_PKG_BUF_LEN;
    //u64 tmp_duration = (u64)(mpFileDemux->getFileDuration()*1000000+1);
    //u64 tmp_duration = (u64)(mpFileDemux->getFileDuration()*1000+1);
    
    if(mpAudioPlay == NULL) {
        if(mMuxParm.audio.sampleSize!=0 && mMuxParm.audio.channels!=0) {
            int frams = (AUDIO_BUF_LEN / mMuxParm.audio.sampleSize ) / mMuxParm.audio.channels;
            log_print(_INFO, "PlayBackJob playFilePacket AudioPlay frames %d sampleRate %d, sampleSize %d\n",frams,mMuxParm.audio.sampleRate,mMuxParm.audio.sampleSize);
             
            #ifdef IMX6_PLATFORM
            mpAudioPlay = new AudioPlay(mMuxParm.audio.sampleRate, mMuxParm.audio.sampleSize, frams);
            #endif   
        }
    }
    do {
        #if 0
        ret = mpFileDemux->read(&pkt);      
        if(ret == 1){
            //sendCurrentTimeEvent((int)(tmp_duration/1000000),(int)(tmp_duration/1000000));
            sendCurrentTimeEvent((int)(tmp_duration/1000),(int)(tmp_duration/1000));
            log_print(_DEBUG,"PlayBackJob playFilePacket  read reach end %d\n",ret);
            return ret;
        }
        if(ret < 0) {
            log_print(_INFO,"PlayBackJob playFilePacket  read error %d\n",ret);
            return ret;
        }
        #endif
        while(fetchPktRingQueue->empty()) {
            log_print(_ERROR, "PlayBackJob playFilePacket no AVPacket input\n");
            if(mpReadPacketJob == NULL || mpReadPacketJob->isTheEnd()) {
                if(mDuration != 0)
                    sendCurrentTimeEvent(mDuration, mDuration);
                usleep(250000);
                break;
            }
            usleep(40000);
        }   
        ret = fetchPktRingQueue->pop();
        if(ret < 0) {
            log_print(_ERROR, "PlaybackJob playFilePacket() fetch AVPacket fail, size:%d\n", fetchPktRingQueue->size());
                break;
        }
        pkt = fetchPktRingQueue->front();
        curtime = pkt.pts / 1000;
        if(mpFileDemux->isVideoStream(pkt.stream_id)) {
            //static struct timeval videoStartTime = {0};
            
            VencBlk vBlk = {0};
            ImgBlk vdecImg;
            if(mHasDecIFrame == false) {
                if(pkt.flags != PKT_FLAG_KEY) {
                    log_print(_DEBUG, "PlayBackJob playFilePacket video stream_id %d, size %d ,keyframe %d, pts %llu Ignore\n", pkt.stream_id, pkt.size, (pkt.flags == PKT_FLAG_KEY), pkt.pts);
                    avSyncSleep(true, pkt.pts);
                    return 0;
                } else {
                    mHasDecIFrame = true;
                }
            } 
            log_print(_DEBUG, "PlayBackJob playFilePacket video stream_id %d, size %d ,keyframe %d, pts %llu\n", pkt.stream_id, pkt.size, (pkt.flags == PKT_FLAG_KEY), pkt.pts);
            vBlk.buf.vStartaddr = (void*)pkt.data;
            vBlk.buf.bufSize = pkt.maxsize;
            vBlk.buf.isRingBuf = 0;
            vBlk.offset = 0;
            vBlk.codec = mpFileDemux->getVideoCodedId();
            vBlk.frameSize = pkt.size;
            vBlk.frameType = (pkt.flags == PKT_FLAG_KEY) ? VIDEO_I_FRAME : VIDEO_P_FRAME;
            vBlk.height = mMuxParm.video.height;
            vBlk.width = mMuxParm.video.width;
            vBlk.pts = pkt.pts;
            //printf("======vBlk.pts:%d\n",(long int)vBlk.pts);
/*
            printf("pkt.size; %d-----------------\n",pkt.size);
            for(int i=0;i<40;i++)
            {
                printf("%02x ",((char*)pkt.data)[i]);
            }
            printf("\n");
*/      
            if(mpVidDec) {
                ret = mpVidDec->decode(&vBlk, &vdecImg);
                //notity other job to display
                //log_print(_DEBUG,"PlayBackJob playFilePacket video dec size %d \n",vdecImg.size);
                #if DUMP_IMG_FILE
                saveImg(&vdecImg);
                #endif
                
                //if(pkt.pts < tmp_duration)
                //{
                    //if(abs(pkt.pts - current_time_timer)>=5000)
                    //if(abs(pkt.pts - current_time_timer)>=5)
                    //{
                    //  current_time_timer = pkt.pts;
                        if(curtime != mCurtime) {
                            mCurtime = curtime;
                            sendCurrentTimeEvent((int)curtime, mDuration);
                        }
                        //sendCurrentTimeEvent((int)(pkt.pts/1000000),(int)(tmp_duration/1000000));
                        //sendCurrentTimeEvent((int)(pkt.pts/1000),(int)(tmp_duration/1000));
                        //printf("### curTime:%d, duration:%d ###\n", pkt.pts/1000, tmp_duration/1000);
                //  }
                //}

#if 0
                //avPlaySync(pkt.pts);
                //static long int st_time = 0;
                //static long int en_time = 0;
                if (acc_AudioTime>0)
                {
                    st_time = sum_AudioTime;
                    en_time = sum_AudioTime+acc_AudioTime;
                    acc_AudioTime=0;
                }
                printf("st_time %ld en_time %ld\n",st_time,en_time);
                if(en_time <= st_time){
                    avPlaySync(pkt.pts);
                }else{
                    gettimeofday(&videoStartTime,NULL);
                    long int curTime;
                    curTime = ((videoStartTime.tv_sec-audioPlayTime.tv_sec)*1000000+videoStartTime.tv_usec-audioPlayTime.tv_usec);
                    printf("curTime---:%ld:%ld:%ld------\n",(long int)vBlk.pts,st_time,curTime);
                    long int video_run_delay= (long int)vBlk.pts - st_time - curTime;
                    printf("video_run_delay---:%d-------\n",video_run_delay);
                    long int audiodelay_in_pervideoframes = delay_time*(((1000000.00/mMuxParm.video.fps))/(en_time-st_time));
                    printf("audiodelay_in_pervideoframes---:%d------\n",audiodelay_in_pervideoframes);
                    long int sleep_time;
                    sleep_time = video_run_delay + audiodelay_in_pervideoframes;
                    printf("sleep_time---:%d-------\n",sleep_time);
                    
                    if (sleep_time>0)
                        usleep(sleep_time);
                }
#endif
                avSyncSleep(true, pkt.pts);
                //printf("display:%lld\n", pkt.pts);

                if (ret >= 0) {
                    mpRingQueue->push(vdecImg);
                    notify();
                }
            }
            ret = 0;
            break;
        } else if(mpFileDemux->isAudioStream(pkt.stream_id)) {       //
                        
            AencBlk aBlk = {0};
            AudBlk adecPcmBlk = {0};
            log_print(_DEBUG, "PlayBackJob playFilePacket audio stream_id %d, size %d, pts %llu\n", pkt.stream_id, pkt.size, pkt.pts);
            aBlk.buf.vStartaddr = (void*)pkt.data;
            aBlk.buf.bufSize = pkt.maxsize;
            aBlk.buf.isRingBuf = 0;
            aBlk.offset = 0;
            aBlk.frameSize = pkt.size;
            aBlk.pts = pkt.pts;
            //int aBlk_dts = pkt.dts;
            memcpy(&aBlk.audioParm, &mMuxParm.audio, sizeof(audioParms));
            //printf("=========aBlk.pts:%d\n",(long int)aBlk.pts);
            adecPcmBlk.vaddr = mpAudBuf;
            adecPcmBlk.size = AUDIO_BUF_LEN;
            memcpy(&adecPcmBlk.audParm, &mMuxParm.audio, sizeof(audioParms));

            
            if(mpAudDec) {
                //printf("------------mpAudDec----------\n");
                ret = mpAudDec->decode(&aBlk, &adecPcmBlk);

                #if 0               
                audioPacketTime = 1000000*((adecPcmBlk.size/mMuxParm.audio.sampleSize)/mMuxParm.audio.channels)/mMuxParm.audio.sampleRate;
                if (acc_AudioTime == 0)
                    sum_AudioTime = audioPacketTime*aBlk_dts;
                acc_AudioTime += audioPacketTime;
                printf("acc_AudioTime %ld, sum_AudioTime %ld\n",acc_AudioTime,sum_AudioTime);
                #endif              
                //write adecPcmBlk to alsa
/*
                if(mpAudioPlay == NULL) {
                    int frams = (adecPcmBlk.size/mMuxParm.audio.sampleSize )/mMuxParm.audio.channels;
                    log_print(_INFO,"PlayBackJob playFilePacket AudioPlay frames %d sampleRate %d, sampleSize %d\n",frams,mMuxParm.audio.sampleRate,mMuxParm.audio.sampleSize);
                    
                    #ifdef IMX6_PLATFORM
                    mpAudioPlay = new AudioPlay(mMuxParm.audio.sampleRate,mMuxParm.audio.sampleSize,frams);
                    #endif
                    
                }
*/
                mAudioPacketDur = 1000*((adecPcmBlk.size/mMuxParm.audio.sampleSize)/mMuxParm.audio.channels)/mMuxParm.audio.sampleRate;
                avSyncSleep(false, pkt.pts);
                //printf("display:%lld\n", pkt.pts);

                #ifdef IMX6_PLATFORM
                if(mpAudioPlay){

                    struct timeval pcmStTime, pcmEndTime;
                    gettimeofday(&pcmStTime, NULL);
                    int rc = mpAudioPlay->write_pcm((char *)adecPcmBlk.vaddr, (int)adecPcmBlk.size);
                    //add buffer to save reserve pcm data have not been play
                    gettimeofday(&pcmEndTime, NULL);
                    int diff = ((pcmEndTime.tv_sec-pcmStTime.tv_sec)*1000 + (pcmEndTime.tv_usec-pcmStTime.tv_usec)/1000);
                    if(diff > 20) log_print(_INFO,"---------------write_pcm time %d ms\n",diff) ;
                    
                    //gettimeofday(&audioPlayTime,NULL);
                    
                    //delay_time = 1000*mpAudioPlay->getDelay()/mMuxParm.audio.sampleRate +diff;    
                    //delay_time = 1000000*mpAudioPlay->getDelay()/mMuxParm.audio.sampleRate +diff; 
                    //printf("=getDelay()=======:%d\n",delay_time);
                    //delay_time = delay_time - audioPacketTime;
                }
                #endif
                log_print(_DEBUG, "PlayBackJob playFilePacket audio dec size %d \n", adecPcmBlk.size);
                
                //printf("audio ========delay_time  %d\n",delay_time);

            }
            ret = 0;
            break;
        }
        else if(mpFileDemux->isDataStream(pkt.stream_id)){
            log_print(_DEBUG, "PlayBackJob playFilePacket data stream_id %d, size %d \n", pkt.stream_id, pkt.size);
            ret =0;
        }else {
            log_print(_DEBUG, "PlayBackJob playFilePacket Error stream_id %d, size %d \n", pkt.stream_id, pkt.size);
            ret = 0;
        }
        
    }while(ret >= 0);
    if(ret >= 0)
        notifyItself(MSG_OP_PLAY);

    return 0;
}

int PlayBackJob::endFile()
{
    if(mpFileDemux) {
        delete mpFileDemux;
        mpFileDemux = NULL;
    }
    if(mpVidDec) {
        delete mpVidDec;
        mpVidDec = NULL;
    }
    #ifdef IMX6_PLATFORM
    if(mpAudDec) {
        delete mpAudDec;
        mpAudDec = NULL;
    }
    #endif
    if(mpPkgBuf) {
        free(mpPkgBuf);
        mpPkgBuf = NULL;
    }
    if(mpAudBuf) {
        free(mpAudBuf);
        mpAudBuf = NULL;
    }
    #ifdef IMX6_PLATFORM
    if(mpAudioPlay) {
        delete mpAudioPlay;
        mpAudioPlay = NULL;
    }
    #endif

    return 0;
}

int PlayBackJob::saveImg(ImgBlk* img)
{
    static int dumpImgId = 0;
    char name[MAX_NAME_LEN];
    sprintf(name, "./PlayBackJob%d.yuv", dumpImgId++);
    log_print(_INFO, "PlayBackJob saveImg IMG %s %d x %d, vaddr 0x%x, size %d", name, img->width, img->height, (unsigned int)img->vaddr, img->size);
    
    RawFile * file = new RawFile(name);
    //BmpFile* file = new BmpFile(name,img->width,img->height,24);
    if(file) {
        file->write((char *)img->vaddr, img->size);
        delete file;
    }
    return 0;
}

void PlayBackJob::avPlaySync(i64 pts)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    u64 curTime = (tv.tv_sec * 1000) + tv.tv_usec / 1000;
    //u64 curTime = (tv.tv_sec * 1000000) + tv.tv_usec;
    if(mStartTime == 0) {
        mStartTime = curTime;
        //log_print(_DEBUG, "PlayBackJob::avPlaySync mStartTime %llu us\n", mStartTime);
    } else {
        int delay = pts - (curTime - mStartTime) ;
        //log_print(_DEBUG, "PlayBackJob::avPlaySync curTime %llu, mStartTime %llu, pts %lld delay %d us\n", curTime, mStartTime, pts,delay);
        //if(delay < 5000){ //5ms
        if(delay < 5) { //5ms
            return;
        } else {
            if(delay > 0) {
                delay = delay < 200 ? delay : 200;
                //delay = delay < 200000 ? delay : 200000;
                usleep(delay);
                printf("PlayBackJob::avPlaySync dealy %d us\n", delay);
                log_print(_DEBUG, "PlayBackJob::avPlaySync dealy %d us\n", delay);
            }
        }
    }
}

void PlayBackJob::get_buffer_info(fbuffer_info* bufInfo)
{
    #ifdef IMX6_PLATFORM
    if(bufInfo)
        if(mpVidDec)
            ((VpuDec*)mpVidDec)->get_fbpool_info(bufInfo);
    #endif
}

void PlayBackJob::clearAVSync(void)
{
    mPlayFileStartTime = 0;
    mLastPacketTime = 0;
    mPlayFileStartPts = 0;
    if(mpAudioPlay) {
        mpAudioPlay->clearBuf();
    }
}

int PlayBackJob::avSyncSleep(bool isVideo, i64 pts)
{
    struct  timeval    tv;
    gettimeofday(&tv, NULL);
    u64 curTime = (tv.tv_sec * 1000) + tv.tv_usec / 1000;

    if(mPlayFileStartTime == 0) {
        if(isVideo) {
            mPlayFileStartTime = curTime;   //ms
            mLastPacketTime = curTime;
            mPlayFileStartPts = pts;
            mCompensateMs = 0;
        }
    }
    int mdelayTime = 0;
    int referencePts = curTime - mPlayFileStartTime - mCompensateMs;
    int audRemainDur = 0;
    if(isVideo) {
        int diff = (pts - mPlayFileStartPts) - referencePts  - 10; //+-10ms
        if(diff > 0) {
            mdelayTime = diff;
            if((mdelayTime > 0) && mAudioPacketDur && mpAudioPlay) {
                    /*
                    diff = curTime - mLastPacketTime;
                    if((diff+mdelayTime) >(mAudioPacketDur -10)){
                        log_print(_INFO,"PlayBackJob::avSyncSleep Audio emergency remain time %d,(Dur %d,diff %d delay %d)\n",mAudioPacketDur-(diff +mdelayTime) ,mAudioPacketDur,diff,mdelayTime);
                        mdelayTime=1;
                    }
                            */
                audRemainDur = mpAudioPlay->getDelay();//ms
                log_print(_DEBUG, "PlayBackJob::Video avSyncSleep Audio audRemainDur %d, mdelayTime %d\n\tpts %lld, referencePts %d, mCompensateMs %d\n", audRemainDur, mdelayTime, pts, referencePts, mCompensateMs);
                if(mdelayTime > (audRemainDur - 40))
                    mdelayTime = audRemainDur - 40; //ms
            }
        } 
        
    } else {
        mLastPacketTime = curTime;
        //audio play directly 
        if(mpAudioPlay) {
            audRemainDur = mpAudioPlay->getDelay();//ms
            log_print(_DEBUG, "PlayBackJob::Audio avSyncSleep Audio audRemainDur %d, mdelayTime %d, referencePts %d\n", audRemainDur, mdelayTime, referencePts);
            if(audRemainDur > 160) {//ms 
                mdelayTime = audRemainDur - 40;
                mCompensateMs += mdelayTime;
                log_print(_DEBUG, "PlayBackJob::avSyncSleep audRemainDur %d mCompensateMs %d, be added %d ms\n", audRemainDur, mCompensateMs, mdelayTime);
                usleep(mdelayTime * 1000);
                mdelayTime = 0;
            }
        }
    }

    if(mdelayTime > 0) { 
        if(mdelayTime > mpFileDemux->getFrameDuration()) {
            log_print(_DEBUG,"PlayBackJob::avSyncSleep isVideo %d, pts %lld, mdelayTime %d ms too big, change to  %d\n", isVideo, pts, mdelayTime, mpFileDemux->getFrameDuration());
            mdelayTime = mpFileDemux->getFrameDuration();
        }
        log_print(_DEBUG,"PlayBack avSync isVideo %d, pts %lld, mdelay %d ms referencePts %d, CompensateMs %d\n", isVideo, pts, mdelayTime, referencePts, mCompensateMs);
        usleep(mdelayTime * 1000);
    } 
    return 0;
}

int PlayBackJob::sendSeveralFrame()
{
    mpReadPacketJob->sendServeralPacket();
    return 0;
}

