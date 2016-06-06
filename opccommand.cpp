#include "opccommand.h"

#include "mediacore.h"
#include "debug/errlog.h"
#include "log/log.h"

#include "camcapture.h"
#include "ffmpeg/avcodec_common.h"
#include "ipc/msgqueue.h"

#include "videocapturejob.h"
#include "imageprocessjob.h"
#include "videoprepencodejob.h"
#include "videoencodejob.h"
#include "recfilejob.h"
#include "playbackjob.h"
#include "motion_detection_technology.h"
#include "audiocapturejob.h"
#include "overridejob.h"

#include "mediaopcode.h"
#include "sensoropcode.h"
#include <time.h>
#include <signal.h>
#include "ipc/ipcclient.h"
#include "parms_dump.h"
#include "alg.h"
#include "AviAMFData.h"

extern vidCaptureJob *gpVidCaptureJob;
extern ImgProcessJob *gpImgProcJob;
extern VideoPrepEncJob *gpVideoPrepEncJob;
extern VideoEncodeJob * gpVidEncJob;
extern RecFileJob *gpRecFileJob;
extern AudioCaptureJob * gpAudCaptureJob;
//!!
extern ExceptionProcessor *gpExceptionProc;
extern PlayBackJob *gpPlayBackJob;
extern MyMotionDetectionTechnology *gpMotionDetectJob;
extern Alg *gpAlgProcess;
extern IpcClient *gpMediaCoreIpc;
extern mediaStatus gMediaStatus;
extern int getCpuTemp();

bool netMonitoring = false;
bool gDriveRecord = false;
bool gParkRecord = false;
bool gpParking = false;
//bool PreEmergencyRecing = false;

int OpcRespond(MsgPacket &msg, int resp)
{
    MsgPacket msgResp;
    OpcResp_t opcResp;
    msgResp.header.src = MsgAddr(DO_MEDIA);
    msgResp.header.dest = msg.header.src;
    msgResp.header.opcode = msg.header.opcode | OPC_RESPONSE_FlAG;
    msgResp.header.len = sizeof(OpcResp_t);
    opcResp.respValue = resp;
    memcpy(msgResp.args, &opcResp, sizeof(OpcResp_t));
    if(gpMediaCoreIpc)
        gpMediaCoreIpc->sendPacket(&msgResp);
    return 0;
}

int NotifyEventToGui(unsigned char opcode, int value)
{
    MsgPacket msgEvent;
    OpcEvent_t opcEvent;
    msgEvent.header.opcode = opcode;
    msgEvent.header.len = sizeof(OpcEvent_t);
    opcEvent.value = value;
    memcpy(msgEvent.args, &opcEvent, sizeof(OpcEvent_t));
    msgEvent.header.src = MsgAddr(DO_MEDIA);
    msgEvent.header.dest = MsgAddr(DO_GUI_EVENT);
    msgEvent.header.len = sizeof(OpcEvent_t);
    if(gpMediaCoreIpc) {
        gpMediaCoreIpc->sendPacket(&msgEvent);
    }
    return 0;
}

int NotifyEventToNet(unsigned char opcode, int value)
{
    MsgPacket msgEvent;
    OpcEvent_t opcEvent;
    msgEvent.header.opcode = opcode;
    msgEvent.header.len = sizeof(OpcEvent_t);
    opcEvent.value = value;
    memcpy(msgEvent.args, &opcEvent, sizeof(OpcEvent_t));
    msgEvent.header.src = MsgAddr(DO_MEDIA);
    msgEvent.header.dest = MsgAddr(DO_NET_EVENT);
    msgEvent.header.len = sizeof(OpcEvent_t);
    if(gpMediaCoreIpc) {
        gpMediaCoreIpc->sendPacket(&msgEvent);
    }
    return 0;
}


int NotifyEventNoArgToGui(unsigned char opcode)
{
    MsgPacket msgEvent;
    msgEvent.header.opcode = opcode;
    msgEvent.header.len = 0;
    msgEvent.header.src = MsgAddr(DO_MEDIA);
    msgEvent.header.dest = MsgAddr(DO_GUI_EVENT);
    if(gpMediaCoreIpc)
        gpMediaCoreIpc->sendPacket(&msgEvent);
    return 0;
}

//TBD
//onoff == 0: Stop coding and recording.
//onoff == 1: Start coding and recording.
//onoff == 3: Start coding, but not recorded.
int commandRecOnOff(int onoff)
{   
    int status;
    log_print(_INFO, "------------commandRecOnOff  onoff %d-------------------\n", onoff);
#ifdef USE_SDCARD
    if(onoff==1) {
        if(gpExceptionProc->sdcard->IsPassCheck() == false) {   
            gpExceptionProc->sdcard->refreshSdcardStorage();
            if(gpExceptionProc->sdcard->IsPassCheck() == false) {
                log_print(_ERROR,"commandRecOnOff sdcard is not in use.\n");
                return -1;
            }
        }
    }
#endif
    if(onoff) { //on
        gpVideoPrepEncJob->resume(); // it will notify gpVidEncJob auto
        gpAudCaptureJob->resume();
    } else { //off 
        if( netMonitoring == false) {
            gpVideoPrepEncJob->pause();
            gpAudCaptureJob->pause();
        }
    }

    if(onoff==1) {
        gpRecFileJob->resume();
    } else {
        gpRecFileJob->pause(); // notify to close rec file quickly, avoid loss more video data
    }

#ifdef USE_SDCARD
    if(onoff==1) {
        gpExceptionProc->sdcard->startOverrideJob();
    } else {
        gpExceptionProc->sdcard->stopOverrideJob();
    }
#endif

    if(onoff == 3) { //TODO 
        NotifyEventToGui(OPC_REC_ONOFF_SUCCESS_EVENT, REC_ALL_OFF);
        return 0;
    }
    
    if(gpMotionDetectJob->isRunning()) {
        if(onoff) {
            gpMotionDetectJob->resume();
        } else {
            gpMotionDetectJob->pause();
        }
    }

    if(onoff) {
        if(gMediaStatus != MEDIA_RECORD) {
            gMediaStatus = MEDIA_RECORD;
            status = REC_REC_ON;
        }else{
            return 0;
        }
    } else {
//      if(gMediaStatus != MEDIA_IDLE)
//          gMediaStatus = MEDIA_IDLE;
        status = REC_ALL_OFF;
    }
    NotifyEventToGui(OPC_REC_ONOFF_SUCCESS_EVENT, status);
    log_print(_INFO, "commandRecOnOff NotifyEventToGui rec status %d\n", status);

    return 0;
}

/*
void endEmergencyRec (int i)
{
    log_print(_INFO,"------------endEmergencyRec-------------------\n");
    if(gpRecFileJob->isEmergnecyRecing())
        commandRecEmergencyOnOff( 0 );//end file
}
*/
/*
void installEmergencyTimer(int delaySec)
{
      struct itimerval tout_val;
      tout_val.it_interval.tv_sec = 0;
      tout_val.it_interval.tv_usec = 0;
      tout_val.it_value.tv_sec = delaySec; // 10 seconds timer 
      tout_val.it_value.tv_usec = 0;
      
      setitimer(ITIMER_REAL, &tout_val,0);
      
      signal(SIGALRM,endEmergencyRec); // set the Alarm signal capture 
      log_print(_INFO,"------------installEmergencyTimer %d-------------------\n",delaySec);
}
*/
/*
int commandRecEmergencyOnOff(int onoff)
{
#ifdef USE_SDCARD
    if(onoff){
        if(gpExceptionProc->sdcard->IsPassCheck() == false)
        {   
            log_print(_INFO,"commandRecEmergencyOnOff sdcard is not in use.\n");
            return -1;
        }
    }
#endif
    //if(gpRecFileJob->isEmergnecyRecing() == true)
    //  return -1;
    int delaySec = 20; // rec emergency file time second
    if( onoff ){
        PreEmergencyRecing = gpRecFileJob->isRecing();
        
        if(PreEmergencyRecing == false) {
            gpVideoPrepEncJob->resume();
            gpAudCaptureJob->resume();
            
        }
        installEmergencyTimer(delaySec);
        gpRecFileJob->recEmergencyOnoff(true);
        gpVidEncJob->GenKeyFrame();
    }else {
        if((PreEmergencyRecing == false )&& (netMonitoring == false)){
            gpVideoPrepEncJob->pause(); 
            gpAudCaptureJob->pause();

        }
        
        gpRecFileJob->recEmergencyOnoff(false);
        NotifyEventNoArgToGui(OPC_REC_EMERGENCY_END_EVENT);

    }
    return 0;
}
*/

int commandRecEmergencyOnOff(int onoff)
{
#ifdef USE_SDCARD
    if(onoff){
        if(gpExceptionProc->sdcard->IsPassCheck() == false)
        {   
            gpExceptionProc->sdcard->refreshSdcardStorage();
            if(gpExceptionProc->sdcard->IsPassCheck() == false){
                log_print(_ERROR, "commandRecEmergencyOnOff sdcard is not in use.\n");
                return -1;
            }
        }
    }
#endif
    if( onoff ){
        gpRecFileJob->recEmcEvent();
    }else{
        log_print(_INFO, "%s receive %d\n", __FUNCTION__, onoff);
    }
    return 0;
}

int commanPowOutages()
{
    gpVidCaptureJob->closeCamera();
    gpRecFileJob->recPowerOutages();
    return 0;
}

//!!
int commandRecOverride(int onoff)
{
#ifdef USE_SDCARD
    gpExceptionProc->sdcard->turnOnOverride((bool)onoff);
#endif
    return 0;
}

int commandIsRecing()
{
    //bool isRecing = (gpRecFileJob->isRecing() || gpRecFileJob->isEmergnecyRecing());
    //printf("#is recing : %d\n",isRecing);
    if(gMediaStatus == MEDIA_RECORD) {
        if(gpRecFileJob->getRecType() == EVENT_EMC)
            return IsEMCrecording;
        else
            return IsRecording;
    } else {
        return IsStoped;
    }
}
int commandRecOverlayOnoff(OpcOverlayParm_t opcOverlayParm)
{
    gpVideoPrepEncJob->setOverlay(opcOverlayParm.timeOnOff);
    gpVideoPrepEncJob->setOverlayGPS(opcOverlayParm.gpsOnOff);
    return 0;
}

int commandRecMetadataOnoff(int onoff)
{
    //log_print(_INFO,"commandRecMetadataOnoff not support yet!\n");
    commandUpdateRecParms();
    return 0;
}

int commandDriveRecordOnOff(int onoff) //option
{
    gDriveRecord = (onoff == 0) ? false : true;
    return 0;
}

int commandParkRecordOnOff(int onoff) //option
{
    gParkRecord = (onoff == 0) ? false : true;
    gpMotionDetectJob->pause();
    motionDectionOnOff(gParkRecord);//todo
    return 0;
}

int motionDectionOnOff(bool onoff)
{
    log_print(_INFO,"motionDectionOnOff %d\n", onoff);
    
    if(onoff) {
        if(gpMotionDetectJob && gpParking) 
            gpMotionDetectJob->start();
    } else {
        if(gpMotionDetectJob->isRunning()) 
            gpMotionDetectJob->exit();
    }
    return 0;
}

int commandParking(int onoff) //the car is parking
{
    int rec;
    gpParking = (onoff > 0) ? false : true;
    motionDectionOnOff(gpParking && gParkRecord);

    if(gpParking) {
        rec = gParkRecord ? 1 : 3;
    } else {
        rec = (gDriveRecord == 1) ? 1 : 3;
    }
    
    if(rec == 1 && gMediaStatus == MEDIA_IDLE_MAIN) {
        log_print(_INFO, "commandParking() start recording\n");
        gpRecFileJob->needRecAhead();
    } else if(rec == 3 && gMediaStatus == MEDIA_RECORD) {
        log_print(_INFO, "commandParking() stop recording\n");
        gMediaStatus = MEDIA_IDLE_MAIN;
    } else {
        return 0;
    }
    commandRecOnOff(rec);
    /*
    if(gRecIniParm.RecFile.motionDetection && gpParking) {
        if(gMediaStatus == MEDIA_IDLE_MAIN) {
            commandRecOnOff(1);
            motionDectionOnOff(1);
        } else if(gMediaStatus == MEDIA_RECORD) {
            motionDectionOnOff(1);
        }
    }
    */
    return 0;
}
    
int commandUpdateRecDuration(OpcRecDurationParm_t opcRecDurationParm)//with one member:int duration(sec)
{
    unsigned int msec = opcRecDurationParm.duration * 1000;
    gpRecFileJob->setMaxFileDuration(msec);
    return 0;
}

int commandUpdateRecParms()
{
    //XXX: parms may be updated by other threads
    //if(DvrSetting()!=0)
    //  return -1;
    load_RecfileIni();
    load_VideoIni();
    load_AudioIni();
    load_CaptureIni();

    videoParms vencParm;
    vencParm.height = gVideoIniParm.RecVideo.height;
    vencParm.width = gVideoIniParm.RecVideo.width;
    vencParm.fourcc = gVideoIniParm.RecVideo.fourcc;//=I420
    vencParm.fps = gVideoIniParm.RecVideo.fps; //for usb camera
    vencParm.codec = gVideoIniParm.RecVideo.codec;//H264
    vencParm.isCBR = gVideoIniParm.RecVideo.isCBR;//
    vencParm.bitRate = gVideoIniParm.RecVideo.bitRate;//3048;//
    vencParm.keyFrameInterval = gVideoIniParm.RecVideo.keyFrameInterval;//

    audioParms aencParm;
    aencParm.sampleRate = gAudioIniParm.Audio.sampleRate;
    aencParm.bitRate = gAudioIniParm.Audio.bitRate;
    aencParm.codec = gAudioIniParm.Audio.codec;
    aencParm.sampleSize = gAudioIniParm.Audio.sampleSize;
    aencParm.channels = gAudioIniParm.Audio.channels;

    vidCapParm capParm;
    capParm.width = gCapIniParm.VidCapParm.width;
    capParm.height = gCapIniParm.VidCapParm.height;
    capParm.fourcc = gCapIniParm.VidCapParm.fourcc; //I422 for usb camera
    capParm.fps = gCapIniParm.VidCapParm.fps;

    gpVideoPrepEncJob->updateConfig(vencParm.width, vencParm.height, capParm.fps, vencParm.fps);
    gpVidEncJob->updateConfig(&vencParm);
    gpVidCaptureJob->updateConfig(&capParm);

    muxParms mux;
    mux.hasAudio = gRecIniParm.RecFile.hasAudio;
    mux.hasMetaData = gRecIniParm.RecFile.hasMetaData;
    memcpy(&mux.video, &vencParm, sizeof(videoParms));
    memcpy(&mux.audio, &aencParm, sizeof(audioParms));

    gpRecFileJob->updateConfig(&mux);
    #ifdef USE_SDCARD
    gpExceptionProc->sdcard->turnOnOverride((gRecIniParm.RecFile.override == 0 ? false : true));
    #endif
    OpcOverlayParm_t opcOverlayParm;
    opcOverlayParm.gpsOnOff = gRecIniParm.RecFile.overlay;
    opcOverlayParm.timeOnOff = gRecIniParm.RecFile.overlay;
    commandRecOverlayOnoff( opcOverlayParm);

    log_print(_INFO,"commandUpdateRecParms.\n");
    return 0;
}

int commandRecAudioVolume(OpcParm_t opcParm)
{
    log_print(_INFO,"commandRecAudioVolume is not implemented yet.\n");
    return -1;
}

////////////////////playback///////////////////

//  1.start to play video normally(choose video from playlist).
//  2.in player,choose next video or previous video,will stop currently playing video
//    (commandPlayBackStop) and start to play another video.
int commandPlayBackStart(OpcPlayFileParm_t opcPlayFileParm)
{ 
    printf("\n-----Start Playback-----\n");
    gpPlayBackJob->startPlay(opcPlayFileParm.fileName);
    log_print(_INFO, "gpPlayBackJob->startPlay fileName %s\n", opcPlayFileParm.fileName);
    return 0;
}

int commandPlayBackStart_net(OpcPlayFileParm_t opcPlayFileParm)
{ 
    printf("\n-----Start Net Playback-----\n");
    gpPlayBackJob->startPlay(opcPlayFileParm.fileName, true);
    log_print(_INFO, "gpPlayBackJob->startPlay fileName %s\n", opcPlayFileParm.fileName);
    return 0;
}


int commandPlayBackStop(bool isNet)
{
    gpPlayBackJob->stopPlay(isNet);
    log_print(_INFO, "gpPlayBackJob->stopPlay %d\n", isNet);
    return 0;
}

int commandPlayBackSeek(OpcParm_t opcParm, bool isNet)
{
    int msec = opcParm.value;
    if(gpPlayBackJob->seek(msec, isNet) != 0)
        return -1;
    return 0;
}

int commandLiveOnOff(int onOff)
{
    if(!onOff) {
        printf("gpVidEncJob->startLiveStreaming()\n");
        return gpVidEncJob->startLiveStreaming();
    } else {
        printf("gpVidEncJob->stopLiveStreaming()\n");
        return gpVidEncJob->stopLiveStreaming();
    }
}
    
int commandPlayBackPauseResume(OpcParm_t opcParm, bool isNet)
{
    printf("commandPlayBackPauseResume() %d\n", opcParm.value);
    int isResume=opcParm.value;
    if(isResume)
        return gpPlayBackJob->resume(isNet);
    else
        return gpPlayBackJob->pause(isNet);
}

int commandPlayBackAudioVolume(OpcParm_t)
{
    log_print(_INFO, "commandRecAudioVolume is not implemented yet.\n");
    return -1;
}

/**
    After calling PlayBackJob::playFile() ,you can get duration of the video.
*/
u32 commandPlayBackGetDuration(bool isNet)
{
    //if(false == gpPlayBackJob->isPlaying())//flag will set true after calling playFile()
        //return -1;
    int duration_msec = gpPlayBackJob->getFileDuration(isNet)*1000;
    printf("### commandPlayBackGetDuration return %d\n", duration_msec);
    return duration_msec;
}

int commandCapture()
{
#ifdef USE_SDCARD
    if(gpExceptionProc->sdcard->IsPassCheck() == false)
    {   
        log_print(_ERROR, "commandCapture sdcard is not in use.\n");
        return -1;
    }
#endif
    if(gpImgProcJob->IsCapturing() == true)
        return -1;
    gpImgProcJob->screenFreeze();
    gpVideoPrepEncJob->captureJPEG();
    return 0;
}

int commandAdjustDisplay(OpcDisplayParm opcDisplayParm)
{
    int ret = gpImgProcJob->adjustDisplay(opcDisplayParm.icrop_width,opcDisplayParm.icrop_height,opcDisplayParm.icrop_top,opcDisplayParm.icrop_left);
    return ret;
}

int commandMainInterfaceInOut(int inOut)
{
    commandVideoDispOnOff(inOut);
    commandAlgImgOnOff(inOut);
    if(inOut) {  
        //log_print(_INFO,"gMediaStatus: MEDIA_IDLE_MAIN.\n");
/*
        if(load_RecfileIni() < 0) {
                log_print(_ERROR,"load_RecfileIni error\n");
            return -1; 
        }
        */
        gMediaStatus = MEDIA_IDLE_MAIN;
        commandRecOnOff(3);
    } else {
        commandRecOnOff(0);
        gMediaStatus = MEDIA_IDLE;
    }
    return 0;
}

int commandUpdateGPSinfo(gpsData_t* gpsData)
{
    if(gpsData == NULL) return -1;
    if(gpAlgProcess)
        gpAlgProcess->updateSpeed(gpsData->speed);
    if(gpsData->mode >= 3) {
        gpVideoPrepEncJob->setLatitude(gpsData->lat);
        gpVideoPrepEncJob->setLongtitude(gpsData->lon);
    } else {
        gpVideoPrepEncJob->setLatitude(0.0);
        gpVideoPrepEncJob->setLongtitude(0.0);
    }
    //bool fileRecing = gpRecFileJob->isRecing();
    
    otherSensorData_t otherSensorData;
    otherSensorData.speed = gpsData->speed;
    otherSensorData.temperature = getCpuTemp();
    
    if(gMediaStatus == MEDIA_RECORD && gRecIniParm.RecFile.hasMetaData) {
        generateGPSBlk(gpsData);
        generateOtherSensorBlk(&otherSensorData);
    }
    return 0;
}

int commandUpdateGSensorInfo(gSensorData_t* gSensorData)
{
    if(gSensorData == NULL)return -1;
    //bool fileRecing = gpRecFileJob->isRecing();
    if(gMediaStatus == MEDIA_RECORD && gRecIniParm.RecFile.hasMetaData)
        generateGSensorBlk(gSensorData);
    
    //gpMotionDetectJob->parkingColisionDetection(gSensorData);
    return 0;
}

int commandFormatSdcard()
{
#ifdef USE_SDCARD
    //if(gpExceptionProc->sdcard->IsPassCheck() == false)
        //return -1;
    if(gpExceptionProc->sdcard->format() == 0)
        return 0;
    else 
#endif
        return -1;
}

int commandSdcardGetStatus()
{
    log_print(_INFO, "-----------commandSdcardGetStatus-----------------\n");
#ifdef USE_SDCARD
    return gpExceptionProc->sdcard->getSdcardStatus();
#endif
    return -1;
}

int commandUpdateAlgParms()
{
    load_FcwsIni();
    load_LdwsIni();
    load_TsrIni();
    load_PdsIni();
    if(gpAlgProcess)
        gpAlgProcess->updateParms();
    return 0;
}

int commandVideoDispOnOff(int onOff)
{
    log_print(_INFO, "------------commandVideoDispOnOff  onOff %d-------------------\n", onOff);
    gpImgProcJob->videoDisplayOnOff( onOff);
    return 0;
}

int commandAlgImgOnOff(int onOff)
{
    log_print(_INFO, "------------commandAlgImgOnOff  onOff %d-------------------\n", onOff);
    if(gpAlgProcess)
        gpAlgProcess->algImgOnOff( onOff);
    return 0;
}

void comandSendSeveralPacket()
{
    gpPlayBackJob->sendSeveralFrame();
}

