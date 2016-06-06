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

#include <sys/socket.h>    
#include <linux/netlink.h>  
#include <sched.h>

#include "mediacore.h"
#include "debug/errlog.h"
#include "log/log.h"
#include "ipc/ipcclient.h"

#include "camcapture.h"
#include "ffmpeg/avcodec_common.h"
#include "ipc/msgqueue.h"

#include "videocapturejob.h"
#include "imageprocessjob.h"
#include "videoprepencodejob.h"
#include "videoencodejob.h"
#include "recfilejob.h"

#include "motion_detection_technology.h"
#include "audiocapturejob.h"
#include "opccommand.h"

#include "playbackjob.h"

#include"parms_dump.h"
#include "alg.h"
#include "exceptionProcessor.h"
#include "AviAMFData.h"

#ifdef VERIFY_LICENSE
#include "verify_part/basic_license.h"
#include "verify_part/xmlParser.h"
#include "verify_part/verify.h"
#include "cpuid/cpuid.h"
#endif
#define LICENSE_PATH "/etc/lic.xml"

static int gMsgProcessRuning = 0; 
int gPlaybackAlg = 0;
bool gDebugMode = false;
mediaStatus gMediaStatus = MEDIA_ERROR;

IpcClient *gpMediaCoreIpc = NULL;
IpcClient *gpMediaCoreIpcEvent = NULL;

vidCaptureJob *gpVidCaptureJob = NULL;
ImgProcessJob *gpImgProcJob = NULL;
VideoPrepEncJob *gpVideoPrepEncJob = NULL;
VideoEncodeJob *gpVidEncJob = NULL;
RecFileJob *gpRecFileJob = NULL;
AudioCaptureJob *gpAudCaptureJob = NULL;
PlayBackJob *gpPlayBackJob = NULL;
MyMotionDetectionTechnology *gpMotionDetectJob = NULL;

//MsgQueue *gpVidCaptMsgQueue = NULL;
MsgQueue *gpImgProcMsgQueue = NULL;
MsgQueue *gpVidPrepVencMsgQueue = NULL;
MsgQueue *gpVidEncMsgQueue = NULL;
MsgQueue *gpRecFileMsgQueue = NULL;
MsgQueue *gpAudCaptureMsgQueue = NULL;
MsgQueue *gpPlayBackMsgQueue = NULL;
//MsgQueue *gpReadPacketMsgQueue = NULL;
MsgQueue *gpMotionDetectMsgQueue = NULL;

//!!
MsgQueue *gpOverrideMsgQueue = NULL;

Alg *gpAlgProcess = NULL;
ExceptionProcessor *gpExceptionProc = NULL;

//parms
vidCapParm gVidCapParm;
imgProcParms gImgProcParm;
vidPrepEncParms gVidPrepEncParms;
vencParms gVencParm;
muxParms gMuxParms;
audioParms gAudioParms;
//
int gNetChannel = -1;
//ini parms
extern CaptureIniParm gCapIniParm;
extern VideoIniParm gVideoIniParm;
extern RecFileIniParm gRecIniParm;
extern FcwsIniParm gFcwsIniParm;
extern TtcIniParm gTtcIniParm;
extern LdwsIniParm gLdwsIniParm;
extern TsrIniParm gTsrIniParm;
extern PdsIniParm gPdsIniParm;
extern UFcwsIniParm gUFcwsIniParm;
extern AudioIniParm gAudioIniParm;
extern DispIniParm gDispIniParm;

extern bool gpParking;
extern bool gParkRecord;
extern bool gDriveRecord;
extern bool isReadyToRec();
int initParms()
{   
    int ret = 0;
    log_print(_DEBUG, "Media Core initParms\n");
    //Video Capture params
    ret = load_CaptureIni();
    if(ret < 0) {
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        return -1;
    }
    gVidCapParm.width = gCapIniParm.VidCapParm.width;
    gVidCapParm.height = gCapIniParm.VidCapParm.height;
    gVidCapParm.fourcc = gCapIniParm.VidCapParm.fourcc; //I422 for usb camera
    gVidCapParm.fps = gCapIniParm.VidCapParm.fps;

    ret = load_VideoIni();
    if(ret < 0) {
        log_print(_DEBUG, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        return -1;
    }
    gVidPrepEncParms.vencParm.height = gVideoIniParm.RecVideo.height;
    gVidPrepEncParms.vencParm.width = gVideoIniParm.RecVideo.width;
    gVidPrepEncParms.vencParm.fourcc = gVideoIniParm.RecVideo.fourcc; //=I420
    gVidPrepEncParms.vencParm.fps = gVideoIniParm.RecVideo.fps; //for usb camera
    gVidPrepEncParms.vencParm.codec = gVideoIniParm.RecVideo.codec;//H264
    gVidPrepEncParms.vencParm.isCBR = gVideoIniParm.RecVideo.isCBR; //
    gVidPrepEncParms.vencParm.bitRate = gVideoIniParm.RecVideo.bitRate; //
    gVidPrepEncParms.vencParm.keyFrameInterval = gVideoIniParm.RecVideo.keyFrameInterval; //

    ret = load_FcwsIni();
    if(ret < 0){
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        //return -1;
    }
    gImgProcParm.fcws.height = gFcwsIniParm.FCWS.height;
    gImgProcParm.fcws.width = gFcwsIniParm.FCWS.width;
    gImgProcParm.fcws.fourcc = gFcwsIniParm.FCWS.fourcc;

    ret = load_TsrIni();
    if(ret < 0){
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        //return -1;
    } else {
        gImgProcParm.tsr.height = gTsrIniParm.TSR.height;
        gImgProcParm.tsr.width = gTsrIniParm.TSR.width;
        gImgProcParm.tsr.fourcc = gTsrIniParm.TSR.fourcc;
    }
        
    ret = load_PdsIni();
    if(ret < 0){
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        //return -1;
    } else {
        gImgProcParm.pds.height = gPdsIniParm.PDS.height;
        gImgProcParm.pds.width = gPdsIniParm.PDS.width;
        gImgProcParm.pds.fourcc = gPdsIniParm.PDS.fourcc;
    }

    ret = load_UFcwsIni();
    if(ret < 0){
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        //return -1;
    } else {
        gImgProcParm.uFcws.height = gUFcwsIniParm.UFCWS.height;
        gImgProcParm.uFcws.width = gUFcwsIniParm.UFCWS.width;
        gImgProcParm.uFcws.fourcc = gUFcwsIniParm.UFCWS.fourcc;
    }


    ret = load_LdwsIni();
    if(ret < 0){
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        //return -1;
    }
    gImgProcParm.ldws.height = gLdwsIniParm.LDWS.height;
    gImgProcParm.ldws.width = gLdwsIniParm.LDWS.width;
    gImgProcParm.ldws.fourcc = gLdwsIniParm.LDWS.fourcc;

    ret = load_DisplayIni();
    if(ret < 0) {
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        return -1;
    }
    gImgProcParm.disp.crop_top = gDispIniParm.LCD.vidTop;
    gImgProcParm.disp.crop_left = gDispIniParm.LCD.vidLeft;
    gImgProcParm.disp.crop_width = gDispIniParm.LCD.vidWidth;
    gImgProcParm.disp.crop_height = gDispIniParm.LCD.vidHeight;
    gImgProcParm.disp.width = gDispIniParm.LCD.width;
    gImgProcParm.disp.height = gDispIniParm.LCD.height;
    gImgProcParm.disp.fourcc = PIX_FMT_RGB565;
    gImgProcParm.disp.rotate = 0;
    gImgProcParm.disp.motion = 0;
    gImgProcParm.disp.fps = 30;
     
    memcpy(&(gVencParm.h264Parm), &(gVidPrepEncParms.vencParm), sizeof(videoParms));
    memcpy(&(gVencParm.jpegParm), &(gVidPrepEncParms.vencParm), sizeof(videoParms));
    gVencParm.jpegParm.fourcc = gCapIniParm.PicCapParm.fourcc;
    gVencParm.jpegParm.codec = gCapIniParm.PicCapParm.codec;//JPEG ?

    ret = load_AudioIni();
    if(ret < 0) {
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        return -1;
    }
    gAudioParms.sampleRate = gAudioIniParm.Audio.sampleRate;
    gAudioParms.channels = gAudioIniParm.Audio.channels;
    gAudioParms.codec = gAudioIniParm.Audio.codec;
    gAudioParms.sampleSize = gAudioIniParm.Audio.sampleSize;
    gAudioParms.bitRate = gAudioIniParm.Audio.bitRate;

    ret = load_RecfileIni();
    if(ret < 0) {
        log_print(_ERROR, "Media Core initParms line %d Error, ret %d\n", __LINE__, ret);
        return -1;
    }
    gDriveRecord = (gRecIniParm.RecFile.drivingRecord == 0) ? false : true;
    gParkRecord = (gRecIniParm.RecFile.parkRecord == 0) ? false : true;
    
    gMuxParms.hasAudio = gRecIniParm.RecFile.hasAudio;
    gMuxParms.hasPicture = 0;
    gMuxParms.hasVideo = 1;
    gMuxParms.hasMetaData = gRecIniParm.RecFile.hasMetaData;
    memcpy(&(gMuxParms.video), &(gVencParm.h264Parm), sizeof(videoParms));
    memcpy(&(gMuxParms.audio), &gAudioParms, sizeof(audioParms));
    return 0;
}


void InitMediaCoreJobs()
{
    log_print(_DEBUG, "Media Core Initial Jobs\n");
    gpVidCaptureJob = new vidCaptureJob(&gVidCapParm);
    //gpVidCaptMsgQueue = new MsgQueue;
    gpAudCaptureJob = new AudioCaptureJob(&gAudioParms); //should create with vidCaptureJob at the same time to ensure pts nearest for capturer
    gpAudCaptureMsgQueue = new MsgQueue;
    
    gpImgProcJob = new ImgProcessJob(&gImgProcParm);
    gpImgProcMsgQueue = new MsgQueue;

    gpVideoPrepEncJob = new VideoPrepEncJob(&gVidPrepEncParms);
    gpVidPrepVencMsgQueue = new MsgQueue;

    gpVidEncJob = new VideoEncodeJob(&gVencParm);
    gpVidEncMsgQueue = new MsgQueue;

    gpRecFileJob = new RecFileJob(&gMuxParms);
    gpRecFileMsgQueue = new MsgQueue;

//!!
    gpOverrideMsgQueue = new MsgQueue;

    gpPlayBackJob = new PlayBackJob();
    gpPlayBackMsgQueue = new MsgQueue;
    //gpReadPacketMsgQueue = new MsgQueue;
    
    gpMotionDetectJob = new MyMotionDetectionTechnology(&gVidCapParm);
    gpMotionDetectMsgQueue = new MsgQueue;
    
}

int initMsgServer()
{
//connectMsgServer:
    log_print(_DEBUG, "MediaCore Try to connect MSG Distributor.\n");
    gpMediaCoreIpc = new IpcClient((char *)CS_PATH_MEDIA);
    if(gpMediaCoreIpc == NULL) {
        log_print(_ERROR, "MediaCore create Ipc Client failed!\n");
        return -1;
    }
    
    gpMediaCoreIpcEvent = new IpcClient((char *)CS_PATH_MEDIA_EVENT);
    if(gpMediaCoreIpcEvent == NULL) {
        log_print(_ERROR, "MediaCore create Ipc Event Client failed!\n");
        return -1;
    }
    
    if(gpMediaCoreIpc->isConnect() == false) {
        if(gpMediaCoreIpc->connect() < 0) {
            log_print(_ERROR, "MediaCore Try to connect MSG Distributor failed.\n");
        }
        else 
            log_print(_DEBUG, "MediaCore connect to server okay!\n");
    }       
    if(gpMediaCoreIpcEvent->isConnect() == false) {
        if(gpMediaCoreIpcEvent->connect() < 0) {
            log_print(_ERROR,"MediaCore Event Try to connect MSG Distributor failed.\n");
        }
        else 
            log_print(_DEBUG, "MediaCore Event connect to server okay!\n");
    }           
    return 0;
}

void setMediaCoreJobsInitStatus()
{
    bool needPauseRec = (gDriveRecord == 0); //read from ini config
    if(needPauseRec){
        //gpVideoPrepEncJob->pause();
        //gpAudCaptureJob->pause();
        //gpRecFileJob->pause();
        //if(gpMotionDetectJob->isRunning())
            //gpMotionDetectJob->pause();
        gMediaStatus = MEDIA_IDLE_MAIN;
    }
    //else gMediaStatus = MEDIA_RECORD;//have set in loadSdcard().

    //bool algStartAlways = false;
    //if(algStartAlways) {
    //  gpImgProcJob->enableFcwsSrc(false);
    //  gpImgProcJob->enableLdwsSrc(false);
    //}
}

void StartMediaCoreJobs()
{
    int status = REC_INIT;
        log_print(_DEBUG, "Media Core Start Jobs\n");
    if(gpAudCaptureJob)
        gpAudCaptureJob->start();
    
    if(gpVidCaptureJob)
        gpVidCaptureJob->start();
        
    if(gpImgProcJob)
        gpImgProcJob->start();

    if(gpVideoPrepEncJob)
        gpVideoPrepEncJob->start();

    if(gpVidEncJob)
        gpVidEncJob->start();
        
    if(gpRecFileJob)
        gpRecFileJob->start();
        
    if(gpPlayBackJob)
        gpPlayBackJob->start();

    if(gParkRecord && gpParking) {
        if(gpMotionDetectJob)
            gpMotionDetectJob->start();
    }
    //sleep(1);
    //gpPlayBackJob->startPlay("./20141126141340.avi");
}

void StopMediaCoreJobs()
{
    log_print(_INFO, "Media Core Stop Jobs\n");

    if(gpRecFileJob)
        gpRecFileJob->stop();
    if(gpAudCaptureJob)
        gpAudCaptureJob->stop();

    if(gpVidEncJob)
        gpVidEncJob->stop();
    if(gpVideoPrepEncJob)
        gpVideoPrepEncJob->stop();

    if(gpImgProcJob)
        gpImgProcJob->stop();
        
    if(gpVidCaptureJob)
        gpVidCaptureJob->stop();
    
    if(gpPlayBackJob)
        gpPlayBackJob->stop();
    
    if(gpMotionDetectJob)
        gpMotionDetectJob->stop();

}

void UnintMediaCoreJobs()
{
    log_print(_INFO, "Media Core Uninit Jobs\n");

    //delete Jobs

    if(gpRecFileJob) {
        delete gpRecFileJob;
        gpRecFileJob = NULL;
    }
    if(gpVidCaptureJob) {
        delete gpVidCaptureJob;
        gpVidCaptureJob = NULL;
    }   
    if(gpVidEncJob) {
        delete gpVidEncJob;
        gpVidEncJob = NULL;
    }
    if(gpVideoPrepEncJob) {
        delete gpVideoPrepEncJob;
        gpVideoPrepEncJob = NULL;
    }
    
    if(gpImgProcJob) {
        delete gpImgProcJob;
        gpImgProcJob = NULL;
    }
    
    if(gpVidCaptureJob) {
        delete gpVidCaptureJob;
        gpVidCaptureJob = NULL;
    }

    if(gpPlayBackJob) {
        delete gpPlayBackJob;
        gpPlayBackJob = NULL;
    }
    
    //delete MSG Queue
    if(gpOverrideMsgQueue) {
        delete gpOverrideMsgQueue;
        gpOverrideMsgQueue = NULL;
    }
    
    if(gpRecFileMsgQueue) {
        delete gpRecFileMsgQueue;
        gpRecFileMsgQueue = NULL;
    }
    if(gpAudCaptureMsgQueue) {
        delete gpAudCaptureMsgQueue;
        gpAudCaptureMsgQueue = NULL;
    }
    
    if(gpVidEncMsgQueue) {
        delete gpVidEncMsgQueue;
        gpVidEncMsgQueue = NULL;
    }
    if(gpVidPrepVencMsgQueue) {
        delete gpVidPrepVencMsgQueue;
        gpVidPrepVencMsgQueue = NULL;
    }
    
    if(gpImgProcMsgQueue) {
        delete gpImgProcMsgQueue;
        gpImgProcMsgQueue = NULL;
    }

    if(gpPlayBackMsgQueue) {
        delete gpPlayBackMsgQueue;
        gpPlayBackMsgQueue = NULL;
    }
    
    if(gpMotionDetectMsgQueue) {
        delete gpMotionDetectMsgQueue;
        gpMotionDetectMsgQueue = NULL;
    }

}


/* function to process abnormal case                                        */
void HandleExcptSig(int signo)
{
    log_print(_ERROR, "Media Core program HandleExcptSig singal %d!\n", signo);
    
    if (SIGINT == signo /*|| SIGTERM == signo || SIGKILL == signo*/)
    {
            log_print(_ERROR, "Media Core program exit abnormally by singal %d!\n", signo);
        gMsgProcessRuning = 0;
        
        StopMediaCoreJobs();
        UnintMediaCoreJobs();
        if(gpMediaCoreIpc) {
            delete gpMediaCoreIpc;
            gpMediaCoreIpc = NULL;
        }       
        exit(0);
        
    }
}

int processMsg(MsgPacket & msg)
{
    //log_print(_DEBUG, "Media Core processMsg Opc 0x%x\n", msg.header.opcode);
    OpcParm_t opcParm;
    //!!
    OpcOverlayParm_t opcOverlayParm;
    OpcRecDurationParm_t opcRecDurationParm;
    OpcPlayFileParm_t opcPlayFileParm;
    OpcDisplayParm opcDispParm;
    int ret = 0;
    bool isNet = false;
    bool ignoreResp = false;
    struct timeval startTime, endTime;
    long useTime;
    switch(msg.header.opcode) {
        case  OPC_REC_ONOFF:
            gettimeofday(&startTime, NULL);
            memcpy(&opcParm, msg.args,sizeof(OpcParm_t));
            log_print(_INFO, "#### OPC_REC_ONOFF\n");
            if(isReadyToRec()) {
                gpRecFileJob->needRecAhead();
                ret = commandRecOnOff(opcParm.value);
            } else {
                commandRecOnOff(3);
            }
            gettimeofday(&endTime, NULL);
            useTime = (endTime.tv_sec - startTime.tv_sec)*1000 + (endTime.tv_usec - startTime.tv_usec)/1000;
            if(useTime > 200)
                log_print(_ERROR, "mediacore processMsg OPC_REC_ONOFF response after %ldms, timeout!\n", useTime);   
            ret = OpcRespond(msg,  ret);
            break;
        case  OPC_REC_EMERGENCY_REC:
            gettimeofday(&startTime, NULL);
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            ret = commandRecEmergencyOnOff(opcParm.value);
            gettimeofday(&endTime, NULL);
            useTime = (endTime.tv_sec - startTime.tv_sec)*1000 + (endTime.tv_usec - startTime.tv_usec)/1000;
            if(useTime > 200)
                log_print(_ERROR, "mediacore processMsg OPC_REC_EMERGENCY_REC response after %ldms, timeout!\n", useTime);    
            ret=OpcRespond(msg,  ret);
            break;
        case  OPC_REC_AUDIO_VOLUME://
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            ret = commandRecAudioVolume(opcParm);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_REC_OVERRIDE:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            ret = commandRecOverride(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_REC_IS_RECORDING:
            ret = commandIsRecing();
            //printf("####commandIsRecing ret %d\n",ret);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_REC_OVERLAY_ONOFF:
            memcpy(&opcOverlayParm, msg.args, sizeof(OpcOverlayParm_t));
            ret = commandRecOverlayOnoff(opcOverlayParm);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_REC_UPDATE_REC_PARMS:
            ret = commandUpdateRecParms();
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_REC_UPDATE_REC_DURATING:
            memcpy(&opcRecDurationParm, msg.args, sizeof(OpcRecDurationParm_t));
            ret = commandUpdateRecDuration(opcRecDurationParm);
            ret = OpcRespond(msg, ret);
            break;
        case OPC_REC_METADATA_ONOFF:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            ret = commandRecMetadataOnoff(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_PLAY:
            memset(&opcPlayFileParm, 0, sizeof(opcPlayFileParm));
            memcpy(&opcPlayFileParm,msg.args,msg.header.len);
            if(msg.header.src == MsgAddr(DO_NET) || msg.header.src == MsgAddr(DO_SENDER) ) {
                printf("### OPC_PLAYBACK_PLAY\n");
                if(gNetChannel == 0) {
                    ret = commandLiveOnOff(1);
                }
                gNetChannel = 1;
                ret = commandPlayBackStart_net(opcPlayFileParm);
            } else {
                ret = commandPlayBackStart(opcPlayFileParm);
            }
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_STOP:
            if(msg.header.src == MsgAddr(DO_NET)) {
                printf("### OPC_PLAYBACK_STOP\n");
                ret = commandPlayBackStop(true);
            } else {
                ret = commandPlayBackStop(false);
            }
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_SEEK:
            if(msg.header.src == MsgAddr(DO_NET) || msg.header.src == MsgAddr(DO_SENDER)) {
                memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
                printf("### OPC_PLAYBACK_SEEK %d\n", opcParm.value);
                ret = commandPlayBackSeek(opcParm, true);
            } else {
                memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
                ret = commandPlayBackSeek(opcParm, false);
            }
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_PAUSE_RESUME:
            if(msg.header.src == MsgAddr(DO_NET)) {
                memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
                printf("### DO_NET  OPC_PLAYBACK_PAUSE_RESUME %d\n", opcParm.value);
                ret = commandPlayBackPauseResume(opcParm, true);
            } else {
                memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
                printf("### OPC_PLAYBACK_PAUSE_RESUME %d\n", opcParm.value);
                ret = commandPlayBackPauseResume(opcParm, false);
            }
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_AUDIO_VOLUME://
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            ret = commandPlayBackAudioVolume(opcParm);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_PLAYBACK_GET_DURATION:
            if(msg.header.src == MsgAddr(DO_NET)) {
                printf("### OPC_PLAYBACK_GET_DURATION\n");
                ret = commandPlayBackGetDuration(true);
            } else {
                ret = commandPlayBackGetDuration(false);
            }
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_NETWORK_TRANSMISSION_CHANNEL:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            printf("### OPC_NETWORK_TRANSMISSION_CHANNEL %d\n", opcParm.value);
            gNetChannel = opcParm.value;
            if(opcParm.value != 1)
                gpPlayBackJob->stopPlay(true);
            ret = commandLiveOnOff(gNetChannel);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_MEDIA_DISPLAY_ONOFF:
            break;
        case  OPC_MEDIA_DISPLAY_IS_DISPLAYING:
            break;
        case  OPC_MEDIA_DISPLAY_SET_EFFECT:
            break;
        case  OPC_MEDIA_VIDEO_IN_SET_EFFECT:
            break;
        case  OPC_MEDIA_CAPTURE_PIC:
            ret = commandCapture();
            ret = OpcRespond(msg, ret);
            log_print(_INFO, "#####current MediaStatus: %d,ret: %d\n", gMediaStatus, ret);
            break;
        case OPC_MEDIA_CAMERA_CALIBRATION: //GUI TODO
            commandRecOnOff(0); 
            gMediaStatus = MEDIA_CAMERA_CALIBRATION;
            ret = OpcRespond(msg, 0);
            printf("#####OPC_MEDIA_CAMERA_CALIBRATION\n");
            break;
        case OPC_MEDIA_DISPLAY_ADJUST:
            memcpy(&opcDispParm, msg.args, sizeof(OpcDisplayParm));
            ret = commandAdjustDisplay(opcDispParm);
            printf("####commandAdjustDisplay ret %d\n", ret);
            ret = OpcRespond(msg, ret);
            break;
        case OPC_MEDIA_SDCARD_FORMAT:
            if(msg.header.src == MsgAddr(DO_NET)) {
                commandRecOnOff(0);
            } else {
                OpcRespond(msg, 1); //begin to format
            }
            ret = commandFormatSdcard();
            
            if(msg.header.src == MsgAddr(DO_NET)) {
                printf("####NotifyEventToNet ret %d\n", ret);
                ret = OpcRespond(msg, ret);
                //NotifyEventToNet(OPC_MEDIA_SDCARD_FORMAT_EVENT, ret);// return the format result
            } else {
                printf("####NotifyEventToGui ret %d\n", ret);
                NotifyEventToGui(OPC_MEDIA_SDCARD_FORMAT_EVENT, ret);
            }
            //ret = OpcRespond(msg, ret);
            break;  
        case OPC_MEDIA_SDCARD_GET_STATUS:
            ret = commandSdcardGetStatus();
            log_print(_INFO, "####commandSdcardGetStatus ret %d\n", ret);
            ret = OpcRespond(msg, ret);
            break;
        case  OPC_MEDIA_UPDATE_ALG_PARMS:
            ret = commandUpdateAlgParms();
            ret = OpcRespond(msg, ret);
            break;
        case OPC_MEDIA_VIDEO_DISPLAY_ONOFF:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            commandVideoDispOnOff(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        case OPC_MEDIA_ALG_IMG_ONOFF: //elimination???
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            printf("######### OPC_MEDIA_ALG_IMG_ONOFF %d\n", opcParm.value);
            commandAlgImgOnOff(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        case OPC_REC_DRIVING_RECORD_ONOFF:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            printf("######### OPC_REC_DRIVING_RECORD_ONOFF %d\n", opcParm.value);
            commandDriveRecordOnOff(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        case OPC_REC_PARKING_RECORD_ONOFF:
            memcpy(&opcParm, msg.args, sizeof(OpcParm_t));
            printf("######### OPC_REC_PARKING_RECORD_ONOFF %d\n", opcParm.value);
            commandParkRecordOnOff(opcParm.value);
            ret = OpcRespond(msg, ret);
            break;
        default:
            log_print(_INFO, "MediaCore Recv invalid opcode: %hhu.\n", msg.header.opcode);
            break;
    }

    return ret;
}

int processMsgEvent(MsgPacket & msg)
{
    //log_print(_DEBUG,"Media Core processMsgEvent Opc 0x%x\n",msg.header.opcode);
    gSensorData_t gSensorData;
    gpsData_t gpsData;
    OpcEvent_t OpcEvent;
    
    int ret = 0;
    bool ignoreResp = false;
    switch(msg.header.opcode) {
        case OPC_LDW_DET_EVT:
            //log_print(_DEBUG,"######### OPC_LDW_DET_EVT\n");
            algInfo_t ldwsInfo;
            memcpy(&ldwsInfo, msg.args, sizeof(algInfo_t));
            if(gpAlgProcess)
                gpAlgProcess->updateLaneInfo(ldwsInfo.lane[LEFT_LANE], ldwsInfo.lane[RIGHT_LANE],ldwsInfo.importantFrameRange,ldwsInfo.vanishLine);
            if(ldwsInfo.lane[RIGHT_LANE].state==LANE_ALARM || ldwsInfo.lane[LEFT_LANE].state==LANE_ALARM)
            {
                log_print(_INFO, "processMsgEvent() OPC_LDW_DET_EVT have detected lane alarm\n");
                gpRecFileJob->recAlgEvent(msg.header.opcode);
            }
            break;
        case OPC_FCW_DET_EVT:
            //log_print(_DEBUG,"######### OPC_FCW_DET_EVT\n");
            algInfo_t fcwsInfo;
            memcpy(&fcwsInfo, msg.args, sizeof(algInfo_t));
            ret = load_TtcIni();
            if(ret < 0){
                    log_print(_ERROR, "Medacore processMsgEvent() load_TtcIni Error\n");
                break;
            }
            if(fcwsInfo.carInfo.ttc < gTtcIniParm.threshold) {
                log_print(_INFO, "OPC_FCW_DET_EVT start Alg Record. ttc: %f, threhold: %f\n", fcwsInfo.carInfo.ttc, gTtcIniParm.threshold);
                gpRecFileJob->recAlgEvent(msg.header.opcode);
            }
        break;
        //below event send from media to other program, no need to process here
        //case  OPC_REC_STORAGE_ERROR_EVENT:
        //  break;
        //case  OPC_PLAYBACK_CUR_POS_EVENT:
            //break;
        //case  OPC_PLAYBACK_END_EVENT:
        //  break;

        //process other program event
        case OPC_GUI_SYS_ONOFF_EVENT:
            break;
        case OPC_GUI_MAIN_INTERFACE_INOUT_EVENT:
            log_print(_INFO, "######### OPC_GUI_MAIN_INTERFACE_INOUT_EVENT\n");
            memcpy(&OpcEvent, msg.args, sizeof(OpcEvent));
            commandMainInterfaceInOut(OpcEvent.value);
            break;
        case OPC_SENSOR_GPS_DATA_EVENT:
            log_print(_DEBUG, "######### OPC_SENSOR_GPS_DATA_EVENT\n");
            memcpy(&gpsData, msg.args, sizeof(gpsData_t));
            commandUpdateGPSinfo(&gpsData);
            break;
        case OPC_SENSOR_GPS_RAW_DATA_EVENT:
            break;
        case OPC_SENSOR_GSENSOR_DATA_EVENT:
            //log_print(_INFO,"######### OPC_SENSOR_GSENSOR_DATA_EVENT\n");
            memcpy(&gSensorData, msg.args, sizeof(gSensorData_t));
            commandUpdateGSensorInfo(&gSensorData);
            break;
        case OPC_CAR_SPEED_EVENT:
            break;
        case OPC_CAR_POWER_OUTAGES_EVENT:
            log_print(_INFO, "######### OPC_CAR_POWER_OUTAGES_EVENT\n");
            commanPowOutages();
            break;
        case OPC_CAR_DRIVING_STATUS_EVENT:  //TODO
            carSignal_t signalInfo;
            memcpy(&signalInfo, msg.args, sizeof(carSignal_t));
            log_print(_INFO, "######### OPC_CAR_DRIVING_STATUS_EVENT %d\n", signalInfo.onoff);
            commandParking(signalInfo.onoff);
            break;
        case OPC_SEND_SEVERAL_PACKETS_EVENT:  //TODO
            log_print(_INFO, "######### OPC_SEND_SEVERAL_PACKETS_EVENT\n");
            //comandSendSeveralPacket();
            break;
        default:
            log_print(_ERROR, "MediaCore Event Recv invalid opcode: %hhu.\n", msg.header.opcode);
            break;
    }

    return ret;
}


int setHighPriority()
{
    struct sched_param param; 
    int maxpri; 
    maxpri = sched_get_priority_max(SCHED_FIFO);
    if(maxpri == -1) { 
        log_print(_ERROR, "sched_get_priority_max() failed"); 
        return -1;
    } 
    param.sched_priority = maxpri; 
    if(sched_setscheduler(getpid(), SCHED_FIFO, &param) == -1) { //设置优先级
        log_print(_ERROR, "sched_setscheduler() failed"); 
        return -1;
    } 
    log_print(_INFO, "###############Set Process Priority to %d###################\n", maxpri);
    return 0;  
}

int gLCDDispEnable = 1;


int  sendInvalidLicenseEvent()
{
    log_print(_DEBUG, "Mediacore OPC_SYS_INVALID_LICENSE\n");
    
    MsgPacket packet;
    OpcEvent_t event;
    event.value = 0;
    packet.header.src = MsgAddr(DO_MEDIA);
    packet.header.dest = MsgAddr(DO_GUI_EVENT);
    packet.header.opcode = OPC_SYS_INVALID_LICENSE;
    packet.header.len = sizeof(OpcEvent_t);
    memcpy(packet.args, &event, sizeof(OpcEvent_t));    
    return gpMediaCoreIpcEvent->sendPacket(&packet);
}

#ifdef VERIFY_LICENSE

void getUnverifyText(char* text,int* text_len)
{
    *text_len = getCpuIdString(text, *text_len);
}

void verify_license()
{
    char sign[256];
    char plaintext[1024];
    int text_len = 1024;
    //printf("\n-----verify_license-----\n");
    readXmlItemContent(LICENSE_PATH, "Signature", sign);
    getUnverifyText(plaintext, &text_len);
    if(verify(sign, plaintext, text_len) != 0) {
        log_print(_ERROR, "\nverify_license cannot pass license verify!\n\n");
        sleep(3);
        int ret = sendInvalidLicenseEvent();
        if(ret < 0)
            log_print(_ERROR, "verify_license sendInvalidLicenseEvent Error!\n");
    }
    log_print(_INFO, "Mediacore verify_license pass license verify.\n");
    //printf("-----verify_license end-----\n\n");
 }
#endif

int main(int argc, char* argv[])
{
    int ret = 0;
    int c;
    int level = 0;
    int maxfd;
    char version[128];
    fd_set fdset;
    struct timeval timeout;
    //int megSockfd = -1;
    //int layer;
    MsgPacket package;
    //MsgPacket wpackage;
    gLCDDispEnable = 1;
    
    sprintf(version, "%s %s", VERSION, __DATE__);
    log_print(_INFO, "#########################################\n");
    log_print(_INFO, "MediaCore Version :%s\n", version);
    log_print(_INFO, "Compile Date :%s\n", __DATE__);
    log_print(_INFO, "#########################################\n");

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)version); 
    Register_debug_signal();

    //setHighPriority();
    while ((c = getopt(argc,argv, "D:d:L:l:pe")) >= 0) {
        switch (c) {
            case 'D':
            case 'd':
              level = atoi(optarg);
              set_log_level(level);
              log_print(_DEBUG, "print_level %d\n", level);
              break;
            case 'l':   // > 0 ,enable to display video, 0 disable
            case 'L':
                gLCDDispEnable = atoi(optarg);
              log_print(_DEBUG, "LCDDispEnable %d\n", gLCDDispEnable);
              break;
            case 'p':
                gPlaybackAlg = 1;
                break;
            case 'e':
                gDebugMode = true;
                break;

            default:
              break;
        }
    }
    
    /* process abnormal case                                                */
    //signal(SIGINT, HandleExcptSig);
    signal(SIGTERM, HandleExcptSig);
    //signal(SIGKILL, HandleExcptSig);
    signal(SIGPIPE, HandleExcptSig);

    gMediaStatus = MEDIA_INITING;
    gMsgProcessRuning = 1;

    ret = initParms();
    if(ret < 0)
        return -1;
    
    ret = initMsgServer();
    if(ret < 0)
        return -1;

    #ifdef VERIFY_LICENSE
    verify_license();
    #else
    log_print(_ERROR, "Mediacore license verify: disabled.\n");
    #endif

    InitMediaCoreJobs();

    gMediaStatus = MEDIA_IDLE_MAIN;
    gpAlgProcess = new Alg(&gImgProcParm);
    
    #ifdef USE_SDCARD
    gpExceptionProc = ExceptionProcessor::GetInstance();
    gpExceptionProc->sdcard->loadSdcard(); //Decide whether to start recording.
    #endif
    
    initAviAmfWriter();

    //setMediaCoreJobsInitStatus();
    
    StartMediaCoreJobs();
    
    log_print(_INFO, "###MediaStatus %d\n", gMediaStatus);
    
    sleep(1);
    
    //commandRecEmergencyOnOff(1);
    
    while( gMsgProcessRuning ) {
        if(gpMediaCoreIpc->isConnect() == false) {
            if(gpMediaCoreIpc->connect() < 0) {
                log_print(_ERROR, "MediaCore Try to connect MSG Distributor failed.\n");
                sleep(1);
                continue;
            } else {
                log_print(_DEBUG, "MediaCore connect to server okay!\n");
            }
        }
        if(gpMediaCoreIpcEvent->isConnect() == false) {
            if(gpMediaCoreIpcEvent->connect() < 0) {
                log_print(_ERROR, "MediaCore  Event Try to connect MSG Distributor failed.\n");
                sleep(1);
                continue;
            } else {
                log_print(_DEBUG, "MediaCore  Event connect to server okay!\n");
            }
        }           
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        FD_ZERO(&fdset);

        if(gpMediaCoreIpc->getClientFd() >= 0) {
            FD_SET(gpMediaCoreIpc->getClientFd(), &fdset);
        }
        if(gpMediaCoreIpcEvent->getClientFd() >= 0) {
            FD_SET(gpMediaCoreIpcEvent->getClientFd(), &fdset);
        }
        #ifdef USE_SDCARD
        int sfd = gpExceptionProc->sdcard->getFd();
        if(sfd >= 0)
            FD_SET(sfd,&fdset);
        #endif
        maxfd = gpMediaCoreIpc->getClientFd();
        #ifdef USE_SDCARD
        maxfd = gpMediaCoreIpc->getClientFd() > sfd ? gpMediaCoreIpc->getClientFd() : sfd;
        #endif
        maxfd = gpMediaCoreIpcEvent->getClientFd() > maxfd ? gpMediaCoreIpcEvent->getClientFd() : maxfd;
        
        ret = select(maxfd+1, &fdset, NULL, NULL, &timeout);
        
        if(ret == -1) {
            log_print(_ERROR, "MediaCore EvetMgr select() failed\n");
        } else if(ret) {    
            if(FD_ISSET(gpMediaCoreIpc->getClientFd(), &fdset)) {
                if(gpMediaCoreIpc->recvPacket(&package) > 0) {
                    //log_print(_DEBUG," MediaCore IPC recvPacket src %x, dest %x, len %d, opcode %x\n",
                        //package.header.src, package.header.dest,package.header.len,package.header.opcode);
                    processMsg(package);
                } else {
                    usleep(10000);  
                    log_print(_ERROR, "MediaCore Recv Packet error\n");
                }
            } 
            if (FD_ISSET(gpMediaCoreIpcEvent->getClientFd(), &fdset)) {
                if(gpMediaCoreIpcEvent->recvPacket(&package) > 0) {
                    //log_print(_DEBUG," MediaCore IPC Event recvPacket src %x, dest %x, len %d, opcode %x\n",
                        //package.header.src, package.header.dest,package.header.len,package.header.opcode);
                    processMsgEvent(package);
                } else {
                    usleep(10000);  
                    log_print(_ERROR, "MediaCore Event Recv Packet error\n");
                }
            } 
            #ifdef USE_SDCARD
            if(FD_ISSET(sfd, &fdset)) {
                gpExceptionProc->sdcard->processKernelMsg();
            }
            #endif
        }
    }
    
    log_print(_ERROR, "MediaCore end\n");
    #ifdef USE_SDCARD
    if(gpExceptionProc){
        gpExceptionProc->sdcard->unloadSdcard();
        gpExceptionProc->releaseInstance();
    }
    #endif
    
    StopMediaCoreJobs();
    UnintMediaCoreJobs();
    uninitAviAmfWriter();
    
    if(gpMediaCoreIpc) {
        delete gpMediaCoreIpc;
        gpMediaCoreIpc = NULL;
    }
    if(gpMediaCoreIpcEvent) {
        delete gpMediaCoreIpcEvent;
        gpMediaCoreIpcEvent = NULL;
    }
    if(gpAlgProcess) {
        delete gpAlgProcess;
        gpAlgProcess = NULL;
    }
    return 0;
}

