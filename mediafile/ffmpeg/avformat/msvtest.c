#include "msv.h"


int WriteRecordPacket(RecordManager_t *pRecordManager,int chn,VENC_STREAM_S *pststream, int isBackup)
{
	 AVPacket pkt = {0};
	 AVFormatContext *pOutFormat =  &pRecordManager->RecFile[isBackup].recordAVFormat;
	 int i;
	 int avCodecCfgLen=0;
	 int unexpect = 0;
	 msv_write_packet(pOutFormat, &pkt); 

