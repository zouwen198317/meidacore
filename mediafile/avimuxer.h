#ifndef _AVI_MUXER_H_
#define _AVI_MUXER_H_
#include "muxer.h"
#include "algchunk.h"

class AviMuxer: public Muxer
{
public:
	AviMuxer(char *name, muxParms *pParm);
	~AviMuxer();
	int write(AVPacket *pkt);
	int writeVideoPkt(AVPacket *pkt);
	int writeAudioPkt(AVPacket *pkt);
	int writeDataPkt(AVPacket *pkt);
	int getVideoStreamId() const { return stream_video.id; }
	int getAudioStreamId() const { return stream_audio.id; }
	int getDataStreamId() const { return stream_data.id; }
	/*
	AlgChk *getFcwChkHead(){ return avFormat.fcwChk_head;}
	AlgChk *getLdwChkHead(){ return avFormat.ldwChk_head;}
	AlgChk *getMdwChkHead(){ return avFormat.mdwChk_head;}
	*/
	AlgChk *getChkHead(enum AlgChkType type) const { return avFormat.algChkHead[type]; }

private:
	int openFile();
	int closeFile();
	int writeHeader();
	int writeTail();
	int initAvFormat(muxParms *pParm);
	int initAlgChk(AlgChk **pAlgChk);

	muxParms mMuxParm;
	
	AVFormatContext avFormat;
	AVMetadataTag elems;
	AVCodecContext codecContext[3];
	
	AVStream stream_video;
	AVStream stream_audio;
	AVStream stream_data;
	AVIContext aviContext;
	AVMetadata aviMetaData;

	char fileName[MAX_PATH_NAME_LEN];
	int mFd;
};

#endif

