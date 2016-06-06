
#include <unistd.h>
#include <stdio.h>
#include <string.h>

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

#include "flv.h"
#include "print/print.h"
#include "math.h"
#include "dvripcpacket.h"

typedef signed char  int8_t;
typedef signed short int16_t;
typedef signed int   int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef signed long long   int64_t;
typedef unsigned long long uint64_t;



unsigned char* putbyte(unsigned char *s, unsigned int b)
{
	*s++ = (unsigned char)b;
	return s;
}

unsigned char* putle32(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val);
    s= putbyte(s, val >> 8);
    s= putbyte(s, val >> 16);
    s= putbyte(s, val >> 24);
    return s;
}

unsigned char* putbe32(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 24);
    s= putbyte(s, val >> 16);
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}

unsigned char* putle16(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val);
    s= putbyte(s, val >> 8);
    return s;
}


unsigned char* putbe16(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}


unsigned char* putle24(unsigned char *s, unsigned int val)
{
    s= putle16(s, val & 0xffff);
    s= putbyte(s, val >> 16);
    return s;
}


unsigned char* putbe24(unsigned char *s, unsigned int val)
{
    s= putbe16(s, val >> 8);
    s= putbyte(s, val);
    return s;
}


unsigned char* puttag(unsigned char *s, const char *tag)
{
    while (*tag) {
        s = putbyte(s, *tag++);
    }
    return s;
}

unsigned char* putamf_bool(unsigned char* ptr, const int bVal)
{
	*ptr = 1;
	*(ptr+1) = bVal ? 1 : 0;
	return ptr + 2;
}

unsigned char* putamf_string(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    *ptr = len >> 8;
    *(ptr+1) = len;
    memcpy(ptr+2, str, len);

    return ptr + len + 2;
}

#define AMF_DATA_TYPE_STRING 0x02

unsigned char* putamf_string_content(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    ptr = putbyte(ptr, AMF_DATA_TYPE_STRING);
    ptr = putbe16(ptr, len);
    memcpy(ptr, str, len);
	return  ptr+len;
}


unsigned char* putbe64(unsigned char* ptr, uint64_t val)
{
	ptr =putbe32(ptr, (uint32_t)(val >> 32));
	ptr =putbe32(ptr, (uint32_t)(val & 0xffffffff));
	return ptr;
}

static int64_t dbl2int(double d){
    int e;
    if     ( !d) return 0;
    else if(d-d) return 0x7FF0000000000000LL + ((int64_t)(d<0)<<63) + (d!=d);
    d= frexp(d, &e);
    return (int64_t)(d<0)<<63 | (e+1022LL)<<52 | (int64_t)((fabs(d)-0.5)*(1LL<<53));
}

unsigned char* putamf_double(unsigned char* ptr, double d)
{
    ptr = putbyte(ptr, 0);
    ptr = putbe64(ptr, dbl2int(d));
	return ptr;
}

/*
unsigned char* putamf_double(unsigned char* ptr, double dVal)
{
    *ptr = 0;
    unsigned char *ci, *co;
    ci = (unsigned char *) &dVal;
    co = ptr+1;
	
    co[0] = ci[7];
    co[1] = ci[6];
    co[2] = ci[5];
    co[3] = ci[4];
    co[4] = ci[3];
    co[5] = ci[2];
    co[6] = ci[1];
    co[7] = ci[0];


    return ptr + 9;
}
*/



//int flv_filesize_offset = 0, flv_duration_offset = 0;
//int64_t flv_duration_seconds = 0;
//int flvConfigRecord = 0;




int FillFLVHeader(FILE * fp,flvHeadInfo_t *pHeadInfo,flvAvInfo_t *pAvInfo)
{
    unsigned char buffer[1024];
    unsigned char *pb = buffer;
    int metadata_size_pos = 0, duration_offset = 0;
    int filesize_offset = 0,data_size = 0;
    int headerTagSize = 0;
    //int audBitRate,audSampleRate, vidFrameRate, vidBitRate, width, height;

	if(fp == NULL)
		return -1;

	pHeadInfo->filesize_offset = 0;
	pHeadInfo->duration_offset = 0;
	pHeadInfo->duration_seconds = 0;
	pHeadInfo->configHead = 0;

    memset(buffer, 0 , 1024);

    pb = puttag(pb,"FLV");
    pb = putbyte(pb,1);
    if(pAvInfo->audiocodecid >= 0){
    	pb = putbyte(pb, 5);
    } else {
    	pb = putbyte(pb, 1);//video only
    }
	
    pb = putbe32(pb,9);
    pb = putbe32(pb,0);  //first tag size

    headerTagSize = (int)(pb-(unsigned char *)buffer);
   //headerTagSize = 13;
   
	fseek(fp, 0, SEEK_SET);
	fwrite(buffer, 1, headerTagSize, fp);

    memset(buffer, 0 , (int)(pb-(unsigned char *)buffer));
    pb = buffer;
	
    /* write meta_tag */
    pb = putbyte(pb, 18);         // tag type META
    metadata_size_pos= (int)(pb-(unsigned char *)buffer);

    pb = putbe24(pb, 0);          // size of data part (sum of all parts below)
    pb = putbe24(pb, 0);          // time stamp
    pb = putbe32(pb, 0);          // reserved

    /* now data of data_size size */

    /* first event name as a string */
    pb = putbyte(pb, 0x02);//AMF_DATA_TYPE_STRING
    pb = putamf_string(pb, "onMetaData"); // 12 bytes

    /* mixed array (hash) with size and string/type/data tuples */
    pb = putbyte(pb, 0x08); //AMF_DATA_TYPE_MIXEDARRAY
    pb = putbe32(pb, 12); // +2 for duration and file size

    pb = putamf_string(pb, "duration");
    duration_offset= (int)(pb-(unsigned char *)buffer);
	pHeadInfo->duration_offset = duration_offset +headerTagSize;
    pb =putamf_double(pb, 0); // delayed write

   // if(video_enc){
        pb =putamf_string(pb, "width");
        //pb =putamf_double(pb, 352.00);
        pb =putamf_double(pb, (double)pAvInfo->width);
print_level(WARN,"%s width %f\n",__FUNCTION__,(double)pAvInfo->width);
        pb =putamf_string(pb, "height");
        //pb =putamf_double(pb, 288.00);
        pb =putamf_double(pb, (double)pAvInfo->height);
print_level(WARN,"%s height %f\n",__FUNCTION__,(double)pAvInfo->height);

        pb =putamf_string(pb, "videodatarate");
        //pb =putamf_double(pb, 600.00);
        pb =putamf_double(pb, (double)pAvInfo->vidBitRate);
print_level(WARN,"%s videodatarate %f\n",__FUNCTION__,(double)pAvInfo->vidBitRate);

        pb =putamf_string(pb, "framerate");
        //pb =putamf_double(pb, 25.00);
        pb =putamf_double(pb,(double)pAvInfo->vidFrameRate);
print_level(WARN,"%s framerate %f\n",__FUNCTION__,(double)pAvInfo->vidFrameRate);

        pb =putamf_string(pb, "videocodecid");
        pb =putamf_double(pb, 7.00); //H264
  //  }

    if(pAvInfo->audiocodecid >= 0){
        pb =putamf_string(pb, "audiodatarate");
        //pb =putamf_double(pb, 64);
        pb =putamf_double(pb, (double)pAvInfo->audBitRate);
print_level(WARN,"%s audiodatarate %f\n",__FUNCTION__,(double)pAvInfo->audBitRate);

        pb =putamf_string(pb, "audiosamplerate");
        //pb =putamf_double(pb, 44100.00);
        pb =putamf_double(pb, (double)pAvInfo->audSampleRate);
print_level(WARN,"%s audSampleRate %f\n",__FUNCTION__,(double)pAvInfo->audSampleRate);

        pb =putamf_string(pb, "audiosamplesize");
        pb =putamf_double(pb, 16);

        pb =putamf_string(pb, "mono");
        pb =putamf_bool(pb,  1);

        pb =putamf_string(pb, "audiocodecid");
        //pb =putamf_double(pb, 10.00); //AAC
        pb =putamf_double(pb, (double)pAvInfo->audiocodecid); 
print_level(WARN,"%s audiocodecid %f\n",__FUNCTION__,(double)pAvInfo->audiocodecid);
    }

    pb =putamf_string(pb, "filesize");
    filesize_offset= (int)(pb-(unsigned char *)buffer);
	pHeadInfo->filesize_offset = filesize_offset + headerTagSize;
    pb =putamf_double(pb, 0); // delayed write

    pb =putamf_string(pb, "");
    pb = putbyte(pb, 0x09);//AMF_END_OF_OBJECT

    /* write total size of tag */
    //data_size= (int)(pb -buffer) - metadata_size_pos - 10;
    data_size= (int)(pb-(unsigned char *)buffer) -11;
    //url_fseek(pb, metadata_size_pos, SEEK_SET);
    putbe24(buffer+metadata_size_pos, data_size);
    //url_fseek(pb, data_size + 10 - 3, SEEK_CUR);

    pb = putbe32(pb, data_size + 11); // Pre Tag Size

	fwrite(buffer, 1, (int)(pb-(unsigned char *)buffer), fp);

    struct timeval tv;
    gettimeofday(&tv,NULL);
    pHeadInfo->duration_seconds = tv.tv_sec;

    return 0;

}

 int FillFlvTrailer(FILE * fp,flvHeadInfo_t *pHeadInfo,flvAvInfo_t *pAvInfo)
{
    int64_t file_size;

    unsigned char buffer[16];
    unsigned char *pb = buffer;
    memset(pb, 0 , 16);

    if( fp == NULL)
		return -1;
	
    file_size = ftell(fp);

    struct timeval tv;
    gettimeofday(&tv,NULL);
    pHeadInfo->duration_seconds = tv.tv_sec - pHeadInfo->duration_seconds ;

print_level(INFO,"%s duration_offset %d\n",__FUNCTION__,pHeadInfo->duration_offset);
print_level(INFO,"%s duration_seconds %f\n",__FUNCTION__,(double)pHeadInfo->duration_seconds);
print_level(INFO,"%s filesize_offset %d\n",__FUNCTION__,pHeadInfo->filesize_offset);
print_level(INFO,"%s file_size %f\n",__FUNCTION__,(double)file_size);
//return 0;	
    /* update informations */
    fseek(fp, pHeadInfo->duration_offset, SEEK_SET);
    pb = putamf_double(pb, (double)pHeadInfo->duration_seconds);
	fwrite(buffer, 1, (int)(pb-(unsigned char *)buffer), fp);

    pb = buffer;
    fseek(fp, pHeadInfo->filesize_offset, SEEK_SET);
     pb = putamf_double(pb, (double)file_size);

	fwrite(buffer, 1, (int)(pb-(unsigned char *)buffer), fp);

    fseek(fp, file_size, SEEK_SET);
	//fflush(fp);
    return 0;
}


 int FillFlvAudioPacket(FILE * fp, flvPkt_t *pFlvpkt,flvAvInfo_t* pAvInfo)
 {
 		char buf[16];
		char PreTagSize[4];
		//ST_AUDIO_BUF *pstAudioBuffer = (ST_AUDIO_BUF *)Buffer;	
		char *audioBuf =pFlvpkt->buf;
		uint32_t totalnallen = pFlvpkt->len;

		int writeNum;
		int offset = 0;
		unsigned int dts, pts = pFlvpkt->pts;
		/* F4V tag header */
		if(fp == NULL )
		   return -1;
		
		/* stream type */
		buf[0] = 0x08;
//print_level(INFO,"%s\n",__FUNCTION__);
		/* media size */
		uint32_t mediasize = totalnallen + 1;//+1; /* unDataLen + 0xaf + 0x1*/
		buf[1] = mediasize >> 16; 
		buf[2] = mediasize >> 8;
		buf[3] = mediasize;

		/* dts */
		dts = pts/1000;// + 0x90;
		buf[4] = (dts) >> 16; 
		buf[5] = (dts) >> 8;
		buf[6] = (dts);
		buf[7] = ((dts) >> 24) & 0x7F; 

    //cprintf("audio dts->%u\n", dts);

		/* reserve  streamId*/ 
		buf[8] = 0;
		buf[9] = 0;
		buf[10] = 0; 

		/* F4V tag mediadatas */
		/* Media Flag */
		switch(pAvInfo->audiocodecid){
			case AUDIO_CODECE_ID_ADPCM:
				buf[11] = 0x16;//adpcm
				break;
			case AUDIO_CODECE_ID_LPCM:
				buf[11] = 0x36;//LPCM
				break;
			case AUDIO_CODECE_ID_G711MU:
				buf[11] = 0x86;//g711u
				break;
			case AUDIO_CODECE_ID_AAC:
				buf[11] = 0xaf;//aac
				break;
			default:
				print_level(ERROR,"%s audiocodecid not support!\n",__FUNCTION__);	
				return -1;
				break;
				
		//buf[11] = 0xaf;//aac
		//buf[11] = 0x16;//adpcm
		//buf[11] = 0x86;//g711u
		//buf[11] = 0x36;//LPCM
		}

		/* AAC sequence */
		//buf[12] = 0x1;

		/* F4V prev tag length*/
		/* prev tag size */
		PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
		PreTagSize[1] = (mediasize+11) >> 16;
		PreTagSize[2] = (mediasize+11) >> 8;
		PreTagSize[3] =  mediasize+11;

	fwrite(buf, 1, 13 -1, fp);
	
	do{
		writeNum = fwrite(audioBuf +offset, 1, totalnallen, fp);
		if(writeNum > 0){
			totalnallen -=writeNum;
			offset +=writeNum;
		}else {
		    printf("%s %s line %d, write FLV Buffer  Failed \n",__FILE__,__FUNCTION__, __LINE__);
			return -1;
		}
		
	} while(writeNum >= 0 && totalnallen > 0 );
	
	fwrite(PreTagSize, 1, 4, fp);
 
	return 0;
 }

int FillFlvDataPacket(FILE * fp, flvPkt_t *pFlvpkt)
{
	 char buffer[128];
	 char PreTagSize[4];
	 char *dataBuf =pFlvpkt->buf;
	 int totalnallen = pFlvpkt->len;
	 int writeNum;
	 int offset = 0;
	 unsigned int dts, pts = pFlvpkt->pts;
	 unsigned char *pb = NULL;
	
	 if(fp == NULL || totalnallen < 0 ){
		 printf("FillFlvDataPacket fp == NULL, len %d\n",totalnallen);
	 	return -1;
	 }

	 memset(buffer, 0 , 128);
	 pb = buffer;
	 
	 /* write meta_tag */
	 pb = putbyte(pb, 18);		   // tag type META
 
	 pb = putbe24(pb, totalnallen);		   // size of data part (sum of all parts below)
	 pb = putbe24(pb, 0);		   // time stamp
	 pb = putbe32(pb, 0);		   // reserved
 	 if(totalnallen> 240){
	 	printf("FillFlvDataPacket size if too long %d\n",totalnallen);
	 	return -1;
 	 }
	 memcpy(pb,pFlvpkt->buf,totalnallen); 
	 pb +=totalnallen;

	 fwrite(buffer, 1, 11+totalnallen , fp);
	 
	 fwrite(&totalnallen, 1, 4, fp);

 return 0;
}



 int FillFlvAudioConfigPacket(FILE * fp, flvAvInfo_t *pAvInfo)
 {
 		char buf[16];
		char PreTagSize[4];	
		uint32_t totalnallen = 2;
		unsigned char freq = 0, objectType = 2, chans= 1;  //AAC audio specific config
		int audSampleRate= pAvInfo->audSampleRate;
		//GetAVConfig(NULL , &audSampleRate, NULL, NULL, NULL, NULL);
		unsigned int dts = 0;
		//int writeNum;
		/* F4V tag header */
		/* stream type */
		if(fp == NULL )
		   return -1;
		
		buf[0] = 0x08;

		/* media size */
		uint32_t mediasize = totalnallen + 2; /* unDataLen + 0xaf + 0x1*/
		buf[1] = mediasize >> 16; 
		buf[2] = mediasize >> 8;
		buf[3] = mediasize;

		dts /= 1000;
		
		/* dts */
		buf[4] = (dts) >> 16; 
		buf[5] = (dts) >> 8;
		buf[6] = (dts);
		buf[7] = ((dts) >> 24) & 0x7F; 

		/* reserve  streamId*/ 
		buf[8] = 0;
		buf[9] = 0;
		buf[10] = 0; 

		/* F4V tag mediadatas */
		/* Media Flag */
		buf[11] = 0xaf;

		/* AAC sequence */
		buf[12] = 0x0;

		switch(audSampleRate) {
				case 44100:
					freq = 4;
					break;
				case 48000:
					freq = 3;
					break;
				case 32000:
					freq = 5;
					break;
				case 24000:
					freq = 6;
					break;
				case 16000:
					freq = 8;
					break;
				case 8000:
					freq =  11;
					break;
				default:
					freq = 4;
		}
					
		
		//buf[13] = 0x12;
		//buf[14] = 0x10;
		buf[13] = (objectType <<3) | (freq >> 1);
		buf[14] = ((freq & 0x01) << 7) | (chans << 3);

		/* F4V prev tag length*/
		/* prev tag size */
		PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
		PreTagSize[1] = (mediasize+11) >> 16;
		PreTagSize[2] = (mediasize+11) >> 8;
		PreTagSize[3] =  mediasize+11;

	fwrite(buf, 1, 15, fp);
	fwrite(PreTagSize, 1, 4, fp);
 
	return 0;
 }


//#define FLVAVCDATA 4

 int FillFlvVideoPacket(FILE * fp, flvHeadInfo_t *pHeadInfo, flvPkt_t *pFlvpkt,flvAVCConf_t *pFlvAVCConf,flvAvInfo_t* pAvInfo)
 {
    char pucaOutRtmpBuffer[32];
    char PreTagSize[4];
    int writeNum = 0;
	//ST_VIDEO_BUF *pstVideoBuffer = (ST_VIDEO_BUF *)Buffer;
    int i, j, ucaData_offset = 0, sps_offset = 0, sps_len= 0, pps_offset= 0, ppslen= 0;

//printf("%s line %d\n",__FUNCTION__,__LINE__);

	if(fp == NULL )
	   return -1;

//flvConfigRecord =1;
    if(pHeadInfo->configHead == 0) {
		if(pFlvAVCConf == NULL){
			printf("%s %d need avc config data to write header\n",__FUNCTION__,__LINE__);
			return -1;
		}
#if 0
printf("%s line %d\n",__FUNCTION__,__LINE__);
for(i = 0; i < pFlvAVCConf->len; i++) {
	printf("%x ",*(pFlvAVCConf->buf+i));
}
printf("\n");
#endif

//printf("%s line %d\n",__FUNCTION__,__LINE__);

#if 1
	if(pFlvAVCConf->len > 0){	
		for(i = 0; i < pFlvAVCConf->len -4; i++) {
	 		if(pFlvAVCConf->buf[i] == 0 && pFlvAVCConf->buf[i +1] == 0 && pFlvAVCConf->buf[i+2] == 0 && pFlvAVCConf->buf[i +3] == 1) {
				ucaData_offset = i +4;
				//cprintf("pstVideoBuffer->ucaData[ucaData_offset] & 0x0F = %x\n",pstVideoBuffer->ucaData[ucaData_offset] & 0x0F);
				if( (pFlvAVCConf->buf[ucaData_offset] & 0x0F) == 7) {
					sps_offset = ucaData_offset;
					//cprintf("sps_offset %d\n",sps_offset);

					for(j = ucaData_offset; j < pFlvAVCConf->len  - ucaData_offset -4 ; j++) {
	 					if(pFlvAVCConf->buf[j] == 0 && pFlvAVCConf->buf[j +1] == 0 && pFlvAVCConf->buf[j+2] == 0 && pFlvAVCConf->buf[j +3] == 1 ) {
							pps_offset = j +4;
							sps_len = pps_offset -4 - sps_offset;
							ppslen =4;
							//ucaData_offset = pps_offset + ppslen + 4;
							ucaData_offset = 0;
							break;
							//cprintf("sps_len %d,pps_offset %d\n",sps_len,pps_offset);
	 					}
					}
					break;
				}
				else {
					break;
				}
	 		}
		}
	}else {
		print_level(DEBUG,"%s line %d, pFlvAVCConf->len %d\n",__FUNCTION__,__LINE__,pFlvAVCConf->len);
	}
	
#endif
   }
	
//printf("%s line %d\n",__FUNCTION__,__LINE__);
	 
   if(sps_offset > 0) {
	char configHeader[64];
	int configHeaderLen = 0;

	pHeadInfo->configHead = 1;

/*
configurationVersion = 01
AVCProfileIndication = 4D
profile_compatibility = 40
AVCLevelIndication = 15
lengthSizeMinusOne = FF <- 非常重要，是 H.264 视频中 NALU 的长度，计算方法是 1 + (lengthSizeMinusOne & 3)
numOfSequenceParameterSets = E1 <- SPS 的个数，计算方法是 numOfSequenceParameterSets & 0x1F
sequenceParameterSetLength = 00 0A <- SPS 的长度
sequenceParameterSetNALUnits = 67 4D 40 15 96 53 01 00 4A 20 <- SPS
numOfPictureParameterSets = 01 <- PPS 的个数
pictureParameterSetLength = 00 05 <- PPS 的长度
pictureParameterSetNALUnits = 68 E9 23 88 00 <- PPS

*/
//	printf("%s line %d\n",__FUNCTION__,__LINE__);
	
	configHeader[0] = 0;
	configHeader[1] = 0x01;//onfigurationVersion 
	
	configHeader[2] = *(pFlvAVCConf->buf +sps_offset +1);//AVCProfileIndication 
	configHeader[3] = *(pFlvAVCConf->buf +sps_offset +2);//profile_compatibility 
	configHeader[4] = *(pFlvAVCConf->buf +sps_offset +3);//AVCLevelIndication 
	
	configHeader[5] = 0xFF;//lengthSizeMinusOne 
	configHeader[6] = 0xE1;//numOfSequenceParameterSets 
	configHeader[7] = sps_len >>8 ;//sequenceParameterSetLength 
	configHeader[8] = sps_len;//sequenceParameterSetLength
	configHeader[9] = 0;//sequenceParameterSetNALUnits 

 	memcpy(&configHeader[9], pFlvAVCConf->buf +sps_offset,sps_len);
	configHeader[9+ sps_len] = 1;//numOfPictureParameterSets
	configHeader[9+ sps_len +1] = ppslen >>8 ;//sequenceParameterSetLength 
	configHeader[9+ sps_len +2] = ppslen;//sequenceParameterSetLength

	memcpy(&configHeader[9 + sps_len +3], pFlvAVCConf->buf +pps_offset ,ppslen);
	configHeaderLen = 9 + sps_len +3 +ppslen;
//printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* F4V tag header */
    /* stream type */
    pucaOutRtmpBuffer[0] = 0x09;
    uint32_t mediasize = configHeaderLen + 4; /* 1byte flag + 1byte mediatype + 3bytes cts + 4bytes totalnallen */
    pucaOutRtmpBuffer[1] = mediasize >> 16; 
    pucaOutRtmpBuffer[2] = mediasize >> 8;
    pucaOutRtmpBuffer[3] = mediasize;

    /* dts */
    unsigned int cts = 0;//pstVideoBuffer->unPTS - pstVideoBuffer->unDTS; 
    unsigned int dts = 0;//pstVideoBuffer->unDTS; 

    cts /= 1000;
    dts /= 1000;

    /* add 500ms to correct the AV out of Sync from QL201B */
    //cts += 500;
    //printf("Aikoc dts->%u, cts->%u\n", dts, cts);

    pucaOutRtmpBuffer[4] = (dts >> 16) & 0xFF; 
    pucaOutRtmpBuffer[5] = (dts >> 8) & 0xFF;
    pucaOutRtmpBuffer[6] = dts & 0xFF;
    pucaOutRtmpBuffer[7] = (dts >> 24) & 0x7F; 

    /* reserve streamId*/ 
    pucaOutRtmpBuffer[8] = 0;
    pucaOutRtmpBuffer[9] = 0;
    pucaOutRtmpBuffer[10] = 0;

    /* F4V tag mediadatas */
    /* Media Flag */
    	pucaOutRtmpBuffer[11] = 0x17;

    /* NALU */
    pucaOutRtmpBuffer[12] = 0x00;

    pucaOutRtmpBuffer[13] = 0;
    pucaOutRtmpBuffer[14] = 0;

		writeNum = fwrite(pucaOutRtmpBuffer, 1, 15, fp);
//printf("%s line %d\n",__FUNCTION__,__LINE__);
    /* copy the nal datas */
	do{
		writeNum = fwrite(configHeader, 1, configHeaderLen, fp);
		if(writeNum > 0){
			configHeaderLen -=writeNum;
		} else {
		      	printf("%s %s line %d, write FLV Buffer  Failed \n",__FILE__,__FUNCTION__, __LINE__);
				return Err_DrvHddError;
				//return -1;
		}
	} while(writeNum >= 0 && configHeaderLen > 0 );
//printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* F4V prev tag length*/
    /* prev tag length */
    PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
    PreTagSize[1] = (mediasize+11) >> 16;
    PreTagSize[2] = (mediasize+11) >> 8;
    PreTagSize[3] =  mediasize+11;

	fwrite(PreTagSize, 1, 4, fp);

//FILL AUDIO Config
//   printf("%s line %d\n",__FUNCTION__,__LINE__);
//   printf("%s line %d\n",__FUNCTION__,__LINE__);

   if(pAvInfo->audiocodecid >= 0){
	  printf("%s line %d\n",__FUNCTION__,__LINE__);
   	  //FillFlvAudioConfigPacket(fp,dts);
   	}

   }

//	printf("%s line %d\n",__FUNCTION__,__LINE__);
	
    /* F4V tag header */
    /* stream type */
    pucaOutRtmpBuffer[0] = 0x09;

    /* media size */
    uint32_t totalnallen = pFlvpkt->len;// + 4 ; /* 4bytes prefix */
   //uint32_t totalnallen = pstVideoBuffer->unDataLen;// + 4 ; /* 4bytes prefix */
#ifndef 	FLVAVCDATA
    uint32_t mediasize = totalnallen + 9; /* 1byte flag + 1byte mediatype + 3bytes cts + 4bytes totalnallen */
#else
    uint32_t mediasize = totalnallen + 4; /* 1byte flag + 1byte mediatype + 3bytes cts + 4bytes totalnallen */

#endif
    pucaOutRtmpBuffer[1] = mediasize >> 16; 
    pucaOutRtmpBuffer[2] = mediasize >> 8;
    pucaOutRtmpBuffer[3] = mediasize;


    /* dts */
    unsigned int cts = 0;//pstVideoBuffer->unPTS - pstVideoBuffer->unDTS; 
    unsigned int dts = pFlvpkt->pts; 

    cts /= 1000;
    dts /= 1000;

    /* add 500ms to correct the AV out of Sync from QL201B */
    //cts += 500;
    //cprintf("video dts->%u, cts->%u\n", dts, cts);

    pucaOutRtmpBuffer[4] = (dts >> 16) & 0xFF; 
    pucaOutRtmpBuffer[5] = (dts >> 8) & 0xFF;
    pucaOutRtmpBuffer[6] = dts & 0xFF;
    pucaOutRtmpBuffer[7] = (dts >> 24) & 0x7F; 

    /* reserve streamId*/ 
    pucaOutRtmpBuffer[8] = 0;
    pucaOutRtmpBuffer[9] = 0;
    pucaOutRtmpBuffer[10] = 0;

    /* F4V tag mediadatas */
    /* Media Flag */
    if (pFlvpkt->keyFrame)
    {
    	pucaOutRtmpBuffer[11] = 0x17;
    }
    else
    {
    	pucaOutRtmpBuffer[11] = 0x27;
    }
	
    /* NALU */
    pucaOutRtmpBuffer[12] = 0x01;

#ifndef 	FLVAVCDATA
    /* cts */
    pucaOutRtmpBuffer[13] = cts >> 16;
    pucaOutRtmpBuffer[14] = cts >> 8;
    pucaOutRtmpBuffer[15] = cts;

    /* NALU datas */ 
    /* fill total nal len including prefix and naltype */
    pucaOutRtmpBuffer[16] = totalnallen >> 24;
    pucaOutRtmpBuffer[17] = totalnallen >> 16;
    pucaOutRtmpBuffer[18] = totalnallen >> 8;
    pucaOutRtmpBuffer[19] = totalnallen;
#else
    pucaOutRtmpBuffer[13] = 0;
    pucaOutRtmpBuffer[14] = 0;
#endif
//printf("%s line %d\n",__FUNCTION__,__LINE__);

#ifndef 	FLVAVCDATA
		writeNum = fwrite(pucaOutRtmpBuffer, 1, 20, fp);
#else
		writeNum = fwrite(pucaOutRtmpBuffer, 1, 15, fp);
#endif
//	printf("%s line %d\n",__FUNCTION__,__LINE__);
    /* copy the nal datas */
	do{
		//writeNum = fwrite(pstVideoBuffer->ucaData, 1, totalnallen, GtSaveFileP);
		writeNum = fwrite(pFlvpkt->buf, 1, totalnallen, fp);
		if(writeNum > 0){
			totalnallen -=writeNum;
		} else {
		      	printf("%s %s line %d, write FLV Buffer  Failed \n",__FILE__,__FUNCTION__, __LINE__);
				return Err_DrvHddError;
				//return -1;
		}
	} while(writeNum >= 0 && totalnallen > 0 );
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* F4V prev tag length*/
    /* prev tag length */
    PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
    PreTagSize[1] = (mediasize+11) >> 16;
    PreTagSize[2] = (mediasize+11) >> 8;
    PreTagSize[3] =  mediasize+11;

	fwrite(PreTagSize, 1, 4, fp);
//	printf("%s line %d\n",__FUNCTION__,__LINE__);
	//fflush(fp);
	return 0;
 }

