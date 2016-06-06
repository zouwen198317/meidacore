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

#include "imageprocessjob.h"
#include "log/log.h"
#include "ipc/ipcclient.h"
#include "debug/errlog.h"
#include "mediacore.h"
#include"parms_dump.h"
#include "alg.h"
//!!
#include "playbackjob.h"
#include "videocapturejob.h"

#ifdef IMX6_PLATFORM
#include "platform/IMX6/imageprocessunit.h"
#endif
#include "mediafile/rawfile.h"
#include "mediafile/bmpfile.h"

#include <assert.h>


#define DUMP_IMG_FILE 0

//#define SPEED_FLEXIBLE_DIFFERENCE 10

//LDWS FCWS
#define ALG_BMP 0

extern mediaStatus gMediaStatus;
extern MsgQueue *gpImgProcMsgQueue;
extern MsgQueue *gpVidEncMsgQueue;
extern int gPlaybackAlg;
//!!
extern PlayBackJob *gpPlayBackJob;
extern vidCaptureJob *gpVidCaptureJob;

extern IpcClient * gpMediaCoreIpc;
extern DispIniParm gDispIniParm;
extern FcwsIniParm gFcwsIniParm;
extern LdwsIniParm gLdwsIniParm;
extern vidCapParm gVidCapParm;

extern int gLCDDispEnable;

extern Alg *gpAlgProcess;
//imgInfo_t gLdwsInfo;
//imgInfo_t gFcwsInfo;

int  sendLdwsImgEvent(imgInfo_t *pInfo)
{
    MsgPacket packet;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_LDWS);
    packet.header.opcode = OPC_IMG;
    packet.header.len = sizeof(imgInfo_t);
    memcpy(packet.args, pInfo, sizeof(imgInfo_t));
    return gpMediaCoreIpc->sendPacket(&packet);
}

int  sendFcwsImgEvent(imgInfo_t *pInfo)
{
    MsgPacket packet;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_FCWS);
    packet.header.opcode = OPC_IMG;
    packet.header.len = sizeof(imgInfo_t);
    memcpy(packet.args, pInfo, sizeof(imgInfo_t));
    return gpMediaCoreIpc->sendPacket(&packet);
}

ImgProcessJob::ImgProcessJob(imgProcParms *pParm)
{
    assert(pParm);
    log_print(_DEBUG, "ImgProcessJob\n");

    mpCamRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_VIN,CAM_BUFFER_NUM,sizeof(ImgBlk));
    mpPlayRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_PLAY_TO_VIN,RING_BUFFER_PLAY_TO_VIN_NUM,sizeof(ImgBlk));

    mDispImgBlkId = 0;
    
    mVidDisp = NULL;
    mRunning = false;
    mPause = false;
    mDispPause = false;
    mSwitchIndisplay = false;
    mDispEnable = true;
    mNeedFreeze = false;//freezed flag of capture
    mFreezeNum = 0;//freezed number of capture
    if(gLCDDispEnable == 0){
        mDispEnable = false;
    }
    mVidDispParm.crop_height = gDispIniParm.LCD.vidHeight;
    mVidDispParm.crop_width = gDispIniParm.LCD.vidWidth;
    mVidDispParm.crop_left = gDispIniParm.LCD.vidLeft;
    mVidDispParm.crop_top = gDispIniParm.LCD.vidTop;
    mVidDispParm.icrop_height = 0;
    mVidDispParm.icrop_width = 0;
    mVidDispParm.icrop_left = 0;
    mVidDispParm.icrop_top = 0;
    mVidDispParm.rotate = 0;
    mVidDispParm.motion = 0;
    //printf("#########constructor mVidDispParm.crop_top %d\n",mVidDispParm.crop_top);
    
    //initHw();
    
}

ImgProcessJob::~ImgProcessJob()
{
    mRunning = false;
    stop();
    sleep(1);
    //unInitHw();   
    /*
    if(mpVencRingQueue)
        delete mpVencRingQueue;
        
    if(mpFcwsRingQueue)
        delete mpFcwsRingQueue;
    if(mpLdwsRingQueue)
        delete mpLdwsRingQueue;
    */  
    if(mVidDisp)
        delete mVidDisp;
}

/*
int ImgProcessJob::initHw()
{
    int i;
    ImgBlk *pImgblk = NULL;
#ifdef IMX6_PLATFORM
    mpImgProc = new CIpu;
#endif  

    if(mpImgProc == NULL) {
        log_print(_INFO,"ImgProcessJob Error, No ImgProcessUnit Created\n");
        return -1;
    }
    for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
        pImgblk = mpImgProc->allocHwBuf(1280, 720, gVidCapParm.fourcc);
        mDispImgBlk[i] = pImgblk;
        log_print(_INFO,"IMG FCWS alloc %d x %d, vaddr 0x%x, paddr 0x%x size %d\n",pImgblk->width,pImgblk->height,(u32)pImgblk->vaddr,(u32)pImgblk->paddr,pImgblk->size);

        //if(pImgblk)
            //memcpy(&(mFcwsImgBlk[i]), pImgblk, sizeof(ImgBlk));
    }
    
    return 0;
}

int ImgProcessJob::unInitHw()
{
    int i;
    if(mpImgProc == NULL)
        return -1;
    
    for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
        if(mDispImgBlk[i])
            mpImgProc->freeHwBuf(mDispImgBlk[i]);
    }

    if(mpImgProc)
        delete mpImgProc;
    return 0;
}
*/
/*
void ImgProcessJob::setGpsSpeed(double speed)
{
    u8 tmp_speed;
    if(speed < 0){
        log_print(_INFO,"ImgProcessJob::setGpsSpeed speed < 0(%f)\n",speed);
        tmp_speed = 0;
    }else if(speed > 255){
        log_print(_INFO,"ImgProcessJob::setGpsSpeed speed > 255(%f)\n",speed);
        tmp_speed = 255;
    }else{
        tmp_speed = (u8)speed;
        log_print(_DEBUG,"ImgProcessJon::setGpsSpeed speed %u\n",tmp_speed);
    }
    mGpsSpeed = tmp_speed;
}
*/
/*
ImgBlk* ImgProcessJob::getDispImgBlk()
{
    ImgBlk* pTmp = mDispImgBlk[mDispImgBlkId];
    mDispImgBlkId++;
    if(mDispImgBlkId >= DISPLAY_BUFFER_NUM)
        mDispImgBlkId=0;
    return pTmp;
}*/

/*
ImgBlk* ImgProcessJob::getFcwsImgBlk()
{
    ImgBlk* pTmp = mpFcwsImgBlk[mFcwsImgBlkId];
    mFcwsImgBlkId++;
    if(mFcwsImgBlkId >= RING_BUFFER_VIN_TO_FCWS_NUM)
        mFcwsImgBlkId=0;
    return pTmp;
}

ImgBlk* ImgProcessJob::getLdwsImgBlk()
{
    ImgBlk* pTmp = mpLdwsImgBlk[mLdwsImgBlkId];
    mLdwsImgBlkId++;
    if(mLdwsImgBlkId >= RING_BUFFER_VIN_TO_LDWS_NUM)
        mLdwsImgBlkId=0;
    return pTmp;

}
*/
/*
int ImgProcessJob::switchVidDispBuffer(vidDispParm * pParm)
{
    if(mVidDisp == NULL)
    {
        log_print(_INFO,"ImgProcessJob:switchVidDispBuffer() mVidDisp have not started \n");
        return -1;
    }
    fbuffer_info * bufInfo;
    if(mMediaStatus== MEDIA_PLAYBACK)
    {
        gpPlayBackJob->get_buffer_info(bufInfo);
    }
    else if(mMediaStatus > MEDIA_PLAYBACK)
    {
        gpVidCaptureJob->get_buffer_info(bufInfo);
    }
    ImgBlk* tmpqueue[DISP_BUFFER_MAX_NUM];
    for(int i=0;i<bufInfo->num;i++)                 
    {                                               
        tmpqueue[i]->paddr = bufInfo->addr[i];      
        tmpqueue[i]->size= bufInfo->length;         
    }
    mVidDisp->switchBuffers(bufInfo->num,tmpqueue,pParm);
    
}*/

int ImgProcessJob::InitDisplay(ImgBlk *imgsrc,mediaStatus pStatus)
{
    
    mVidDispParm.width = imgsrc->width; //input picture
    mVidDispParm.height = imgsrc->height;
    mVidDispParm.fourcc = imgsrc->fourcc;
    
    log_print(_INFO, "ImgProcessJob InitDisplay %dx%d,fourcc:0x%x\n", imgsrc->width, imgsrc->height, imgsrc->fourcc);
    
    memcpy(mVidDispParm.dev_path, gDispIniParm.LCD.vidoFbName, MAX_PATH_NAME_LEN);
    //printf("#########imgsrc->width %d\n",imgsrc->width);
    //printf("#########mVidDispParm.crop_top %d\n",mVidDispParm.crop_top);
    mVidDisp = new VidDisp(mVidDispParm,V4L2_MEMORY_USERPTR);
    //mVidDisp = new VidDisp(&vidDispParmtest,V4L2_MEMORY_MMAP);
    //mpPlayRingQueue->clear();
    fbuffer_info  bufInfo;
    
    if(pStatus == MEDIA_PLAYBACK) {
        gpPlayBackJob->get_buffer_info(&bufInfo);
    } else if(pStatus > MEDIA_PLAYBACK) {
        gpVidCaptureJob->get_buffer_info(&bufInfo);
    }
    
    for(int i=0; i<bufInfo.num; i++) {  
        mVidDisp->addBuffer(NULL,bufInfo.addr[i],bufInfo.length);
    }
    mMediaStatus = pStatus;
    
    mVidDisp->startDisplay();   
    return 0;
}

//FIXME: not useful yet 
int ImgProcessJob::adjustDisplay(int width,int height,int top,int left)
{
    printf("----------alterDisplay-----------\n");
    int ret = -1;
    //mVidDispParm = mVidDisp->getParm();
    //mVidDispParm->width = pParm->width; //input picture
      //mVidDispParm->height = pParm->height;
    //mVidDispParm->fps = pParm->fps;
    if(width > 0) {
        mVidDispParm.icrop_left = left + mVidDispParm.crop_left;
        mVidDispParm.icrop_top = top + mVidDispParm.crop_top;
        mVidDispParm.icrop_width = width;
        mVidDispParm.icrop_height = height;
        mVidDispParm.crop_left = 0;
        mVidDispParm.crop_top = 0;
        mVidDispParm.crop_width = width -1; //TODO
        mVidDispParm.crop_height = height -1;
        //mVidDispParm.crop_width = width;
        //mVidDispParm.crop_height = height;
    } else if(width == 0) {
        mVidDispParm.icrop_left = 0;
        mVidDispParm.icrop_top = 0;
        mVidDispParm.icrop_width = 0; //720
        mVidDispParm.icrop_height = 0;//400
        mVidDispParm.crop_left = 0;
        mVidDispParm.crop_top = 0;
        mVidDispParm.crop_width = gDispIniParm.LCD.vidWidth;
        mVidDispParm.crop_height = gDispIniParm.LCD.vidHeight;
    }
    printf("############mVidDispParm %d,%d,%d,%d,%d,%d,%d,%d\n",mVidDispParm.icrop_left,mVidDispParm.icrop_top,mVidDispParm.icrop_width,mVidDispParm.icrop_height,
    mVidDispParm.crop_height,mVidDispParm.crop_width,mVidDispParm.crop_left,mVidDispParm.crop_top);
    ret = notifyItself(MSG_OP_DISP_ADJUST);
    return ret;
}

int ImgProcessJob::display(ImgBlk *img)
{
    //ImgBlk *tmpBlk = img;
    //if(mDispAdjust)
    //{
        //tmpBlk = getDispImgBlk();
        //tmpBlk->width=720;
        //tmpBlk->height=400;
        //mpImgProc->crop(img,tmpBlk);
        /*
        int crop_left=280;
        int crop_top=160;
        char* ptr=(char*)img->vaddr;
        char* ptr2 = (char*)tmpBlk->vaddr;
        int pos=0;
        for(int i =crop_top;i < img->height-crop_top;++i)
        {
            for(int j= crop_left;i < img->width-crop_left;++j)
            {
                int tmp_pos=(i*img->width+j)<<1;
                ptr2[pos++] = ptr[tmp_pos++];
                ptr2[pos++] = ptr[tmp_pos];
            }
        }*/
    //}
    mVidDisp->display(img);
    //log_print(_DEBUG,"ImgProcessJob display img %dx%d,fourcc:0x%x\n",img->width,img->height,img->fourcc);
    return 0;

}

int ImgProcessJob::saveBmpFile(char *name, ImgBlk* img)
{
    log_print(_INFO, "ImgProcessJob saveBmpFile--%s, %d x %d\n", name, img->width, img->height);
    BmpFile * file = new BmpFile(name,img->width, img->height, 24);//24Bit
    if(file) {  
        file->write((char*)img->vaddr, img->size);
        delete file;
    }
    return 0;
}

int ImgProcessJob::notifyItself(int msgopc)
{
    int ret = -1;
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpImgProcMsgQueue) {
        log_print(_DEBUG, "ImgProcessJob notifyItself %d \n", msgopc);
        gpImgProcMsgQueue->send(&msg);
        ret = 0;
    }
    return ret;
}

int ImgProcessJob::notify()
{
    log_print(_DEBUG, "ImgProcessJob notify, no need to do\n");
    
    //if(status() == MEDIA_RECORD)
    /*
    mpVencRingQueue->push(*(getVencImgBlk()));

    msgbuf_t msg;
    msg.type=MSGQUEU_TYPE_NORMAL;
    msg.len=0;
    msg.opcode=MSG_OP_NORMAL;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpVidEncMsgQueue) {
        log_print(_DEBUG,"ImgProcessJob notify queue num %d\n",gpVidEncMsgQueue->msgNum());
        gpVidEncMsgQueue->send(&msg);
    }
      */

    return 0;
}

/**
int ImgProcessJob::status()
{
    return gMediaStatus;
}
*/

int ImgProcessJob::saveResizeImg(ImgBlk* img)
{
    static int dumpImgId=0;
    char name[MAX_NAME_LEN];
    sprintf(name, "./ImgProcDump_%d.bmp", dumpImgId++);
    log_print(_INFO, "saveResizeImg IMG %s %d x %d, vaddr 0x%x, size %d", name, img->width, img->height, (u32)img->vaddr, img->size);
    
    //RawFile * file= new RawFile(name);
    BmpFile* file = new BmpFile(name, img->width, img->height, 24);
    if(file) {
        file->write((char *)img->vaddr, img->size);
        delete file;
    }
    return 0;
}

#if 0
int ImgProcessJob::processImg(msgbuf_t *msgbuf)
{
    int ret=-1;
    ImgBlk inputImgBlk;
    //ImgBlk *pOutImgBlk;
    log_print(_DEBUG,"ImgProcessJob processImg\n");
    switch(status()) {
        case MEDIA_PLAYBACK:
            ret = mpPlayRingQueue->pop();
            if(ret < 0) {
                log_print(_INFO,"ImgProcessJob processImg mpCamRingQueue no data input %d\n",ret);
                return ret;
            }
            inputImgBlk = mpPlayRingQueue->front();
            break;
        case MEDIA_RECORD:
            ret = mpCamRingQueue->pop();
            if(ret < 0) {
                log_print(_INFO,"ImgProcessJob processImg mpCamRingQueue no data input %d\n",ret);
                return ret;
            }
            inputImgBlk = mpCamRingQueue->front();
            /*
            if(mpImgProc) {
                //Venc
                mpImgProc->resize(&inputImgBlk, &(mVencImgBlk[mVencImgBlkId]));
                notify();
            }
            */
        case MEDIA_IDLE:
            if(ret <0)   {
                ret = mpCamRingQueue->pop();
                if(ret < 0) {
                    log_print(_INFO,"ImgProcessJob processImg mpCamRingQueue no data input %d\n",ret);
                    return ret;
                }
                inputImgBlk = mpCamRingQueue->front();
            }

            if(mpImgProc && (mPause == false)) {
                //FCWS
                if(mFcwsEn) {
                    fcwsOpertations(&inputImgBlk);
                }
                
                //LDWS
                if(mLdwsEn) {
                    ldwsOpertations(&inputImgBlk);
                }
                
                //notify();
            }
            break;
        default:
            break;
            
    }
    if(( ret >=0 )&& (mDispPause == false)){
        //LCD display
        display(&inputImgBlk);
    }
    
    return 0;
}
#endif
int ImgProcessJob::processImg(msgbuf_t *msgbuf,mediaStatus pStatus)
{
    int ret = -1;
    ImgBlk inputImgBlk;
    //ImgBlk *pOutImgBlk;
    int msgNum = 0, ringNum = 0;

    msgNum = gpImgProcMsgQueue->msgNum();
    ringNum = mpCamRingQueue->size();
    if(msgNum>=10 || ringNum>=CAM_BUFFER_NUM)
        log_print(_DEBUG, "ImgProcessJob processImg  MsgQueue size %d, RingQueue size %d\n", msgNum, ringNum);

    if((ringNum >= CAM_BUFFER_NUM) || (ringNum>2)){
        while( ringNum-- >1 ) { //leave the newest img to process
            ret = mpCamRingQueue->pop();
        }
    }
    switch(pStatus) {
        case MEDIA_PLAYBACK:
            ret = mpPlayRingQueue->pop();
            if(ret < 0) {
                log_print(_ERROR, "ImgProcessJob processImg mpPlayRingQueue no data input %d\n", ret);
                return ret;
            }
            inputImgBlk = mpPlayRingQueue->front();
            break;
        case MEDIA_RECORD:
            ret = mpCamRingQueue->pop();
            if(ret < 0) {
                log_print(_DEBUG, "ImgProcessJob processImg mpCamRingQueue no data input %d\n", ret);
                return ret;
            }
            inputImgBlk = mpCamRingQueue->front();
            /*
            if(mpImgProc) {
                //Venc
                mpImgProc->resize(&inputImgBlk, &(mVencImgBlk[mVencImgBlkId]));
                notify();
            }
            */
            break;
        default:
            break;
            
    }
    
    if(gpAlgProcess != NULL && mPause == false) {
        //if(pStatus == MEDIA_RECORD || pStatus == MEDIA_IDLE_MAIN 
            //|| (pStatus==MEDIA_PLAYBACK)&&gPlaybackAlg)
            gpAlgProcess->algProcess(&inputImgBlk);
    }
    
    if(gLCDDispEnable == 0){
        mDispEnable = false;
    }
    
    if((long long)inputImgBlk.pts < 0) {
        delete mVidDisp;
        mVidDisp = NULL;
        mpCamRingQueue->clear();    
        log_print(_ERROR, "ImgProcessJob processImg wrong pts\n");
    }
//#ifndef NO_CAM
    if(mVidDisp==NULL && mDispEnable) {
        log_print(_INFO, "----------InitDisplay-----------\n");
        InitDisplay(&inputImgBlk,pStatus);
    }
//#endif
    /*
    if((mVidDisp != NULL) && (mMediaStatus != pStatus))
    {
        printf("----------switchDisplay-----------\n");
        struct vidDispParm_ *vidDispParmtest;
        vidDispParmtest = mVidDisp->getParm();
        vidDispParmtest->width=inputImgBlk.width; //input picture
            vidDispParmtest->height=inputImgBlk.height;
            vidDispParmtest->fourcc=inputImgBlk.fourcc;
        mMediaStatus = pStatus;
        switchVidDispBuffer(vidDispParmtest);
        
    }*/
    if(mNeedFreeze) {//screen freeze for n frames after capture
        mFreezeNum--;
        if(mFreezeNum == 0)
            mNeedFreeze = false;
    } else if((mVidDisp != NULL) && (ret >= 0) && (mDispPause == false) && mDispEnable) {
        //LCD display
        
        //printf("ImgProcessJob display pts %llu\n",inputImgBlk.pts);
        display(&inputImgBlk);
    }
    
    return 0;
}
/*
ImgBlk* ImgProcessJob::fcwsOperations(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mFcwsOnOff == false){
        mFcwsOnOff = (tmpSpeed >= mFcwsStartSpeed)?true:false;
        if(mFcwsOnOff == false)
            return;
        log_print(_INFO,"ImgProcessJob::fcwsOperations Fcws turn on.\n");
    }else{//true
        mFcwsOnOff = (tmpSpeed >= (mFcwsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        if(mFcwsOnOff == false){
            log_print(_INFO,"ImgProcessJob::fcwsOperations Fcws turn off.\n");
            return;
        }
    }
    
    
    ImgBlk *pOutImgBlk;
    log_print(_DEBUG,"IMG resize pts %lld, input %d x %d, vaddr 0x%x, size %d\n",input->pts,input->width,input->height,(u32)input->vaddr,input->size);
    //pOutImgBlk = mpFcwsImgBlk[mFcwsImgBlkId];
    pOutImgBlk = getFcwsImgBlk();
    log_print(_DEBUG,"IMG resize out %d x %d, vaddr 0x%x, size %d\n",pOutImgBlk->width,pOutImgBlk->height,(u32)pOutImgBlk->vaddr,pOutImgBlk->size);
    mpImgProc->resize(input, pOutImgBlk);
    #if DUMP_IMG_FILE
    saveResizeImg((mpFcwsImgBlk[mFcwsImgBlkId]));
    #endif
    mFcwsInfo.img.org_width =input->width;
    mFcwsInfo.img.org_height= input->height;

#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"ImgProcessJob fcws notify----add save data to bmp file\n");
    sprintf(mFcwsInfo.img.imgName,"/tmp/fcws/%d.bmp",mFcwsImgBlkId);
    saveBmpFile(mFcwsInfo.img.imgName, (mpFcwsImgBlk[mFcwsImgBlkId]));
#endif
    
    
    //memset(gFcwsInfo.imgName,0,MAX_NAME_LEN);
    mFcwsInfo.img.width = mImgProcParms.fcws.width;
    mFcwsInfo.img.height = mImgProcParms.fcws.height;
    memcpy(&(mFcwsInfo.imgBlk),pOutImgBlk,sizeof(ImgBlk));
    mFcwsInfo.onOff = 1;
    mFcwsInfo.speed= tmpSpeed;
    mpFcwsRingQueue->push(mFcwsInfo);

    //sendFcwsImgEvent(&gFcwsInfo);

    return pOutImgBlk;
}

ImgBlk* ImgProcessJob::ldwsOperations(ImgBlk* input)
{
    u8 tmpSpeed = mGpsSpeed;
    
    if(mLdwsOnOff== false){
        mLdwsOnOff = (tmpSpeed >= mLdwsStartSpeed)?true:false;
        if(mLdwsOnOff == false)
            return;
        log_print(_INFO,"ImgProcessJob::ldwsOperations Fcws turn on.\n");
    }else{//true
        mLdwsOnOff = (tmpSpeed >= (mLdwsStartSpeed-SPEED_FLEXIBLE_DIFFERENCE))?true:false;
        if(mLdwsOnOff == false){
            log_print(_INFO,"ImgProcessJob::ldwsOperations Fcws turn off.\n");
            return;
        }
    }

    ImgBlk *pOutImgBlk;
    //pOutImgBlk = mpLdwsImgBlk[mLdwsImgBlkId];
    pOutImgBlk = getLdwsImgBlk();
    mpImgProc->resize(input, pOutImgBlk);
    mLdwsInfo.img.org_width =input->width;
    mLdwsInfo.img.org_height= input->height;
#if ALG_BMP
    //"save data to bmp file";
    log_print(_INFO,"ImgProcessJob ldws notify----add save data to bmp file\n");
    sprintf(mLdwsInfo.img.imgName,"/tmp/ldws/%d.bmp",mLdwsImgBlkId);
    saveBmpFile(mLdwsInfo.img.imgName, (mpLdwsImgBlk[mLdwsImgBlkId]));
    
#endif
    
    //memset(mLdwsInfo.img.imgName,0,MAX_NAME_LEN);
    mLdwsInfo.img.width = mImgProcParms.ldws.width;
    mLdwsInfo.img.height = mImgProcParms.ldws.height;
    memcpy(&(mLdwsInfo.imgBlk),pOutImgBlk,sizeof(ImgBlk));
    mLdwsInfo.onOff = 1;
    mLdwsInfo.speed= mGpsSpeed;
    mpLdwsRingQueue->push(mLdwsInfo);
    //sendLdwsImgEvent(&gLdwsInfo);

    return pOutImgBlk;

}
*/

void ImgProcessJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_INFO,"ImgProcessJob stop() \n");

    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = MSG_OP_STOP;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpImgProcMsgQueue) {
        log_print(_DEBUG, "vidCaptureJob notify to stop \n");
        gpImgProcMsgQueue->send(&msg);
    }
}

void ImgProcessJob::pause()
{
    log_print(_INFO, ">>>>>ImgProcessJob pause()\n");
    mPause = true;
    //no need to notify next job
}

void ImgProcessJob::resume()
{
    if(mPause) {
        mPause = false;
        gpAlgProcess->clearRingBuffer();
    }
    //no need to notify next job
}

void ImgProcessJob::run()
{
    mRunning = true;
    msgbuf_t msg;
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION);
    Register_debug_signal();
    log_print(_DEBUG,"ImgProcessJob run()\n");

    mMediaStatus = MEDIA_ERROR;

    while( mRunning ) {
        gpImgProcMsgQueue->recv(&msg);
        switch(msg.opcode) {
            //case MSG_OP_CAPTURE:
            case MSG_OP_NORMAL:
                if(status() != MEDIA_PLAYBACK)
                    processImg(&msg, MEDIA_RECORD);
                break;
            case MSG_OP_PLAY:
                processImg(&msg, MEDIA_PLAYBACK);
                break;
            case MSG_OP_RESUME:
                resume();
                break;
            case MSG_OP_START:
                break;
            case MSG_OP_PAUSE:
                pause();
                break;
            case MSG_OP_STOP:
                mRunning = false;
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                break;
            case MSG_OP_DISHSTOP:
                printf("ImgProcessJob::run() MSG_OP_DISHSTOP\n");
                //mpPlayRingQueue->clear();
                delete mVidDisp;
                mVidDisp = NULL;
                mpCamRingQueue->clear();                
                break;
            case MSG_OP_DISP_ADJUST:
                //printf("#########MSG_OP_DISP_ADJUST\n");
                delete mVidDisp;
                mVidDisp = NULL;
                mpCamRingQueue->clear();
                //printf("#########MSG_OP_DISP_ADJUST\n");
                break;
            default:
                break;
        }       
    }
    log_print(_ERROR, "ImgProcessJob run() exit\n");
    
}

int ImgProcessJob::pauseDisplay(bool pause)
{
    log_print(_INFO, "ImgProcessJob pauseDisplay() %d \n",pause);
    mDispPause = pause;
    return 0;

}

int ImgProcessJob::status()
{
    return gMediaStatus;
}

void ImgProcessJob::videoDisplayOnOff(int onOff)
{
    mDispEnable = (onOff == 0) ? false : true;
    log_print(_INFO, "videoDisplayOnOff %d\n",onOff);
}

int ImgProcessJob::screenFreeze( )//screen freeze after capture
{
    log_print(_DEBUG, "ImgProcessJob screenFreeze() \n");
    mNeedFreeze = true;
    mFreezeNum = FREEZE_FRAMES_NUM;
    return 0;
}

/*
int ImgProcessJob::enableDisplay(bool enable)
{
    log_print(_INFO,"ImgProcessJob enableDisplay() \n");
    return 0;

}
*/

