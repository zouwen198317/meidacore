#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "debug/errlog.h"
#include "log/log.h"
#include "mediablock.h"
#include "ipc/ipcclient.h"
#include "readPacketjob.h"
#include "mediaopcode.h"
#include "ffmpeg/avformat/utils.h"

#define PB_PKG_BUF_LEN 307200
//extern MsgQueue *gpReadPacketMsgQueue;
extern IpcClient * gpMediaCoreIpc;

int  sendEndFileEvent_net(int endFlag)
{
    log_print(_INFO, "ReadPacketJob sendEndFileEvent\n");
    MsgPacket packet;
    OpcEvent_t event;
    event.value = endFlag;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_NET);
    packet.header.opcode = OPC_PLAYBACK_END_EVENT;
    packet.header.len = sizeof(OpcEvent_t);
    memcpy(packet.args, &event, sizeof(OpcEvent_t));    
    return gpMediaCoreIpc->sendPacket(&packet);
}

ReadPacketJob::ReadPacketJob(Demuxer *fileDemux, bool net)
{
    mpFileDemux = fileDemux; 
    isEOF = false;
    mNet = net;
    mPause = false;
    mReadAVPacketId = 0;
    borderNum = 2;
    gpReadPacketMsgQueue = new MsgQueue;    
    if(!mNet) {
        readPktRingQueue = new ShmRingQueue<AVPacket>((key_t)RING_BUFFER_SDCARD_TO_PLAY, RING_BUFFER_SDCARD_TO_PLAY_NUM, sizeof(AVPacket));
        readPktRingQueue->clear();
        for(int i=0; i<RING_BUFFER_SDCARD_TO_PLAY_NUM; i++)
            mPkgBuf[i] = (char *)malloc(PB_PKG_BUF_LEN);// 300K. if  there have one image frame  larger than 300k in the video, it will be cause segment fault.

        bufferSize = RING_BUFFER_SDCARD_TO_PLAY_NUM;
    } else {
        mpRTSPRingQueue = new ShmRingQueue<RTSPBlk>((key_t)RING_BUFFER_AVENC_TO_RTSP_BUF,RING_BUFFER_AVENC_TO_RTSP_NUM,sizeof(RTSPBlk));
        printf("##### ReadPacketJob RingBuffer num: %d #####\n", RING_BUFFER_AVENC_TO_RTSP_NUM);
        mpRTSPRingQueue->clear();
        bufferSize = RING_BUFFER_AVENC_TO_RTSP_NUM;

        mpRTSPAudRingQueue = new ShmRingQueue<RTSPBlk>((key_t)RING_BUFFER_AUDENC_TO_RTSP_BUF,RING_BUFFER_AUDENC_TO_RTSP_NUM,sizeof(RTSPBlk));
        mpRTSPAudRingQueue->clear();
        //bufferSize = RING_BUFFER_AUDENC_TO_RTSP_NUM;
    }
    notifyItself(MSG_OP_PLAY);
}

ReadPacketJob::~ReadPacketJob()
{
    if(gpReadPacketMsgQueue) {
        delete gpReadPacketMsgQueue;
        gpReadPacketMsgQueue = NULL;
    }    
    if(!mNet) {
        for(int i=0; i<RING_BUFFER_SDCARD_TO_PLAY_NUM; i++)
            free(mPkgBuf[i]);
        if(readPktRingQueue) {
            delete readPktRingQueue;
            readPktRingQueue = NULL;
        }
    } else {
        if(mpRTSPRingQueue) {
            delete mpRTSPRingQueue;
            mpRTSPRingQueue = NULL;
        }
        if(mpRTSPAudRingQueue) {
            delete mpRTSPAudRingQueue;
            mpRTSPAudRingQueue = NULL;
        }    
    }
}

int ReadPacketJob::notifyItself(int msgopc)
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        //log_print(_DEBUG, "ReadPacketJob notifyItself \n");
        gpReadPacketMsgQueue->send(&msg);
    }
    return 0;
}

int ReadPacketJob::pause()
{
    printf("ReadPacketJob::pause()\n");
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_PAUSE;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        log_print(_DEBUG, "ReadPacketJob notify to pause\n");
        gpReadPacketMsgQueue->send(&msg);
    }
    return 0;
}

int ReadPacketJob::resume()
{
    printf("ReadPacketJob::resume()\n");
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_RESUME;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        log_print(_DEBUG, "ReadPacketJob notify to resume\n");
        gpReadPacketMsgQueue->send(&msg);
    }
    return 0;
}

int ReadPacketJob::exit()
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_EXIT;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        log_print(_DEBUG, "ReadPacketJob notify to exit\n");
        gpReadPacketMsgQueue->send(&msg);
    }
    join();
    return 1;
}

int ReadPacketJob::sendServeralPacket()
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_NORMAL;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        gpReadPacketMsgQueue->send(&msg);
    }
    return 0;
}

void ReadPacketJob::run()
{
    mRunning = true;
    msgbuf_t msg;
    int ret;
    
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    
    log_print(_DEBUG,"ReadPacketJob run()\n");
    if(mNet) {
        mpRTSPRingQueue->clear();
        mpRTSPAudRingQueue->clear();
    }
    while( mRunning ) {
        gpReadPacketMsgQueue->recv(&msg);
        switch(msg.opcode) {
            case MSG_OP_PLAY:
            case MSG_OP_NORMAL:
                if(!mPause) {
                    ret = sendPacket();
                    if (ret < 0 || ret == 1) {
                        log_print(_ERROR, "readPacketJob read reach end %d\n", ret);
                        sendEndFileEvent_net(ret);
                        isEOF = true;
                    } else {
                        notifyItself(MSG_OP_NORMAL);
                    }
                }
                break;
            case MSG_OP_PAUSE:
                if(!mPause) {
                    log_print(_INFO, "ReadPacketJob::run() MSG_OP_PAUSE\n");
                    mPause = true;
                }
                break;
            case MSG_OP_RESUME:
                if(mPause) {
                    log_print(_INFO, "ReadPacketJob::run() MSG_OP_RESUME\n");
                    mPause = false;
                    notifyItself(MSG_OP_PLAY);
                }
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                break;
            default:
                break;
        }//switch()
    }//while()
    log_print(_INFO,"ReadPacketJob run() exit\n");
}

int ReadPacketJob::sendPacket()
{
    int ret = 0, queueNum;
    if(!mNet) {
        queueNum = readPktRingQueue->size();
     } else {
        queueNum = mpRTSPRingQueue->size();

        if(queueNum >= RING_BUFFER_AVENC_TO_RTSP_NUM) {
            usleep(40000);
            return ret;
        }
     }
    if(queueNum < bufferSize-borderNum && !isEOF) {
        if(queueNum < 3)
            log_print(_ERROR, "ReadPacketJob run() readPktRingQueue size %d\n", queueNum);
        ret = readPacket();
    } else {
        usleep(50000);
    }
    return ret;
}

int ReadPacketJob::readPacket()
{
    int ret;
    AVPacket pkt = {0};
    RTSPBlk rtspblock;
    av_init_packet(&pkt);
    if(!mNet) {
        pkt.data = (uint8_t *)mPkgBuf[mReadAVPacketId++];
        pkt.maxsize = PB_PKG_BUF_LEN;
        if(mReadAVPacketId >= RING_BUFFER_SDCARD_TO_PLAY_NUM) {
            mReadAVPacketId = 0;
        }
    } else {
        memset(rtspblock.mediaData, 0, MAX_FRAME_SIZE);
        pkt.data = (uint8_t*)rtspblock.mediaData;
        pkt.maxsize = MAX_FRAME_SIZE;
    }
    
    ret = mpFileDemux->read(&pkt);
    
    if(ret == 1 || ret < 0)
        return ret;

    if(!mNet) {
        readPktRingQueue->push(pkt);
    } else {
        if(pkt.stream_id == 0) {
            rtspblock.mediaSize = pkt.size;
            rtspblock.frameType = VIDEO_I_FRAME;
            
            if(rtspblock.mediaSize < MAX_FRAME_SIZE) {
                if(mpRTSPRingQueue != NULL) {
                    mpRTSPRingQueue->push(rtspblock);
                    //printf("### mpRTSPRingQueue size %d\n", mpRTSPRingQueue->size());
                }
            } else {
                log_print(_ERROR, "ReadPacketJob::readPacket() packet size %u, ignore!\n", rtspblock.mediaSize);
            }
        } else if(pkt.stream_id == 1) {
            rtspblock.mediaSize = pkt.size;
            //printf("ReadPacketJob::readPacket() audio packet size %d\n", pkt.size);
            if(mpRTSPAudRingQueue != NULL) {
                mpRTSPAudRingQueue->push(rtspblock);
            }
        }
    }
    return ret;
}
void ReadPacketJob::clearRingQueue_net()
{
    if(mpRTSPRingQueue != NULL) {
        mpRTSPRingQueue->clear();
    }
    if(mpRTSPAudRingQueue != NULL) {
        mpRTSPAudRingQueue->clear();
    }
}




