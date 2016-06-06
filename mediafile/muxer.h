#ifndef _MUXER_H_
#define _MUXER_H_
#include "mediaparms.h"
#include "algchunk.h"
#include "ffmpeg/avcodec_common.h"

class Muxer
{
public:
	explicit Muxer(char *name, muxParms *pParm);
	virtual ~Muxer();
	virtual int write(AVPacket *pkt);
	virtual int read(AVPacket *pkt);
	virtual int size() const { return mSize; }
	virtual int writeVideoPkt(AVPacket *pkt);
	virtual int writeAudioPkt(AVPacket *pkt);
	virtual int writeDataPkt(AVPacket *pkt);
	/*
	virtual AlgChk *getFcwChkHead();
	virtual AlgChk *getLdwChkHead();
	virtual AlgChk *getMdwChkHead();
	*/
	virtual AlgChk *getChkHead(enum AlgChkType type) const {return NULL;}

protected:	
	u32 mSize;
private:
};

#endif

