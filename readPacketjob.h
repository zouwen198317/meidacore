#ifndef _READ_PACKET_JOB_H_
#define _READ_PACKET_JOB_H_

#include "libthread/threadpp.h"
#include "mediacore.h"
#include "mediablock.h"
#include "shmringqueue.h"
#include "demuxer.h"

class ReadPacketJob:public Thread
{
public:
    ReadPacketJob(Demuxer *fileDemux, bool net = false);
    ~ReadPacketJob();
    bool isTheEnd() const {return isEOF;}
    int notifyItself(int msgopc);
    int pause();
    int resume();
    int exit();
    int sendServeralPacket();
    void clearRingQueue_net();
private:
    void run();
    int sendPacket();
    int readPacket();

    Demuxer *mpFileDemux;
    MsgQueue *gpReadPacketMsgQueue;
    bool isEOF;
    bool mNet;
    bool mRunning;
    bool mPause;
    int bufferSize;
    int mReadAVPacketId;
    int borderNum;
    char *mPkgBuf[RING_BUFFER_SDCARD_TO_PLAY_NUM];
    ShmRingQueue<AVPacket> *readPktRingQueue;
    ShmRingQueue<RTSPBlk> *mpRTSPRingQueue;
    ShmRingQueue<RTSPBlk> *mpRTSPAudRingQueue;
};

#endif

