#include <stdlib.h>
#include "avimuxer.h"
#include "log/log.h"
#include "ffmpeg/avformat/avienc.h"
#include "ffmpeg/avformat/avio.h"
#include "ffmpeg/audiocodec/common_t.h"
static unsigned char ms_adpcm_extradata[32]= {0xf4,0x07,0x07,0x0,0x0,
                                       0x1,0x0,0x0,0x0,0x2,
                                       0x0,0xff,0x0,0x0,0x0,
                                       0x0,0xc0,0x0,0x40,0x0,
                                       0xf0,0x0,0x0,0x0,0xcc,
                                       0x1,0x30,0xff,0x88,0x1,
                                       0x18,0xff};


int AviMuxer::initAvFormat(muxParms *pParm)
{
    elems.key=(char*)"ISFT";
    elems.value = (char*)"Lavf52.64.2";
    URLContext* purlc=NULL;
    purlc=(URLContext*) avFormat.pb->opaque;
    
    mFd = (size_t)purlc->priv_data;

    pParm->hasVideo =1;
    if(pParm->hasVideo) {
        avFormat.streams[0] = &stream_video;
        log_print(_DEBUG, "Video  streams 0x%x\n",(int)avFormat.streams[0]);
        avFormat.nb_streams++;  //stream number
        //vidoe
        stream_video.nb_frames = 0;
        stream_video.codec = &codecContext[0];//???
        stream_video.id = 0;
        stream_video.codec_type= CODEC_TYPE_VIDEO;
        stream_video.codec->codec_type = AVMEDIA_TYPE_VIDEO;
        stream_video.codec->codec_id = (CodecID)mMuxParm.video.codec;//CODEC_ID_H264;
        stream_video.codec->bit_rate = mMuxParm.video.bitRate;
        stream_video.codec->frame_size = 0;
        stream_video.codec->sample_rate = 0;
        stream_video.codec->block_align = 0;
        stream_video.codec->extradata_size= 0;
        stream_video.codec->codec_tag= MKTAG_T('h', '2', '6', '4');//875967080
        //printf("stream_video.codec->codec_tag %u\n", stream_video.codec->codec_tag);
        stream_video.codec->bits_per_coded_sample= 0;
        stream_video.codec->time_base.num= 1;

        stream_video.codec->time_base.den= mMuxParm.video.fps;

        stream_video.codec->width       = mMuxParm.video.width;
        stream_video.codec->height      = mMuxParm.video.height;
        stream_video.metadata= NULL;
        stream_video.sample_aspect_ratio.den= 1;
        stream_video.sample_aspect_ratio.num= 0;
        aviMetaData.count =1;
        aviMetaData.elems=&elems;           
    }

    if(pParm->hasAudio) {
        avFormat.streams[1] = &stream_audio;
        log_print(_DEBUG, "Audio streams 0x%x\n",(int)avFormat.streams[1]);
        avFormat.nb_streams++;  //channel number
        stream_audio.nb_frames= 0;
        stream_audio.codec = &codecContext[1];//???
        stream_audio.id             = 1;
        stream_audio.codec_type     = CODEC_TYPE_AUDIO;
        stream_audio.codec_id           = (CodecID)mMuxParm.audio.codec;//CODEC_ID_ADPCM_MS;//MAYbe
        //stream_audio.bits_per_sample = 4;//MAYbe
        stream_audio.bits_per_sample = 16;//MAYbe
        stream_audio.sample_rate = mMuxParm.audio.sampleRate;//8000;//MAYbe              
        stream_audio.channels= mMuxParm.audio.channels;// 1;//MAYbe             
        stream_audio.codec->codec_type = AVMEDIA_TYPE_AUDIO;
        stream_audio.codec->codec_id = (CodecID)mMuxParm.audio.codec;//CODEC_ID_ADPCM_MS;

        stream_audio.codec->bit_rate = mMuxParm.audio.bitRate*1024;//64000;
//printf("-------stream_audio.codec->bit_rate  mMuxParm.audio.bitRate %d\n", mMuxParm.audio.bitRate);   
        if(mMuxParm.audio.codec == CODEC_ID_ADPCM_MS) {
            stream_audio.codec->frame_size = 2036;//CODEC_ID_ADPCM_MS frame_size
            stream_audio.codec->codec_tag = 0x0002;//CODEC_ID_ADPCM_MS,   
        } else if(mMuxParm.audio.codec == CODEC_ID_ADPCM_IMA_WAV) {
            stream_audio.codec->frame_size = 2041;
            stream_audio.codec->codec_tag = 0x0011;//CODEC_ID_ADPCM_IMA_WAV,   
        }
        stream_audio.codec->sample_rate = mMuxParm.audio.sampleRate;//8000;
        stream_audio.codec->block_align = 1024;
        stream_audio.codec->time_base.num = 1;
        stream_audio.codec->time_base.den = 8000;
        if(mMuxParm.audio.codec == CODEC_ID_ADPCM_MS) {
            stream_audio.codec->extradata_size = 32;//!!!!!
            stream_audio.codec->extradata = ms_adpcm_extradata;
        }
        stream_audio.codec->bits_per_coded_sample = 0;
        stream_audio.codec->channel_layout = 0;
        stream_audio.codec->channels = 1;
        
        stream_audio.metadata = NULL;
        stream_audio.sample_aspect_ratio.den = 1;
        stream_audio.sample_aspect_ratio.num = 0;

    }
    if(pParm->hasMetaData){
        avFormat.nb_streams++;  //channel number
        avFormat.streams[avFormat.nb_streams -1] = &stream_data;
        log_print(_DEBUG, "Data  streams  0x%x\n",(int)avFormat.streams[avFormat.nb_streams -1]);
        stream_data.nb_frames= 0;
        stream_data.codec = &codecContext[2];//???
        stream_data.id              = avFormat.nb_streams -1;
        stream_data.codec_type      = CODEC_TYPE_DATA;
        stream_data.codec_id            = CODEC_ID_TEXT;//MAYbe
        stream_data.bit_rate = 0;
        stream_data.codec->bit_rate = 0;
        stream_data.codec->codec_type = AVMEDIA_TYPE_DATA;
        stream_data.codec->codec_id = CODEC_ID_TEXT;//MAYbe

    }
    avFormat.metadata = &aviMetaData;
    avFormat.aviIdxBuf.buffer =NULL;
    avFormat.aviIdxBuf.bufSize =0;
    avFormat.aviIdxBuf.len = 0;
    avFormat.priv_data =  &aviContext;  

    return 0;
}

int AviMuxer::initAlgChk(AlgChk **pAlgChk)
{
    *pAlgChk = (AlgChk *)malloc(sizeof(AlgChk));
    if(*pAlgChk == NULL) {
        printf("AviMuxer initAlgChk() fail\n");
        return -1;
    }
    (*pAlgChk )->tmPoint = 0;
    (*pAlgChk )->Duration = 0;
    INIT_LIST_HEAD(&(*pAlgChk )->listEntry);
    return 0;
}


AviMuxer::AviMuxer(char *name, muxParms *pParm):Muxer(name,pParm)
{
    int i = 0;
    mFd = -1;
    memcpy(&mMuxParm, pParm, sizeof(muxParms));
    strcpy(avFormat.filename,name);
    log_print(_INFO, "AVI File: %s\n", avFormat.filename);

    mSize = 0;
    
    avFormat.nb_streams = 0;
    avFormat.file_size = 0;
    avFormat.buffer_data_len = 0;

    for(i=0; i<ALGCHK_TYPE_NB; i++) {
        initAlgChk(&avFormat.algChkHead[i]);
    }
    /*
    initAlgChk(&avFormat.fcwChk_head);
    initAlgChk(&avFormat.ldwChk_head);
    initAlgChk(&avFormat.mdwChk_head);
    */
    //avFormat.head = (AlgmChk *)malloc(sizeof(AlgmChk));
    
    //memset(avFormat.head, 0, sizeof(AlgmChk));
    
    if (openFile() < 0) {
        log_print(_ERROR,"AviMuxer cant't open file %s\n",avFormat.filename);
        //return Err_DrvHddError;//-1;
    } else {
        initAvFormat( pParm);
        log_print(_INFO,"AVI Header: %s nb_streams %d\n",avFormat.filename,avFormat.nb_streams);
        writeHeader();
    }   
}

AviMuxer::~AviMuxer()
{
    writeTail();
    closeFile();
}

int AviMuxer::openFile()
{
    log_print(_DEBUG, "AVI openFile: %s\n", avFormat.filename);
    int ret = url_fopen(&avFormat.pb, avFormat.filename, URL_WRONLY);
    if(ret < 0)
        log_print(_ERROR, "AviMuxer openFile openFile Erro %d\n", ret);
    return ret;
}

int AviMuxer::closeFile()
{
    //AlgmChk *pTemp;
    log_print(_DEBUG,"AVI closeFile: %s\n",avFormat.filename);
    int ret = url_fclose(avFormat.pb);
    int i = 0;
    avFormat.pb = NULL;
    
    /*while(avFormat.head->next != NULL)
    {
        pTemp = avFormat.head->next;
        avFormat.head->next = pTemp->next;
        free(pTemp);
    }
    pTemp = NULL;
    */
    //free(avFormat.head);
//if(list_empty(&avFormat.fcwChk_head->listEntry))
    //printf("coseFile() fcwChk_head empty\n");
//if(list_empty(&avFormat.ldwChk_head->listEntry))
//  printf("coseFile() ldwChk_head empty\n");
    for(i=0; i<ALGCHK_TYPE_NB; i++) {
        free(avFormat.algChkHead[i]);
    }
/*
    free(avFormat.fcwChk_head);
    free(avFormat.ldwChk_head);
    free(avFormat.mdwChk_head);
*/
    //avFormat.head = NULL;
    
    if(ret < 0)
        log_print(_ERROR,"AviMuxer closeFile url_fclose Erro %d\n",ret);
    return ret;
}

int AviMuxer::writeHeader()
{
    log_print(_DEBUG,"AVI writeHeader: %s\n",avFormat.filename);
    int ret =  avi_write_header(&avFormat);
    if(ret < 0)
        log_print(_ERROR,"AviMuxer writeHeader avi_write_header Erro %d\n",ret);
    return ret;
}

int AviMuxer::writeTail()
{
    log_print(_DEBUG,"AVI writeTail: %s\n",avFormat.filename);
    int ret= avi_write_trailer(&avFormat);
    if(ret < 0)
        log_print(_ERROR,"AviMuxer writeTail avi_write_trailer Erro %d\n",ret);
    return ret;
}


int AviMuxer::writeVideoPkt(AVPacket *pkt)
{
     int ret = 0 ;
     int64_t pts = pkt->pts;
     int id = stream_video.id;
    //pkt->data = msvDataBuf;
    //pkt->size = msvDataBufLen;
    //pkt->flags = flags;
        
    pkt->stream_id =id;
    pkt->stream_index= id;
    if(avFormat.start_time == 0) {
        avFormat.start_time = pts;
    }
    if(avFormat.streams[id]->start_time == 0) {
        avFormat.streams[id]->start_time = pts;
    }   
    pkt->pts =  pts - avFormat.streams[id]->start_time;
    pkt->dts = AV_NOPTS_VALUE;

    //log_print(_DEBUG,"AVI write packet: data 0x%x, size %d, flags 0x%x, stream_id %d, pts %lld\n",(u32)pkt->data,pkt->size,(u32)pkt->flags,pkt->stream_id,pkt->pts);
     ret = avi_write_packet(&avFormat, pkt); 
    if(ret < 0) {
        log_print(_ERROR,"AviMuxer writeVideoPkt avi_write_packet Erro %d\n",ret);
    } else 
        mSize += pkt->size;
     return ret;
}

int AviMuxer::writeAudioPkt(AVPacket *pkt)
{
     int ret = 0 ;
     int64_t pts = pkt->pts;
     int flags = PKT_FLAG_KEY;
     int id = stream_audio.id;
    //pkt.data = msvDataBuf;
    //pkt.size = msvDataBufLen;
    pkt->flags = flags;
        
    pkt->stream_id =id;
    pkt->stream_index= id;
    if(avFormat.start_time == 0) {
        avFormat.start_time = pts;
    }
    if(avFormat.streams[id]->start_time == 0) {
        avFormat.streams[id]->start_time = pts;
    }   
    pkt->pts =  pts - avFormat.streams[id]->start_time;
    pkt->dts = AV_NOPTS_VALUE;

    //log_print(_DEBUG,"AVI write packet: data 0x%x, size %d, flags 0x%x, stream_id %d, pts %lld\n",(u32)pkt->data,pkt->size,(u32)pkt->flags,pkt->stream_id,pkt->pts);
     ret = avi_write_packet(&avFormat, pkt); 
    if(ret < 0) {
        log_print(_ERROR,"AviMuxer writeAudioPkt avi_write_packet Erro %d\n",ret);
    } else 
        mSize += pkt->size;
     return ret;
}

int AviMuxer::writeDataPkt(AVPacket *pkt)
{
     int ret = 0 ;
     int64_t pts = pkt->pts;
     int flags = 0;//PKT_FLAG_KEY;
     int id = stream_data.id;//stream_audio.id;
    //pkt.data = msvDataBuf;
    //pkt.size = msvDataBufLen;
    pkt->flags = flags;
        
    pkt->stream_id =id;
    pkt->stream_index= id;
    //if(avFormat.start_time == 0) {
    //  avFormat.start_time = pts;
    //}
    if(avFormat.streams[id]->start_time == 0) {
        avFormat.streams[id]->start_time = pts;
    }   
    pkt->pts =  pts - avFormat.streams[id]->start_time;
    pkt->dts = AV_NOPTS_VALUE;

    //log_print(_DEBUG,"AVI write packet: data 0x%x, size %d, flags 0x%x, stream_id %d, pts %lld\n",(u32)pkt->data,pkt->size,(u32)pkt->flags,pkt->stream_id,pkt->pts);
     ret = avi_write_packet(&avFormat, pkt); 
    if(ret < 0) {
        log_print(_ERROR,"AviMuxer writeDataPkt avi_write_packet Erro %d\n",ret);
    } else 
        mSize += pkt->size;
     return ret;
}

int AviMuxer::write(AVPacket *pkt)
{
     int ret = 0 ;
     int64_t pts = pkt->pts;
     int id = pkt->stream_id;
        
    if(avFormat.streams[id]->start_time == 0) {
        avFormat.streams[id]->start_time = pts;
    }   
    pkt->pts =  pts - avFormat.streams[id]->start_time;
    pkt->dts = AV_NOPTS_VALUE;

    //log_print(_DEBUG,"AVI write packet: data 0x%x, size %d, flags 0x%x, stream_id %d, pts %lld\n",(u32)pkt->data,pkt->size,(u32)pkt->flags,pkt->stream_id,pkt->pts);
     ret = avi_write_packet(&avFormat, pkt); 
    if(ret < 0) {
        log_print(_ERROR,"AviMuxer write avi_write_packet Erro %d\n",ret);
    } else 
        mSize += pkt->size;
     return ret;
}

