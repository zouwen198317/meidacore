#include "microphone.h"
#include "log/log.h"
#include<sys/time.h>

#define ALSA_FRAMES 128
#define MIC_CHANNEL_NUM 1
int MicroPhone::setHwParams()
{
	int ret = 0;
	int rc;
	int size;
	unsigned int val;
	snd_pcm_uframes_t frames =mFrames;
	int dir;
	

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	ret = snd_pcm_hw_params_any(handle, params);
	if(ret < 0) { 
         printf("MicroPhone snd_pcm_hw_params_any fail\n");  
         return -1; 
    	}

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	ret = snd_pcm_hw_params_set_access(handle, params,SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) { 
        printf("MicroPhone snd_pcm_hw_params_set_access fail\n"); 
        return -1; 
    } 

	switch(mSampleSize)
	{
		case 1: //u8
			ret = snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S8);
			break;
		case 2://u16
			/* Signed 16-bit little-endian format */
			ret = snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S16_LE);
			break;
		case 3://u24
			ret = snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S24_LE);
			break;
		case 4://u32
			ret = snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S32_LE);
			break;
		default:
			ret = snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S16_LE);
			break;							
	}
	if (ret < 0) { 
        	printf("MicroPhone snd_pcm_hw_params_set_format fail\n"); 
        	return -1; 
    } 
	
	/* Two channels (stereo) */
	ret = snd_pcm_hw_params_set_channels(handle, params, mChannel);
	if (ret < 0) { 
        	printf("MicroPhone snd_pcm_hw_params_set_channels fail\n"); 
        	return -1; 
    } 

	/* 44100 bits/second sampling rate (CD quality) */
	val = mSampleRate;
	ret = snd_pcm_hw_params_set_rate_near(handle, params,&val, &dir);
	if (ret < 0) { 
        	printf("MicroPhone snd_pcm_hw_params_set_rate_near fail\n"); 
        	return -1; 
    } 

	/* Set period size to 32 frames. */
	//frames = 128;
	frames = frames*mChannel;
	ret = snd_pcm_hw_params_set_period_size_near(handle,params, &frames, &dir);
	if (ret < 0)  
	{ 
	 	printf("MicroPhone Unable to set period size %li : %s\n", frames,  snd_strerror(ret)); 
	} 

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		log_print(_ERROR,"MicroPhone::setHwParams unable to set hw parameters: %s/n",snd_strerror(rc));
		return -1;
	}

	/* Use a buffer large enough to hold one period */
	ret = snd_pcm_hw_params_get_period_size(params,&frames, &dir);
	size = frames * mSampleSize*mChannel;// 4; /* 2 bytes/sample, 2 channels */
	//buffer = (char *) malloc(size);
	mFrameSize = size;
	
	/* We want to loop for 5 seconds */
	//snd_pcm_hw_params_get_period_time(params,&val, &dir);
	//loops = 5000000 / val;
}

u32 MicroPhone::getFrameDuration()
{
	int rc;
	int dir;
	unsigned int val;

	snd_pcm_hw_params_get_period_time(params,&val, &dir);//us
	return val;
}


MicroPhone::MicroPhone(int sampleRate, int sampleSize, int frames)
{
	int rc;

	mSampleSize = sampleSize;
	mSampleRate = sampleRate;
	mChannel =MIC_CHANNEL_NUM;
	mFrames = frames;
	params = NULL;
	handle = NULL;
	gettimeofday(&mStarTv, NULL);
	
	log_print(_INFO,"MicroPhone sampleSize %d, sampleRate %d, frames %d, chn %d\n",sampleSize,sampleRate,frames,mChannel);
	/* Open PCM device for recording (capture). */
	rc = snd_pcm_open(&handle, "default",SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		log_print(_ERROR,"MicroPhone  unable to open pcm device: %s/n",snd_strerror(rc));
		handle = NULL;
	} else {
		rc = setHwParams();
		if(rc < 0)
		{
			printf("MicroPhone :: setHwParams() fail\n");
			snd_pcm_close(handle);
			handle = NULL;		
		}	
	}
}


MicroPhone::~MicroPhone()
{
	if(handle ) {
		snd_pcm_drain(handle);
		if(params)
			snd_pcm_hw_params_free(params);
		snd_pcm_close(handle);
	}

}

int MicroPhone::read(char *buf,  u32 size)
{
	if(!handle)
	{
		log_print(_ERROR,"MicroPhone read() pcm device unopened \n");
		return -1;
	}
	int frames  = size/ mSampleSize/mChannel;
	frames = frames < mFrames ? frames: mFrames;
	int rc = snd_pcm_readi(handle, buf, frames);
	if (rc == -EPIPE) {
	/* EPIPE means overrun */
		log_print(_ERROR,"MicroPhone::read overrun occurred\n");
		snd_pcm_prepare(handle);
		return rc;
	} else if (rc < 0) {
		log_print(_ERROR,"MicroPhone::read error from read: %s\n",snd_strerror(rc));
		return rc;
	} else if (rc != (int)frames) {
		log_print(_ERROR,"MicroPhone::read  short read, read %d frames\n", rc);	
	} else{

	}
	
	return rc*mSampleSize;
}


u64 MicroPhone::getPts()
{
	u64 pts =0,pts_sec = 0;
	struct  timeval    tv;
	//log_print(_DEBUG," CamCapture:: capture gettimeofday to get PTS %lld\n",pts);
	gettimeofday(&tv, NULL);
	pts_sec = tv.tv_sec;
	pts = ((pts_sec- mStarTv.tv_sec) * 1000) + tv.tv_usec/1000;
	return pts;
}

int MicroPhone::caputre(AudBlk* pAudBlk)
{
	int ret = -1;
	ret = read((char *)pAudBlk->vaddr, pAudBlk->size);
	if(ret < 0)
	{
		log_print(_ERROR,"MicroPhone caputre() read fail \n");
		return -1;
	}
	pAudBlk->pts = getPts();
	return ret;
}



