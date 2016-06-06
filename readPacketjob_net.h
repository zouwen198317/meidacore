#ifndef _READ_PACKET_JOB_NET_H_
#define _READ_PACKET_JOB_NET_H_

#include "libthread/threadpp.h"
#include "mediacore.h"
#include "mediablock.h"
#include "shmringqueue.h"
#include "demuxer.h"

class ReadPacketJob_net:public Thread
{
public:
    ReadPacketJob_net(Demuxer *fileDemux);
    ~ReadPacketJob_net();
    bool isTheEnd() const {return isEOF;}
    int exit();

private:
    void run();
    int readPacket();

    Demuxer *mpFileDemux;
    bool mRunning;
    bool isEOF;
    ShmRingQueue<RTSPBlk> *mpRTSPRingQueue;
};

#endif

