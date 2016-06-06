#ifndef _DEMUXER_H_
#define _DEMUXER_H_
#include "mediaparms.h"
#include "ffmpeg/avcodec_common.h"

class Demuxer
{
public:
	Demuxer(char *name) { };
	virtual ~Demuxer(){ };
	virtual int read(AVPacket *pkt) { return -1;}
	virtual int getFileMuxParms(muxParms *parms) {return -1;}
	virtual u32 getVideoCodedId() const{ return 0; }
	virtual u32 getAudioCodedId() const{ return 0; }
	
	virtual int getVideoStreamId() const{ return -1; }
	virtual int getAudioStreamId() const{ return -1; }
	virtual int getDataStreamId()  const{ return -1; }
	virtual bool isVideoStream(int streamId) const { return false;}
	virtual bool isAudioStream(int streamId) const  { return false;}
	virtual bool isDataStream(int streamId) const  { return false;}

	virtual u32 getFileDuration() const{ return 0; }
	virtual u32 getFrameDuration() const{ return 0; }
	virtual int seek(u32 msec) { return 0; } 
	
	virtual int size() const { return mSize; }
protected:	
	u32 mSize;
// private:
};

#endif
