#include<assert.h>
#include<stdlib.h>
#include<memory.h>
#include<stdio.h>
#include"alg.h"
#include"parms_dump.h"
#include"log/log.h"
#ifdef IMX6_PLATFORM
#include "platform/IMX6/imageprocessunit.h"
#endif


extern IpcClient * gpMediaCoreIpc;


//#define SPEED_FLEXIBLE_DIFFERENCE 10
#define CPU_TEMPERATURE_FILE "/sys/class/thermal/thermal_zone0/temp"

extern LdwsIniParm gLdwsIniParm;
extern FcwsIniParm gFcwsIniParm;
extern TsrIniParm gTsrIniParm;
extern PdsIniParm gPdsIniParm;
extern UFcwsIniParm gUFcwsIniParm;
extern CaptureIniParm gCapIniParm;
/*
enum{
    SIXTY_DEGREE=0,
    SEVENTY_DEGREE,
    EIGHTY_DEGREE,
    NINETY_DEGREE,
    HUNDRED_DEGREE
};
*/

static bool notLessThan(float a,float b)
{
    float c = a-b;
    if((c > - 0.000001 && c < 0.000001) || a > b){
        //log_print(_INFO,"notLessThan: a %f,b %f true\n",a,b);
        return true;
    }
    else{
        //log_print(_INFO,"notLessThan: a %f,b %f false\n",a,b);
        return false;
    }
}
/*
const static bool flameMask_25[5][25] =
{
    {1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,0,1,1,1},//15    <60
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,1},//12    60< <70
    {1,0,1,0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,0,0,1},//9     70< <80
    {1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,1},//6     80< <90
    {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1}//3      90< <100
};

const static bool flameMask_30[5][30] = 
{
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},//15
    {1,0,0,1,0,0,1,0,1,0,1,0,0,1,0,1,0,1,0,0,0,1,0,1,0,0,1,0,0,1},//12
    {1,0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,1,0,0,0,1},//9
    {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1},//6
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1}//3
};
*/

int getCpuTemp()
{
    FILE* fp;
    fp = fopen(CPU_TEMPERATURE_FILE,"r");
    if(fp == NULL)
        return -1;
    int temp(-1);
    fscanf(fp,"%d",&temp);
    fclose(fp);
    return (temp/1000);
}

Alg::Alg(imgProcParms* parm)
{
    assert(parm);
    log_print(_DEBUG,"Alg constructor\n");
    memcpy(&mImgProcParms,parm,sizeof(imgProcParms));

    mpFcwsRingQueue = new ShmRingQueue<algInfo_t>((key_t)RING_BUFFER_VIN_TO_FCWS,RING_BUFFER_VIN_TO_FCWS_NUM,sizeof(algInfo_t));
    mpLdwsRingQueue = new ShmRingQueue<algInfo_t>((key_t)RING_BUFFER_VIN_TO_LDWS,RING_BUFFER_VIN_TO_LDWS_NUM,sizeof(algInfo_t));
    mpTsrRingQueue = new ShmRingQueue<algInfo_t>((key_t)RING_BUFFER_VIN_TO_TSR,RING_BUFFER_VIN_TO_TSR_NUM,sizeof(algInfo_t));
    mpPdsRingQueue = new ShmRingQueue<algInfo_t>((key_t)RING_BUFFER_VIN_TO_PDS,RING_BUFFER_VIN_TO_PDS_NUM,sizeof(algInfo_t));
    mpUFcwsRingQueue = new ShmRingQueue<algInfo_t>((key_t)RING_BUFFER_VIN_TO_UFCWS,RING_BUFFER_VIN_TO_UFCWS_NUM,sizeof(algInfo_t));

    clearRingBuffer();
    mFcwsImgBlkId = 0;
    mLdwsImgBlkId = 0;
    mTsrImgBlkId = 0;
    mPdsImgBlkId = 0;
    mUFcwsImgBlkId = 0;    
    mFcwsFlameId = 0;
    mLdwsFlameId = 0;
    mTsrFlameId = 0;
    mPdsFlameId = 0;
    mUFcwsFlameId = 0;
    mFcwsOpCnt = 0;
    mLdwsOpCnt = 0;     
    mTsrOpCnt = 0;    
    mPdsOpCnt = 0;     
    mUFcwsOpCnt = 0;      
    memset(&mLdwsInfo, 0, sizeof(algInfo_t));
    memset(&mFcwsInfo, 0, sizeof(algInfo_t));
    memset(&mTsrInfo, 0, sizeof(algInfo_t));
    memset(&mPdsInfo, 0, sizeof(algInfo_t));
    memset(&mUFcwsInfo, 0, sizeof(algInfo_t));
    mImgProc = NULL;    
    mLdwsEnable = gLdwsIniParm.LDWS.enable;
    mFcwsEnable = gFcwsIniParm.FCWS.enable;
    mTsrEnable = gTsrIniParm.TSR.enable;
    mPdsEnable = gPdsIniParm.PDS.enable;
    mUFcwsEnable = gUFcwsIniParm.UFCWS.enable;
    mLdwsOnOff = false;
    mFcwsOnOff = false;
    mTsrOnOff = false;
    mPdsOnOff = false;
    mUFcwsOnOff = false;
    mLdwsStartSpeed = gLdwsIniParm.LDWS.startSpeed;
    mFcwsStartSpeed = gFcwsIniParm.FCWS.startSpeed;
    mTsrStartSpeed = gTsrIniParm.TSR.startSpeed;
    mPdsStartSpeed = gPdsIniParm.PDS.startSpeed;
    mUFcwsStartSpeed = gUFcwsIniParm.UFCWS.startSpeed;
    mCaptureFps = gCapIniParm.VidCapParm.fps;
    mFcwsFps = gFcwsIniParm.FCWS.fps;
    mLdwsFps = gLdwsIniParm.LDWS.fps;
    mTsrFps = gTsrIniParm.TSR.fps;
    mPdsFps = gPdsIniParm.PDS.fps;
    mUFcwsFps = gUFcwsIniParm.UFCWS.fps;
    mGpsSpeed = 0;
    mCpuTemp = 0;
    mImportFrameRange = 0;
    lane_t lane;
    lane.state = LANE_NONE;
    lane.k = 0;
    lane.b = 0;
    mVanishLine = 0;
    updateLaneInfo(lane,lane,mImportFrameRange,mVanishLine);
    mAlgImgOn = true;
    initHw();
}


Alg::~Alg()
{
    unInitHw(); 
    if(mpFcwsRingQueue)
        delete mpFcwsRingQueue;
    if(mpLdwsRingQueue)
        delete mpLdwsRingQueue;
    if(mpTsrRingQueue)
        delete mpTsrRingQueue;
    if(mpPdsRingQueue)
        delete mpPdsRingQueue;
    if(mpUFcwsRingQueue)
        delete mpUFcwsRingQueue;
}

void Alg::updateParms()
{
    log_print(_INFO,"Alg::updateParms\n");

    mLdwsEnable = gLdwsIniParm.LDWS.enable;
    mFcwsEnable = gFcwsIniParm.FCWS.enable;
    mTsrEnable = gTsrIniParm.TSR.enable;
    mPdsEnable = gPdsIniParm.PDS.enable;
    mUFcwsEnable = gUFcwsIniParm.UFCWS.enable;
    mLdwsStartSpeed = gLdwsIniParm.LDWS.startSpeed;
    mFcwsStartSpeed = gFcwsIniParm.FCWS.startSpeed;
    mTsrStartSpeed = gTsrIniParm.TSR.startSpeed;
    mPdsStartSpeed = gPdsIniParm.PDS.startSpeed;
    mUFcwsStartSpeed = gUFcwsIniParm.UFCWS.startSpeed;
    mCaptureFps = gCapIniParm.VidCapParm.fps;
    mFcwsFps = gFcwsIniParm.FCWS.fps;
    mLdwsFps = gLdwsIniParm.LDWS.fps;
    mTsrFps = gTsrIniParm.TSR.fps;
    mPdsFps = gPdsIniParm.PDS.fps;
    mUFcwsFps = gUFcwsIniParm.UFCWS.fps;
}


void Alg::algProcess(ImgBlk* inputImgBlk)
{
    if((mAlgImgOn == false) || (mLdwsEnable==false && mFcwsEnable==false && mTsrEnable==false && mPdsEnable==false && mUFcwsEnable==false))
        return;

    ImgBlk* pOutImgBlk = inputImgBlk;
    if(mImgProc)
    {
        if(mFcwsEnable)
        {
            if(mFcwsFlameId == 0) {
                initFcwsFlameFilter();
            }
            if(FcwsflameFilter(mFcwsFlameId++)) {
                pOutImgBlk = fcwsOperation(inputImgBlk);
                //log_print(_INFO,"Alg::algProcess  fcwsOperation# done\n");
            }//else log_print(_INFO,"Alg::algProcess  ~\n");
            if(mFcwsFlameId >= mCaptureFps) {
                //log_print(_DEBUG,"Alg FCWS fps %d(ini:%d); CpuTemp %d\n",mFcwsOpCnt,mFcwsFps,mCpuTemp);
                mFcwsFlameId = 0;
                mFcwsOpCnt =0;
            }
        }
        if(mTsrEnable)
        {
            if(mTsrFlameId == 0) {
                initTsrFlameFilter();
            }
            if(TsrflameFilter(mTsrFlameId++)) {
                pOutImgBlk = tsrOperation(pOutImgBlk);
                //log_print(_INFO,"Alg::algProcess  tsrOperation# done\n");
            }//else log_print(_INFO,"Alg::algProcess  ~\n");
            if(mTsrFlameId >= mCaptureFps) {
                //log_print(_DEBUG,"Alg TSR fps %d(ini:%d); CpuTemp %d\n",mTsrOpCnt,mTsrFps,mCpuTemp);
                mTsrFlameId = 0;
                mTsrOpCnt =0;
            }
        }
        if(mPdsEnable)
        {
            if(mPdsFlameId == 0) {
                initPdsFlameFilter();
            }
            if(PdsflameFilter(mPdsFlameId++)) {
                pOutImgBlk = pdsOperation(pOutImgBlk);
                //log_print(_INFO,"Alg::algProcess  pdsOperation# done\n");
            }//else log_print(_INFO,"Alg::algProcess  ~\n");
            if(mPdsFlameId >= mCaptureFps) {
                //log_print(_DEBUG,"Alg PDS fps %d(ini:%d); CpuTemp %d\n",mPdsOpCnt,mPdsFps,mCpuTemp);
                mPdsFlameId = 0;
                mPdsOpCnt =0;
            }
        }
        
        if(mLdwsEnable)
        {
            if(mLdwsFlameId == 0)
                initLdwsFlameFilter();
            if(LdwsflameFilter(mLdwsFlameId++)) {
                pOutImgBlk = ldwsOperation(pOutImgBlk);
                //log_print(_INFO,"##########Alg::algProcess ldwsOperation  done\n");
            }//else log_print(_DEBUG,"##########Alg::algProcess  ignore\n");
            if(mLdwsFlameId >= mCaptureFps) {
                //log_print(_DEBUG,"Alg LDWS fps %d(ini:%d); CpuTemp %d\n",mLdwsOpCnt,mLdwsFps,mCpuTemp);
                mLdwsFlameId = 0;
                mLdwsOpCnt = 0;
            }
        }
        if(mUFcwsEnable)
        {
            if(mUFcwsFlameId == 0)
                initUFcwsFlameFilter();
            if(uFcwsflameFilter(mUFcwsFlameId++)) {
                pOutImgBlk = uFcwsOperation(pOutImgBlk);
                log_print(_INFO,"##########Alg::algProcess uFcwsOperation  done\n");
            }//else log_print(_DEBUG,"##########Alg::algProcess  ignore\n");
            if(mUFcwsFlameId >= mCaptureFps) {
                log_print(_DEBUG,"Alg uFcws fps %d(ini:%d); CpuTemp %d\n",mUFcwsOpCnt,mUFcwsFps,mCpuTemp);
                mUFcwsFlameId = 0;
                mUFcwsOpCnt = 0;
            }
        }

    }
        
}
void Alg::updateSpeed(double speed)
{
    u8 tmp_speed;
    if(speed < 0){
        log_print(_ERROR,"Alg::setGpsSpeed speed < 0(%f)\n",speed);
        tmp_speed = 0;
    }else if(speed > 255){
        log_print(_ERROR,"Alg::setGpsSpeed speed > 255(%f)\n",speed);
        tmp_speed = 255;
    }else{
        tmp_speed = (u8)speed;
        log_print(_DEBUG, "Alg::setGpsSpeed speed %u\n",tmp_speed);
    }
    mGpsSpeed = tmp_speed;
}

void Alg::clearRingBuffer()
{
    mpFcwsRingQueue->clear();
    mpLdwsRingQueue->clear();
    mpTsrRingQueue->clear();
    mpPdsRingQueue->clear();
    mpUFcwsRingQueue->clear();
}

int Alg::initHw()
{
    int i;
    ImgBlk *pImgblk = NULL;
#ifdef IMX6_PLATFORM
    mImgProc = new CIpu;
#endif  
    if(mImgProc == NULL) {
        log_print(_ERROR,"Alg Error, No ImgProcessUnit Created\n");
        return -1;
    }
    log_print(_DEBUG,"Alg fcws %dx%d, fourcc 0x%x\n",mImgProcParms.fcws.width, mImgProcParms.fcws.height, mImgProcParms.fcws.fourcc);
    
    for(i = 0; i < RING_BUFFER_VIN_TO_FCWS_NUM; i++){
        pImgblk = mImgProc->allocHwBuf(mImgProcParms.fcws.width, mImgProcParms.fcws.height, mImgProcParms.fcws.fourcc);
        mpFcwsImgBlk[i] = pImgblk;
        if(pImgblk)
            log_print(_DEBUG,"Alg::initHw FCWS alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);

        //if(pImgblk)
            //memcpy(&(mFcwsImgBlk[i]), pImgblk, sizeof(ImgBlk));
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_TSR_NUM; i++){
        pImgblk = mImgProc->allocHwBuf(mImgProcParms.tsr.width, mImgProcParms.tsr.height, mImgProcParms.tsr.fourcc);
        mpTsrImgBlk[i] = pImgblk;
        if(pImgblk)
            log_print(_DEBUG,"Alg::initHw TSR alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_PDS_NUM; i++){
        pImgblk = mImgProc->allocHwBuf(mImgProcParms.pds.width, mImgProcParms.pds.height, mImgProcParms.pds.fourcc);
        mpPdsImgBlk[i] = pImgblk;
        if(pImgblk)
            log_print(_DEBUG,"Alg::initHw PDS alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_UFCWS_NUM; i++){
        pImgblk = mImgProc->allocHwBuf(mImgProcParms.uFcws.width, mImgProcParms.uFcws.height, mImgProcParms.uFcws.fourcc);
        mpUFcwsImgBlk[i] = pImgblk;
        if(pImgblk)
            log_print(_DEBUG,"Alg::initHw UFCWS alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);
    }

    
    log_print(_DEBUG,"Alg ldws %dx%d, fourcc 0x%x\n",mImgProcParms.ldws.width, mImgProcParms.ldws.height, mImgProcParms.ldws.fourcc);
    for(i = 0; i < RING_BUFFER_VIN_TO_LDWS_NUM; i++){
        pImgblk = mImgProc->allocHwBuf(mImgProcParms.ldws.width, mImgProcParms.ldws.height, mImgProcParms.ldws.fourcc);
        mpLdwsImgBlk[i] = pImgblk;
        if(pImgblk)
            log_print(_DEBUG,"Alg::initHw LDWS alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);
        //if(pImgblk)
            //memcpy(&(mLdwsImgBlk[i]), pImgblk, sizeof(ImgBlk));
    }
    
    return 0;
}

int Alg::unInitHw()
{
    int i;
    if(mImgProc == NULL)
        return -1;
    for(i = 0; i < RING_BUFFER_VIN_TO_FCWS_NUM; i++){
        if(mpFcwsImgBlk[i])
            mImgProc->freeHwBuf(mpFcwsImgBlk[i]);
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_TSR_NUM; i++){
        if(mpTsrImgBlk[i])
            mImgProc->freeHwBuf(mpTsrImgBlk[i]);
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_PDS_NUM; i++){
        if(mpPdsImgBlk[i])
            mImgProc->freeHwBuf(mpPdsImgBlk[i]);
    }    
    for(i = 0; i < RING_BUFFER_VIN_TO_LDWS_NUM; i++){
        if(mpLdwsImgBlk[i])
            mImgProc->freeHwBuf(mpLdwsImgBlk[i]);
    }
    for(i = 0; i < RING_BUFFER_VIN_TO_UFCWS_NUM; i++){
        if(mpUFcwsImgBlk[i])
            mImgProc->freeHwBuf(mpUFcwsImgBlk[i]);
    }

    if(mImgProc)
        delete mImgProc;
    mImgProc = NULL;
    return 0;
}

ImgBlk* Alg::getFcwsImgBlk()
{
    ImgBlk* pTmp = mpFcwsImgBlk[mFcwsImgBlkId];
    mFcwsImgBlkId++;
    if(mFcwsImgBlkId >= RING_BUFFER_VIN_TO_FCWS_NUM)
        mFcwsImgBlkId=0;
    return pTmp;
}

ImgBlk* Alg::getTsrImgBlk()
{
    ImgBlk* pTmp = mpTsrImgBlk[mTsrImgBlkId];
    mTsrImgBlkId++;
    if(mTsrImgBlkId >= RING_BUFFER_VIN_TO_TSR_NUM)
        mTsrImgBlkId=0;
    return pTmp;
}

ImgBlk* Alg::getPdsImgBlk()
{
    ImgBlk* pTmp = mpPdsImgBlk[mPdsImgBlkId];
    mPdsImgBlkId++;
    if(mPdsImgBlkId >= RING_BUFFER_VIN_TO_PDS_NUM)
        mPdsImgBlkId=0;
    return pTmp;
}

ImgBlk* Alg::getLdwsImgBlk()
{
    ImgBlk* pTmp = mpLdwsImgBlk[mLdwsImgBlkId];
    mLdwsImgBlkId++;
    if(mLdwsImgBlkId >= RING_BUFFER_VIN_TO_LDWS_NUM)
        mLdwsImgBlkId=0;
    return pTmp;
}

ImgBlk* Alg::getUFcwsImgBlk()
{
    ImgBlk* pTmp = mpUFcwsImgBlk[mUFcwsImgBlkId];
    mUFcwsImgBlkId++;
    if(mUFcwsImgBlkId >= RING_BUFFER_VIN_TO_UFCWS_NUM)
        mUFcwsImgBlkId=0;
    return pTmp;
}


ImgBlk* Alg::fcwsOperation(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mFcwsOnOff == false){
        mFcwsOnOff = (tmpSpeed >= mFcwsStartSpeed)?true:false;
        if(mFcwsOnOff == false){
            //log_print(_DEBUG,"Alg::fcwsOperations Fcws current off( Speed %d, StartSpeed %d).\n",tmpSpeed,mFcwsStartSpeed);
            return input;   
        }
        log_print(_DEBUG,"Alg::fcwsOperation Fcws turn on. speed %d\n", tmpSpeed);
    }else{//true
        //mFcwsOnOff = (tmpSpeed >= (mFcwsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        mFcwsOnOff = (tmpSpeed >= mFcwsStartSpeed)?true:false;
        if(mFcwsOnOff == false){
            log_print(_INFO,"Alg::fcwsOperation Fcws turn off. speed %d\n", tmpSpeed);
            mpFcwsRingQueue->clear();
            SendAlgStopEvent(DO_FCWS);
            return input;
        }
    }
    
    mFcwsOpCnt++;
    
    ImgBlk *pOutImgBlk;
    //log_print(_DEBUG,"Alg::IMG fcwsOperation pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpFcwsImgBlk[mFcwsImgBlkId];
    pOutImgBlk = getFcwsImgBlk();
    mImgProc->resize(input, pOutImgBlk);
    log_print(_DEBUG,"Alg::IMG fcwsOperation resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    #if DUMP_IMG_FILE
    saveResizeImg((mpFcwsImgBlk[mFcwsImgBlkId]));
    #endif
    mFcwsInfo.img.org_width = gCapIniParm.VidCapParm.width;//input->width;
    mFcwsInfo.img.org_height = gCapIniParm.VidCapParm.height;//input->height;

#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"Alg::Alg fcws notify----add save data to bmp file\n");
    sprintf(mFcwsInfo.img.imgName, "/tmp/fcws/%d.bmp", mFcwsImgBlkId);
    saveBmpFile(mFcwsInfo.img.imgName, (mpFcwsImgBlk[mFcwsImgBlkId]));
#endif

    mFcwsInfo.lane[LEFT_LANE].state = mLdwsInfo.lane[LEFT_LANE].state;
    mFcwsInfo.lane[LEFT_LANE].k = mLdwsInfo.lane[LEFT_LANE].k;
    mFcwsInfo.lane[LEFT_LANE].b = mLdwsInfo.lane[LEFT_LANE].b;
    mFcwsInfo.lane[RIGHT_LANE].state = mLdwsInfo.lane[RIGHT_LANE].state;
    mFcwsInfo.lane[RIGHT_LANE].k = mLdwsInfo.lane[RIGHT_LANE].k;
    mFcwsInfo.lane[RIGHT_LANE].b = mLdwsInfo.lane[RIGHT_LANE].b;
    mFcwsInfo.vanishLine = mVanishLine;
    mFcwsInfo.hasLane = mLdwsInfo.hasLane;
    
    
    //memset(gFcwsInfo.imgName,0,MAX_NAME_LEN);
    mFcwsInfo.img.width = mImgProcParms.fcws.width;
    mFcwsInfo.img.height = mImgProcParms.fcws.height;
    memcpy(&(mFcwsInfo.imgBlk), pOutImgBlk, sizeof(ImgBlk));
    mFcwsInfo.onOff = 1;
    mFcwsInfo.speed = tmpSpeed;
    mpFcwsRingQueue->push(mFcwsInfo);

    //sendFcwsImgEvent(&gFcwsInfo);

    return pOutImgBlk;
}

ImgBlk* Alg::tsrOperation(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mTsrOnOff == false){
        mTsrOnOff = (tmpSpeed >= mTsrStartSpeed)?true:false;
        if(mTsrOnOff == false){
            //log_print(_DEBUG,"Alg::tsrOperations Tsr current off( Speed %d, StartSpeed %d).\n",tmpSpeed,mTsrStartSpeed);
            return input;   
        }
        log_print(_INFO,"Alg::tsrOperation Tsr turn on. speed %d\n", tmpSpeed);
    }else{//true
        //mTsrOnOff = (tmpSpeed >= (mTsrStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        mTsrOnOff = (tmpSpeed >= mTsrStartSpeed)?true:false;
        if(mTsrOnOff == false){
            log_print(_INFO,"Alg::tsrOperation Tsr turn off. speed %d\n", tmpSpeed);
            mpTsrRingQueue->clear();
            SendAlgStopEvent(DO_TSR);
            return input;
        }
    }
    
    mTsrOpCnt++;
    
    ImgBlk *pOutImgBlk;
    //log_print(_DEBUG,"Alg::IMG fcwsOperation pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpFcwsImgBlk[mFcwsImgBlkId];
    pOutImgBlk = getTsrImgBlk();
    if((input->width != pOutImgBlk->width)||(input->height!= pOutImgBlk->height)) {
        mImgProc->resize(input, pOutImgBlk);
        log_print(_DEBUG,"Alg::IMG tsrOperation resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    } else {
        pOutImgBlk = input;
        log_print(_DEBUG,"Alg::IMG tsrOperation reuse ImgBlk %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    }
    #if DUMP_IMG_FILE
    saveResizeImg((mpTsrImgBlk[mTsrImgBlkId]));
    #endif
    mTsrInfo.img.org_width = gCapIniParm.VidCapParm.width;//input->width;
    mTsrInfo.img.org_height = gCapIniParm.VidCapParm.height;//input->height;

#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"Alg::Alg tsr notify----add save data to bmp file\n");
    sprintf(mTsrInfo.img.imgName,"/tmp/tsr/%d.bmp",mTsrImgBlkId);
    saveBmpFile(mTsrInfo.img.imgName, (mpTsrImgBlk[mTsrImgBlkId]));
#endif

    mTsrInfo.lane[LEFT_LANE].state = mLdwsInfo.lane[LEFT_LANE].state;
    mTsrInfo.lane[LEFT_LANE].k = mLdwsInfo.lane[LEFT_LANE].k;
    mTsrInfo.lane[LEFT_LANE].b = mLdwsInfo.lane[LEFT_LANE].b;
    mTsrInfo.lane[RIGHT_LANE].state = mLdwsInfo.lane[RIGHT_LANE].state;
    mTsrInfo.lane[RIGHT_LANE].k = mLdwsInfo.lane[RIGHT_LANE].k;
    mTsrInfo.lane[RIGHT_LANE].b = mLdwsInfo.lane[RIGHT_LANE].b;
    mTsrInfo.vanishLine = mVanishLine;
    mTsrInfo.hasLane = mLdwsInfo.hasLane;
    
    
    //memset(gFcwsInfo.imgName,0,MAX_NAME_LEN);
    mTsrInfo.img.width = mImgProcParms.tsr.width;
    mTsrInfo.img.height = mImgProcParms.tsr.height;
    memcpy(&(mTsrInfo.imgBlk), pOutImgBlk, sizeof(ImgBlk));
    mTsrInfo.onOff = 1;
    mTsrInfo.speed = tmpSpeed;
    mpTsrRingQueue->push(mTsrInfo);

    //sendTsrImgEvent(&gTsrInfo);

    return pOutImgBlk;
}

ImgBlk* Alg::pdsOperation(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mPdsOnOff == false){
        //mPdsOnOff = (tmpSpeed >= mPdsStartSpeed)?true:false;
        mPdsOnOff = (tmpSpeed >= mPdsStartSpeed && tmpSpeed < mFcwsStartSpeed)?true:false;
        if(mPdsOnOff == false){
            //log_print(_DEBUG,"Alg::tsrOperations Pds current off( Speed %d, StartSpeed %d).\n",tmpSpeed,mPdsStartSpeed);
            return input;   
        }
        log_print(_INFO,"Alg::pdsOperation Pds turn on. speed %d\n", tmpSpeed);
    }else{//true
        //mPdsOnOff = (tmpSpeed >= (mPdsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        mPdsOnOff = (tmpSpeed >= mPdsStartSpeed && tmpSpeed < mFcwsStartSpeed)?true:false;
        if(mPdsOnOff == false){
            log_print(_INFO,"Alg::pdsOperation Pds turn off. speed %d\n", tmpSpeed);
            mpPdsRingQueue->clear();
            SendAlgStopEvent(DO_PDS);
            return input;
        }
    }
    
    mPdsOpCnt++;
    
    ImgBlk *pOutImgBlk;
    //log_print(_DEBUG,"Alg::IMG fcwsOperation pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpFcwsImgBlk[mFcwsImgBlkId];
    pOutImgBlk = getPdsImgBlk();
    if((input->width != pOutImgBlk->width)||(input->height!= pOutImgBlk->height)) {
        mImgProc->resize(input, pOutImgBlk);
        log_print(_DEBUG,"Alg::pdsOperation resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    } else {
        pOutImgBlk = input;
        log_print(_DEBUG,"Alg::pdsOperation reuse ImgBlk %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    }
    #if DUMP_IMG_FILE
    saveResizeImg((mpPdsImgBlk[mPdsImgBlkId]));
    #endif
    mPdsInfo.img.org_width = gCapIniParm.VidCapParm.width;//input->width;
    mPdsInfo.img.org_height = gCapIniParm.VidCapParm.height;//input->height;

#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"Alg::Alg pds notify----add save data to bmp file\n");
    sprintf(mPdsInfo.img.imgName,"/tmp/pds/%d.bmp",mPdsImgBlkId);
    saveBmpFile(mPdsInfo.img.imgName, (mpPdsImgBlk[mPdsImgBlkId]));
#endif

    mPdsInfo.lane[LEFT_LANE].state = mLdwsInfo.lane[LEFT_LANE].state;
    mPdsInfo.lane[LEFT_LANE].k = mLdwsInfo.lane[LEFT_LANE].k;
    mPdsInfo.lane[LEFT_LANE].b = mLdwsInfo.lane[LEFT_LANE].b;
    mPdsInfo.lane[RIGHT_LANE].state = mLdwsInfo.lane[RIGHT_LANE].state;
    mPdsInfo.lane[RIGHT_LANE].k = mLdwsInfo.lane[RIGHT_LANE].k;
    mPdsInfo.lane[RIGHT_LANE].b = mLdwsInfo.lane[RIGHT_LANE].b;
    mPdsInfo.vanishLine = mVanishLine;
    mPdsInfo.hasLane = mLdwsInfo.hasLane;
    
    
    //memset(gFcwsInfo.imgName,0,MAX_NAME_LEN);
    mPdsInfo.img.width = mImgProcParms.pds.width;
    mPdsInfo.img.height = mImgProcParms.pds.height;
    memcpy(&(mPdsInfo.imgBlk), pOutImgBlk, sizeof(ImgBlk));
    mPdsInfo.onOff = 1;
    mPdsInfo.speed = tmpSpeed;
    mpPdsRingQueue->push(mPdsInfo);

    //sendPdsImgEvent(&mPdsInfo);

    return pOutImgBlk;
}

ImgBlk* Alg::uFcwsOperation(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mUFcwsOnOff == false) {
        //mPdsOnOff = (tmpSpeed >= mPdsStartSpeed)?true:false;
        mUFcwsOnOff = (tmpSpeed >= mUFcwsStartSpeed && tmpSpeed < mFcwsStartSpeed)?true:false;//!!
        if(mUFcwsOnOff == false) {
            //log_print(_DEBUG,"Alg::tsrOperations Pds current off( Speed %d, StartSpeed %d).\n",tmpSpeed,mPdsStartSpeed);
            return input;   
        }
        log_print(_INFO,"Alg::uFcwsOperation uFcws turn on. speed %d\n", tmpSpeed);
    }else{//true
        //mPdsOnOff = (tmpSpeed >= (mPdsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        mUFcwsOnOff = (tmpSpeed >= mUFcwsStartSpeed && tmpSpeed < mFcwsStartSpeed)?true:false;
        if(mUFcwsOnOff == false){
            log_print(_INFO,"Alg::uFcwsOperation uFcws turn off. speed %d\n", tmpSpeed);
            mpUFcwsRingQueue->clear();
            SendAlgStopEvent(DO_UFCWS);
            return input;
        }
    }
    
    mUFcwsOpCnt++;
    
    ImgBlk *pOutImgBlk;
    //log_print(_DEBUG,"Alg::IMG fcwsOperation pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpFcwsImgBlk[mFcwsImgBlkId];
    pOutImgBlk = getUFcwsImgBlk();
    if((input->width != pOutImgBlk->width)||(input->height!= pOutImgBlk->height)) {
        mImgProc->resize(input, pOutImgBlk);
        log_print(_DEBUG,"Alg::uFcwsOperation resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    } else {
        pOutImgBlk = input;
        log_print(_DEBUG,"Alg::uFcwsOperation reuse ImgBlk %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    }
    #if DUMP_IMG_FILE
    saveResizeImg(mpUFcwsImgBlk[mUFcwsImgBlkId]));
    #endif
    mUFcwsInfo.img.org_width = gCapIniParm.VidCapParm.width;//input->width;
    mUFcwsInfo.img.org_height = gCapIniParm.VidCapParm.height;//input->height;

#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"Alg::Alg pds notify----add save data to bmp file\n");
    sprintf(mUFcwsInfo.img.imgName,"/tmp/uFcws/%d.bmp",mUFcwsImgBlkId);
    saveBmpFile(mUFcwsInfo.img.imgName, (mpUFcwsImgBlk[mUFcwsImgBlkId]));
#endif

    mUFcwsInfo.lane[LEFT_LANE].state = mLdwsInfo.lane[LEFT_LANE].state;
    mUFcwsInfo.lane[LEFT_LANE].k = mLdwsInfo.lane[LEFT_LANE].k;
    mUFcwsInfo.lane[LEFT_LANE].b = mLdwsInfo.lane[LEFT_LANE].b;
    mUFcwsInfo.lane[RIGHT_LANE].state = mLdwsInfo.lane[RIGHT_LANE].state;
    mUFcwsInfo.lane[RIGHT_LANE].k = mLdwsInfo.lane[RIGHT_LANE].k;
    mUFcwsInfo.lane[RIGHT_LANE].b = mLdwsInfo.lane[RIGHT_LANE].b;
    mUFcwsInfo.vanishLine = mVanishLine;
    mUFcwsInfo.hasLane = mLdwsInfo.hasLane;
    
    mUFcwsInfo.img.width = mImgProcParms.pds.width;
    mUFcwsInfo.img.height = mImgProcParms.pds.height;
    memcpy(&(mUFcwsInfo.imgBlk), pOutImgBlk, sizeof(ImgBlk));
    mUFcwsInfo.onOff = 1;
    mUFcwsInfo.speed = tmpSpeed;
    mpUFcwsRingQueue->push(mUFcwsInfo);
    return pOutImgBlk;
}


ImgBlk* Alg::ldwsOperation(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mLdwsOnOff== false){
        mLdwsOnOff = (tmpSpeed >= mLdwsStartSpeed)?true:false;
        if(mLdwsOnOff == false) {
            //log_print(_INFO,"Alg::ldwsOperation ldws current off( Speed %d, StartSpeed %d).\n",tmpSpeed,mLdwsStartSpeed);
            return input;
        }
        log_print(_INFO,"Alg::ldwsOperation ldws turn on. speed %d\n",tmpSpeed);
    }else{//true
        //mLdwsOnOff = (tmpSpeed >= (mLdwsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        mLdwsOnOff = (tmpSpeed >= mLdwsStartSpeed)?true:false;
        if(mLdwsOnOff == false){
            log_print(_INFO,"Alg::ldwsOperation ldws turn off. speed %d\n",tmpSpeed);
            lane_t lane;
            lane.state = LANE_NONE;
            lane.k = 0;
            lane.b = 0;
            updateLaneInfo(lane,lane,mImportFrameRange,mVanishLine);
            mpLdwsRingQueue->clear();
            SendAlgStopEvent(DO_LDWS);
            return input;
        }
    }
    mLdwsOpCnt++;       
    ImgBlk *pOutImgBlk;
    //log_print(_DEBUG,"Alg::IMG ldwsOperation pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpLdwsImgBlk[mLdwsImgBlkId];
    pOutImgBlk = getLdwsImgBlk();
    mImgProc->resize(input, pOutImgBlk);
    log_print(_DEBUG,"Alg::ldwsOperation resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    mLdwsInfo.img.org_width = gCapIniParm.VidCapParm.width;//input->width;
    mLdwsInfo.img.org_height = gCapIniParm.VidCapParm.height;//input->height;
#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"ImgProcessJob ldws notify----add save data to bmp file\n");
    sprintf(mLdwsInfo.img.imgName, "/tmp/ldws/%d.bmp", mLdwsImgBlkId);
    saveBmpFile(mLdwsInfo.img.imgName, (mpLdwsImgBlk[mLdwsImgBlkId]));
    
#endif
    
    //memset(mLdwsInfo.img.imgName,0,MAX_NAME_LEN);
    mLdwsInfo.img.width = mImgProcParms.ldws.width;
    mLdwsInfo.img.height = mImgProcParms.ldws.height;
    memcpy(&(mLdwsInfo.imgBlk), pOutImgBlk, sizeof(ImgBlk));
    mLdwsInfo.onOff = 1;
    mLdwsInfo.speed = mGpsSpeed;
    mpLdwsRingQueue->push(mLdwsInfo);
    //sendLdwsImgEvent(&gLdwsInfo);

    return pOutImgBlk;
}

int Alg::matchFpsWithCPUTemp(int temp,int maxFps)
{
    int fps = maxFps;
    if(fps > 5)
    {
        if(mCpuTemp < 60)
            ;
        else if(mCpuTemp < 70)
            fps = fps - fps/8;
        else if(mCpuTemp < 80)
            fps = fps - 2*fps/8;
        else if(mCpuTemp < 90)
            fps = fps - 3*fps/8;
        else if(mCpuTemp < 100)
            fps = fps - 4*fps/8;
        else
            fps = 0;
    }
    return fps;
}


void Alg::initFcwsFlameFilter()
{
    mCpuTemp = getCpuTemp();
    int fps = matchFpsWithCPUTemp(mCpuTemp,mFcwsFps);
    mFcwsFlameRate = mCaptureFps*1.0/(mCaptureFps - fps);
    mFcwsHasThrown = 0;
    //log_print(_INFO,"####Alg::algProcess initFcwsFlameFilter over temp %d,fps %d,rate %f.\n",mCpuTemp,fps,mFcwsFlameRate);
}

void Alg::initTsrFlameFilter()
{
    mCpuTemp = getCpuTemp();
    int fps = matchFpsWithCPUTemp(mCpuTemp,mTsrFps);
    mTsrFlameRate = mCaptureFps*1.0/(mCaptureFps - fps);
    mTsrHasThrown = 0;
    //log_print(_INFO,"####Alg::algProcess initTsrFlameFilter over temp %d,fps %d,rate %f.\n",mCpuTemp,fps,mTsrFlameRate);
}

void Alg::initPdsFlameFilter()
{
    mCpuTemp = getCpuTemp();
    int fps = matchFpsWithCPUTemp(mCpuTemp,mPdsFps);
    mPdsFlameRate = mCaptureFps*1.0/(mCaptureFps - fps);
    mPdsHasThrown = 0;
    //log_print(_INFO,"####Alg::algProcess initPdsFlameFilter over temp %d,fps %d,rate %f.\n",mCpuTemp,fps,mPdsFlameRate);
}

void Alg::initUFcwsFlameFilter()
{
    mCpuTemp = getCpuTemp();
    int fps = matchFpsWithCPUTemp(mCpuTemp,mUFcwsFps);
    mUFcwsFlameRate = mCaptureFps*1.0/(mCaptureFps - fps);
    mUFcwsHasThrown = 0;
    //log_print(_INFO,"####Alg::algProcess initPdsFlameFilter over temp %d,fps %d,rate %f.\n",mCpuTemp,fps,mPdsFlameRate);
}


void Alg::initLdwsFlameFilter()
{
    mCpuTemp = getCpuTemp();
    int fps = matchFpsWithCPUTemp(mCpuTemp,mLdwsFps);
    mLdwsFlameRate = mCaptureFps*1.0/(mCaptureFps - fps);
    mLdwsHasThrown = 0;
    log_print(_DEBUG,"Alg::initLdwsFlameFilter over tempture %d,fps %d,rate %f.\n",mCpuTemp,fps,mLdwsFlameRate);
}

bool Alg::FcwsflameFilter(int flameIndex)
{
    if( notLessThan((float)(flameIndex+1),mFcwsFlameRate*(mFcwsHasThrown+1)) )
    {
        ++mFcwsHasThrown;
        return false;
    }
    return true;
}

bool Alg::TsrflameFilter(int flameIndex)
{
    if( notLessThan((float)(flameIndex+1),mTsrFlameRate*(mTsrHasThrown+1)) )
    {
        ++mTsrHasThrown;
        return false;
    }
    return true;
}

bool Alg::PdsflameFilter(int flameIndex)
{
    if( notLessThan((float)(flameIndex+1),mPdsFlameRate*(mPdsHasThrown+1)) )
    {
        ++mPdsHasThrown;
        return false;
    }
    return true;
}

bool Alg::uFcwsflameFilter(int flameIndex)
{
    if( notLessThan((float)(flameIndex+1),mUFcwsFlameRate*(mUFcwsHasThrown+1)) )
    {
        ++mUFcwsHasThrown;
        return false;
    }
    return true;
}


bool Alg::LdwsflameFilter(int flameIndex)
{
    if( notLessThan((float)(flameIndex+1),mLdwsFlameRate*(mLdwsHasThrown+1)) )
    {
        ++mLdwsHasThrown;
        return false;
    }
    return true;
}


int Alg::SendAlgStopEvent(enum DO_MSG_TYPE doType) 
{
    log_print(_DEBUG,"Alg::SendAlgStopEvent\n");
    
    MsgPacket packet;
    OpcEvent_t event;
    event.value = 0;
    packet.header.src= MsgAddr(DO_MEDIA);
    packet.header.dest= MsgAddr(doType);
    packet.header.opcode= OPC_ALG_STOP_EVT;
    packet.header.len = sizeof(OpcEvent_t);
    memcpy(packet.args,&event,sizeof(OpcEvent_t));  
    return gpMediaCoreIpc->sendPacket(&packet);
}


void  Alg::updateLaneInfo(lane_t &left, lane_t &right, int importFrameRange, int vanishLine)
{
    mLdwsInfo.lane[LEFT_LANE].state= left.state;
    mLdwsInfo.lane[LEFT_LANE].k= left.k;
    mLdwsInfo.lane[LEFT_LANE].b= left.b;
    mLdwsInfo.lane[RIGHT_LANE].state= right.state;
    mLdwsInfo.lane[RIGHT_LANE].k = right.k;
    mLdwsInfo.lane[RIGHT_LANE].b = right.b;
    if((left.state != LANE_NONE) && (right.state != LANE_NONE)) {
        mLdwsInfo.hasLane = 1;
    } else {
        mLdwsInfo.hasLane = 0;
    }
    mLdwsInfo.importantFrameRange = importFrameRange;
    if(importFrameRange) {
            mLdwsFlameRate = mCaptureFps*1.0/(mCaptureFps - 15); //force to change to 15fps
    }
    mVanishLine = vanishLine;
}

void Alg::algImgOnOff(int onOff)
{
    mAlgImgOn = (onOff ==0) ? false : true;
    log_print(_INFO,"Alg::algImgOnOff %d\n",onOff);
    
}

