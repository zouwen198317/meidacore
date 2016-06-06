#ifndef _REC_FILE_JOB_H_
#define _REC_FILE_JOB_H_
#include "libthread/threadpp.h"
#include "platform/camcapture.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "mediablock.h"
#include "shmringqueue.h"
#include "muxer.h"
#include "jpgfile.h"

class RecFileJob: public Thread
{
public:
    RecFileJob(muxParms *pParm);
    ~RecFileJob();
    bool isRunning() const {return mRunning;}
    bool isSkipping() const{return mSkipping;}
    //bool isRecing() const {return (!mPause)&&mRunning&&(gRecIniParm.RecFile.bootRecord);}//!!
    //bool isRecing();
    int getRecType() const {return mRecType;}
    void needRecAhead();
    int recEmcEvent();
    int recAlgEvent(unsigned char opcode);
    int recPowerOutages();
    void stop();
    void pause();
    void pauseImmediately();
    void resume();
    void exit();
    //!!
    void setMaxFileDuration(u32 value);
    int updateConfig(muxParms *pParms);
    int captureJPEG( VencBlk *pVencBlk);

private:
    //int notify();
    int notifyItself(int msgopc); 
    void run();
    //int status();
    int openRecFile(time_t captureTime);
    int closeRecFile(bool writable=true);
    int closeFilePowerOutages();

    
    int writeAencBlk(MediaBlk *pMediaBlk);
    int writeVencBlk(MediaBlk *pMediaBlk);
    int writeSensorBlk(MediaBlk *pMediaBlk);
    int writeAlgData(enum AlgChkType type);

    bool mRunning;
    bool mPause;
    bool mIgnore;
    bool mSkipping;
    bool mMotionDetection;
    bool mTailProcess;
    bool mFlitered;
    //consumer
    ShmRingQueue<MediaBlk> *mpMediaRingQueue;
    ShmRingQueue<VencBlk> *mpCaptureQueue;
    ShmRingQueue<MediaBlk> *mpRTSPRingQueue;

    int processMedia(MediaBlk *pMediaBlk);
    int recEmergency();
    int recAlgorithm(enum AlgChkType chkType, recType recType);
    int recMotionDection(MediaBlk *pMediaBlk);

    Muxer *mpRecMuxer;
    JpgFile *mpJpgFile;

    muxParms mParm;
    
    u8 *mpVideoBuf;
    int mVideoBufLen;
    bool mRecAudio;
    bool mRecSensor;
    //bool mEndTypeFlag;
    //bool mPreEmergencyRecing;

    u32 mMaxFileDuration;//configure by init
    u32 mMaxFileDuration_backup;
    u32 mRecFileDuration;//increase to max time to end file ; ms
    u32 mRecFrameCount;
    u64 mRecLastPts;//last video packet pts; us

    recType mRecType;
    //!!
    FILE *mDataFp; //   #ifdef DUMP_ALG_FILE
    //time_t mCaptureTime;  //sec
    char mFileName[MAX_PATH_NAME_LEN];
    char mFileName_previous[MAX_PATH_NAME_LEN];
    char mRecPath[MAX_PATH_NAME_LEN];
    char mPicPath[MAX_PATH_NAME_LEN];
    u32 mNextMaxFileDuration;

    int updateRecFileConfig();
    muxParms mNewMuxParm;

    int audioFrameCount;
    int filterCount;
    u64 pts_temp;
    u64 mEmcPts;
    bool pass;

};

#endif

