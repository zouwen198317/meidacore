#include "adpcmdecoder.h"
#include "ffmpeg/audiocodec/adpcm.h"
#include "log/log.h"
#include <assert.h>

#define BUF_NUM_OF_SAMPLES 64
#define AUDSAMPLE_MAX 5000

AdpcmDec::AdpcmDec(audioParms * parms):CAudioDec(parms)
{
	mAvCodecContext.channels = parms->channels;
	mAvCodecContext.trellis = 0;//16U;
	mAvCodecContext.codec = &mCodec;
	mAvCodecContext.codec->id = (CodecID)parms->codec;//CODEC_ID_ADPCM_MS;//CODEC_ID_ADPCM_MS;
	mAvCodecContext.codec_id = (CodecID)parms->codec;//CODEC_ID_ADPCM_MS;//CODEC_ID_ADPCM_MS;
	mAvCodecContext.sample_rate = parms->sampleRate;	
	mAvCodecContext.priv_data = (void*)malloc(sizeof(ADPCMContext));
	if(mAvCodecContext.codec_id == CODEC_ID_ADPCM_MS || mAvCodecContext.codec_id == CODEC_ID_ADPCM_IMA_WAV)
        	mAvCodecContext.block_align = 1024;
	
	memcpy(&mAudioParms , parms, sizeof(audioParms) );

	log_print(_DEBUG," AdpcmDec codec %x, sampleRate %d, channel %d\n",parms->codec,parms->sampleRate,parms->channels);
	mSampleSize = 2; //only use U16
	isInit = false;
	if(parms->sampleSize == mSampleSize){
		adpcm_decode_init(&mAvCodecContext);
		isInit = true;
	} else {
		log_print(_ERROR,"AdpcmDec sampleSize %d error\n",parms->sampleSize);
	}
}

AdpcmDec::~AdpcmDec()
{
	free(mAvCodecContext.priv_data);
}

int AdpcmDec::decode(AencBlk * src, AudBlk * dest)
{
	int ret =-1;
	assert(src);
	assert(dest);
	int outsize=0;	
	
	AVPacket pkt;
	pkt.data = (uint8_t*)src->buf.vStartaddr;
	pkt.size =  src->frameSize;
	pkt.pts = src->pts;
	
	
	if(src == NULL || dest == NULL) {
		log_print(_ERROR,"AdpcmDec::decode src == 0x%x || dest == 0x%x \n",(u32)src,(u32)dest);
		return ret;
	}
	if(isInit == false ) {
		log_print(_ERROR,"AdpcmDec::decode not init OK\n");
		return ret;
	}
	/*
	u32 minEncodeSize = mAvCodecContext.frame_size ;//sample per frame * 2bytes per sample
	if(src->frameSize < minEncodeSize) {
		log_print(_INFO,"AdpcmDec::decode input size must bigger than %d, not %d\n",minEncodeSize,src->frameSize);
		return ret;
	}
	*/
	log_print(_DEBUG,"AdpcmDec::decode dest->vaddr %x, size %d, src vStartaddr 0x%x, offset 0x%x, bufSize %d\n",(u32)dest->vaddr, dest->size,(u32)src->buf.vStartaddr,src->offset,src->buf.bufSize);

	dest->pts = src->pts;

	if(mAvCodecContext.codec_id != CODEC_ID_ADPCM_IMA_QT){
		if(dest->vaddr== NULL) {
			log_print(_ERROR,"AdpcmDec::decode dest->vaddr== NULL\n");
			return -1;
		}
		outsize = AUDSAMPLE_MAX;
		adpcm_decode_frame(&mAvCodecContext,dest->vaddr, &outsize, &pkt);
		dest->size = outsize;
		/*
		if(pPlayBackManager->audCodecId == CODEC_ID_ADPCM_SWF){
			audReSampleSize = audio_resample(pPlayBackManager->reSampleContext,
									  (short *)audReSampleBuf, (short *)audSampleBuf,
									  audSampleSize /(2));
			audSampleSize = audReSampleSize * 1 * 2;
			tmp = audSampleBuf;
			audSampleBuf = audReSampleBuf;
			audReSampleBuf = tmp;
		}
		*/
		log_print(_DEBUG,"AdpcmDec::decode adpcm_decode_frame %d -> %d pts %lld\n",pkt.size,outsize,pkt.pts);
		if(outsize == 0) {
			log_print(_ERROR,"AdpcmDec::decode   outsize 0 pts %lld\n",pkt.pts);
			return -1;
		}
	} 
	return outsize;
}



bufZone* AdpcmDec::allocHwBuffer(u32 size)
{
	bufZone* tmpZone = (bufZone*)malloc( sizeof( bufZone));
	tmpZone->isRingBuf = 0;
	tmpZone->pStartAddr = 0;
	tmpZone->vStartaddr = (void *)malloc(size);
	tmpZone->bufSize = size;
}

void AdpcmDec::freeHwBuffer(bufZone* zone)
{

	if(zone) {
		if(zone->vStartaddr)
			free(zone->vStartaddr);
		free(zone);
	}
}
