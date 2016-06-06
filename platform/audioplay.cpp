#include "audioplay.h"
#include "log/log.h"
#include<sys/time.h>

//#define ALSA_FRAMES 128
#define MIC_CHANNEL_NUM 1


#define ALSA_PLAY_BUF_MAX_LEN 1024 

#if MEDIA_CORE_CONTROL_VOL
int AudioPlay::setVolume(int value)
{
	if(value > 100 || value < 0){
		log_print(_ERROR,"AudioPlay::setVolume: Invalid parameter %d error\n",value);
		return 1;
	}
    
	if(mixer == NULL){
		if(snd_mixer_open(&mixer, 0) != 0){
			log_print(_ERROR,"AudioPlay::setVolume: snd_mixer_open error\n");
			return -1;
		}
    	if(snd_mixer_attach(mixer, "default") != 0){
			log_print(_ERROR,"AudioPlay::setVolume: snd_mixer_attach error\n");
			return -2;
		}
		if(snd_mixer_selem_register(mixer, NULL, NULL) != 0){
			log_print(_ERROR,"AudioPlay::setVolume: snd_mixer_elem_register error\n");
			return -3;
		}
		if(snd_mixer_load(mixer) != 0){
			log_print(_ERROR,"AudioPlay::setVolume: snd_mixer_load error\n");
			return -4; 
		}
		if((master_element = snd_mixer_first_elem(mixer)) == NULL){ 
			log_print(_ERROR,"AudioPlay::setVolume: snd_mixer_first_elem error\n");
			return -5;
		}
		snd_mixer_selem_set_playback_volume_range(master_element, 0, 100); 
	}
    snd_mixer_selem_set_playback_volume_all(master_element, value); 
	
    snd_mixer_selem_set_playback_switch_all(master_element, 1);  
      
	return 0;
}
#endif
int AudioPlay::setHwParams()
{
    int ret = 0;
    unsigned int val;
    snd_pcm_uframes_t frames =mFrames;
    snd_pcm_uframes_t periodsize;
    int dir = 0;


    /* Allocate a hardware parameters object. */
    ret = snd_pcm_hw_params_malloc(&params);
    if (ret < 0) { 
          log_print(_ERROR,"AudioPlay snd_pcm_hw_params_malloc fail"); 
          return -1; 
    }
    /* Fill it in with default values. */
    ret = snd_pcm_hw_params_any(handle, params);
    if(ret < 0) { 
           log_print(_ERROR,"AudioPlay snd_pcm_hw_params_any fail");  
         return -1; 
    }
    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    ret = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED); 
    if (ret < 0) { 
         log_print(_ERROR,"AudioPlay snd_pcm_hw_params_set_access fail"); 
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
        	 log_print(_ERROR,"AudioPlay snd_pcm_hw_params_set_format fail"); 
        	return -1; 
    } 

    /* Two channels (stereo) */
    ret = snd_pcm_hw_params_set_channels(handle, params, mChannel);
	if (ret < 0) { 
        	 log_print(_ERROR,"AudioPlay snd_pcm_hw_params_set_channels fail"); 
        	return -1; 
    } 
    /* 44100 bits/second sampling rate (CD quality) */
    val = mSampleRate;
    ret = snd_pcm_hw_params_set_rate_near(handle, params,&val, &dir);
	if (ret < 0) { 
        	 log_print(_ERROR,"AudioPlay snd_pcm_hw_params_set_rate_near fail"); 
        	return -1; 
    } 
	
    /* Set period size to 32 frames. */ 
      periodsize = frames * 3; 
      ret = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize); 
      if (ret < 0)  
      { 
            log_print(_ERROR,"AudioPlay Unable to set buffer size %li : %s\n", frames * mChannel, snd_strerror(ret)); 
            
      } 
      periodsize /= 3; 
   
      ret = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0); 
      if (ret < 0)  
      { 
           log_print(_ERROR,"AudioPlay Unable to set period size %li : %s\n", periodsize,  snd_strerror(ret)); 
      } 

    /* Write the parameters to the driver */
    ret = snd_pcm_hw_params(handle, params); 
    if (ret < 0) { 
          log_print(_ERROR,"AudioPlay snd_pcm_hw_params fail"); 
        return -1; 
    } 
    
    snd_pcm_hw_params_get_period_size(params, &frames, &dir); 
    mFrames = frames;
    mFrameSize = frames * mSampleSize *mChannel;  
     log_print(_INFO,"AudioPlay::setHwParams mFrames %d\n",mFrames);                       
    return 0;
}

u32 AudioPlay::getFrameDuration()
{
    int dir;
    unsigned int val;

    snd_pcm_hw_params_get_period_time(params,&val, &dir);//us
    return val;
}

/*
int AudioPlay::getDelay()
{
	snd_pcm_sframes_t availp;
	snd_pcm_sframes_t delayp; 	
	snd_pcm_avail_delay(handle,&availp,&delayp);
	int a = (int)availp;
	
	int ret = (int)delayp;
	//printf("availp===%d:%d==\n",a,ret);
	return ret;
}
*/

/* delay in seconds between first and last sample in buffer */
int  AudioPlay::getDelay(void)
{
	if(handle == NULL) {
		 log_print(_ERROR,"AudioPlay::getDelay handle == NULL\n");
		return -1;
	}
	int space = getSpace();
	int remainDur =0;
	if(space < mAlsaBufSpace ){

		remainDur= (mAlsaBufSpace - space)*1000/( mSampleSize *mChannel*mSampleRate);
		log_print(_DEBUG,"AudioPlay::getDelay %d space %d ,mAlsaBufSpace %d\n",remainDur, space ,mAlsaBufSpace);	
		//snd_pcm_prepare(handle);
	}
	//log_print(_INFO,"AudioPlay::getDelay %d\n", remainDur);
	return remainDur;
#if  0
  if (handle) {
	    snd_pcm_sframes_t delay;

	    if (snd_pcm_delay(handle, &delay) < 0){
	        log_print(_ERROR,"AudioPlay::snd_pcm_delay error\n");

	      return 0;
	    }

	    if (delay < 0) {
	      /* underrun - move the application pointer forward to catch up */
#if SND_LIB_VERSION >= 0x000901 /* snd_pcm_forward() exists since 0.9.0rc8 */
	      snd_pcm_forward(handle, -delay);
#endif
	      delay = 0;
	    }
	   int remainDur = delay*1000 / mSampleRate;
	   log_print(_INFO,"AudioPlay::getDelay %d\n", remainDur);
	   return remainDur;
  } else {
 	   log_print(_ERROR,"AudioPlay::getDelay handle=NULL\n");
  	   return 0;
  }
  #endif
}


/* how many byes are free in the buffer */
int AudioPlay::getSpace(void)
{
    snd_pcm_status_t *status;
    int ret;
	if(handle == NULL) {
		 log_print(_ERROR,"AudioPlay::getSpace handle == NULL\n");
		return -1;
	}

    snd_pcm_status_alloca(&status);

    if ((ret = snd_pcm_status(handle, status)) < 0)
    {
	log_print(_ERROR,"AudioPlay::getSpace ALSA_CannotGetPcmStatus %s\n", snd_strerror(ret));
	return 0;
    }

    ret = snd_pcm_status_get_avail(status) * mSampleSize *mChannel; 
    //if (ret > ao_data.buffersize)  // Buffer underrun?
	//ret = ao_data.buffersize;
    log_print(_DEBUG,"AudioPlay::getSpace %d\n",ret);	
    return ret;
}


AudioPlay::AudioPlay(int sampleRate, int sampleSize, int frames)
{
    int rc;

    mSampleSize = sampleSize;
    mSampleRate = sampleRate;
    mChannel =MIC_CHANNEL_NUM;
    mFrames = frames;
    params = NULL;
    handle = NULL;
    mpBuf = (unsigned char*)malloc(ALSA_PLAY_BUF_MAX_LEN);
    mBufLen =0;
    //mFrameSize = frames * mSampleSize *mChannel;  
    log_print(_DEBUG,"AudioPlay sampleSize %d, sampleRate %d, frames %d, chn %d\n",sampleSize,sampleRate,frames,mChannel);
    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, "default",SND_PCM_STREAM_PLAYBACK, 0);
	snd_pcm_nonblock(handle, 0);
//	snd_pcm_nonblock(handle, 1);
    if (rc < 0) {
         	log_print(_ERROR,"AudioPlay  unable to open pcm device: %s/n",snd_strerror(rc));
	 	handle = NULL;
    } else {
        rc = setHwParams();
		if(rc < 0)
		{
			 log_print(_ERROR,"AudioPlay:: setHwParams() fail\n");
			snd_pcm_close(handle);
			handle = NULL;		
		} else {
			mAlsaBufSpace = getSpace();
			 log_print(_ERROR,"AudioPlay:: mAlsaBufSpace %d\n",mAlsaBufSpace);
		}
    }
#if MEDIA_CORE_CONTROL_VOL
	//setVolume(50);
#endif
}


AudioPlay::~AudioPlay()
{
    mBufLen =0;
    if(mpBuf){
		free(mpBuf);
		mpBuf = NULL;
    }
    if(handle) {
        snd_pcm_drain(handle);
        if(params)
            snd_pcm_hw_params_free(params);
        snd_pcm_close(handle);
    }

}

int AudioPlay::write_pcm(char *buf, int size)
{
	int ret = 0;
	int rc = 0;
	if(handle == NULL) {
		 log_print(_ERROR,"AudioPlay::write_pcm handle == NULL\n");
		return -1;
	}
	if(size < mFrameSize) {
		 log_print(_ERROR,"AudioPlay::write_pcm size %d < mFrameSize%d\n",size,mFrameSize);
		return 0;
	}
//getDelay();
//getSpace();
#if 0
	while(1) 
          { 
  		ret = -11;
		snd_pcm_sframes_t availp;
		snd_pcm_sframes_t delayp; 	
		snd_pcm_avail_delay(handle,&availp,&delayp);
		int a = (int)availp;
		int d = (int)delayp;
		printf("st %d||%d\n",a,d);
		if (a>=frames)
      			ret = snd_pcm_writei(handle, buf, frames);
		printf("st %d||%d frames %d\n",a,d,frames);

		if(ret>=0)
		{
			break;
		}
		usleep(2000); 
			  
              if (ret == -EPIPE) 
              { 
                    /* EPIPE means underrun */ 
                    fprintf(stderr, "underrun occurred!\n"); 
                    //完成硬件参数设置，使设备准备好 
                    snd_pcm_prepare(handle); 
              }  
		else if (ret = -11)
		{
		  		rc++;			  
		}
              else if (ret < 0)  
              { 
                    fprintf(stderr, "error from writei: %s\n", snd_strerror(ret)); 
              }   
          } 
 
	return  rc*2000;
#endif
	snd_pcm_sframes_t res = 0;
	int offset = 0;
	if(mBufLen>0){

		offset = mFrameSize-mBufLen;
		memcpy(mpBuf+mBufLen, buf,mFrameSize-mBufLen);
		res = snd_pcm_writei(handle,  mpBuf, mFrames);
		if(res <0) {
			snd_pcm_prepare(handle);
			res = snd_pcm_writei(handle,  mpBuf, mFrames);
		}		
		mBufLen = 0;
		if(res < 0) {
  		 	log_print(_ERROR,"AudioPlay::write_pcm remain size %d, res %d:  %s\n",mBufLen,res,snd_strerror(res));
			if ((res = snd_pcm_prepare(handle)) < 0) {
				log_print(_ERROR,"ALSA_PcmPrepareError, %s\n",snd_strerror(res));
				return 0;
			}

		}
	}

	int outsize = (size-offset)/mFrameSize*mFrameSize;
	
	int frames = outsize/mSampleSize/mChannel;


	do {
		res = snd_pcm_writei(handle,  buf+offset, frames);

		if (res == -EINTR) {
			/* nothing to do */
			res = 0;
		}
		else if (res == -ESTRPIPE) {	/* suspend */
			//log_print(_ERROR,"ALSA_PcmInSuspendModeTryingResume\n");
			while ((res = snd_pcm_resume(handle)) == -EAGAIN)
				sleep(1);
			}
		if (res < 0) {
			log_print(_ERROR,"ALSA_WriteError, snd_strerror(res %d): %s\n", res,snd_strerror(res));
			//log_print(_ERROR,"ALSA_TryingToResetSoundcard\n");
			if ((res = snd_pcm_prepare(handle)) < 0) {
				log_print(_ERROR,"ALSA_PcmPrepareError, snd_strerror(res)\n");
				return 0;
			}
		}
	} while (res == 0);
	if((size-offset - outsize) >0) {
		memcpy(mpBuf, buf+offset + outsize,(size-offset - outsize));		
		mBufLen = size-offset - outsize;
	}

  //if(res>=0)printf("AudioPlay::write_pcm input size %d, write size %d\n",size,res * mSampleSize*mChannel);
  //if(res>=0)log_print(_ERROR,"AudioPlay::write_pcm input size %d, write size %d\n",size,res * mSampleSize*mChannel);
  return res < 0 ? res : size;
}

void AudioPlay::clearBuf(void)
{
	if(handle == NULL) {
		 log_print(_ERROR,"AudioPlay::clearBuf handle == NULL\n");
		return ;
	}
	int dur = getDelay();
	if(dur>0) {
		 log_print(_DEBUG,"AudioPlay::clearBuf usleep %d ms\n",dur);
		 usleep(dur*1000);
	}
}


