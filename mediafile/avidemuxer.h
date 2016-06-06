#ifndef _AVI_DEMUXER_H_
#define _AVI_DEMUXER_H_
#include "demuxer.h"

class AviDemuxer: public Demuxer
{
public:
	AviDemuxer(char *name);
	~AviDemuxer();
	int read(AVPacket *pkt);
	int getFileMuxParms(muxParms *parms); 
	u32 getVideoCodedId() const { return stream_video.codec_id; }
	u32 getAudioCodedId() const { return stream_audio.codec_id; }
	
	int getVideoStreamId() const { return stream_video.id; }
	int getAudioStreamId() const { return stream_audio.id; }
	int getDataStreamId() const { return stream_data.id; }
	bool isVideoStream(int streamId) const { return streamId== stream_video.id ? true:false;}
	bool isAudioStream(int streamId) const { return streamId== stream_audio.id ? true:false;}
	bool isDataStream(int streamId) const { return streamId== stream_data.id ? true:false;}
	u32 getFileDuration() const { return mFileDuration; }
	u32 getFrameDuration() const { return mFrameDur; }
	int seek(u32 msec);
private:
	int openFile();
	int closeFile();
	int readHeader();
	muxParms mMuxParm;
	
	AVFormatContext avFormat;
	//AVMetadataTag elems;
	//AVCodecContext codecContext[3];
	
	AVStream stream_video;
	AVStream stream_audio;
	AVStream stream_data;
	AVIContext aviContext;
	//AVMetadata aviMetaData;

	char fileName[MAX_PATH_NAME_LEN];
	//int mFd;
	int mFrameDur;
	int mFrameRate;
	int mPktCnt;
	u32 mFileDuration;
	int64_t mVpts;
};

#endif

