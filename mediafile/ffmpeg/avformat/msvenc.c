/*
 * MSV encoder.
 * Copyright (c) 2003 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef CONFIG_WIN32
#include "stdafx.h"
#endif
#include "msv.h"
#include "avio.h"
#include "intfloat_readwrite.h"
#include "avc.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/time.h> 
#include <time.h> 
#include <stdio.h>
//#include "dvripcpacket.h"

#undef NDEBUG
#include <assert.h>



static const AVCodecTag msv_video_codec_ids[] = {
    {CODEC_ID_MSV1,    MSV_CODECID_H263  },
    {CODEC_ID_H264,    MSV_CODECID_H264  },
    {CODEC_ID_NONE,    0}
};

static const AVCodecTag msv_audio_codec_ids[] = {
    {CODEC_ID_ADPCM_SWF, MSV_CODECID_ADPCM  >> MSV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_AAC,       MSV_CODECID_AAC    >> MSV_AUDIO_CODECID_OFFSET},
    {CODEC_ID_NONE,      0}
};

/*
static int get_audio_flags_t(AVCodecContext_b *enc){
    int flags = (enc->bits_per_sample == 16) ? 0x2 : 0x0;

    switch (enc->sample_rate) {
        case    44100:
            flags |= 0x0C;
            break;
        case    22050:
            flags |= 0x08;
            break;
        case    11025:
            flags |= 0x04;
            break;
        case     8000: //nellymoser only
        case     5512: //not mp3
            flags |= 0x00;
            break;
        default:
            return -1;
    }

    if (enc->channels > 1) {
        flags |= 0x01;
    }

    switch(enc->codec_id){
    case CODEC_ID_MP3:
        flags |= 0x20 | 0x2;
        break;
    case CODEC_ID_PCM_S8:
        break;
    case CODEC_ID_PCM_S16BE:
        flags |= 0x60 | 0x2;
        break;
    case CODEC_ID_PCM_S16LE:
        flags |= 0x2;
        break;
    case CODEC_ID_ADPCM_SWF:
        flags |= 0x10;
        break;
    default:
        return -1;
    }

    return flags;
}
*/

static int get_audio_flags(AVStream *stream){
    int flags = (stream->bits_per_sample == 16) ? 0x2 : 0x0;

    switch (stream->sample_rate) {
        case    44100:
            flags |= 0x0C;
            break;
        case    22050:
            flags |= 0x08;
            break;
        case    11025:
            flags |= 0x04;
            break;
        case     8000: //nellymoser only
        case     5512: //not mp3
            flags |= 0x00;
            break;
        default:
            return -1;
    }

    if (stream->channels > 1) {
        flags |= 0x01;
    }

    switch(stream->codec_id){
    case CODEC_ID_MP3:
        flags |= 0x20 | 0x2;
        break;
    case CODEC_ID_PCM_S8:
        break;
    case CODEC_ID_PCM_S16BE:
        flags |= 0x60 | 0x2;
        break;
    case CODEC_ID_PCM_S16LE:
        flags |= 0x2;
        break;
	case CODEC_ID_ADPCM_SWF:
	case CODEC_ID_ADPCM_MS:
	case CODEC_ID_ADPCM_IMA_QT:
        flags |= 0x10;
        break;
    default:
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

static void put_amf_string_content(ByteIOContext *pb, const char *str)
{
    size_t len = strlen(str);
    put_byte(pb, AMF_DATA_TYPE_STRING);
    put_be16(pb, len);
    put_buffer(pb, (const unsigned char *)str, len);
}


static void put_amf_double(ByteIOContext *pb, double d)
{
    put_byte(pb, AMF_DATA_TYPE_NUMBER);
    put_be64(pb, av_dbl2int(d));
}

static void put_amf_be64(ByteIOContext *pb, uint64_t d)
{
    put_byte(pb, AMF_DATA_TYPE_NUMBER);
    put_be64(pb, d);
}


static void put_amf_bool(ByteIOContext *pb, int b) {
    put_byte(pb, AMF_DATA_TYPE_BOOL);
    put_byte(pb, !!b);
}




int msv_write_header(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    MSVContext *msv = (MSVContext *)s->priv_data;
    int audio_enc = 0, video_enc = 0, data_enc = 0;
    int i;
#ifdef USESYNC
		int sync_header_size =4;
#else
		int sync_header_size =0;
#endif
	uint64_t encry;
    double framerate = 0.0;
    unsigned int metadata_size_pos, data_size;

	msv->posListNum = 0;

	
	s->file_size = 0;
    for(i=0; i<MAX_STREAMS; i++)
	{
		if(s->streams[i]== NULL)
			continue;	
        if (s->streams[i]->codec_type == CODEC_TYPE_VIDEO)
		{
            video_enc++;
        } 
		else  if (s->streams[i]->codec_type == CODEC_TYPE_AUDIO)
		{
            audio_enc++;
        }
		else  if (s->streams[i]->codec_type == CODEC_TYPE_DATA)
		{
            data_enc++;
        }
    }
	memcpy(&encry,s->encryptData,8);
	//文件头
    //put_tag(pb,"MSV");
	put_tag(pb,"MSV");
    put_byte(pb,1);
    put_byte(pb,   MSV_HEADER_FLAG_HASAUDIO * video_enc			//视频流数
                 + MSV_HEADER_FLAG_HASVIDEO * audio_enc			//音频流数
                 + MSV_HEADER_FLAG_HASDATA * data_enc);			//数据流数
    put_be32(pb,9);

	//第一个Previous Tag Size
    put_be32(pb,0);
#ifdef USESYNC

	//packet sync Header
    put_byte(pb,0x00);
    put_byte(pb,0x00);
    put_byte(pb,0x01);
    put_byte(pb,0xFF);
#endif

    /* write meta_tag */
    put_byte(pb, 18);         // tag type META		//tag 的类型
    metadata_size_pos= url_ftell(pb);
    put_be24(pb, 0);          // size of data part (sum of all parts below)  //表示该Tag Data部分的大小
    put_be24(pb, 0);          // time stamp			//时间戳
    put_be32(pb, 0);          // reserved


    /* now data of data_size size */
	//第一个AMF开始
    /* first event name as a string */
    put_byte(pb, AMF_DATA_TYPE_STRING);
    put_amf_string(pb, "onMetaData"); // 12 bytes
	//结束第一个AMF

	//第二个AMF开始
    /* mixed array (hash) with size and string/type/data tuples */
    put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
    put_be32(pb, 6); // +2 for duration and file size

    put_amf_string(pb, "start_time");
    put_amf_double(pb, time(NULL));
	//printf("start_time %ld nb_streams %d\n",time(NULL),s->nb_streams);

    put_amf_string(pb, "duration");
    msv->duration_offset= url_ftell(pb);
    put_amf_double(pb, 0); // delayed write

    put_amf_string(pb, "filesize");
    msv->filesize_offset= url_ftell(pb);
    put_amf_double(pb, 0); // delayed write
    
    put_amf_string(pb, "encryptData");
    put_amf_be64(pb, encry);

    put_amf_string(pb, "frameListPosition");
	msv->frameListPosition_offset= url_ftell(pb);
    put_amf_be64(pb, 0);

    put_amf_string(pb, "busnum");
    put_amf_string_content(pb, msv->busnum);

    put_amf_string(pb, "");
    put_byte(pb, AMF_END_OF_OBJECT);
	//数组结束 ，结束第二个AMF

	//具体流信息
//printf("msv->filesize_offset %d\n",msv->filesize_offset);

    for(i=0; i<MAX_STREAMS; i++)
	{
		//printf("nb_streams index  %d\n",i);
	
		if(s->streams[i]== NULL)
			continue;
	//printf("nb_streams %d codec_type %d\n",s->nb_streams,s->streams[i]->codec_type);
        if (s->streams[i]->codec_type == CODEC_TYPE_VIDEO) 
		{
            if (s->streams[i]->r_frame_rate.den && s->streams[i]->r_frame_rate.num) 
			{
                framerate = av_q2d(s->streams[i]->r_frame_rate);
            } 
			else 
			{
                framerate = 1/av_q2d(s->streams[i]->time_base);
            }
//printf("r_frame_rate.den  %d, r_frame_rate.num %d\n",s->streams[i]->r_frame_rate.den , s->streams[i]->r_frame_rate.num);
			
			put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
			put_be32(pb, 7);

			put_amf_string(pb, "streamtype");
			put_amf_double(pb, s->streams[i]->codec_type);
//printf("streamtype %d\n",s->streams[i]->codec_type);
			put_amf_string(pb, "streamid");
			put_amf_double(pb, s->streams[i]->id);
//printf("streamid %d\n",s->streams[i]->id);


			put_amf_string(pb, "width");
			put_amf_double(pb, s->streams[i]->width);
//printf("width %d\n",s->streams[i]->width);

			put_amf_string(pb, "height");
			put_amf_double(pb, s->streams[i]->height);
//printf("height %d\n",s->streams[i]->height);

			put_amf_string(pb, "videodatarate");
			put_amf_double(pb, s->streams[i]->bit_rate / 1024.0);
//printf("bit_rate %d, videodatarate %f\n",s->streams[i]->bit_rate ,s->streams[i]->bit_rate / 1024.0);

			put_amf_string(pb, "framerate");
			put_amf_double(pb, framerate);
//printf("framerate %f\n",framerate);

			put_amf_string(pb, "videocodecid");
			put_amf_double(pb, s->streams[i]->codec_id);
			//printf("videocodecid 0x%x\n",s->streams[i]->codec_id);

			put_amf_string(pb, "");
			put_byte(pb, AMF_END_OF_OBJECT);
        } 
		else if (s->streams[i]->codec_type == CODEC_TYPE_AUDIO)
		{
			//printf("msv enc CODEC_TYPE_AUDIO\n");
            if(get_audio_flags(s->streams[i])<0){
				printf("CODEC_TYPE_AUDIO get_audio_flags error!\n");
                return -1;
            }
			
			put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
			put_be32(pb, 6);

			put_amf_string(pb, "streamtype");
			put_amf_double(pb, s->streams[i]->codec_type);

			put_amf_string(pb, "streamid");
			put_amf_double(pb, s->streams[i]->id);

			put_amf_string(pb, "audiodatarate");
			put_amf_double(pb, s->streams[i]->bit_rate / 1024.0);

			put_amf_string(pb, "audiosamplerate");
			put_amf_double(pb, s->streams[i]->sample_rate);

			put_amf_string(pb, "audiosamplesize");
			put_amf_double(pb, s->streams[i]->codec_id == CODEC_ID_PCM_S8 ? 8 : 16);

			put_amf_string(pb, "stereo");
			put_amf_bool(pb, s->streams[i]->channels == 2);

			put_amf_string(pb, "audiocodecid");
			put_amf_double(pb, s->streams[i]->codec_id);
			//printf("audiocodecid 0x%x\n",s->streams[i]->codec_id);

			put_amf_string(pb, "");
			put_byte(pb, AMF_END_OF_OBJECT);

        }
		else if(s->streams[i]->codec_type == CODEC_TYPE_DATA)
		{
			put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
			put_be32(pb, 2);
			
			put_amf_string(pb, "streamtype");
			put_amf_double(pb, s->streams[i]->codec_type);
			
			put_amf_string(pb, "streamid");
			put_amf_double(pb, s->streams[i]->id);
			put_amf_string(pb, "");
			put_byte(pb, AMF_END_OF_OBJECT);
	    }       
		av_set_pts_info(s->streams[i], 32, 1, 1000); /* 32 bit pts in ms */
    }


    /* write total size of tag */
    data_size= url_ftell(pb) - metadata_size_pos - 10;
    url_fseek(pb, metadata_size_pos, SEEK_SET);
    put_be24(pb, data_size);
    url_fseek(pb, data_size + 10 - 3, SEEK_CUR);
    put_be32(pb, data_size + 11 +sync_header_size);

put_flush_packet(pb);
//printf("%s %d\n",__FUNCTION__,__LINE__);

//printf("data_size %d\n" ,data_size);
    return 0;
}

#if FRAME_POSLIST	

int msv_write_frameList(AVFormatContext *s)
{
    int64_t frameListPos;

    ByteIOContext *pb = s->pb;
    MSVContext *msv = (MSVContext *)s->priv_data;
    int metadata_size_pos, data_size,i;
	assert(msv);
#ifdef USESYNC
			int sync_header_size =4;
#else
			int sync_header_size =0;
#endif

	msv->frameListPos = url_ftell(pb);

#ifdef USESYNC
		//packet sync Header
		put_byte(pb,0x00);
		put_byte(pb,0x00);
		put_byte(pb,0x01);
		put_byte(pb,0xFF);
#endif
	
		/* write meta_tag */
		put_byte(pb, 18);		  // tag type META		//tag 的类型
		metadata_size_pos= url_ftell(pb);
		put_be24(pb, 0);		  // size of data part (sum of all parts below)  //表示该Tag Data部分的大小
		put_be24(pb, msv->duration/1000);		  //sec Media Duration
		put_be32(pb, 0);		  // reserved
	
	
		/* now data of data_size size */
		//第一个AMF开始
		/* first event name as a string */
		put_byte(pb, AMF_DATA_TYPE_STRING);
		put_amf_string(pb, "onPosData"); // 12 bytes
		//结束第一个AMF
	
		//第二个AMF开始
		/* mixed array (hash) with size and string/type/data tuples */
		put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
		put_be32(pb, msv->posListNum); // +2 for duration and file size
		for(i = 0; i < msv->posListNum; i++){
			put_amf_string(pb, "pos");
			put_amf_be64(pb, msv->posList[i]);
		}
	
		put_amf_string(pb, "");
		put_byte(pb, AMF_END_OF_OBJECT);

    /* write total size of tag */
    data_size= url_ftell(pb) - metadata_size_pos - 10;
    url_fseek(pb, metadata_size_pos, SEEK_SET);
    put_be24(pb, data_size);
    url_fseek(pb, data_size + 10 - 3, SEEK_CUR);
    put_be32(pb, data_size + 11 +sync_header_size);

    return 0;
}
#endif

void dvr_flush_buffer(AVFormatContext *s)
{
	if(s == NULL)
		return;
    ByteIOContext *pb = s->pb;
	if(pb != NULL){
		put_flush_packet(pb);
		s->buffer_data_len = 0;
	}

}

int msv_write_trailer(AVFormatContext *s)
{
    int64_t file_size;

    ByteIOContext *pb = s->pb;
    MSVContext *msv = (MSVContext *)s->priv_data;
	assert(msv);
#if FRAME_POSLIST	
	msv_write_frameList(s);
#endif
	
    file_size = url_ftell(pb);
	
	printf("Msv trailer duration %lld %f us, filesize_offset %lld file_size %lld %f\n",msv->duration,msv->duration / (double)1000,msv->filesize_offset,s->file_size,(double)s->file_size);
	//printf("msv->filesize_offset %d\n",msv->filesize_offset);
    /* update informations */
    url_fseek(pb, msv->duration_offset, SEEK_SET);
    put_amf_double(pb, msv->duration / (double)1000);
    url_fseek(pb, msv->filesize_offset, SEEK_SET);
    //put_amf_double(pb, file_size);
	put_amf_double(pb,(double)s->file_size);
#if FRAME_POSLIST	
    url_fseek(pb, msv->frameListPosition_offset, SEEK_SET);
	put_amf_double(pb,(double)msv->frameListPos);
#endif

	
    url_fseek(pb, file_size, SEEK_SET);
	
	s->file_size+=file_size;
	put_flush_packet(pb);
	//printf("%s %d\n",__FUNCTION__,__LINE__);
    return 0;
}

int msv_update_info(AVFormatContext *s)
{
	int64_t file_size;

	ByteIOContext *pb = s->pb;
	MSVContext *msv = (MSVContext *)s->priv_data;
	assert(msv);
	
	file_size = url_ftell(pb);
	
	printf("Msv_update_info duration %lld %f us, filesize_offset %lld file_size %lld %f\n",msv->duration,msv->duration / (double)1000,msv->filesize_offset,s->file_size,(double)s->file_size);
	//printf("msv->filesize_offset %d\n",msv->filesize_offset);
	/* update informations */
	url_fseek(pb, msv->duration_offset, SEEK_SET);
	put_amf_double(pb, msv->duration / (double)1000);
	url_fseek(pb, msv->filesize_offset, SEEK_SET);
	//put_amf_double(pb, file_size);
	put_amf_double(pb,(double)s->file_size);

	url_fseek(pb, file_size, SEEK_SET);
	
	put_flush_packet(pb);
	//printf("%s %d\n",__FUNCTION__,__LINE__);
	return 0;

}

int msv_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    ByteIOContext *pb = s->pb;
    MSVContext *msv = (MSVContext *)s->priv_data;
    unsigned int ts;
    unsigned int size= pkt->size;
    int flags=0, flags_size=0;
#ifdef USESYNC
	int sync_header_size =4;
#else
	int sync_header_size =0;
#endif
	//printf("%s %d\n",__FUNCTION__,__LINE__);
    flags_size= 1; //类型标志
	s->file_size += size;
	//printf("%s %d\n",__FUNCTION__,__LINE__);

	if(s->streams[pkt->stream_id]==NULL){
		printf("s->streams[%d] NULL\n",pkt->stream_id);
	}
	//printf("%s %d\n",__FUNCTION__,__LINE__);
#if FRAME_POSLIST	
	int64_t pktPos;
    pktPos = url_ftell(pb);
#endif

#ifdef USESYNC
	//packet sync Header
    put_byte(pb,0x00);
    put_byte(pb,0x00);
    put_byte(pb,0x01);
    put_byte(pb,0xFF);
#endif
    if (s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_VIDEO) 
	{
        flags |= pkt->flags & PKT_FLAG_KEY ? MSV_FRAME_KEY : MSV_FRAME_INTER;
        put_byte(pb, MSV_TAG_TYPE_VIDEO);
    } 
	else if (s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_AUDIO) 
	{
        flags = get_audio_flags(s->streams[pkt->stream_id]);
        assert(size);
        put_byte(pb, MSV_TAG_TYPE_AUDIO);
    }
	else
	{
		flags = 0;
        assert(s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_DATA);
        assert(size);
        put_byte(pb, MSV_TAG_TYPE_DATA);
    }
	//printf("%s %d\n",__FUNCTION__,__LINE__);

    if (s->streams[pkt->stream_id]->codec_id == CODEC_ID_H264) 
	{
        //if (!msv->delay && pkt->dts < 0)
        //    msv->delay = -pkt->dts;
    }
	//printf("%s %d\n",__FUNCTION__,__LINE__);

    //ts = pkt->dts + msv->delay; // add delay to force positive dts
    ts = pkt->dts; // add delay to force positive dts
    put_be24(pb,size + flags_size);
    put_be24(pb,ts);
    //put_byte(pb,(ts >> 24) & 0x7F); // timestamps are 32bits _signed_
    put_byte(pb,(ts >> 24) & 0xFF); // timestamps are 32bits unsigned _signed_
	put_be24(pb,pkt->stream_id);
    put_byte(pb,flags);

	//printf("pkt flags %d\n",flags);

	//实际数据
    put_buffer(pb, pkt->data, pkt->size);

    put_be32(pb,size+flags_size+11 +sync_header_size); // previous tag size
	//printf("%s %d msv->duration %lld,pkt->pts %lld+ msv->delay %d + pkt->duration %d\n",__FUNCTION__,__LINE__,msv->duration,pkt->pts , msv->delay , pkt->duration);
    msv->duration = FFMAX(msv->duration, pkt->pts + msv->delay + pkt->duration);

#if FRAME_POSLIST	
	if(flags && (s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_VIDEO)){
		unsigned int dur = msv->duration;
		if((msv->posListNum< MSV_POSLISTNUM) && (msv->posListNum*10000000 < dur)){
			memcpy(&(msv->posList[msv->posListNum]),&pktPos,sizeof(int64_t));
			printf("MSV Add PosList[%d]= 0x%x\n",msv->posListNum,msv->posList[msv->posListNum]);
			//printf("msv->posListNum*10000000 %d,msv->duration %d\n",msv->posListNum*10000000,dur);
			msv->posListNum++;
		}
	}
#endif
	s->buffer_data_len += size+flags_size+11 +sync_header_size;

	//printf("%s %d pb->error %d\n",__FUNCTION__,__LINE__,pb->error);
   // put_flush_packet(pb);
   	if(pb->error != 0) 
		return -1;
    return pb->error;//0
}

#if 0
int msv_write_video_packet(AVFormatContext *s, AVPacket *pkt,VENC_STREAM_S *pststream)
{
    ByteIOContext *pb = s->pb;
    MSVContext *msv = (MSVContext *)s->priv_data;
    unsigned ts;
    //int size= pkt->size;
    int size= 0;
    int flags, flags_size;//,frameCnt;
	int i=0;
	//printf("pstStream->u32PackCount%d\n",pststream->u32PackCount);
//	printf("%s %d\n",__FUNCTION__,__LINE__);

	for (i = 0; i < pststream->u32PackCount; i++)
	{
		size+=pststream->pstPack[i].u32Len[0];
		if(pststream->pstPack[i].u32Len[1] > 0){
			size+=pststream->pstPack[i].u32Len[1];
		}
		
#if 0	
		//printf("u32Len[0]%d ,pts %x\n",stStream.pstPack[i].u32Len[0],stStream.pstPack[i].u64PTS);
		if(pststream->pstPack[i].u32Len[0] <13){
			int i =0;
			for(i; i< pststream->pstPack[i].u32Len[0]; i++){
				printf("%02x ",*((pststream->pstPack[i].pu8Addr[0])+i));
			}
		}
		printf("\nu32Len[1]%d\n",pststream->pstPack[i].u32Len[1]);
#endif	
	}
	s->file_size += size;

    flags_size= 1; //类型标志

    if (s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_VIDEO) 
	{
        flags |= pkt->flags & PKT_FLAG_KEY ? MSV_FRAME_KEY : MSV_FRAME_INTER;
        put_byte(pb, MSV_TAG_TYPE_VIDEO);
    } 
	else if (s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_AUDIO) 
	{
        flags = get_audio_flags(s->streams[pkt->stream_id]);
        assert(size);
        put_byte(pb, MSV_TAG_TYPE_AUDIO);
    }
	else
	{
		flags = 0;
        assert(s->streams[pkt->stream_id]->codec_type == CODEC_TYPE_DATA);
        assert(size);
        put_byte(pb, MSV_TAG_TYPE_DATA);
    }
	//printf("%s %d\n",__FUNCTION__,__LINE__);

    if (s->streams[pkt->stream_id]->codec_id == CODEC_ID_H264) 
	{
        if (!msv->delay && pkt->dts < 0)
            msv->delay = -pkt->dts;
    }
	//frameCnt = pkt->frameCnt;
    ts = pkt->dts + msv->delay; // add delay to force positive dts
    put_be24(pb,size + flags_size);
    put_be24(pb,ts);
    put_byte(pb,(ts >> 24) & 0x7F); // timestamps are 32bits _signed_
	put_be24(pb,pkt->stream_id);
    //put_byte(pb,frameCnt);
    put_byte(pb,flags);

	//printf("%s %d\n",__FUNCTION__,__LINE__);

	for (i = 0; i < pststream->u32PackCount; i++){
		//实际数据
    	//put_buffer(pb, pkt->data, pkt->size);
    	put_buffer(pb, pststream->pstPack[i].pu8Addr[0], pststream->pstPack[i].u32Len[0]);
		if(pststream->pstPack[i].u32Len[1] > 0){
			put_buffer(pb, pststream->pstPack[i].pu8Addr[1], pststream->pstPack[i].u32Len[1]);
			//printf("pststream->pstPack[%d].u32Len[1] %d\n",i,pststream->pstPack[i].u32Len[1]);
		}
	}
	//printf("%s %d\n",__FUNCTION__,__LINE__);

    put_be32(pb,size+flags_size+11); // previous tag size
    msv->duration = FFMAX(msv->duration, pkt->pts + msv->delay + pkt->duration);
    //put_flush_packet(pb);
	//printf("%s %d\n",__FUNCTION__,__LINE__);

   	if(pb->error != 0) 
		return Err_DrvHddError;

	return pb->error;;

}
#endif


