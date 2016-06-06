#ifndef ALG_CLASS_H
#define ALG_CLASS_H
#include "shmringqueue.h"
#include "mediacore.h"
#include "platform/HAL/imageprocess.h"
#include "mediaparms.h"
#include "algopcode.h"
#include "ipc/ipcclient.h"


class Alg
{
public:
    Alg(imgProcParms* parm);
    ~Alg();
    void algProcess(ImgBlk*);
    void updateSpeed(double);
    void clearRingBuffer();
    void updateParms();
    void updateLaneInfo(lane_t &left, lane_t &right, int importFrameRange,int vanishLine);
    void algImgOnOff(int onOff);

private://member
    u8 mGpsSpeed;
    u8 mLdwsStartSpeed;
    u8 mFcwsStartSpeed;
    u8 mTsrStartSpeed;
    u8 mPdsStartSpeed;
    u8 mUFcwsStartSpeed;
    int mFcwsImgBlkId;
    int mLdwsImgBlkId;
    int mTsrImgBlkId;
    int mPdsImgBlkId;
    int mUFcwsImgBlkId;
    int mFcwsFlameId;
    int mLdwsFlameId;
    int mTsrFlameId;
    int mPdsFlameId;
    int mUFcwsFlameId;
    int mFcwsFps;
    int mLdwsFps;
    int mTsrFps;
    int mPdsFps;
    int mUFcwsFps;
    int mCaptureFps;
    float mFcwsFlameRate;
    float mLdwsFlameRate;
    float mTsrFlameRate;
    float mPdsFlameRate;
    float mUFcwsFlameRate;
    int mFcwsOpCnt;
    int mLdwsOpCnt; 
    int mTsrOpCnt;  
    int mPdsOpCnt;
    int mUFcwsOpCnt;
    int mFcwsHasThrown;
    int mLdwsHasThrown;
    int mTsrHasThrown;
    int mPdsHasThrown;
    int mUFcwsHasThrown;
    int mCpuTemp;
    bool mLdwsEnable;
    bool mFcwsEnable;
    bool mTsrEnable;
    bool mPdsEnable;
    bool mUFcwsEnable;
    bool mLdwsOnOff;
    bool mFcwsOnOff;
    bool mTsrOnOff;
    bool mPdsOnOff;
    bool mUFcwsOnOff;
    algInfo_t mLdwsInfo;
    algInfo_t mFcwsInfo;
    algInfo_t mTsrInfo;
    algInfo_t mPdsInfo;
    algInfo_t mUFcwsInfo;
    imgProcParms mImgProcParms;
    CImageProcess *mImgProc;
    ShmRingQueue<algInfo_t> *mpFcwsRingQueue;
    ShmRingQueue<algInfo_t> *mpLdwsRingQueue;   
    ShmRingQueue<algInfo_t> *mpTsrRingQueue;
    ShmRingQueue<algInfo_t> *mpPdsRingQueue;    
    ShmRingQueue<algInfo_t> *mpUFcwsRingQueue;  
    ImgBlk *mpFcwsImgBlk[RING_BUFFER_VIN_TO_FCWS_NUM];
    ImgBlk *mpLdwsImgBlk[RING_BUFFER_VIN_TO_LDWS_NUM];
    ImgBlk *mpTsrImgBlk[RING_BUFFER_VIN_TO_TSR_NUM];
    ImgBlk *mpPdsImgBlk[RING_BUFFER_VIN_TO_PDS_NUM];
    ImgBlk *mpUFcwsImgBlk[RING_BUFFER_VIN_TO_UFCWS_NUM];
    int mImportFrameRange;
private://function
    int initHw();
    int unInitHw();
    ImgBlk* getFcwsImgBlk();
    ImgBlk* getLdwsImgBlk();
    ImgBlk* getTsrImgBlk();
    ImgBlk* getPdsImgBlk();
    ImgBlk* getUFcwsImgBlk();
    ImgBlk* fcwsOperation(ImgBlk*);
    ImgBlk* ldwsOperation(ImgBlk*);
    ImgBlk* tsrOperation(ImgBlk*);
    ImgBlk* pdsOperation(ImgBlk*);
    ImgBlk* uFcwsOperation(ImgBlk*);
    bool flameFilter(const bool*,int);
    void initFcwsFlameFilter();
    void initLdwsFlameFilter();
    void initTsrFlameFilter();
    void initPdsFlameFilter();
    void initUFcwsFlameFilter();
    bool FcwsflameFilter(int);
    bool LdwsflameFilter(int);
    bool TsrflameFilter(int);
    bool PdsflameFilter(int);
    bool uFcwsflameFilter(int);
    int matchFpsWithCPUTemp(int,int);
    int SendAlgStopEvent(enum DO_MSG_TYPE doType) ;
    bool mAlgImgOn;
    int mVanishLine;
};
#endif
