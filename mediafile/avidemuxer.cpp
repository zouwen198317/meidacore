#include "avidemuxer.h"
#include "log/log.h"
#include "ffmpeg/avformat/avidec.h"
#include "ffmpeg/avformat/avio.h"
#include "ffmpeg/audiocodec/common_t.h"
#include <assert.h>
AviDemuxer::AviDemuxer(char * name):Demuxer(name)
{
	assert(name);
	strcpy(fileName,name);
	mPktCnt = 0;
	memset(&stream_audio, 0, sizeof(AVStream));
	memset(&stream_video, 0, sizeof(AVStream));
	memset(&stream_data, 0, sizeof(AVStream));
	stream_video.codec_id = CODEC_ID_NONE;
	stream_audio.codec_id = CODEC_ID_NONE;
	mMuxParm.hasAudio = 0;
	mMuxParm.hasVideo= 0;
	mMuxParm.hasMetaData= 0;
	mMuxParm.hasPicture= 0;
	mVpts =0;
	memset(&avFormat, 0, sizeof(AVFormatContext));
	memset(&aviContext, 0, sizeof(AVIContext));//If there is no initialization, It may lead to can't fast forward.
	//avFormat.metadata = NULL;//5/12 avi_read_header() bug
	openFile();
	readHeader();
}

AviDemuxer::~AviDemuxer()
{
	closeFile();
}

int AviDemuxer::getFileMuxParms(muxParms *parms)
{
	if(parms)
		memcpy(parms,&mMuxParm,sizeof(muxParms));
}

int AviDemuxer::closeFile()
{
	AVFormatContext *pInFormat = &avFormat;
	if(pInFormat == NULL || pInFormat->pb == NULL)
		return -1;
	
	log_print(_DEBUG,"AviDemuxer::closeFile()\n");
	avi_read_close(pInFormat);
	if(pInFormat->pb){
		log_print(_DEBUG,"AviDemuxer::closeFile file %s\n",pInFormat->filename);
		url_fclose(pInFormat->pb);
		pInFormat->pb= NULL;
	}
	return 0;
}


int AviDemuxer::openFile()
{
	AVFormatContext *pInFormat = &avFormat;
	strcpy(pInFormat->filename,fileName);
	log_print(_DEBUG,"AviDemuxertry to open %s\n",pInFormat->filename);

	AVIContext *pavi = &aviContext;
	pInFormat->priv_data = pavi;	
	if (url_fopen(&pInFormat->pb, pInFormat->filename, URL_RDONLY) < 0)
	{
		log_print(_ERROR,"AviDemuxer url_fopen can't open %s file \n",pInFormat->filename);
		
		return -1;
	}
	return 0;
}

int AviDemuxer::readHeader()
{
	int id =0;
	AVFormatContext *pInFormat = &avFormat;
	avi_read_header(pInFormat, NULL);
	//printf("AviDemuxer readHeader() nb_index_entries%d \n", pInFormat->streams[0]->nb_index_entries);


//!!
//dwLength/(dwRate/dwScale) = duration
	if(pInFormat->streams[0] == NULL)
		return -1;
	int video_scale = ((int*)pInFormat->streams[0]->priv_data)[4];
	int video_rate = ((int*)pInFormat->streams[0]->priv_data)[5];
	int total_frames = pInFormat->streams[0]->nb_frames;
	//if(total_frames == 0)
		//total_frames = avi_read_videoFrames(pInFormat);
	mFileDuration = total_frames/(video_rate/video_scale);
	log_print(_INFO,"AviDemuxer total_frames %d \n", total_frames);
	//log_print(_INFO,"AviDemuxer video_rate %d \n", video_rate);
	//log_print(_INFO,"AviDemuxer video_scale %d \n", video_scale);
	
	//time_t timeS= pInFormat->start_time;
	log_print(_DEBUG,"AviDemuxer start_time %lld sec\n",pInFormat->start_time);

	log_print(_DEBUG,"AviDemuxer duration %lld\n",pInFormat->duration);
	log_print(_DEBUG,"AviDemuxer file_size %lld\n",pInFormat->file_size);
	log_print(_DEBUG,"AviDemuxer nb_streams %d\n",pInFormat->nb_streams);	
	mSize = pInFormat->file_size;
	
	for(int i = 0; i< pInFormat->nb_streams; i++){
		pInFormat->streams[i]->codec_type =(enum CodecType)pInFormat->streams[i]->codec->codec_type;
		if(pInFormat->streams[i]->codec_type == CODEC_TYPE_VIDEO){
			id = pInFormat->streams[i]->id ;

			log_print(_DEBUG,"AviDemuxer  video streams[%d] stream id %d\n",i, pInFormat->streams[i]->id);			
			pInFormat->streams[i]->width = pInFormat->streams[i]->codec->width;
			pInFormat->streams[i]->height = pInFormat->streams[i]->codec->height;
			pInFormat->streams[i]->codec_id= pInFormat->streams[i]->codec->codec_id;
			
			memcpy(&stream_video,pInFormat->streams[i],sizeof(AVStream));

			log_print(_DEBUG,"AviDemuxer Resolution %dx%d\n",stream_video.width,stream_video.height);
			log_print(_DEBUG,"AviDemuxer code_id 0x%x, codec_type %x\n",stream_video.codec_id,stream_video.codec_type);

			mFrameRate= pInFormat->streams[i]->time_base.den/pInFormat->streams[i]->time_base.num;
			mFrameDur = 1.0/mFrameRate *1000;//ms
			//mFrameDur = 1.0/mFrameRate *1000000;
			log_print(_INFO,"AviDemuxer stream[%d] framerate %d frameDur %d\n",i,mFrameRate,mFrameDur);
			mMuxParm.hasVideo=1;
			mMuxParm.video.codec = stream_video.codec_id;
			mMuxParm.video.width = stream_video.width;
			mMuxParm.video.height= stream_video.height;
			mMuxParm.video.fps= mFrameRate;

		}
		else
		if(pInFormat->streams[i]->codec_type == CODEC_TYPE_AUDIO){
			id = pInFormat->streams[i]->id;
			pInFormat->streams[i]->codec_id= pInFormat->streams[i]->codec->codec_id;
			memcpy(&stream_audio,pInFormat->streams[i],sizeof(AVStream));
		
			mMuxParm.hasAudio =1;
			mMuxParm.audio.codec = stream_audio.codec->codec_id;
			mMuxParm.audio.bitRate= stream_audio.codec->bit_rate;
			mMuxParm.audio.sampleRate= stream_audio.codec->sample_rate;
			mMuxParm.audio.channels= stream_audio.codec->channels;
			mMuxParm.audio.sampleSize= 2; 
			log_print(_DEBUG,"AviDemuxer audio streams[%d] stream id %d\n",i,stream_audio.id);			
			log_print(_DEBUG,"AviDemuxer code_id 0x%x, codec_type %x, codec_tag %x,bitRate %d,sampleRate %d,channels %d\n",
				stream_audio.codec->codec_id,stream_audio.codec_type,stream_audio.codec->codec_tag,mMuxParm.audio.bitRate,mMuxParm.audio.sampleRate,mMuxParm.audio.channels);
			
			
		}
		else if(pInFormat->streams[i]->codec_type == CODEC_TYPE_DATA)
		{
			log_print(_DEBUG,"AviDemuxer CODEC_TYPE_DATA streams[%d] stream id %d\n",i,pInFormat->streams[i]->id);			
			mMuxParm.hasMetaData=1;
		}		
	}
	return 0;
}


int AviDemuxer::read(AVPacket *pkt)
{
	AVFormatContext *pInFormat = &avFormat;
	if(pInFormat->pb == NULL) {
		log_print(_ERROR,"AviDemuxer read pInFormat->pb == NULL\n");
		return -1;
	}
	int ret = avi_read_packet(pInFormat, pkt);
	if(pkt->flags & PKT_FLAG_KEY )
		log_print(_DEBUG,"AviDemuxer read PKT_FLAG_KEY stream Id %d stream_index %d size %d \n",pkt->stream_id,pkt->stream_index,pkt->size);
	if(url_feof(pInFormat->pb))
		return 1;
	if (ret <0 || url_ferror(pInFormat->pb) != 0){ 
		log_print(_ERROR,"AviDemuxer::read avi_read_packet Error %d %s\n",ret,fileName);
		return -2;
	}

	pkt->stream_id = pkt->stream_index;
	log_print(_DEBUG," stream_index %d,pkt->dts %lld,frameDur %d\n",pkt->stream_index,pkt->dts,mFrameDur);

	if(this->isVideoStream(pkt->stream_id )){
		pkt->pts = pkt->dts*mFrameDur;
		mVpts = pkt->pts;
	}
	else
	if(this->isAudioStream(pkt->stream_id )){
		if((unsigned int)CODEC_ID_ADPCM_MS == stream_audio.codec_id 
          || (unsigned int)CODEC_ID_ADPCM_IMA_WAV == stream_audio.codec_id ){
			if(pkt->dts == 0) {
				//pkt->pts = mVpts+pkt->dts*(pkt->size*2/stream_audio.codec->channels*1000000/stream_audio.codec->sample_rate);
				pkt->pts = mVpts+pkt->dts*(pkt->size*2/stream_audio.codec->channels*1000/stream_audio.codec->sample_rate);
			}else {
				//pkt->pts = pkt->dts*(pkt->size*2/stream_audio.codec->channels*1000000/stream_audio.codec->sample_rate);
				pkt->pts = pkt->dts*(pkt->size*2/stream_audio.codec->channels*1000/stream_audio.codec->sample_rate);
			}
		}else {
			pkt->pts = pkt->dts;
			log_print(_INFO,"AviDemuxer::read Audio not  CODEC_ID_ADPCM_MS pts need to update\n");
		}
	}
	else
	if(this->isDataStream(pkt->stream_id ))	
		pkt->pts = mVpts;
		
	mPktCnt++;
	return 0;
}

int AviDemuxer::seek(u32 msec)//if no keyframe was found , it would try to find non-keyframe
{
	AVFormatContext *pInFormat = &avFormat;
	signed long long timestamp = msec*mFrameRate/1000;
	printf("AviDemuxer seek() timestamp:%lld\n", timestamp);
	//timeStamp : the frame with which time match
    int index = avi_read_seek(pInFormat,0,timestamp,AVSEEK_FLAG_BACKWARD);
   // int index = avi_read_seek(pInFormat,0,msec*mFrameRate,AVSEEK_FLAG_BACKWARD);
	if(index < 0){
		log_print(_ERROR,"AviDemuxer seek %ums,could not locate at keyFrame.\n",msec);
		/*
		//try to locate non-keyFrame
        index = avi_read_seek(pInFormat,0,msec*mFrameRate/1000,AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
		if(index < 0){
			log_print(_INFO,"AviDemuxer seek %ums,could not locate at any frame.\n",msec);
		}else{
			log_print(_DEBUG,"AviDemuxer seek %ums,FrameIndex %d\n",msec,index);
		}
		*/
	}else{
		log_print(_INFO,"AviDemuxer seek %ums,keyFrameIndex %d\n",msec,index);
	}

	return index;
}
