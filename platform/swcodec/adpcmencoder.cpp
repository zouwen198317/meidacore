#include "adpcmencoder.h"
#include "ffmpeg/audiocodec/adpcm.h"
#include "log/log.h"

#define BUF_NUM_OF_SAMPLES 100 //64

AdpcmEnc::AdpcmEnc(audioParms * parms):CAudioEnc(parms)
{
	mAvCodecContext.channels =parms->channels;
	mAvCodecContext.trellis =0;//16U;
	mAvCodecContext.codec = &mCodec;
	mAvCodecContext.codec->id = (CodecID)parms->codec;//CODEC_ID_ADPCM_MS;//CODEC_ID_ADPCM_MS;
	mAvCodecContext.sample_rate = parms->sampleRate;	
	mAvCodecContext.priv_data = (void*)malloc(sizeof(ADPCMContext));
	
	mOffset = 0;
	memcpy(&mAudioParms , parms, sizeof(audioParms) );

	log_print(_INFO, "AdpcmEnc codec %x, sampleRate %d, channel %d\n",parms->codec,parms->sampleRate,parms->channels);
	mSampleSize = 2; //only use U16
	isInit = false;
	if(parms->sampleSize == mSampleSize){
		adpcm_encode_init(&mAvCodecContext);	
		mpBufZone  = allocHwBuffer(BUF_NUM_OF_SAMPLES*getFrameSize()*compressRate());
		isInit = true;
	} else {
		log_print(_ERROR,"AdpcmEnc  sampleSize %d error\n",parms->sampleSize);
	}
	log_print(_INFO,"AdpcmEnc audioParms: %d %d %d %d %d\n",parms->codec,parms->bitRate,parms->sampleRate,parms->sampleSize,parms->channels);
}

AdpcmEnc::~AdpcmEnc()
{

	if(isInit)
		adpcm_encode_close(&mAvCodecContext);
	
	if(mpBufZone) {
		freeHwBuffer(mpBufZone);
		mpBufZone =NULL;
	}
	free(mAvCodecContext.priv_data);
}

int AdpcmEnc::encode(AudBlk * src, AencBlk * dest)
{
	int ret =-1;
	if(src == NULL || dest == NULL) {
		log_print(_ERROR,"AdpcmEnc::encode src == 0x%x || dest == 0x%x \n",(u32)src,(u32)dest);
		return ret;
	}
	if(isInit == false || mpBufZone == NULL) {
		log_print(_ERROR,"AdpcmEnc::encode not init OK, mBufZone 0x%x\n",(u32)mpBufZone);
		return ret;
	}
	int minEncodeSize = mAvCodecContext.frame_size ;//sample per frame * 2bytes per sample
	if(src->size < minEncodeSize) {
		log_print(_ERROR,"AdpcmEnc::encode input size must bigger than %d, not %d\n",minEncodeSize,src->size);
		return ret;
	}

	if((mOffset + src->size/compressRate()) > mpBufZone->bufSize) {
		mOffset=0;
	}
      //log_print(_INFO,"AdpcmEnc::encode src->vaddr %x, size %d, vStartaddr 0x%x, offset 0x%x, bufSize %d\n",(u32)src->vaddr, src->size,(u32)mpBufZone->vStartaddr,mOffset,mpBufZone->bufSize);
	int sizeout= adpcm_encode_frame(&mAvCodecContext,
		(unsigned char*)((u32)mpBufZone->vStartaddr+mOffset), 
		mpBufZone->bufSize - mOffset, 
		src->vaddr);
	//log_print(_INFO,"AdpcmEnc::encode minEncodeSize %d, src size %d --> dest size %d, bufSize %d\n",minEncodeSize,src->size, sizeout,mpBufZone->bufSize);
	
	dest->pts= src->pts;
	dest->frameSize = sizeout;//1024
	dest->offset = mOffset;
	memcpy(&(dest->buf) , mpBufZone, sizeof(bufZone) );
	memcpy(&(dest->audioParm) , &mAudioParms, sizeof(audioParms) );

	if(((u32)mOffset + sizeout) < mpBufZone->bufSize) {
		mOffset += sizeout;
	} else {
		mOffset =0;
	}

	return sizeout;
}

int AdpcmEnc::getFrames()
{
	return mAvCodecContext.frame_size;
}

int AdpcmEnc::getFrameSize()
{
	return mAvCodecContext.frame_size * mSampleSize;

}

bufZone* AdpcmEnc::allocHwBuffer(u32 size)
{

	bufZone* tmpZone = (bufZone*)malloc( sizeof( bufZone));
	tmpZone->isRingBuf = 1;
	tmpZone->pStartAddr =0;
	tmpZone->vStartaddr = (void *)malloc(size);
	tmpZone->bufSize = size;
	log_print(_DEBUG,"AdpcmEnc::allocHwBuffer size %d\n",size);
	return tmpZone;
}

void AdpcmEnc::freeHwBuffer(bufZone* zone)
{

	if(zone) {
		if(zone->vStartaddr)
			free(zone->vStartaddr);
		free(zone);
	}
}


