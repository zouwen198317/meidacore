/*
 * FLV muxer
 * Copyright (c) 2003 The FFmpeg Project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "avcodec_common.h"
//#include "avformat.h"
#include "flv_ffmpeg.h"
#include "flv.h"
#include "riff.h"
#include "avc.h"

#include "bytestream.h"
//#include "libavcodec/mpeg4audio.h"
#include "intfloat_readwrite.h"
#include "avio.h"
#include "assert.h"
//#include "avformat.h"
#include "flv_ffmpeg.h"
#include "utils.h"

//#include "dvripcpacket.h"

#undef NDEBUG
#include <assert.h>

static const AVCodecTag flv_video_codec_ids[] = {
//    {CODEC_ID_FLV1,    FLV_CODECID_H263  },
//    {CODEC_ID_FLASHSV, FLV_CODECID_SCREEN},
//    {CODEC_ID_FLASHSV2, FLV_CODECID_SCREEN2},
//    {CODEC_ID_VP6F,    FLV_CODECID_VP6   },
//    {CODEC_ID_VP6,     FLV_CODECID_VP6   },
    {CODEC_ID_H264,    FLV_CODECID_H264  },
    {CODEC_ID_NONE,    0}
};

static const AVCodecTag flv_audio_codec_ids[] = {
    {CODEC_ID_MP3,       FLV_CODECID_MP3    >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_PCM_U8,    FLV_CODECID_PCM    >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_PCM_S16BE, FLV_CODECID_PCM    >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_PCM_S16LE, FLV_CODECID_PCM_LE >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_ADPCM_SWF, FLV_CODECID_ADPCM  >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_AAC,       FLV_CODECID_AAC    >> FLV_AUDIO_CODECID_OFFSET},
//    {CODEC_ID_NELLYMOSER, FLV_CODECID_NELLYMOSER >> FLV_AUDIO_CODECID_OFFSET},
//    {CODEC_ID_SPEEX,     FLV_CODECID_SPEEX  >> FLV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_NONE,      0}
};


static int get_audio_flags(AVCodecContext *enc){
    int flags = (enc->bits_per_coded_sample == 16) ? FLV_SAMPLESSIZE_16BIT : FLV_SAMPLESSIZE_8BIT;
/*
    if (enc->codec_id == CODEC_ID_AAC) // specs force these parameters
        return FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
    else if (enc->codec_id == CODEC_ID_SPEEX) {
        if (enc->sample_rate != 16000) {
            av_log(enc, AV_LOG_ERROR, "flv only supports wideband (16kHz) Speex audio\n");
            return -1;
        }
        if (enc->channels != 1) {
            av_log(enc, AV_LOG_ERROR, "flv only supports mono Speex audio\n");
            return -1;
        }
        if (enc->frame_size / 320 > 8) {
            av_log(enc, AV_LOG_WARNING, "Warning: Speex stream has more than "
                                        "8 frames per packet. Adobe Flash "
                                        "Player cannot handle this!\n");
        }
        return FLV_CODECID_SPEEX | FLV_SAMPLERATE_11025HZ | FLV_SAMPLESSIZE_16BIT;
    } 
    */
//	else {
    switch (enc->sample_rate) {
        case    44100:
            flags |= FLV_SAMPLERATE_44100HZ;
            break;
        case    22050:
            flags |= FLV_SAMPLERATE_22050HZ;
            break;
        case    11025:
            flags |= FLV_SAMPLERATE_11025HZ;
            break;
        case     8000: //nellymoser only
        case     5512: //not mp3
            if(enc->codec_id != CODEC_ID_MP3){
                flags |= FLV_SAMPLERATE_SPECIAL;
                break;
            }
        default:
            printf( "flv does not support that sample rate, choose from (44100, 22050, 11025).\n");
            return -1;
    }
//    }

    if (enc->channels > 1) {
        flags |= FLV_STEREO;
    }

    switch(enc->codec_id){
    case CODEC_ID_MP3:
        flags |= FLV_CODECID_MP3    | FLV_SAMPLESSIZE_16BIT;
        break;
    case CODEC_ID_PCM_U8:
        flags |= FLV_CODECID_PCM    | FLV_SAMPLESSIZE_8BIT;
        break;
    case CODEC_ID_PCM_S16BE:
        flags |= FLV_CODECID_PCM    | FLV_SAMPLESSIZE_16BIT;
        break;
    case CODEC_ID_PCM_S16LE:
        flags |= FLV_CODECID_PCM_LE | FLV_SAMPLESSIZE_16BIT;
        break;
    case CODEC_ID_ADPCM_SWF:
        flags |= FLV_CODECID_ADPCM | FLV_SAMPLESSIZE_16BIT;
        break;
/*		
    case CODEC_ID_NELLYMOSER:
        if (enc->sample_rate == 8000) {
            flags |= FLV_CODECID_NELLYMOSER_8KHZ_MONO | FLV_SAMPLESSIZE_16BIT;
        } else {
            flags |= FLV_CODECID_NELLYMOSER | FLV_SAMPLESSIZE_16BIT;
        }
        break;
*/       
    case 0:
        flags |= enc->codec_tag<<4;
        break;
    default:
        printf("codec not compatible with flv\n");
        return -1;
    }

    return flags;
}

static void put_amf_string(ByteIOContext *pb, const char *str)
{
    size_t len = strlen(str);
    put_be16(pb, len);
    put_buffer(pb, (const unsigned char *)str, len);
}

static void put_amf_double(ByteIOContext *pb, double d)
{
    put_byte(pb, AMF_DATA_TYPE_NUMBER);
    put_be64(pb, av_dbl2int(d));
}

static void put_amf_bool(ByteIOContext *pb, int b) {
    put_byte(pb, AMF_DATA_TYPE_BOOL);
    put_byte(pb, !!b);
}
#if 0
int FillFlvAudioConfigPacket(AVFormatContext *s, flvAvInfo_t *pAvInfo)
{
    ByteIOContext *pb = s->pb;
    FLVContext *flv = s->priv_data;
	   char buf[16];
	   char PreTagSize[4]; 
	   uint32_t totalnallen = 2;
	   unsigned char freq = 0, objectType = 2, chans= 1;  //AAC audio specific config
	   int audSampleRate= pAvInfo->audSampleRate;
	   //GetAVConfig(NULL , &audSampleRate, NULL, NULL, NULL, NULL);
	   unsigned int dts = 0;
	   //int writeNum;
	   /* F4V tag header */
	   /* stream type */
	   if(fp == NULL )
		  return -1;
	   
	   put_byte(pb,0x08);

	   /* media size */
	   uint32_t mediasize = totalnallen + 2; /* unDataLen + 0xaf + 0x1*/
	   //buf[1] = mediasize >> 16; 
	   //buf[2] = mediasize >> 8;
	   //buf[3] = mediasize;
	   put_be32(pb,mediasize);

	   dts /= 1000;
	   
	   /* dts */
	   //buf[4] = (dts) >> 16; 
	   //buf[5] = (dts) >> 8;
	   //buf[6] = (dts);
	   //buf[7] = ((dts) >> 24) & 0x7F; 
	   put_be24(pb,dts);
	   put_byte(pb,(dts >> 24) & 0x7F); // timestamps are 32bits _signed_

	   /* reserve  streamId*/ 
	   put_be24(pb,flv->reserved);
	   //buf[8] = 0;
	   //buf[9] = 0;
	   //buf[10] = 0; 

	   /* F4V tag mediadatas */
	   /* Media Flag */
	   //buf[11] = 0xaf;
	   put_byte(pb,0xaf);

	   /* AAC sequence */
	   //buf[12] = 0x0;
	   put_byte(pb,0x0);

	   switch(audSampleRate) {
			   case 44100:
				   freq = 4;
				   break;
			   case 48000:
				   freq = 3;
				   break;
			   case 32000:
				   freq = 5;
				   break;
			   case 24000:
				   freq = 6;
				   break;
			   case 16000:
				   freq = 8;
				   break;
			   case 8000:
				   freq =  11;
				   break;
			   default:
				   freq = 4;
	   }
				   
	   
	   //buf[13] = 0x12;
	   //buf[14] = 0x10;
	   //buf[13] = (objectType <<3) | (freq >> 1);
	   //buf[14] = ((freq & 0x01) << 7) | (chans << 3);
	   put_byte(pb,(objectType <<3) | (freq >> 1));
	   put_byte(pb,((freq & 0x01) << 7) | (chans << 3));

	   /* F4V prev tag length*/
	   /* prev tag size */
	   //PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
	   //PreTagSize[1] = (mediasize+11) >> 16;
	   //PreTagSize[2] = (mediasize+11) >> 8;
	   //PreTagSize[3] =	mediasize+11;
	   put_be32(pb, mediasize + 11);

   //fwrite(buf, 1, 15, fp);
   //fwrite(PreTagSize, 1, 4, fp);

   return 0;
}
#endif

int flv_write_header(AVFormatContext *s,flvAvInfo_t *pAvInfo)
{
    ByteIOContext *pb = s->pb;
    FLVContext *flv = s->priv_data;
    AVCodecContext *audio_enc = NULL, *video_enc = NULL;
    int i;
    //double framerate = 0.0;
    int metadata_size_pos, data_size;

    for(i=0; i<s->nb_streams; i++){
        AVCodecContext *enc = s->streams[i]->codec;
        if (enc->codec_type == AVMEDIA_TYPE_VIDEO) {
			/*
            if (s->streams[i]->r_frame_rate.den && s->streams[i]->r_frame_rate.num) {
                framerate = av_q2d(s->streams[i]->r_frame_rate);
            } else {
                framerate = 1/av_q2d(s->streams[i]->codec->time_base);
            }
			*/
            video_enc = enc;
            if(enc->codec_tag == 0) {
                printf("video codec not compatible with flv\n");
                return -1;
            }
        } else 
		if (enc->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_enc = enc;
            if(get_audio_flags(enc)<0)
                return -1;
        }
        //av_set_pts_info(s->streams[i], 32, 1, 1000); /* 32 bit pts in ms */
    }
    put_tag(pb,"FLV");
    put_byte(pb,1);
    put_byte(pb,   FLV_HEADER_FLAG_HASAUDIO * !!audio_enc
                 + FLV_HEADER_FLAG_HASVIDEO * !!video_enc);
    put_be32(pb,9);
    put_be32(pb,0);

    for(i=0; i<s->nb_streams; i++){
        if(s->streams[i]->codec->codec_tag == 5){
            put_byte(pb,8); // message type
            put_be24(pb,0); // include flags
            put_be24(pb,0); // time stamp
            put_be32(pb,0); // reserved
            put_be32(pb,11); // size
            flv->reserved=5;
        }
    }

    /* write meta_tag */
    put_byte(pb, 18);         // tag type META
    metadata_size_pos= url_ftell(pb);
    put_be24(pb, 0);          // size of data part (sum of all parts below)
    put_be24(pb, 0);          // time stamp
    put_be32(pb, 0);          // reserved

    /* now data of data_size size */

    /* first event name as a string */
    put_byte(pb, AMF_DATA_TYPE_STRING);
    put_amf_string(pb, "onMetaData"); // 12 bytes

    /* mixed array (hash) with size and string/type/data tuples */
    put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
    put_be32(pb, 5*!!video_enc + 5*!!audio_enc + 2); // +2 for duration and file size

    put_amf_string(pb, "duration");
    flv->duration_offset= url_ftell(pb);
    put_amf_double(pb, s->duration / AV_TIME_BASE); // fill in the guessed duration, it'll be corrected later if incorrect

    if(video_enc){
        put_amf_string(pb, "width");
        //put_amf_double(pb, video_enc->width);
        put_amf_double(pb, pAvInfo->width);

        put_amf_string(pb, "height");
        //put_amf_double(pb, video_enc->height);
        put_amf_double(pb, pAvInfo->height);

        put_amf_string(pb, "videodatarate");
        //put_amf_double(pb, video_enc->bit_rate / 1024.0);
        put_amf_double(pb, pAvInfo->vidBitRate);

        put_amf_string(pb, "framerate");
        //put_amf_double(pb, framerate);
        put_amf_double(pb, pAvInfo->vidFrameRate);

        put_amf_string(pb, "videocodecid");
        //put_amf_double(pb, video_enc->codec_tag);
        put_amf_double(pb, 7.0);
    }

    if(audio_enc){
        put_amf_string(pb, "audiodatarate");
        //put_amf_double(pb, audio_enc->bit_rate / 1024.0);
        put_amf_double(pb, pAvInfo->audBitRate);

        put_amf_string(pb, "audiosamplerate");
        //put_amf_double(pb, audio_enc->sample_rate);
        put_amf_double(pb, pAvInfo->audSampleRate);

        put_amf_string(pb, "audiosamplesize");
        //put_amf_double(pb, audio_enc->codec_id == CODEC_ID_PCM_U8 ? 8 : 16);
        put_amf_double(pb, 16);

        //put_amf_string(pb, "stereo");
        //put_amf_bool(pb, audio_enc->channels == 2);
        put_amf_string(pb, "mono");
        put_amf_bool(pb, 1);

        put_amf_string(pb, "audiocodecid");
        //put_amf_double(pb, audio_enc->codec_tag);
        put_amf_double(pb, pAvInfo->audiocodecid);
    }

    put_amf_string(pb, "filesize");
    flv->filesize_offset= url_ftell(pb);
    put_amf_double(pb, 0); // delayed write

    put_amf_string(pb, "");
    put_byte(pb, AMF_END_OF_OBJECT);

    /* write total size of tag */
    data_size= url_ftell(pb) - metadata_size_pos - 10;
    url_fseek(pb, metadata_size_pos, SEEK_SET);
    put_be24(pb, data_size);
    url_fseek(pb, data_size + 10 - 3, SEEK_CUR);
    put_be32(pb, data_size + 11);
/*
    for (i = 0; i < s->nb_streams; i++) {
        AVCodecContext *enc = s->streams[i]->codec;
        if (enc->codec_id == CODEC_ID_AAC || enc->codec_id == CODEC_ID_H264) {
            int64_t pos;
            put_byte(pb, enc->codec_type == AVMEDIA_TYPE_VIDEO ?
                     FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);
            put_be24(pb, 0); // size patched later
            put_be24(pb, 0); // ts
            put_byte(pb, 0); // ts ext
            put_be24(pb, 0); // streamid
            pos = url_ftell(pb);
            if (enc->codec_id == CODEC_ID_AAC) {
                put_byte(pb, get_audio_flags(enc));
                put_byte(pb, 0); // AAC sequence header
                put_buffer(pb, enc->extradata, enc->extradata_size);
            } else {
                put_byte(pb, enc->codec_tag | FLV_FRAME_KEY); // flags
                put_byte(pb, 0); // AVC sequence header
                put_be24(pb, 0); // composition time
                ff_isom_write_avcc(pb, enc->extradata, enc->extradata_size);
            }
            data_size = url_ftell(pb) - pos;
            url_fseek(pb, -data_size - 10, SEEK_CUR);
            put_be24(pb, data_size);
            url_fseek(pb, data_size + 10 - 3, SEEK_CUR);
            put_be32(pb, data_size + 11); // previous tag size
        }
    }
*/
    put_flush_packet(pb);
    return 0;
}

int flv_write_trailer(AVFormatContext *s)
{
    int64_t file_size;

    ByteIOContext *pb = s->pb;
    FLVContext *flv = s->priv_data;

    file_size = url_ftell(pb);
	//print_level(INFO,"%s duration_offset %d\n",__FUNCTION__,flv->duration_offset);
	//print_level(INFO,"%s duration_seconds %lld\n",__FUNCTION__,flv->duration/ 1000000);
	//print_level(INFO,"%s filesize_offset %d\n",__FUNCTION__,flv->filesize_offset);
	//print_level(INFO,"%s file_size %lld\n",__FUNCTION__,file_size);

    /* update informations */
    url_fseek(pb, flv->duration_offset, SEEK_SET);
    put_amf_double(pb, flv->duration / (double)1000000);
    url_fseek(pb, flv->filesize_offset, SEEK_SET);
    put_amf_double(pb, file_size);

    url_fseek(pb, file_size, SEEK_SET);
    put_flush_packet(pb);
    return 0;
}

int flv_write_packet(AVFormatContext *s, AVPacket *pkt,flvAVCConf_t *pFlvAVCConf,flvAvInfo_t* pAvInfo)
{
    ByteIOContext *pb = s->pb;
    AVCodecContext *enc = s->streams[pkt->stream_index]->codec;
    FLVContext *flv = s->priv_data;
    unsigned ts;
    int size= pkt->size;
    //uint8_t *data= NULL;
    int flags, flags_size,size_flag=0;

unsigned char pucaOutRtmpBuffer[32];
//char PreTagSize[4];
//int writeNum = 0;
//ST_VIDEO_BUF *pstVideoBuffer = (ST_VIDEO_BUF *)Buffer;
int i, j, ucaData_offset = 0, sps_offset = 0, sps_len= 0, pps_offset= 0, ppslen= 0;


//    av_log(s, AV_LOG_DEBUG, "type:%d pts: %"PRId64" size:%d\n", enc->codec_type, timestamp, size);
	if( (pFlvAVCConf != NULL) && (pFlvAVCConf->configHead == 0)) {

		if(pFlvAVCConf->pkgCnt++ > 60){
			pFlvAVCConf->configHead =1;
			printf("FLV Can't Get AVC PPS and SPS, without set CODEC Config\n");
		}
		
		if(enc->codec_type != AVMEDIA_TYPE_VIDEO ){
			//printf("%s %d need avc config data to write header\n",__FUNCTION__,__LINE__);
			return 0;
		}
#if 0
	printf("%s line %d\n",__FUNCTION__,__LINE__);
	for(i = 0; i < pFlvAVCConf->len; i++) {
		printf("%x ",*(pFlvAVCConf->buf+i));
	}
	printf("\n");
#endif
	
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
	
#if 1
		if(pFlvAVCConf->len > 0){	
			for(i = 0; i < pFlvAVCConf->len -4; i++) {
				if(pFlvAVCConf->buf[i] == 0 && pFlvAVCConf->buf[i +1] == 0 && pFlvAVCConf->buf[i+2] == 0 && pFlvAVCConf->buf[i +3] == 1) {
					ucaData_offset = i +4;
					//cprintf("pstVideoBuffer->ucaData[ucaData_offset] & 0x0F = %x\n",pstVideoBuffer->ucaData[ucaData_offset] & 0x0F);
					if( (pFlvAVCConf->buf[ucaData_offset] & 0x0F) == 7) {
						sps_offset = ucaData_offset;
						//cprintf("sps_offset %d\n",sps_offset);
	
						for(j = ucaData_offset; j < pFlvAVCConf->len  - ucaData_offset -4 ; j++) {
							if(pFlvAVCConf->buf[j] == 0 && pFlvAVCConf->buf[j +1] == 0 && pFlvAVCConf->buf[j+2] == 0 && pFlvAVCConf->buf[j +3] == 1 ) {
								pps_offset = j +4;
								sps_len = pps_offset -4 - sps_offset;
								ppslen =4;
								//ucaData_offset = pps_offset + ppslen + 4;
								ucaData_offset = 0;
								break;
								//cprintf("sps_len %d,pps_offset %d\n",sps_len,pps_offset);
							}
						}
						break;
					}
					else {
						break;
					}
				}
			}
		}else {
			printf("%s line %d, pFlvAVCConf->len %d\n",__FUNCTION__,__LINE__,pFlvAVCConf->len);
		}
		
#endif
	   }

	   if(sps_offset > 0) {
		unsigned char configHeader[64];
		int configHeaderLen = 0;
	
		pFlvAVCConf->configHead = 1;
	
	/*
	configurationVersion = 01
	AVCProfileIndication = 4D
	profile_compatibility = 40
	AVCLevelIndication = 15
	lengthSizeMinusOne = FF <- 非常重要，是 H.264 视频中 NALU 的长度，计算方法是 1 + (lengthSizeMinusOne & 3)
	numOfSequenceParameterSets = E1 <- SPS 的个数，计算方法是 numOfSequenceParameterSets & 0x1F
	sequenceParameterSetLength = 00 0A <- SPS 的长度
	sequenceParameterSetNALUnits = 67 4D 40 15 96 53 01 00 4A 20 <- SPS
	numOfPictureParameterSets = 01 <- PPS 的个数
	pictureParameterSetLength = 00 05 <- PPS 的长度
	pictureParameterSetNALUnits = 68 E9 23 88 00 <- PPS
	
	*/
	//	printf("%s line %d\n",__FUNCTION__,__LINE__);
		
		configHeader[0] = 0;
		configHeader[1] = 0x01;//onfigurationVersion 
		
		configHeader[2] = *(pFlvAVCConf->buf +sps_offset +1);//AVCProfileIndication 
		configHeader[3] = *(pFlvAVCConf->buf +sps_offset +2);//profile_compatibility 
		configHeader[4] = *(pFlvAVCConf->buf +sps_offset +3);//AVCLevelIndication 
		
		configHeader[5] = 0xFF;//lengthSizeMinusOne 
		configHeader[6] = 0xE1;//numOfSequenceParameterSets 
		configHeader[7] = sps_len >>8 ;//sequenceParameterSetLength 
		configHeader[8] = sps_len;//sequenceParameterSetLength
		configHeader[9] = 0;//sequenceParameterSetNALUnits 
	
		memcpy(&configHeader[9], pFlvAVCConf->buf +sps_offset,sps_len);
		configHeader[9+ sps_len] = 1;//numOfPictureParameterSets
		configHeader[9+ sps_len +1] = ppslen >>8 ;//sequenceParameterSetLength 
		configHeader[9+ sps_len +2] = ppslen;//sequenceParameterSetLength
	
		memcpy(&configHeader[9 + sps_len +3], pFlvAVCConf->buf +pps_offset ,ppslen);
		configHeaderLen = 9 + sps_len +3 +ppslen;
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
	
		/* F4V tag header */
		/* stream type */
		pucaOutRtmpBuffer[0] = 0x09;
		uint32_t mediasize = configHeaderLen + 4; /* 1byte flag + 1byte mediatype + 3bytes cts + 4bytes totalnallen */
		pucaOutRtmpBuffer[1] = mediasize >> 16; 
		pucaOutRtmpBuffer[2] = mediasize >> 8;
		pucaOutRtmpBuffer[3] = mediasize;
	
		/* dts */
		unsigned int cts = 0;//pstVideoBuffer->unPTS - pstVideoBuffer->unDTS; 
		unsigned int dts = 0;//pstVideoBuffer->unDTS; 
	
		cts /= 1000;
		dts /= 1000;
	
		/* add 500ms to correct the AV out of Sync from QL201B */
		//cts += 500;
		//printf("Aikoc dts->%u, cts->%u\n", dts, cts);
	
		pucaOutRtmpBuffer[4] = (dts >> 16) & 0xFF; 
		pucaOutRtmpBuffer[5] = (dts >> 8) & 0xFF;
		pucaOutRtmpBuffer[6] = dts & 0xFF;
		pucaOutRtmpBuffer[7] = (dts >> 24) & 0x7F; 
	
		/* reserve streamId*/ 
		pucaOutRtmpBuffer[8] = 0;
		pucaOutRtmpBuffer[9] = 0;
		pucaOutRtmpBuffer[10] = 0;
	
		/* F4V tag mediadatas */
		/* Media Flag */
			pucaOutRtmpBuffer[11] = 0x17;
	
		/* NALU */
		pucaOutRtmpBuffer[12] = 0x00;
	
		pucaOutRtmpBuffer[13] = 0;
		pucaOutRtmpBuffer[14] = 0;
	
    put_buffer(pb, (unsigned char *)pucaOutRtmpBuffer, 15);
    put_buffer(pb, (unsigned char *)configHeader, configHeaderLen);
#if 0
	//		writeNum = fwrite(pucaOutRtmpBuffer, 1, 15, fp);
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
		/* copy the nal datas */
		do{
			writeNum = fwrite(configHeader, 1, configHeaderLen, fp);
			if(writeNum > 0){
				configHeaderLen -=writeNum;
			} else {
					printf("%s %s line %d, write FLV Buffer  Failed \n",__FILE__,__FUNCTION__, __LINE__);
					return Err_DrvHddError;
					//return -1;
			}
		} while(writeNum >= 0 && configHeaderLen > 0 );
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
	
		/* F4V prev tag length*/
		/* prev tag length */
		PreTagSize[0] = (mediasize+11) >> 24; /* mediasize + 11bytes header */ 
		PreTagSize[1] = (mediasize+11) >> 16;
		PreTagSize[2] = (mediasize+11) >> 8;
		PreTagSize[3] =  mediasize+11;
	
		fwrite(PreTagSize, 1, 4, fp);
	
	//FILL AUDIO Config
	//	 printf("%s line %d\n",__FUNCTION__,__LINE__);
	//	 printf("%s line %d\n",__FUNCTION__,__LINE__);
#endif	
	   put_be32(pb,mediasize+11); // previous tag size

	   if(pAvInfo->audiocodecid >= 0){
		  printf("%s line %d\n",__FUNCTION__,__LINE__);
		  //FillFlvAudioConfigPacket(fp,dts);
		}
	
	   }

	if(pFlvAVCConf->configHead == 0)
		return 0;


    if( enc->codec_id == CODEC_ID_AAC)
        flags_size= 2;
    else if(enc->codec_id == CODEC_ID_H264)
        flags_size= 5;
    else
        flags_size= 1;

    if (enc->codec_type == AVMEDIA_TYPE_VIDEO) {
        put_byte(pb, FLV_TAG_TYPE_VIDEO);
		size_flag = 4;
        flags = enc->codec_tag;
        if(flags == 0) {
            printf( "video codec %X not compatible with flv\n",enc->codec_id);
            return -1;
        }

        //flags |= pkt->flags & AV_PKT_FLAG_KEY ? FLV_FRAME_KEY : FLV_FRAME_INTER;
        flags |= pkt->flags & AV_PKT_FLAG_KEY ? FLV_FRAME_KEY : FLV_FRAME_INTER;
    } else {
        if (enc->codec_type == AVMEDIA_TYPE_AUDIO){
        	flags = get_audio_flags(enc);
		//	flags = 16;//ADPCM 5512byte
	        //assert(size);
			if(size == 0 ){
				printf("FLV AVMEDIA_TYPE_AUDIO size 0 \n");
				return 0;
			}
			//printf("FLV_TAG_TYPE_AUDIO add \n");
	        put_byte(pb, FLV_TAG_TYPE_AUDIO);
			
        }else {
        	if (enc->codec_type == AVMEDIA_TYPE_DATA){
	        	//flags = get_audio_flags(enc);
	        	flags = 0;
				//flags_size = 0;
				if(size == 0 )return 0;
		        put_byte(pb, FLV_TAG_TYPE_META);
        	}
        }
    }
#if 0
    if (enc->codec_id == CODEC_ID_H264) {
        /* check if extradata looks like mp4 formated */
        if (enc->extradata_size > 0 && *(uint8_t*)enc->extradata != 1) {
            if (ff_avc_parse_nal_units_buf(pkt->data, &data, &size) < 0)
                return -1;
        }
        if (!flv->delay && pkt->dts < 0)
            flv->delay = -pkt->dts;
    }
#endif 
    ts = pkt->dts /1000;//+ flv->delay; // add delay to force positive dts
    put_be24(pb,size + flags_size +size_flag);
    put_be24(pb,ts);
    put_byte(pb,(ts >> 24) & 0x7F); // timestamps are 32bits _signed_
    put_be24(pb,flv->reserved);
	
    put_byte(pb,flags);
//    if (enc->codec_id == CODEC_ID_VP6)
//    	  put_byte(pb,0);
//    if (enc->codec_id == CODEC_ID_VP6F)
//        put_byte(pb, enc->extradata_size ? enc->extradata[0] : 0);
//    else 
	if (enc->codec_id == CODEC_ID_AAC)
        put_byte(pb,1); // AAC raw
    else if (enc->codec_id == CODEC_ID_H264) {
        put_byte(pb,1); // AVC NALU
        put_be24(pb,pkt->pts - pkt->dts);
    }
	
    if(size_flag)put_be32(pb,size);

    //put_buffer(pb, data ? data : pkt->data, size);
    put_buffer(pb, pkt->data, size);

    put_be32(pb,size+flags_size+11 +size_flag ); // previous tag size 
    if (enc->codec_type != AVMEDIA_TYPE_DATA)flv->duration = FFMAX(flv->duration, pkt->pts + flv->delay + pkt->duration);

	s->buffer_data_len += size+flags_size+11 +size_flag;

    //put_flush_packet(pb);

    //av_free_t(data);

   	if(pb->error != 0) 
		return -1;
    return pb->error;//0
}

/*
AVOutputFormat flv_muxer = {
    "flv",
    NULL_IF_CONFIG_SMALL("FLV format"),
    "video/x-flv",
    "flv",
    sizeof(FLVContext),
#if CONFIG_LIBMP3LAME
    CODEC_ID_MP3,
#else // CONFIG_LIBMP3LAME
    CODEC_ID_ADPCM_SWF,
#endif // CONFIG_LIBMP3LAME
    CODEC_ID_FLV1,
    flv_write_header,
    flv_write_packet,
    flv_write_trailer,
    .codec_tag= (const AVCodecTag* const []){flv_video_codec_ids, flv_audio_codec_ids, 0},
    .flags= AVFMT_GLOBALHEADER | AVFMT_VARIABLE_FPS,
};
*/
