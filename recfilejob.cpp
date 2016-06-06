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

#include "log/log.h"
#include "ipc/ipcclient.h"
#include "debug/errlog.h"
#include "mediacore.h"
#include "opccommand.h"
#include "videoencodejob.h"
#include "audiocapturejob.h"
#include "videocapturejob.h"
#include "videoprepencodejob.h"
#include "motion_detection_technology.h"

#include "recfilejob.h"
#include "avimuxer.h"
#include "list/list.h"
#include "overridejob.h"
#include "parms_dump.h"

extern mediaStatus gMediaStatus;
extern MsgQueue *gpRecFileMsgQueue;

extern IpcClient * gpMediaCoreIpc;
extern MsgQueue *gpOverrideMsgQueue;

extern ExceptionProcessor *gpExceptionProc;
extern VideoEncodeJob *gpVidEncJob;
extern VideoPrepEncJob *gpVideoPrepEncJob;
extern AudioCaptureJob * gpAudCaptureJob;
extern vidCaptureJob *gpVidCaptureJob;
extern MyMotionDetectionTechnology *gpMotionDetectJob;

extern RecFileIniParm gRecIniParm;
extern CaptureIniParm gCapIniParm;
extern bool PreEmergencyRecing;

extern int gDebugMode;
extern bool gpParking;
extern bool gParkRecord;
extern bool gDriveRecord;

extern int NotifyEventNoArgToGui(unsigned char opcode);


#define VIDEO_BUF_LEN 512000
#define DUMP_ALG_FILE 1
#define ALG_REC_INTERVAL 10  //if alg alarm interval greater than n seconds, start another alarm record.
#define PRE_REC_TIME 10         //to record for n seconds before alg/emc event occur.
#define SKIPPING_DELAY_NUM 0  //delay n buffers in order to record the motion object.

int renameSuffix(char *srcName, const char *suffix)
{
    char tempName[MAX_PATH_NAME_LEN];
    strcpy(tempName, srcName);
    char *pDotPos = strrchr(tempName, '.');
    if(pDotPos == NULL) {
        log_print(_ERROR, "RecFileJob renameSuffix error\n");
        return -1;
    }
    *pDotPos = '\0';
    snprintf(srcName, 64, "%s%s", tempName, suffix);
    log_print(_DEBUG, "RecFileJob renameSuffix %s\n", srcName);
    return 0;
}

//test.avi ->  test_emc.avi
int insertFiletypeFlag(char *srcName,const char *flag)
{
    char tempName[MAX_PATH_NAME_LEN];
    char originalName[MAX_PATH_NAME_LEN];
    char suffix[8] = {0};
    
    if(access(srcName, F_OK)) {
        log_print(_ERROR, "RecFileJob insertFiletypeFlag fail, %s\n", strerror(errno));
        return -1;
    }

    strcpy(tempName, srcName);
    strcpy(originalName, srcName);

    //remove suffix, get the file name
    char *pDotPos = strrchr(tempName, '.');
    if(pDotPos != NULL) {
        strcpy(suffix, pDotPos);
        *pDotPos = '\0'; 
    }
    
    snprintf(srcName, MAX_PATH_NAME_LEN, "%s%s%s", tempName, flag, suffix);
    int ret = rename(originalName, srcName);
    if(ret < 0) {
        log_print(_ERROR, "RecFileJob insertFiletypeFlag rename error\n");
        log_print(_ERROR, "originalName: %s\n", originalName);
        log_print(_ERROR, "srcName: %s\n", srcName);
        return ret;
    }
    log_print(_INFO, "RecFileJob insertFiletypeFlag %s\n", srcName);
    return 0;
}

bool isReadyToRec()
{
    log_print(_INFO, "isReadyToRec() gDriveRecord:%d gParkRecord:%d Parking%d\n",gDriveRecord, gParkRecord, gpParking);
    return (gDriveRecord && !gpParking || gParkRecord && gpParking);
}

RecFileJob::RecFileJob(muxParms *pParm)
{
    memcpy(&mParm,pParm ,sizeof(muxParms));
    mpMediaRingQueue = new ShmRingQueue<MediaBlk>((key_t)RING_BUFFER_AVENC_TO_MEDIA_BUF,RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM,sizeof(MediaBlk));
    mpCaptureQueue = new ShmRingQueue<VencBlk>((key_t)RING_BUFFER_AVENC_TO_CAPTURE_BUF,RING_BUFFER_AVENC_TO_CAPTURE_BUF_NUM,sizeof(MediaBlk));
    //log_print(_INFO, "RecFileJob ShmRingQueue size:%d\n", sizeof(MediaBlk));//56 Byte

    mpVideoBuf = (u8*)malloc(VIDEO_BUF_LEN);
    mVideoBufLen = 0;
    mRecFrameCount = 0;
    mDataFp = NULL;

    audioFrameCount = 0;
    filterCount = 0;
    pass = false;
    pts_temp = -1;
    
    mRunning =false;
    mIgnore = false;
    mPause = true; //!!!
    mSkipping = false;
    mMotionDetection = false;
    mTailProcess = false;
    mFlitered = false;
    //mEndTypeFlag = false;
    mpRecMuxer = NULL;
    mpJpgFile = NULL;
    mRecAudio = pParm->hasAudio > 0 ? true : false;
    mRecSensor = pParm->hasMetaData > 0 ? true : false;
    mMaxFileDuration = gDebugMode ? 60*1000 : gRecIniParm.RecFile.duration * 60 * 1000; // 3min = 180000ms
    //mMaxFileDuration =   60 * 1000; // 3min = 180000ms

    log_print(_DEBUG, "VideoEncodeJob MaxFileDuration %d\n", mMaxFileDuration);
    memset(mFileName_previous, 0, strlen(mFileName_previous));
    //mEmergency = false;
    mRecType = EVENT_NORMAL;
    //mPreEmergencyRecing = false;

    mNextMaxFileDuration = 0;
    mRecLastPts = 0;
}

RecFileJob::~RecFileJob()
{
    mRunning = false;
    exit();
    sleep(1);

    if(mpRecMuxer) {
        closeRecFile();
    }
    
    if(mpMediaRingQueue) {
        delete mpMediaRingQueue;
        mpMediaRingQueue = NULL;
    }
    
    if(mpVideoBuf) {
        free(mpVideoBuf);
        mpVideoBuf = NULL;
    }
    mVideoBufLen = 0;
    mIgnore = false;
    
}

int RecFileJob::openRecFile(time_t captureTime)
{
    memset(mFileName, 0, MAX_PATH_NAME_LEN);
    struct tm *pTm = NULL;
    struct tm fileTm;
    pTm = gmtime(&captureTime);
    memcpy(&fileTm, pTm, sizeof(struct tm));
    pTm = &fileTm;

//!!
    //char dataFileName[MAX_PATH_NAME_LEN];
    char pathName[MAX_PATH_NAME_LEN];
    memset(pathName, 0, MAX_PATH_NAME_LEN);
    printf("\n-----Start Recording-----\n");
    snprintf(pathName, 64, "%s/%04d%02d%02d/", mRecPath, (1900+pTm->tm_year), (1+pTm->tm_mon), pTm->tm_mday);
    mkdir(pathName, S_IRWXU | S_IRWXG | S_IRWXO);//777
    
    if(mRecType == EVENT_EMC) {
        snprintf(mFileName, 64, "%s%04d%02d%02d%02d%02d%02d_emc.avi", pathName,
            (1900+pTm->tm_year), (1+pTm->tm_mon), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    } else if(mRecType == EVENT_ALG) {
        snprintf(mFileName, 64, "%s%04d%02d%02d%02d%02d%02d_alg.avi", pathName,
            (1900+pTm->tm_year), (1+pTm->tm_mon), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    } else {
        snprintf(mFileName, 64, "%s%04d%02d%02d%02d%02d%02d.avi", pathName,
            (1900+pTm->tm_year), (1+pTm->tm_mon), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    }
    log_print(_INFO, "RecFileJob openRecFile %s\n", mFileName);
    
    #ifdef USE_SDCARD
    gpExceptionProc->sdcard->writeFileFlag(mFileName);
    #endif
    if(access(mFileName,F_OK) == 0){
        remove(mFileName);
        sync();
    }
    log_print(_INFO, "RecFileJob openRecFile record duration: %dms\n", mMaxFileDuration);
    mpRecMuxer = new AviMuxer(mFileName, &mParm);
    if(mpRecMuxer == NULL) {
        log_print(_ERROR, "RecFileJob openRecFile mpRecMuxer NULL error\n");
        //send storage err mRecType
    }
    mRecFrameCount = 0;
    mRecFileDuration = 0;
    mRecLastPts = 0;
    //if(!mSkipping)
        //mEndTypeFlag = false;
    return 0;
}

int RecFileJob::closeFilePowerOutages()
{
    if(mpRecMuxer) {
        delete mpRecMuxer;
        mpRecMuxer = NULL;
    }
    if(mDataFp) {
        fclose(mDataFp);
        mDataFp=NULL;
    }
    
#ifdef USE_SDCARD
    gpExceptionProc->sdcard->removeFileFlag(mFileName);
#endif
    log_print(_ERROR,"RecFileJob closeRecFile %s\n",mFileName);
    return 0;
}

int RecFileJob::closeRecFile(bool normalClose)
{
    if(mDataFp) {
        fclose(mDataFp);
        mDataFp=NULL;
    }

    if(mRecType == EVENT_EMC || mRecType == EVENT_ALG || mRecType == EVENT_MOTION) {
        log_print(_INFO,"RecFileJob closeRecFile emc/alg Rec close\n");
        if(!isReadyToRec()) {
        //if(gRecIniParm.RecFile.bootRecord == 0) {
            log_print(_INFO, "RecFileJob stop recording\n");
            //gpVideoPrepEncJob->pause(); 
            //gpAudCaptureJob->pause();
            gpExceptionProc->sdcard->stopOverrideJob();
            if(gMediaStatus == MEDIA_RECORD)
                gMediaStatus = MEDIA_IDLE_MAIN; 
            //mPause = true;
            mIgnore = true;  //ignore to open a new recfile

            //mpMediaRingQueue->clear();
            NotifyEventToGui(OPC_REC_ONOFF_SUCCESS_EVENT, REC_ALL_OFF);
        }
        //mEmergency = false;
        if(mMaxFileDuration_backup != 0) {//Restore the recording time
            log_print(_INFO,"RecFileJob closeRecFile rectime change %d to %d\n", mMaxFileDuration, mMaxFileDuration_backup);
            mMaxFileDuration = mMaxFileDuration_backup;
            mMaxFileDuration_backup = 0;
        }
        if(gpMotionDetectJob->isRunning())
            gpMotionDetectJob->resume();

        NotifyEventNoArgToGui(OPC_REC_EMERGENCY_END_EVENT);
        mRecType = EVENT_NORMAL;
    }
    
    if(mpRecMuxer) {
        log_print(_INFO,"RecFileJob closeRecFile %s\n",mFileName);
        delete mpRecMuxer;
        mpRecMuxer = NULL;
//!!
        if(normalClose) {
            //if(strchr(mFileName, '_') == NULL) // find not _emc.avi
            if(strstr(mFileName, "_emc") == NULL) {
                msgbuf_t msg;
                memset(&msg, 0, sizeof(msg));
                msg.type = MSGQUEU_TYPE_NORMAL;
                msg.len = strlen(mFileName);
                strncpy(msg.data, mFileName, msg.len);
                gpOverrideMsgQueue->send(&msg);
            }
        #ifdef USE_SDCARD
            gpExceptionProc->sdcard->removeFileFlag(mFileName);
        #endif
        }
        strcpy(mFileName_previous, mFileName);
        if(mNextMaxFileDuration != 0) {
            mMaxFileDuration = mNextMaxFileDuration;
            mNextMaxFileDuration = 0;
        }
    }
    return 0;
}

int RecFileJob::notifyItself(int msgopc)
{
    log_print(_DEBUG, "RecFileJob notifyItself() msgopc %d\n", msgopc);
    msgbuf_t msg;
    msg.type = MSGQUEU_TYPE_NORMAL;
    msg.len = 0;
    msg.opcode = msgopc;
    memset(msg.data, 0, MSGQUEUE_BUF_SIZE);
    if(gpRecFileMsgQueue) {
        int msgNum = gpRecFileMsgQueue->msgNum();
        if(msgNum < MSGQUEUE_MSG_LIMIT_NUM || msgopc == MSG_OP_NORMAL) //if msgNum >61, msgsnd() may be block
            gpRecFileMsgQueue->send(&msg);
        if(msgNum > MSGQUEUE_MSG_LIMIT_NUM)
            log_print(_ERROR, "RecFileJob notifyItself() gpRecFileMsgQueue %d\n", msgNum);
    }
    return 0;
}

void RecFileJob::stop()
{
    if(mRunning == false)
        return;
    log_print(_DEBUG, "RecFileJob stop()\n");

    notifyItself(MSG_OP_STOP);
}

void RecFileJob::exit()
{
    if(mRunning == false)
        return;
    log_print(_INFO, "RecFileJob stop()\n");
    //mRunning = false;
    notifyItself(MSG_OP_EXIT);
}

void RecFileJob::pause()
{
    audioFrameCount = 0;
    filterCount = 0;
    pass = false;
    pts_temp = -1;
    if(mPause == false) {
        log_print(_INFO, "RecFileJob pause()\n");
        notifyItself(MSG_OP_NORMAL); //To record the rest of the packets in the ringqueue.
        notifyItself(MSG_OP_PAUSE);
    }
}

void RecFileJob::pauseImmediately()
{
    log_print(_ERROR, "RecFileJob pauseImmediately()\n");
    mPause = true;
    closeRecFile(false);
}

void RecFileJob::resume()
{
    if(mPause) {
        log_print(_INFO, "RecFileJob resume()\n");
        notifyItself(MSG_OP_RESUME);
    }
}

//!!
void RecFileJob::setMaxFileDuration(u32 value)
{
    if(value == mMaxFileDuration)
        return;
    
    if(gMediaStatus==MEDIA_RECORD && getRecType()!=EVENT_EMC) {//now recording but no emc
        if(value > mMaxFileDuration) {
            mMaxFileDuration = value;
        } else {
            if(mRecFileDuration <= value)
                mMaxFileDuration = value;
            else
                mNextMaxFileDuration = value;
        }
    } else {
        mMaxFileDuration = value;
    }
    log_print(_INFO, "RecFileJob::setMaxFileDuration %u ms.\n", value);
        
}
void RecFileJob::run()
{
    struct timeval startTime,endTime;
    struct timeval tv;
    long useTime;
    mRunning = true;
    msgbuf_t msg;
    int status = -1;
    int ret = 0;
    int delayNum = 0;
    int queueNum = 0, msgNum = 0, displayNum;
    MediaBlk inputMediaBlk;
    VencBlk inputVencBlk;
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG, "RecFileJob run()\n");
    
    memcpy(mRecPath, gRecIniParm.RecFile.path, MAX_PATH_NAME_LEN);
    memcpy(mPicPath, gCapIniParm.PicCapParm.path, MAX_PATH_NAME_LEN);
    
    //log_print(_INFO, "RecFileJob run() MSG_OP_NORMAL:queueNum %d\n", mpMediaRingQueue->size());
    while( mRunning ) {
        gpRecFileMsgQueue->recv(&msg);
        //log_print(_DEBUG, "RecFileJob run() recv opcode %d\n", msg.opcode);
        queueNum = mpMediaRingQueue->size();
        if((queueNum==0 || queueNum < delayNum) && msg.opcode==MSG_OP_NORMAL) {
            sleep(1);
            notifyItself(MSG_OP_NORMAL);
            continue;
        }
        msgNum = gpRecFileMsgQueue->msgNum();
        displayNum = mSkipping ? 120 : 10;
        if(queueNum>displayNum || msg.opcode!=MSG_OP_NORMAL)
                log_print(_INFO, "RecFileJob run() recv opcode %d msgNum %d, queueNum %d isSkipping %d\n", msg.opcode, msgNum, queueNum, mSkipping);
        switch(msg.opcode) {
            case MSG_OP_UPDATE_CONFIG:
                updateRecFileConfig();
                break;
            case MSG_OP_CAPTURE:
                ret = mpCaptureQueue->pop();
                if(ret < 0) {
                    log_print(_WARN, "RecFileJob run() MSG_OP_CAPTURE no MediaBlk input, queueNum:%d MediaStatus:%d\n", queueNum, gMediaStatus);
                    break;
                }
                inputVencBlk = mpCaptureQueue->front();
                captureJPEG(&inputVencBlk);
                break;
            case MSG_OP_NORMAL:
                if(mPause) 
                    break;
                do {
                    //if(queueNum>=10)
                        //printf("MSG_OP_NORMAL queueNum:%d, msgNum%d\n", queueNum, msgNum);
                    ret = mpMediaRingQueue->pop();
                    if(ret < 0) {
                        log_print(_WARN, "RecFileJob run() MSG_OP_NORMAL no MediaBlk input, queueNum:%d MediaStatus:%d\n", queueNum, gMediaStatus);
                        break;
                    }
                    inputMediaBlk = mpMediaRingQueue->front();
                    //if(!gRecIniParm.RecFile.bootRecord && !mFlitered) {
                    if(mFlitered) {
                        if(inputMediaBlk.blkType != VENC_BLK || inputMediaBlk.blk.vid.pts < mEmcPts) {
                            //printf("mFlitered video block\n");
                            //if(inputMediaBlk.blkType == VENC_BLK)
                                //printf("mFlitered video block, pts:%lld, emcPts%lld\n", inputMediaBlk.blk.vid.pts, mEmcPts);
                            queueNum--;
                            continue;
                        }
                        log_print(_INFO, "RecFileJob run() mFlitered ok, pts:%llu, emcPts%llu, queueNum%d\n", inputMediaBlk.blk.vid.pts, mEmcPts, queueNum);
                        mFlitered = false;
                    }
                    if(mSkipping) {
                        if(inputMediaBlk.blkType == VENC_BLK)
                            ret = processMedia(&inputMediaBlk);
                        //ret = recMotionDection(&inputMediaBlk);
                    } else {
                        ret = processMedia(&inputMediaBlk);
                    }
                    if(ret < 0) {
                        pauseImmediately();
                        mpMediaRingQueue->clear();
                    }
                }while((--queueNum > delayNum) || mpRecMuxer == NULL);//mpRecMuxer == NULL,will clear the queue buffer
                notifyItself(MSG_OP_NORMAL);
                break;
            case MSG_OP_EMC:
                printf("\n-----Emergency Record-----\n");
                recEmergency();
                break;
            case MSG_OP_LDW:
                recAlgorithm(ALGCHK_TYPE_LDW, EVENT_ALG);
                break;
            case MSG_OP_FCW:
                recAlgorithm(ALGCHK_TYPE_FCW, EVENT_ALG);
                break;
            case MSG_OP_FRAME_SKIPPING:
                if(!mSkipping) {
                    printf("\n-----Frame-skipping Record-----\n");
                    //mRecType = EVENT_SKIPPING;
                    //mEndTypeFlag = true;
                    mSkipping = true;
                    delayNum = SKIPPING_DELAY_NUM;
                }
                break;
                /*
            case MSG_OP_PCD:
                //recAlgorithm(1, EVENT_MOTION);
                if(gRecIniParm.RecFile.bootRecord == 0) {
                    if(gMediaStatus != MEDIA_RECORD) 
                        printf("\n-----Parking Collision Detection-----\n");
                } else if(mSkipping) {
                    //mRecType = EVENT_NORMAL;
                    printf("\n-----Parking Collision Detection-----\n");
                    mSkipping = false;
                    mMotionDetection = true;
                    delayNum = 0;
                }

                break;*/
            case MSG_OP_MOTION_DET://18
                recAlgorithm(ALGCHK_TYPE_MDW, EVENT_MOTION);
                /*
                if(gRecIniParm.RecFile.bootRecord == 0) {
                    if(gMediaStatus != MEDIA_RECORD) 
                        printf("\n-----Motion Detection-----\n");
                } else*/ if(mSkipping) {
                    //mRecType = EVENT_NORMAL;
                    printf("\n-----Motion Detection-----\n");
                    mSkipping = false;
                    mMotionDetection = true;
                    delayNum = 0;
                }
                break;

            case MSG_OP_NEXTFILE:
                if(mpRecMuxer) {
                    closeRecFile();
                }                   
                break;              
            case MSG_OP_RESUME:
                mPause = false;
                mIgnore = false;//do not ignore to open new a rec file
                mSkipping = false;
                notifyItself(MSG_OP_NORMAL);
                //mpMediaRingQueue->clear();
                break;
            case MSG_OP_START:
                if(mMaxFileDuration_backup == 0) {
                    mMaxFileDuration_backup = mMaxFileDuration;
                }
                needRecAhead();
                /*
                gettimeofday(&tv, NULL);
                capTime = gpVidCaptureJob->getCaptureTime();
                mEmcPts = (tv.tv_sec - capTime) * 1000 + tv.tv_usec / 1000; //ms
                //printf("mEmcPts:%lld\n", mEmcPts);
                tmpPts = (PRE_REC_TIME + 1) * 1000;
                mEmcPts = (mEmcPts< tmpPts) ? 0 : (mEmcPts-tmpPts);
                */
                //gpVidEncJob->GenKeyFrame();
                gpVideoPrepEncJob->resume();
                gpAudCaptureJob->resume();
                mPause = false;
                mFlitered = true;
                mIgnore = false; 
                
                gpExceptionProc->sdcard->startOverrideJob();
                if(gMediaStatus != MEDIA_RECORD)
                    gMediaStatus = MEDIA_RECORD;
                if(gDebugMode){
                    mMaxFileDuration = 30 * 1000;   //rec 30s
                } else {
                    if(mRecType == EVENT_EMC )
                        mMaxFileDuration = 5 * 60 * 1000;   //rec 5min
                    else if(mRecType == EVENT_ALG)
                        mMaxFileDuration = 30 * 1000;   //rec 30s
                    else if(mRecType == EVENT_MOTION)
                        mMaxFileDuration = 60 * 1000;
                }

                if(mRecType==EVENT_ALG || mRecType==EVENT_MOTION)
                    status = REC_REC_ON;
                if(mRecType == EVENT_EMC)
                    status = REC_EMC_ON;
                NotifyEventToGui(OPC_REC_ONOFF_SUCCESS_EVENT, status);
                notifyItself(MSG_OP_NORMAL);
                break;
                
            case MSG_OP_PAUSE:
                log_print(_INFO, "RecFileJob run() MSG_OP_PAUSE clear RingQueue %d\n", mpMediaRingQueue->size());
                mPause = true;
                mpMediaRingQueue->clear();
                if(mpRecMuxer) {//end rec file
                        gettimeofday(&startTime, NULL);
                    closeRecFile();
                        gettimeofday(&endTime, NULL);
                        useTime = (endTime.tv_sec - startTime.tv_sec) * 1000 + (endTime.tv_usec - startTime.tv_usec) / 1000;
                        log_print(_INFO, "RecFileJob run() closeRecFile() time %ldms\n",useTime);
                        gettimeofday(&startTime, NULL);
                    sync();//write data to physical storage immediately
                        gettimeofday(&endTime, NULL);
                        useTime = (endTime.tv_sec - startTime.tv_sec) * 1000 + (endTime.tv_usec - startTime.tv_usec) / 1000;
                        log_print(_INFO, "RecFileJob run() sync() time %ldms\n", useTime);
                }
                break;
            case MSG_OP_STOP:
                //mIgnore = true;
                if(mpRecMuxer) {
                    gettimeofday(&startTime, NULL);
                    closeFilePowerOutages();
                    sync();//write data to physical storage immediately
                    gettimeofday(&endTime, NULL);
                    useTime = (endTime.tv_sec - startTime.tv_sec)*1000 + (endTime.tv_usec - startTime.tv_usec)/1000;
                    log_print(_INFO, "RecFileJob run() closeFilePowerOutages() time %ldms\n", useTime); 
                }
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                if(mpRecMuxer) {
                    closeRecFile();
                    sync();//write data to physical storage immediately
                }
                break;
            default:
                break;
        }
    }
    log_print(_ERROR,"RecfileJob run() exit\n");
    
}

int RecFileJob::captureJPEG( VencBlk *pVencBlk)
{
    AVPacket pkg = {0};
    pkg.data = (u8*)(pVencBlk->buf.vStartaddr) + pVencBlk->offset;
    pkg.size = pVencBlk->frameSize;
    log_print(_DEBUG, "RecFileJob captureJPEG data 0x%x, size %d \n", (u32)(pkg.data), pkg.size);
    if(pVencBlk->codec != CODEC_ID_MJPEG) {
        log_print(_ERROR, "RecFileJob captureJPEG codec 0x%x != CODEC_ID_MJPEG \n", pVencBlk->codec);
        return -1;
    }

    char fileName[MAX_PATH_NAME_LEN];
    struct tm *pTm = NULL;
    struct tm fileTm;
    //time_t nowTime = time(NULL);
    //pTm = gmtime(&nowTime);
    
    time_t  captureTime = gpVidCaptureJob->getCaptureTime() + time_t(pVencBlk->pts / 1000); //sec
    pTm = gmtime(&captureTime);

    memcpy(&fileTm, pTm, sizeof(struct tm));
    pTm = &fileTm;
    if(gMediaStatus == MEDIA_CAMERA_CALIBRATION){
        static int id = 0;
        ++id;
        snprintf(fileName, 64, "%s/%d.jpg", CAMERA_CALIBRATION_DUMP_PATH, id);
        printf("dump photos in %s\n", fileName);
    }else{
        snprintf(fileName, 64, "%s/%04d%02d%02d%02d%02d%02d.jpg", mPicPath,
            (1900+pTm->tm_year), (1+pTm->tm_mon), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    }
    
    mpJpgFile = new JpgFile(fileName, pVencBlk->width, pVencBlk->height);
    if(mpJpgFile) {
        #ifdef USE_SDCARD
        gpExceptionProc->sdcard->writeFileFlag(fileName);
        #endif
        mpJpgFile->write((char*)pkg.data, pkg.size);
        log_print(_INFO, "RecFileJob captureJPEG fileName %s\n", fileName);
        #ifdef USE_SDCARD
        gpExceptionProc->sdcard->removeFileFlag(fileName);
        #endif
        delete mpJpgFile;
    }
    mpJpgFile = NULL;
    
    return 0;
}

int RecFileJob::writeAencBlk(MediaBlk *pMediaBlk)
{
    AencBlk *pAencBlk = &(pMediaBlk->blk.aud);

    if(mpRecMuxer == NULL){
        log_print(_DEBUG, "RecFileJob::writeAencBlk Ignore\n");
        return 1;
    }

/*  //need first audio packet to avi file
    if(mpRecMuxer && mRecLastPts !=0 ){

        int diff = (pAencBlk->pts - mRecLastPts);///1000 ;
        if(diff > 80){//80 ms
            log_print(_INFO,"RecFileJob writeAencBlk diff %d ms > 80 ms, pVencBlk->pts %llu , mRecLastPts %llu, msgNum %d, RingQueue size %d\n",diff,pAencBlk->pts , mRecLastPts,gpRecFileMsgQueue->msgNum(),mpMediaRingQueue->size());
            if(diff > 100000)
                diff = 40; //or 33, can ignore this serval ms when pts bigger than max u64 value
        }
        mRecFileDuration += diff;
        if(mRecFileDuration > (mMaxFileDuration -500)) {// *min instand by ini config var, 500 ms 0.5sec
            log_print(_INFO,"RecFileJob writeAencBlk reach mRecFileDuration %d of one rec file\n",mRecFileDuration);
            //if(pAencBlk->frameType == VIDEO_I_FRAME ) {
                closeRecFile();
            //}
        }
    }
    mRecLastPts = pAencBlk->pts;
    if(mpRecMuxer == NULL ){
        openRecFile();
        
    }   
*/
    
    AVPacket pkt = {0};
    pkt.data = (u8*)(pAencBlk->buf.vStartaddr) + pAencBlk->offset;
    pkt.size = pAencBlk->frameSize;
    pkt.pts =  pAencBlk->pts;
    int ret = mpRecMuxer->writeAudioPkt(&pkt);

    //log_print(_DEBUG,"RecFileJob writeAencBlk data 0x%x ,size %d , pts %lld, flags %d\n",(u32)pkt.data,pkt.size,pkt.pts,pkt.flags);
    return ret;
}

int RecFileJob::writeVencBlk(MediaBlk *pMediaBlk)
{
    struct timeval startTime,endTime;
    //u32 recDuration;
    long useTime;
    time_t captureTime;
    VencBlk *pVencBlk = &(pMediaBlk->blk.vid);
    if(mRecLastPts > pVencBlk->pts){
            log_print(_INFO,"RecFileJob writeVencBlk mRecLastPts %llu ,pVencBlk->pts %llu\n", mRecLastPts,pVencBlk->pts );
    }

    /*
    if(mParm.hasAudio && mpRecMuxer==NULL){
        log_print(_DEBUG,"RecFileJob::writeVencBlk Ignore\n");
        return 1;
    }
    */
    if(mpRecMuxer && mRecLastPts != 0 ){
        //if(!mEndTypeFlag) {
            int diff = (pVencBlk->pts - mRecLastPts);///1000 ;
            if(diff > 80){//80 ms
                //log_print(_INFO,"RecFileJob writeVencBlk diff %d ms > 80 ms, pVencBlk->pts %llu , mRecLastPts %llu, msgNum %d, RingQueue size %d\n",diff,pVencBlk->pts , mRecLastPts,gpRecFileMsgQueue->msgNum(),mpMediaRingQueue->size());
                //if(diff > 400)
                    diff = 40; //or 33, can ignore this serval ms when pts bigger than max u64 value
            }
            mRecFileDuration += diff;
        //} 
        //mRecFrameCount+=40;//40ms per frame
        //recDuration = mEndTypeFlag ? mRecFrameCount : mRecFileDuration;
        if(mRecFileDuration > (mMaxFileDuration -500)) {// *min instand by ini config var, 500 ms 0.5sec
            log_print(_INFO,"RecFileJob writeVencBlk reach mRecFileDuration %d of one rec file\n",mRecFileDuration);
            if(pVencBlk->frameType == VIDEO_I_FRAME ) {
                //if(!mSkipping || mTailProcess)
                //{
                    mTailProcess = false;
                    gettimeofday(&startTime, NULL);
                    closeRecFile();
                    gettimeofday(&endTime, NULL);
                    useTime = (endTime.tv_sec - startTime.tv_sec) * 1000 + (endTime.tv_usec - startTime.tv_usec) / 1000;
                    log_print(_INFO, "RecFileJob closeRecFile() time %ldms\n",useTime);
                //} else {
                //  mMotionDetection = true;
                    //mTailProcess  = true;
                //}
            }
        }
    }
    if(mpRecMuxer == NULL ){
        if(pVencBlk->frameType != VIDEO_I_FRAME || mIgnore) {
            log_print(_DEBUG, "RecFileJob writeVencBlk frameType %x != VIDEO_I_FRAME or mIgnore %d to New RecFile \n",pVencBlk->frameType,(int)mIgnore );
            return 1;
        }
        captureTime = gpVidCaptureJob->getCaptureTime() + time_t(pVencBlk->pts / 1000); //sec
        /*printf("CaptureTime:%ld, mEmcPts:%ld\n", mCaptureTime, mEmcPts);
            if(pVencBlk->pts < mEmcPts - 10 * 1000)
            {
                printf("mFlitered\n");
                return 0;
            }*/
        openRecFile(captureTime);
            /***************Be carefully*****************
            VLC Player need the first packet is written with audio packet, otherwise the audio can't be played on VLC
            window player ,MXC player on android            are OK
            ****************************************** */
    }
    mRecLastPts = pVencBlk->pts;

    AVPacket pkt = {0};
    
    pkt.data =  (u8*)(pVencBlk->buf.vStartaddr) + pVencBlk->offset;
    pkt.size = pVencBlk->frameSize;
    if((pVencBlk->offset + pVencBlk->frameSize) >= pVencBlk->buf.bufSize) {
        int len = pkt.size > VIDEO_BUF_LEN ? VIDEO_BUF_LEN : pkt.size;
        int offset = pVencBlk->buf.bufSize - pVencBlk->offset;
        log_print(_DEBUG, "RecFileJob writeVencBlk using Software Video Buf %d \n", pkt.size );
        memcpy(mpVideoBuf, pkt.data, offset);
        memcpy(mpVideoBuf + offset, pVencBlk->buf.vStartaddr, len - offset);
        pkt.data = mpVideoBuf;
        pkt.size = len;
    }
    pkt.pts = pVencBlk->pts;
    pkt.flags = pVencBlk->frameType == VIDEO_I_FRAME ? PKT_FLAG_KEY : 0;
    int ret = mpRecMuxer->writeVideoPkt(&pkt);
    //log_print(_DEBUG,"RecFileJob writeVencBlk data %x ,size %d ,frameType %x, pts %llu, flags %d, RecFileDuration %d, MaxFileDuration %d\n",
    //  (u32)pkt.data,pkt.size,pVencBlk->frameType ,pkt.pts,pkt.flags,mRecFileDuration,mMaxFileDuration);
    
    return ret;
}

int RecFileJob::writeSensorBlk(MediaBlk *pMediaBlk)
{
    //log_print(_INFO,"##writeSensorBlk.\n");
    SensorBlk *pSensorBlk = &(pMediaBlk->blk.sensor);
    if(mpRecMuxer == NULL) {
        log_print(_DEBUG, "RecFileJob::writeSensorBlk Ignore\n");
        return 1;
    }
    AVPacket pkt = {0};
    pkt.data = (u8*)(pSensorBlk->buf.vStartaddr) + pSensorBlk->offset;
    pkt.size = pSensorBlk->frameSize;
    pkt.pts =  pSensorBlk->pts;
    #if 0
        FILE *fp;
        fp = fopen("dump_rec_sensor","ab");
        fwrite(pkt.data,1,pkt.size,fp);
        printf("##gsensor pkt data: %s\t%d\n",pkt.data,pkt.size);
        fclose(fp);
    #endif
    //log_print(_DEBUG,"RecFileJob writeSensorBlk() data %x ,size %d , pts %lld, flags %d\n",(u32)pkt.data,pkt.size,pkt.pts,pkt.flags);
    /*
    printf("\n");
    for(int i=0; i<pkt.size; i++ ){
        printf("%c",*(pkt.data+i) );
    }
    printf("\n");
    */      
    int ret = mpRecMuxer->writeDataPkt(&pkt);
    
    return ret;
}
int RecFileJob::writeAlgData(enum AlgChkType type)
{
    //static int fcw_alarm_duration = 0;
    //static int ldw_alarm_duration = 0;
    //static int mdw_alarm_duration = 0;

    //int *alarm_duration = NULL;
    static bool isStartNewChk;
    static u32 pretime = 0;
    u32 nowtime;
    //static unsigned char preOpcode = 0;
    static enum AlgChkType preChkType = ALGCHK_TYPE_NULL;
    char dataFileName[MAX_PATH_NAME_LEN];
    AlgChk *newChk, *algHead, *preAlgChk;
    //nowtime = time(NULL);
    nowtime = mRecFileDuration / 1000 - 1;
    //printf("nowtime %u, mRecFileDuration %u\n", nowtime, mRecFileDuration);
    //if(pretime==nowtime && preOpcode==opcode)
    if(pretime==nowtime && preChkType==type)
        return -1;
    pretime = nowtime;
    preChkType = type;

    if(nowtime<0) nowtime = 0;

    if(mpRecMuxer == NULL)
        return -1;
    
    algHead = mpRecMuxer->getChkHead(type);
    if(algHead == NULL) {
        log_print(_ERROR, "RecFileJob::writeAlgData() getChkHead fail\n");
        return -1;
    }
    
    if(list_empty(&algHead->listEntry))
        isStartNewChk = true;

    preAlgChk = list_entry(algHead->listEntry.prev, AlgChk, listEntry);
    if(nowtime > preAlgChk->tmPoint + preAlgChk->Duration + ALG_REC_INTERVAL) {
        //if alarm interval greater than ALG_REC_INTERVAL seconds, start another alarm record.
        //*alarm_duration = 0;
        isStartNewChk = true;
    }
    //printf("now timepoint:%d, pre:%d %d\n", tmPoint_now, preAlgChk->tmPoint, preAlgChk->Duration);
    //if(*alarm_duration != 0)  {
    if(!isStartNewChk) {
        preAlgChk->Duration = nowtime - preAlgChk->tmPoint + 1;
    } else {
        log_print(_INFO, "RecFileJob writeAlgData() list_add at %u sec\n", nowtime);
        newChk = (AlgChk *)malloc(sizeof(AlgChk));
        newChk->tmPoint = nowtime;
        //newChk->Duration = ++(*alarm_duration);
        newChk->Duration = 1;
        INIT_LIST_HEAD(&newChk->listEntry);
        list_add_tail(&newChk->listEntry, &algHead->listEntry);
        isStartNewChk = false;
    }
    
#if DUMP_ALG_FILE
    /*dump data to dat file*/
    char timestring[128] = { 0 };
    if(mDataFp == NULL){
        strcpy(dataFileName, mFileName);
        renameSuffix(dataFileName, ".dat");
        mDataFp = fopen(dataFileName, "w+");
        if(mDataFp == NULL){
            log_print(_ERROR, "RecFileJob openDataFile failed\n");
            return -1;
        }
        log_print(_INFO, "RecFileJob openDataFile %s\n", dataFileName);
        setlinebuf(mDataFp);
    }
    if(type == ALGCHK_TYPE_LDW)
        sprintf(timestring,"ldws %u\n", nowtime);
    if(type == ALGCHK_TYPE_FCW)
        sprintf(timestring,"fcws %u\n", nowtime);
    if(type == ALGCHK_TYPE_MDW)
        sprintf(timestring,"mdws %u\n", nowtime);

    if(mDataFp != NULL) {
        fwrite(timestring, 1, strlen(timestring), mDataFp);
        //printf("timestring:%s, strlen:%d, ret:%d\n", timestring, strlen(timestring), ret);

    } else {
        printf("fwrite error\n");
    }
#endif

    return 0;
}

int RecFileJob::processMedia(MediaBlk *pMediaBlk)
{
    //log_print(_DEBUG,"RecFileJob::processMedia blkType %d, mPause %d, mRecAudio %d, mRecType %d\n",pMediaBlk->blkType,mPause , mRecAudio , mRecType);
    int ret = 0;
    switch(pMediaBlk->blkType) {
        case AENC_BLK:
            //printf("AENC_BLK\n");
            if( mRecType!=EVENT_EMC && (mPause || mRecAudio==false)){
                log_print(_DEBUG,"RecFileJob::processMedia AENC_BLK ignore.\n");
                break;
            }
            ret = writeAencBlk(pMediaBlk);
            break;
        case VENC_BLK:
            //printf("VENC_BLK\n");
            if(pMediaBlk->blk.vid.codec == CODEC_ID_MJPEG ) {
                ret = captureJPEG( &(pMediaBlk->blk.vid));
            } else {
                if(mPause && ( mRecType != EVENT_EMC)) {
                    log_print(_DEBUG, "RecFileJob::processMedia VENC_BLK ignore.\n");
                    break;
                }
                //if(mSkipping) {
                    //if(pMediaBlk->blk.vid.frameType ==VIDEO_I_FRAME)
                        ret = writeVencBlk(pMediaBlk);
                //}else {
                //  ret = writeVencBlk(pMediaBlk);
                //}
            }
            break;
        case SENSOR_BLK:
            if( mRecType!=EVENT_EMC && (mPause || mRecSensor==false)) {
                log_print(_DEBUG,"RecFileJob::processMedia SENSOR_BLK ignore.\n");
                break;
            }
            //log_print(_INFO,"### enter writeSensorBlk.\n");
            ret = writeSensorBlk(pMediaBlk);
            break;
        case OTHER_BLK:
            if( mRecType!=EVENT_EMC && (mPause || mRecAudio==false)) {
                log_print(_DEBUG,"RecFileJob::processMedia OTHER_BLK ignore.\n");
                break;
            }
            break;
        default:
            break;
    }
    return ret;
}

/*
int RecFileJob::recEmergencyOnoff(bool onoff)
{
    if(mEmergency == onoff)
        return -1;
    mEmergency = onoff;
    //if(onoff && mPause && mEmergency)
    //  mpMediaRingQueue->clear();
    notifyItself(MSG_OP_NEXTFILE);
    return 0;
}*/

int RecFileJob::recPowerOutages()
{
    mPause = true;
    mpMediaRingQueue->clear();
    notifyItself(MSG_OP_STOP);
    return 0;
}
int RecFileJob::recEmcEvent()
{
    /*
    if(mEmergency == onoff)
        return -1;
    mEmergency = onoff;
    */
    if(mRecType == EVENT_EMC)
        return -1;
    notifyItself(MSG_OP_EMC);
    return 0;
}
int RecFileJob::recAlgEvent(unsigned char opcode)
{
    static u32 preLdwTime = -1, preFcwTime = -1;
    if(gpExceptionProc->sdcard->IsPassCheck() == false) {               
        gpExceptionProc->sdcard->refreshSdcardStorage();
        if(gpExceptionProc->sdcard->IsPassCheck() == false) {
            log_print(_ERROR,"RecFileJob::recAlgEvent() sdcard is not in use.\n");
            return -1;
        }
    }
    u32 nowtime = mRecFileDuration / 1000 - 1;
    //log_print(_ERROR,"RecFileJob::recAlgEvent() nowtime %d.\n", nowtime);
    if(opcode == OPC_LDW_DET_EVT) {
        if(preLdwTime != nowtime) {
            preLdwTime = nowtime;
            notifyItself(MSG_OP_LDW);
        }
    } else if(opcode == OPC_FCW_DET_EVT) {
        if(preFcwTime != nowtime) {
            preFcwTime = nowtime;
            notifyItself(MSG_OP_FCW);
        }
    }
    return 0;
}

int RecFileJob::recAlgorithm(enum AlgChkType chkType, recType recType)
{
    int timeLeft;
    if(gMediaStatus != MEDIA_RECORD) {
        //mRecType = recType;
        //notifyItself(MSG_OP_START);
    } else {
        writeAlgData(chkType);
        if(mRecType == recType){
            if(mMaxFileDuration_backup == 0) {
                mMaxFileDuration_backup = mMaxFileDuration;
            }
            timeLeft = (mMaxFileDuration - mRecFileDuration) / 1000; //seconds
            
            if(timeLeft < 30) {
                mMaxFileDuration = mMaxFileDuration - timeLeft * 1000 + 30000;  //append 30s
                log_print(_INFO, "RecFileJob recEmergencyOnoff TimeLeft: %d s, reset RecTime: %d ms\n", timeLeft, mMaxFileDuration);
            }
        }
    }
    return 0;
}

int RecFileJob::recEmergency()
{
    int length, timeLeft;
    char tmpFile[MAX_PATH_NAME_LEN];
    mRecType = EVENT_EMC;
    //if(!isRecing()) 
    if(gMediaStatus != MEDIA_RECORD) {      
        notifyItself(MSG_OP_START);
        return 0;
    }
    /*
    if(mSkipping) {
        gpVidEncJob->setSkipping(false);
        mSkipping = false;
    }*/
    if(mSkipping == true)
        mSkipping = false;
    if(gpMotionDetectJob->isRunning())
        gpMotionDetectJob->pause();
    if(mMaxFileDuration_backup == 0) {
        mMaxFileDuration_backup = mMaxFileDuration;
    }
    NotifyEventToGui(OPC_REC_ONOFF_SUCCESS_EVENT, REC_EMC_ON);
    if(mDataFp) {// #ifdef DUMP_ALG_FILE
        strcpy(tmpFile, mFileName);
        renameSuffix(tmpFile, ".dat");
        insertFiletypeFlag(tmpFile, "_emc");
    }
    sprintf(tmpFile, "%s~", mFileName);//rename current Fileflag
    insertFiletypeFlag(tmpFile, "_emc");
    insertFiletypeFlag(mFileName, "_emc");

    /*If recorded time less than 3 minutes, then the previous recorded files attached on _emc suffix.*/
    length = strlen(mFileName_previous);
    if(length>0 && mRecFileDuration < 180000 && strstr(mFileName_previous, "_emc")==NULL) {
        printf( "-----rename previous file-----\n");
        strcpy(tmpFile, mFileName_previous);
        renameSuffix(tmpFile, ".dat");
        insertFiletypeFlag(tmpFile, "_emc");
        insertFiletypeFlag(mFileName_previous, "_emc");
    }
    
    /*If the remaining time is less than 5 minutes, then make up for 5 minutes.*/
    timeLeft = (mMaxFileDuration - mRecFileDuration) / 1000 / 60;
    if(timeLeft < 5) {
        if(gDebugMode) {
            mMaxFileDuration = mMaxFileDuration - timeLeft * 60 * 1000 + 30000;  //append 30
        } else {
            mMaxFileDuration = mMaxFileDuration - timeLeft * 60 * 1000 + 300000;  //append 5min
        }
        log_print(_INFO, "RecFileJob recEmergency time left: %d min, reset duration: %d min\n", timeLeft, mMaxFileDuration/60000);
    }
    return 0;
}

void RecFileJob::needRecAhead() 
{   
    time_t capTime;
    u64 tmpPts;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    capTime = gpVidCaptureJob->getCaptureTime();
    mEmcPts = (tv.tv_sec - capTime) * 1000 + tv.tv_usec / 1000; //ms
    log_print(_INFO, "RecFileJob::needRecAhead() mEmcPts:%llu\n", mEmcPts);
    tmpPts = PRE_REC_TIME * 1000;
    mEmcPts = (mEmcPts< tmpPts) ? 0 : (mEmcPts-tmpPts);

    mFlitered = true;
}


/*
int RecFileJob::recMotionDection(MediaBlk *pMediaBlk)
{
    static int audioFrameCount = 0;
    static int filterCount = 0;
    static bool pass = false;

    int ret = 0;

    if(pMediaBlk->blkType == AENC_BLK && audioFrameCount == 6) {//25/(8000*4/8/1024)
        ret = processMedia(pMediaBlk);
        audioFrameCount = 0;
    }
    if(pMediaBlk->blkType == VENC_BLK && pMediaBlk->blk.vid.frameType == VIDEO_I_FRAME) {
        if(mMotionDetection) {
            mSkipping = false;
            mMotionDetection = false;
        }

        audioFrameCount++;
        ret = processMedia(pMediaBlk);
    }
    return ret;
}
*/
#if 0
int RecFileJob::recMotionDection(MediaBlk *pMediaBlk)
{
    int ret = 0;
    /*
    if(pMediaBlk->blkType == AENC_BLK  && audioFrameCount == 6) {//25/(8000*4/8/1024)
        //printf("processMedia: audioFrameCount %d\n", audioFrameCount);
        ret = processMedia(pMediaBlk);
        audioFrameCount = 0;
    }
    */
    /*
    static u64 pts = 0;
    if(pMediaBlk->blkType == AENC_BLK) {
        if(pts == 0)
            pts = pMediaBlk->blk.aud.pts;
         if(pMediaBlk->blk.aud.pts - pts >= 1000) {
            ret = processMedia(pMediaBlk);
            pts = pMediaBlk->blk.aud.pts;
         }
    }*/
    if(pMediaBlk->blkType == AENC_BLK ) {//25/(8000*4/8/1024)
        if(pts_temp == -1 || pMediaBlk->blk.aud.pts -pts_temp >= 6250) {
            printf("pts %lld\n", pMediaBlk->blk.aud.pts);
            //ret = processMedia(pMediaBlk);
            pts_temp = pMediaBlk->blk.aud.pts;
        }
    }else
            ret = processMedia(pMediaBlk);

    
    if(pMediaBlk->blkType == VENC_BLK) {
        if(pMediaBlk->blk.vid.frameType == VIDEO_I_FRAME || pass || mTailProcess) {

            if(pass) {
                if(filterCount--<=0) { 
                    if(pMediaBlk->blk.vid.frameType == VIDEO_I_FRAME ) {
                        printf("skipping set false\n");
                        if(!mTailProcess){
                            mSkipping = false;
                        }
                        pass = false;
                        filterCount = 0;
                        audioFrameCount = 0;
                        pts_temp = -1;
                        ret = processMedia(pMediaBlk);
                    }
                    return ret;
                }
            }
            if(mMotionDetection) {
                printf("filterCount: %d\n",filterCount);
                int temp1 = filterCount/25;
                int temp2 = 25 - filterCount%25;
                filterCount = 25 - (temp2+temp1 + 1)%25;
                printf("filterCount: %d\n",filterCount);
                pass = true;
                mMotionDetection = false;
                printf("RecFileJob recMotionDection() compensation frame num%d\n", filterCount);
            }
            audioFrameCount++;
            ret = processMedia(pMediaBlk);
        }
        
        if(!pass) {
            filterCount++;
        }
    }

    return ret;
}
#endif

int RecFileJob::updateConfig(muxParms *pParms)
{
    memcpy(&mNewMuxParm, pParms, sizeof(muxParms));
    notifyItself(MSG_OP_UPDATE_CONFIG);
    return 0;
}

int RecFileJob::updateRecFileConfig()
{
    mMaxFileDuration = gRecIniParm.RecFile.duration * 60 * 1000; // 3min = 180000ms
    mRecAudio =  gRecIniParm.RecFile.hasAudio>0 ? true : false;
    mRecSensor =  gRecIniParm.RecFile.hasMetaData>0 ? true : false;

    memcpy(&mParm, &mNewMuxParm, sizeof(muxParms));

    //mParm.hasAudio = gRecIniParm.RecFile.hasAudio;
    //mParm.hasMetaData= gRecIniParm.RecFile.hasMetaData;
    return 0;
}
/*
bool RecFileJob::isRecing() 
{
    return (!mPause)&&mRunning&&(gRecIniParm.RecFile.bootRecord);
}//!!*/


