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

#include "readPacketjob_net.h"

#include "ffmpeg/avformat/utils.h"


extern MsgQueue *gpReadPacketMsgQueue;

ReadPacketJob_net::ReadPacketJob_net(Demuxer *fileDemux)
{
    mpFileDemux = fileDemux; 
    isEOF = false;
        
    mpRTSPRingQueue = new ShmRingQueue<RTSPBlk>((key_t)RING_BUFFER_AVENC_TO_RTSP_BUF,RING_BUFFER_AVENC_TO_RTSP_NUM,sizeof(RTSPBlk));

    mpRTSPRingQueue->clear();
}

ReadPacketJob_net::~ReadPacketJob_net()
{
    if(mpRTSPRingQueue) {
        delete mpRTSPRingQueue;
        mpRTSPRingQueue = NULL;
    }
}

int ReadPacketJob_net::exit()
{
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_EXIT;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpReadPacketMsgQueue) {
        log_print(_DEBUG, "ReadPacketJob notify to exit \n");
        gpReadPacketMsgQueue->send(&msg);
    }
    join();
    return 1;
}

void ReadPacketJob_net::run()
{
    mRunning = true;
    msgbuf_t msg;
    int ret = 0, queueNum;

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG,"ReadPacketJob run()\n");
    
    mpRTSPRingQueue->clear();
    while( mRunning ) {
        if(gpReadPacketMsgQueue->msgNum() > 0) {
            gpReadPacketMsgQueue->recv(&msg);
            switch(msg.opcode) {
                case MSG_OP_EXIT:
                    mRunning = false;
                    break;
                case MSG_OP_NORMAL:
                    break;
                default:
                    break;
            }
        }
        queueNum = mpRTSPRingQueue->size();
        if(queueNum < RING_BUFFER_AVENC_TO_RTSP_NUM-1 && !isEOF) {
            if(queueNum < 5)
                log_print(_INFO, "ReadPacketJob run()  readPktRingQueue size %d\n", queueNum);          
            ret = readPacket();
            if (ret < 0) {
                log_print(_ERROR, "readPacketJob  read error %d\n", ret);
                isEOF = true;
            }
            if(ret == 1) {
                log_print(_ERROR, "readPacketJob  read reach end %d\n", ret);
                isEOF = true;
            }
        } else {
            usleep(100000);
        }
    }//while
    log_print(_INFO,"ReadPacketJob run() exit\n");
}


int ReadPacketJob_net::readPacket()
{
    int ret;
    AVPacket pkt = {0};
    RTSPBlk rtspblock;
    av_init_packet(&pkt);
    pkt.data = (uint8_t*)rtspblock.mediaData;
    pkt.maxsize = MAX_FRAME_SIZE;
    
    ret = mpFileDemux->read(&pkt);
    
    if(ret == 1 || ret < 0)
        return ret;

    rtspblock.mediaSize = pkt.size;
    rtspblock.frameType = VIDEO_I_FRAME;
    
    if(rtspblock.mediaSize < MAX_FRAME_SIZE) {
        if(mpRTSPRingQueue == NULL) {
            printf("mpRTSPRingQueue NULL\n");
            return ret;
        }
        mpRTSPRingQueue->push(rtspblock);
    } else {
        printf("Too big, frame size %u\n", rtspblock.mediaSize);
    }
    return ret;
}

