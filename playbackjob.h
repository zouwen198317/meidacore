#ifndef _PLAY_BACK_JOB_H_
#define _PLAY_BACK_JOB_H_

#include "libthread/threadpp.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "readPacketjob.h"

#include "shmringqueue.h"
#include "demuxer.h"
#include "platform/HAL/videodecoder.h"
#include "platform/HAL/audiodecoder.h"
#include "mediaopcode.h"

#include "platform/audioplay.h"

class PlayBackJob:public Thread
{
public:
    PlayBackJob();
    ~PlayBackJob();
    bool isRunning() const {return mRunning;}
    bool isPlaying() const {return mPlaying;}
    bool isPause() const {return mPause;}
    int startPlay(char *fileName, bool net = false);
    int stopPlay(bool isNet);
    int pause(bool isNet);
    int resume(bool isNet);
    void exit();
    unsigned int getFileDuration(bool isNet);
    int seek(unsigned int msec, bool isNet);
    void get_buffer_info(fbuffer_info*);
    int sendSeveralFrame();
    
private:
    int notify();
    int notifyItself(int msgopc);
    int notifyDishStop();
    void run();
    int playFile(char *filename);
    int playFile_net(char *filename);
    int playFilePacket();
    int endFile();
    void endFile_net();
    void avPlaySync(i64 pts);
    int seekNextPacket(unsigned int msec, Demuxer *fileDemux);
    u64 mStartTime;//play time start time from gettimeofday
    bool mRunning;
    bool mPlaying;
    bool mPause;
    //productor
    ShmRingQueue<ImgBlk> *mpRingQueue;
    ShmRingQueue<AVPacket> *fetchPktRingQueue;

    ReadPacketJob *mpReadPacketJob;
    ReadPacketJob *mpReadPacketJob_net;

    Demuxer *mpFileDemux;
    Demuxer *mpFileDemux_net;
    CAudioDec *mpAudDec;
    CVideoDec *mpVidDec;

    AudioPlay *mpAudioPlay;

    char mPlayFileName[MAX_FILE_NAME_LEN];
    char *mpPkgBuf;
    char *mpAudBuf;
    muxParms mMuxParm;

    int saveImg(ImgBlk* img);//debug
    u32 mFileDuration;

    int mCurtime; //display curtime in GUI
    int mDuration;//display duration in GUI
      
    //long int st_time;
    //long int en_time;
    //long int acc_AudioTime;


    //av sync function
    int avSyncSleep(bool isVideo, i64 pts); //ms
    void clearAVSync(void);
    int  mAudioPacketDur;//ms
    u64 mPlayFileStartTime; //ms
    u64 mLastPacketTime;    //ms
    i64 mPlayFileStartPts;//ms
    int mCompensateMs; // ms audio may play slower than normal

    //H264 need to decode I frame first
    bool mHasDecIFrame;
    bool mNet;
};

#endif

