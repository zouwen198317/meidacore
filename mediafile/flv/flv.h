#ifndef DVR_FLV_H
#define DVR_FLV_H

#include <unistd.h>
#include <stdio.h>

#define AUDIO_CODECE_ID_LPCM 3
#define AUDIO_CODECE_ID_ADPCM 1
#define AUDIO_CODECE_ID_MP3 2
#define AUDIO_CODECE_ID_G711MU 8
#define AUDIO_CODECE_ID_AAC 10

typedef struct tag_flvHeadInfo{
	int filesize_offset; 
	int duration_offset;
	long long duration_seconds;
	int configHead;
}flvHeadInfo_t;

typedef struct tag_flvAvInfo{
	int audiocodecid;
    int audBitRate;
	int audSampleRate;
	int vidFrameRate;
	int vidBitRate;
	int width;
	int height;
}flvAvInfo_t;

#define FLV_PKT_TYPE_VIDEO 0
#define FLV_PKT_TYPE_AUDIO 1
#define FLV_PKT_TYPE_OTHER 2

typedef struct tag_flvPkt{
	int pktType;
	int keyFrame;
	unsigned int pts;
	unsigned int len;
	unsigned char *buf;
}flvPkt_t;

typedef struct tag_flvAVCConf{
	unsigned int len;
	unsigned char *buf;
	int configHead;
	int pkgCnt;
}flvAVCConf_t;

typedef struct tag_flvFile{
	FILE* pFlvFile; 
	int64_t vnb_frames;
	int64_t anb_frames;
	flvHeadInfo_t flvHeadInfo;
	flvAvInfo_t flvAvInfo;
	flvAVCConf_t flvAVCConf;
}flvFile_t;



unsigned char* putbyte(unsigned char *s, unsigned int b);
unsigned char* putle32(unsigned char *s, unsigned int val);
unsigned char* putbe32(unsigned char *s, unsigned int val);
unsigned char* putle16(unsigned char *s, unsigned int val);
unsigned char* putbe16(unsigned char *s, unsigned int val);
unsigned char* putle24(unsigned char *s, unsigned int val);
unsigned char* putbe24(unsigned char *s, unsigned int val);
unsigned char* puttag(unsigned char *s, const char *tag);
unsigned char* putamf_bool(unsigned char* ptr, const int bVal);
unsigned char* putamf_string(unsigned char* ptr, const char *str);
unsigned char* putamf_string_content(unsigned char* ptr, const char *str);


int FillFLVHeader(FILE * fp,flvHeadInfo_t* pHeadInfo,flvAvInfo_t* pAvInfo);
int FillFlvTrailer(FILE * fp,flvHeadInfo_t *pHeadInfo,flvAvInfo_t *pAvInfo);
int FillFlvAudioPacket(FILE * fp, flvPkt_t *pFlvpkt,flvAvInfo_t* pAvInfo);
int FillFlvAudioConfigPacket(FILE * fp, flvAvInfo_t *pAvInfo);
int FillFlvVideoPacket(FILE * fp, flvHeadInfo_t *pHeadInfo, flvPkt_t *pFlvpkt,flvAVCConf_t *pFlvAVCConf,flvAvInfo_t* pAvInfo);

#endif

