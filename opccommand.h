#ifndef _OPC_COMMAND_H_
#define _OPC_COMMAND_H_

#include "ipc/ipcstruct.h"
#include "ipcopcode.h"
int OpcRespond(MsgPacket &msg, int resp);
//!!
int NotifyEventToGui(unsigned char opcode,int value);
int NotifyEventToNet(unsigned char opcode, int value);

//REC
int commandRecOnOff(int onoff);
int commandRecEmergencyOnOff(int onoff);
int commanPowOutages();
int commandParking(int onoff);

//!!
int commandRecOverride(int onoff);
int commandIsRecing();
int commandRecOverlayOnoff(OpcOverlayParm_t);
int commandUpdateRecDuration(OpcRecDurationParm_t);
int commandUpdateRecParms();
int commandRecAudioVolume(OpcParm_t);

int commandRecMetadataOnoff(int onoff);
int commandDriveRecordOnOff(int onoff); //option
int commandParkRecordOnOff(int onoff); //option
int motionDectionOnOff(bool onoff);

//ALG Params
int commandUpdateAlgParms();

//playback
int commandPlayBackStart(OpcPlayFileParm_t);
int commandPlayBackStart_net(OpcPlayFileParm_t);
int commandPlayBackStop(bool isNet);
int commandPlayBackSeek(OpcParm_t, bool isNet);
int commandLiveOnOff(int onOff);
int commandPlayBackPauseResume(OpcParm_t, bool isNet);
int commandPlayBackAudioVolume(OpcParm_t);
unsigned int commandPlayBackGetDuration(bool isNet);

int commandCapture();
int commandAdjustDisplay(OpcDisplayParm);
int commandMainInterfaceInOut(int inOut);

//event
int commandUpdateGPSinfo(gpsData_t*);
int commandUpdateGSensorInfo(gSensorData_t*);

//format
int commandFormatSdcard();

int commandSdcardGetStatus();

int commandVideoDispOnOff(int onOff);
int commandAlgImgOnOff(int onOff);
void comandSendSeveralPacket();

#endif
