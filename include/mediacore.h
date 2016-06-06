#ifndef _MEDIA_CORE_H_
#define _MEDIA_CORE_H_
#include "ffmpeg/avcodec_common.h"
#include "imageblock.h"
#include "vencblock.h"
#include "mediaparms.h"
#include "types.h"
#include "ipc/msgqueue.h"
#include "msgqueueopcode.h"

#include "version.h"

#define ERROR_LOG_PATH 	"./"
#define CAMERA_CALIBRATION_DUMP_PATH "/tmp/calibrate/"

enum mediaStatus{
	MEDIA_ERROR=0,
	MEDIA_INITING,
	MEDIA_PLAYBACK,
	MEDIA_IDLE,
	MEDIA_IDLE_MAIN,
	MEDIA_RECORD,
	MEDIA_CAMERA_CALIBRATION
};

enum recType{
	EVENT_ERROR=0,
	EVENT_INITING,
	EVENT_NORMAL,
	EVENT_EMC,
	EVENT_ALG,
	EVENT_MOTION
};

typedef struct vidCapParm_
{
	int width;
	int height;
	u32 fourcc;
	int fps;
	
}vidCapParm;

typedef struct vidDispParm_
{
	int width; //input picture
	int height;
	u32 fourcc;
	int fps;

	//output dispzone
	int crop_top;
	int crop_left;
	int crop_width;
	int crop_height;
	int icrop_top;
	int icrop_left;
	int icrop_width;
	int icrop_height;
	int rotate;//h 180, v 180 
	int motion;
	char dev_path[MAX_PATH_NAME_LEN];
}vidDispParm;


typedef struct imgProcParms_
{
	imgParms ldws;
	imgParms fcws;
	imgParms tsr;
	imgParms pds;
	imgParms uFcws;
	vidDispParm disp;
	//....TBD..
}imgProcParms;

typedef struct vidPrepEncParms_
{
	videoParms vencParm;
	//....TBD..
}vidPrepEncParms;


typedef struct vencParms_
{
	videoParms h264Parm;
	videoParms jpegParm;
	//....TBD..
}vencParms;



enum {
	FILE_AVI=0,
	FILE_MP4,
	FILE_MOV,
	//TBD
};

typedef struct fbuffer_info_
{	
	u32 addr[10];
	u32 length;
	u32 num;
}fbuffer_info;

#endif

