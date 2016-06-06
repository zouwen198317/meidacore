/*
 * MSV demuxer
 * Copyright (c) 2003 The FFmpeg Project
 *
 * This demuxer will generate a 1 byte extradata for VP6F content.
 * It is composed of:
 *  - upper 4bits: difference between encoded width and visible width
 *  - lower 4bits: difference between encoded height and visible height
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

#ifdef CONFIG_WIN32
#include "stdafx.h"
#endif
#include "msv.h"
#include "intfloat_readwrite.h"
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#ifndef CONFIG_WIN32
#include <stdio.h>
#include <linux/errno.h>
#endif
#include "assert.h"
#include "utils.h"

#ifdef USESYNC
unsigned int SYNChead = 0x1FF;//0x000001FF; 

#endif
#if USESYNC
#define SYNCLEN 4
#else
#define SYNCLEN 0

#endif
/*
static int msv_probe(AVProbeData *p)
{
    const uint8_t *d;

    d = p->buf;
    if (d[0] == 'F' && d[1] == 'L' && d[2] == 'V' && d[3] < 5 && d[5]==0) {
        return AVPROBE_SCORE_MAX;
    }
    return 0;
}
*/
static void msv_set_audio_codec(AVFormatContext *s, AVStream *astream, int msv_codecid) {

    switch(msv_codecid) {
        //no distinction between S16 and S8 PCM codec flags
        case MSV_CODECID_PCM:
            astream->codec_id = astream->bits_per_coded_sample == 8 ? CODEC_ID_PCM_S8 :
#ifdef WORDS_BIGENDIAN
                                CODEC_ID_PCM_S16BE;
#else
                                CODEC_ID_PCM_S16LE;
#endif
            break;
        case MSV_CODECID_PCM_LE:
            astream->codec_id = astream->bits_per_coded_sample == 8 ? CODEC_ID_PCM_S8 : CODEC_ID_PCM_S16LE; break;
        case MSV_CODECID_AAC  : astream->codec_id = CODEC_ID_AAC;                                    break;
        case MSV_CODECID_ADPCM: astream->codec_id = CODEC_ID_ADPCM_SWF;                              break;
        case MSV_CODECID_NELLYMOSER_8KHZ_MONO:
            astream->sample_rate = 8000; //in case metadata does not otherwise declare samplerate
        default:
			astream->codec_id = msv_codecid;
            break;
    }
}

static int msv_set_video_codec(AVFormatContext *s, AVStream *vstream, int msv_codecid) {

    switch(msv_codecid) {
        case MSV_CODECID_H263  : vstream->codec_id = CODEC_ID_MSV1   ; break;
        case MSV_CODECID_H264:
            vstream->codec_id = CODEC_ID_H264;
            return 3; // not 4, reading packet type will consume one byte
        default:
			vstream->codec_id = msv_codecid;
           break;
    }

    return 0;
}

static int amf_get_string(ByteIOContext *ioc, char *buffer, int buffsize) {
    int length = get_be16(ioc);
    if(length >= buffsize) {
        url_fskip(ioc, length);
        return -1;
    }

    get_buffer(ioc, (unsigned char *)buffer, length);

    buffer[length] = '\0';

    return length;
}

static int amf_parse_object(AVFormatContext *s, AVStream *astream, AVStream *vstream, const char *key, int64_t max_pos, int depth) {
    ByteIOContext *ioc;
    AMFDataType amf_type;
    char str_val[256];
    double num_val;

    num_val = 0;
    ioc = s->pb;

    amf_type = (AMFDataType)get_byte(ioc);

    switch(amf_type) {
        case AMF_DATA_TYPE_NUMBER:
            num_val = av_int2dbl(get_be64(ioc)); break;
        case AMF_DATA_TYPE_BOOL:
            num_val = get_byte(ioc); break;
        case AMF_DATA_TYPE_STRING:
            if(amf_get_string(ioc, str_val, sizeof(str_val)) < 0)
                return -1;
            break;
        case AMF_DATA_TYPE_OBJECT: {
            unsigned int keylen;

            while(url_ftell(ioc) < max_pos - 2 && (keylen = get_be16(ioc))) {
                url_fskip(ioc, keylen); //skip key string
                if(amf_parse_object(s, NULL, NULL, NULL, max_pos, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
            if(get_byte(ioc) != AMF_END_OF_OBJECT)
                return -1;
        }
            break;
        case AMF_DATA_TYPE_NULL:
        case AMF_DATA_TYPE_UNDEFINED:
        case AMF_DATA_TYPE_UNSUPPORTED:
            break; //these take up no additional space
        case AMF_DATA_TYPE_MIXEDARRAY:
            url_fskip(ioc, 4); //skip 32-bit max array index
            while(url_ftell(ioc) < max_pos - 2 && amf_get_string(ioc, str_val, sizeof(str_val)) > 0) {
                //this is the only case in which we would want a nested parse to not skip over the object
                if(amf_parse_object(s, astream, vstream, str_val, max_pos, depth + 1) < 0)
                    return -1;
            }
            if(get_byte(ioc) != AMF_END_OF_OBJECT)
                return -1;
            break;
        case AMF_DATA_TYPE_ARRAY: {
            unsigned int arraylen, i;

            arraylen = get_be32(ioc);
            for(i = 0; i < arraylen && url_ftell(ioc) < max_pos - 1; i++) {
                if(amf_parse_object(s, NULL, NULL, NULL, max_pos, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
        }
            break;
        case AMF_DATA_TYPE_DATE:
            url_fskip(ioc, 8 + 2); //timestamp (double) and UTC offset (int16)
            break;
        default: //unsupported type, we couldn't skip
            return -1;
    }

    if(/*depth == 1 && */key) { //fixed me: 修改为遍历每一层//only look for metadata values when we are not nested and key != NULL
        if(amf_type == AMF_DATA_TYPE_BOOL) 
		{
            if(!strcmp(key, "stereo") && astream) astream->channels = num_val > 0 ? 2 : 1;
        } 
		else if(amf_type == AMF_DATA_TYPE_NUMBER) 
		{
            if(!strcmp(key, "streamid")) astream->id = num_val;
            else if(!strcmp(key, "duration")) s->duration = num_val * 1000;//IAN //AV_TIME_BASE;
//            else if(!strcmp(key, "width")  && vstream && num_val > 0) vstream->width  = num_val;
//            else if(!strcmp(key, "height") && vstream && num_val > 0) vstream->height = num_val;
            else if(!strcmp(key, "videodatarate") && vstream && 0 <= (int)(num_val * 1024.0))
                vstream->bit_rate = num_val * 1024.0;
            else if(!strcmp(key, "audiocodecid") && astream && 0 <= (int)num_val)
                msv_set_audio_codec(s, astream, (int)num_val << MSV_AUDIO_CODECID_OFFSET);
            else if(!strcmp(key, "videocodecid") && vstream && 0 <= (int)num_val)
                msv_set_video_codec(s, vstream, (int)num_val);
            else if(!strcmp(key, "audiosamplesize") && astream && 0 < (int)num_val) 
			{
                astream->bits_per_coded_sample = num_val;
                //we may have to rewrite a previously read codecid because MSV only marks PCM endianness.
                if(num_val == 8 && (astream->codec_id == CODEC_ID_PCM_S16BE || astream->codec_id == CODEC_ID_PCM_S16LE))
                    astream->codec_id = CODEC_ID_PCM_S8;
            }
            else if(!strcmp(key, "audiosamplerate") && astream && num_val >= 0) 
			{
                //some tools, like MSVTool2, write consistently approximate metadata sample rates
                if (!astream->sample_rate) 
				{
                    switch((int)num_val) 
					{
                        case 44000: astream->sample_rate = 44100  ; break;
                        case 22000: astream->sample_rate = 22050  ; break;
                        case 11000: astream->sample_rate = 11025  ; break;
                        case 5000 : astream->sample_rate = 5512   ; break;
                        default   : astream->sample_rate = num_val;
                    }
                }
            }
        }
    }

    return 0;
}

static int amf_parse_object_t(AVFormatContext *s, AVStream *stream, const char *key, int64_t max_pos, int depth) {
    ByteIOContext *ioc;
    AMFDataType amf_type;
    char str_val[256];
    double num_val;
	uint64_t num_val64;
    num_val = 0;
    ioc = s->pb;

    amf_type = (AMFDataType)get_byte(ioc);

    switch(amf_type) {
        case AMF_DATA_TYPE_NUMBER:
			if(!strcmp(key, "encryptData")){
				num_val64 = get_be64(ioc);
				//printf("%s encryptData AMF_DATA_TYPE_NUMBER\n",__FUNCTION__);
			}
			else
            	num_val = av_int2dbl(get_be64(ioc)); 
			break;
        case AMF_DATA_TYPE_BOOL:
            num_val = get_byte(ioc); break;
        case AMF_DATA_TYPE_STRING:
            if(amf_get_string(ioc, str_val, sizeof(str_val)) < 0)
                return -1;
            break;
        case AMF_DATA_TYPE_OBJECT: {
            unsigned int keylen;

            while(url_ftell(ioc) < max_pos - 2 && (keylen = get_be16(ioc))) {
                url_fskip(ioc, keylen); //skip key string
                if(amf_parse_object_t(s, NULL, NULL, max_pos, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
            if(get_byte(ioc) != AMF_END_OF_OBJECT)
                return -1;
        }
            break;
        case AMF_DATA_TYPE_NULL:
        case AMF_DATA_TYPE_UNDEFINED:
        case AMF_DATA_TYPE_UNSUPPORTED:
            break; //these take up no additional space
        case AMF_DATA_TYPE_MIXEDARRAY:
            url_fskip(ioc, 4); //skip 32-bit max array index
			//int num = get_be32(ioc);
			//for(int i = 0; i < num; i++)
			//{
			//}
            while(url_ftell(ioc) < max_pos - 2 && amf_get_string(ioc, str_val, sizeof(str_val)) > 0) {
                //this is the only case in which we would want a nested parse to not skip over the object
                if(amf_parse_object_t(s, stream, str_val, max_pos, depth + 1) < 0)
                    return -1;
            }
            if(get_byte(ioc) != AMF_END_OF_OBJECT)
                return -1;
            break;
        case AMF_DATA_TYPE_ARRAY: {
            unsigned int arraylen, i;

            arraylen = get_be32(ioc);
            for(i = 0; i < arraylen && url_ftell(ioc) < max_pos - 1; i++) {
                if(amf_parse_object_t(s, NULL, NULL, max_pos, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
        }
            break;
        case AMF_DATA_TYPE_DATE:
            url_fskip(ioc, 8 + 2); //timestamp (double) and UTC offset (int16)
            break;
        default: //unsupported type, we couldn't skip
            return -1;
    }

    if(/*depth == 2 &&*/ key) { //fixed me: 修改为遍历第二层//only look for metadata values when we are not nested and key != NULL
        
		if(amf_type == AMF_DATA_TYPE_NUMBER) 
		{
		
            if(!strcmp(key, "streamtype"))
			{
			    stream->codec_type = (enum CodecType)(int)num_val;
			}
            else if(!strcmp(key, "streamid"))
			{
				stream->id = num_val;
			}
            else if(!strcmp(key, "start_time")) s->start_time = num_val;
            else if(!strcmp(key, "duration")) {
					s->duration = num_val * 1000;
					//printf("%s duration %f\n",__FUNCTION__,num_val * 1000);
				}//AV_TIME_BASE; //ms
            else if(!strcmp(key, "filesize")) s->file_size= num_val;
			else if(!strcmp(key, "encryptData")) {
					memcpy(s->encryptData,&num_val64,8);
					//printf("%s encryptData %02x %02x %02x %02x\n",__FUNCTION__,s->encryptData[0],s->encryptData[1],s->encryptData[2],s->encryptData[3]);
				}

			else if(!strcmp(key, "width")  && stream && num_val > 0) stream->width  = num_val;
            else if(!strcmp(key, "height") && stream && num_val > 0) stream->height = num_val;
            else if(!strcmp(key, "framerate") && stream && num_val > 0){
				stream->r_frame_rate.num= num_val;
				stream->r_frame_rate.den= 1;
            }

			else if(!strcmp(key, "videodatarate") && stream && 0 <= (int)(num_val * 1024.0))
                stream->bit_rate = num_val * 1024.0;
            else if(!strcmp(key, "audiocodecid") && stream && 0 <= (int)num_val)
                msv_set_audio_codec(s, stream, (int)num_val /*<< MSV_AUDIO_CODECID_OFFSET*/);
            else if(!strcmp(key, "videocodecid") && stream && 0 <= (int)num_val)
                msv_set_video_codec(s, stream, (int)num_val);
            else if(!strcmp(key, "audiosamplesize") && stream && 0 < (int)num_val) 
			{
                stream->bits_per_coded_sample = num_val;
                //we may have to rewrite a previously read codecid because MSV only marks PCM endianness.
                if(num_val == 8 && (stream->codec_id == CODEC_ID_PCM_S16BE || stream->codec_id == CODEC_ID_PCM_S16LE))
                    stream->codec_id = CODEC_ID_PCM_S8;
            }
            else if(!strcmp(key, "audiosamplerate") && stream && num_val >= 0) 
			{
                //some tools, like MSVTool2, write consistently approximate metadata sample rates
                if (!stream->sample_rate) 
				{
                    switch((int)num_val) 
					{
                        case 44000: stream->sample_rate = 44100  ; break;
                        case 22000: stream->sample_rate = 22050  ; break;
                        case 11000: stream->sample_rate = 11025  ; break;
                        case 5000 : stream->sample_rate = 5512   ; break;
                        default   : stream->sample_rate = num_val;
                    }
                }
            }
        }
		else if(amf_type == AMF_DATA_TYPE_BOOL) 
		{
            if(!strcmp(key, "stereo") && stream) stream->channels = num_val > 0 ? 2 : 1;
        } 		
		else if(amf_type == AMF_DATA_TYPE_STRING) 
		{
            if(!strcmp(key, "busnum") ) printf("MSV busnum %s\n",str_val);
        }
    }

    return 0;
}

static int msv_read_metabody(AVFormatContext *s, int64_t next_pos) {
    AMFDataType type;
    ByteIOContext *ioc;
    int i, keylen;
    char buffer[11]; //only needs to hold the string "onMetaData". Anything longer is something we don't want.

    keylen = 0;
    ioc = s->pb;

    //first object needs to be "onMetaData" string
    type = (AMFDataType)get_byte(ioc);
    if(type != AMF_DATA_TYPE_STRING || amf_get_string(ioc, buffer, sizeof(buffer)) < 0 || strcmp(buffer, "onMetaData"))
        return -1;

	assert(s->streams[0]);//IAN
    //parse the second object (we want a mixed array)
    if(amf_parse_object_t(s, s->streams[0], buffer, next_pos, 0) < 0)
        return -1;

    //find the streams now so that amf_parse_object doesn't need to do the lookup every time it is called.
    for(i = 0; i < s->nb_streams; i++) 
	{
		assert(s->streams[i]);//IAN
	
		//parse the second object (we want a mixed array)
		if(amf_parse_object_t(s, s->streams[i], buffer, next_pos, 0) < 0)
			return -1;
    }

    return 0;
}

int msv_read_metadata(AVFormatContext *s);

static AVStream *create_stream(AVFormatContext *s, int streamid){
    AVStream *st = av_new_stream(s, streamid);
    if (!st)
        return NULL;
    av_set_pts_info(st, 32, 1, 1000); /* 32 bit pts in ms */
    return st;
}


int msv_read_header_allInfo(AVFormatContext *s)
{
    int offset, flags;
    int audio_enc = 0, video_enc = 0, data_enc = 0;
	int i;

    url_fskip(s->pb, 4);
    flags = get_byte(s->pb);
    /* old msvtool cleared this field */
    /* FIXME: better fix needed */
    if (!flags) {
        flags =1*MSV_HEADER_FLAG_HASDATA| 4*MSV_HEADER_FLAG_HASVIDEO | 4*MSV_HEADER_FLAG_HASAUDIO;
    }

    //if((flags & (MSV_HEADER_FLAG_HASVIDEO|MSV_HEADER_FLAG_HASAUDIO))
    //         != (MSV_HEADER_FLAG_HASVIDEO|MSV_HEADER_FLAG_HASAUDIO))
    //    s->ctx_flags |= AVFMTCTX_NOHEADER;
	video_enc = flags / MSV_HEADER_FLAG_HASAUDIO;
	audio_enc = (flags % MSV_HEADER_FLAG_HASAUDIO) / MSV_HEADER_FLAG_HASVIDEO;
	data_enc = flags % MSV_HEADER_FLAG_HASVIDEO;
	

	 for(i=0; i<video_enc; i++)
	{
		if(!create_stream(s, i))
            return AVERROR(ENOMEM);
		s->nb_vstreams ++;
	 }
	 
	 for(i=0; i<audio_enc; i++)
	{
		if(!create_stream(s, s->nb_vstreams + i))
            return AVERROR(ENOMEM);
		s->nb_astreams ++;
	 } 

	for(i=0; i<data_enc; i++)
	{
		if(!create_stream(s, s->nb_vstreams + s->nb_astreams + i))
            return AVERROR(ENOMEM);
		s->streams[s->nb_vstreams + s->nb_astreams + i]->codec_type = CODEC_TYPE_DATA;
		s->nb_dstreams ++;
	 }

    offset = get_be32(s->pb);
    url_fseek(s->pb, offset, SEEK_SET);

	//read body
	int ret = 0;
	ret = msv_read_metadata(s);

    return 0;
}


int msv_read_header(AVFormatContext *s,
                           AVFormatParameters *ap)
{
    int offset, flags;
    int audio_enc = 0, video_enc = 0, data_enc = 0;
	int i;

    url_fskip(s->pb, 4);
    flags = get_byte(s->pb);
    /* old msvtool cleared this field */
    /* FIXME: better fix needed */
    if (!flags) {
        flags =1*MSV_HEADER_FLAG_HASDATA| 4*MSV_HEADER_FLAG_HASVIDEO | 4*MSV_HEADER_FLAG_HASAUDIO;
    }

    //if((flags & (MSV_HEADER_FLAG_HASVIDEO|MSV_HEADER_FLAG_HASAUDIO))
    //         != (MSV_HEADER_FLAG_HASVIDEO|MSV_HEADER_FLAG_HASAUDIO))
    //    s->ctx_flags |= AVFMTCTX_NOHEADER;
	video_enc = flags / MSV_HEADER_FLAG_HASAUDIO;
	audio_enc = (flags % MSV_HEADER_FLAG_HASAUDIO) / MSV_HEADER_FLAG_HASVIDEO;
	data_enc = flags % MSV_HEADER_FLAG_HASVIDEO;
	

	 for(i=0; i<video_enc; i++)
	{
		if(!create_stream(s, i))
            return AVERROR(ENOMEM);
		s->nb_vstreams ++;
	 }
	 
	 for(i=0; i<audio_enc; i++)
	{
		if(!create_stream(s, s->nb_vstreams + i))
            return AVERROR(ENOMEM);
		s->nb_astreams ++;
	 } 

	for(i=0; i<data_enc; i++)
	{
		if(!create_stream(s, s->nb_vstreams + s->nb_astreams + i))
            return AVERROR(ENOMEM);
		s->streams[s->nb_vstreams + s->nb_astreams + i]->codec_type = CODEC_TYPE_DATA;
		s->nb_dstreams ++;
	 }


    offset = get_be32(s->pb);
    url_fseek(s->pb, offset, SEEK_SET);

    s->start_time = 0;

    return 0;
}

int msv_read_release(AVFormatContext *s)
{
	if(s)
	{	
		int i;
		for(i =0; i< MAX_STREAMS; i++)
		if(s->streams[i])
		{
			free(s->streams[i]);
			s->streams[i] = NULL;
		}
	}
	return 0;
}

#ifdef USESYNC
int findSYNChead(ByteIOContext *pb, int* type)
{
	unsigned int sync = -1,ret = 0;
	while (1)
	{
		ret = get_byte(pb);
		sync = sync <<8 | ret;
		//printf("sync 0x%08x,url_ftell(pb) %lld\n",sync,url_ftell(pb));
		if(sync == SYNChead)
		{
			ret = get_byte(pb);
			if((ret == MSV_TAG_TYPE_AUDIO) || (ret == MSV_TAG_TYPE_VIDEO) || (ret == MSV_TAG_TYPE_META) || (ret == MSV_TAG_TYPE_DATA))
			{
				*type = ret;
				break;
			}
			else
			{
				sync = sync <<8 | ret;
			}
		}
		
		if (url_feof(pb))
			return AVERROR(32);//AVERROR_EOF;
	}
	return 0;
}
#endif


#define MAX_PACKET_LEN 1000000
int msv_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    //MSVContext *msv = (MSVContext *)s->priv_data;
    int ret, i, type, size, flags, codec_type, stream_id;
    int64_t next, pos;
    int64_t dts=0, pts = AV_NOPTS_VALUE;
    AVStream *st = NULL;
	unsigned int sync = -1;

	for(;;)
	{
		pos = url_ftell(s->pb);
//printf("%s pos %x\n",__FUNCTION__,pos);
		url_fskip(s->pb, 4); /* size of previous packet */
#ifdef USESYNC
resync_fun:

		sync = get_be32(s->pb);
		//printf("sync 0x%08x\n",sync);
		if(SYNChead != sync )
		{
			if(findSYNChead(s->pb, &type) == AVERROR(32))
				return AVERROR(32);
		}else {
			type = get_byte(s->pb);
		}
#else
		type = get_byte(s->pb);
#endif

//printf("%s type %x\n",__FUNCTION__,type);
		size = get_be24(s->pb);
#ifdef USESYNC
		if(size > MAX_PACKET_LEN)
			goto resync_fun;
#endif
//printf("%s type %x\n",__FUNCTION__,size);
		dts = get_be24(s->pb);
		dts |= (uint32_t)get_byte(s->pb) << 24;
//printf("%s dts %x\n",__FUNCTION__,dts);
		if (url_feof(s->pb))
			return AVERROR(32);//AVERROR_EOF;
		stream_id = get_be24(s->pb);/* stream id, always 0 */
//printf("%s stream_id %x\n",__FUNCTION__,stream_id);

		
		//pkt->frameCnt = get_byte(s->pb);

		flags = 0;

		if(size == 0)
			continue;

		next= size + url_ftell(s->pb);

		if (type == MSV_TAG_TYPE_DATA) {
			codec_type = 2;
			flags = get_byte(s->pb);
			size--;
		}else  if (type == MSV_TAG_TYPE_AUDIO) {
			codec_type=1;
			flags = get_byte(s->pb);
			size--;
		} else if (type == MSV_TAG_TYPE_VIDEO) {
			codec_type=0;
			flags = get_byte(s->pb);
			size--;
			if ((flags & 0xf0) == 0x50) /* video info / command frame */
				goto skip;
		} else {
			if (type == MSV_TAG_TYPE_META && size > 13+1+4)
				msv_read_metabody(s, next);
skip:
			url_fseek(s->pb, next, SEEK_SET);
			continue;
		}
//printf("dec flags %x\n",flags);
		/* skip empty data packets */
		if (!size)
			continue;

		/* now find stream */
		for(i=0;i<s->nb_streams;i++) {
			st = s->streams[i];
			if (st->id == stream_id)
				break;
		}
		if(i == s->nb_streams){
			st= create_stream(s, i);
			if(codec_type == 2)
				st->codec_type = CODEC_TYPE_DATA;
			else
				st->codec_type = codec_type ? CODEC_TYPE_AUDIO : CODEC_TYPE_VIDEO;
			s->ctx_flags &= ~AVFMTCTX_NOHEADER;
		}
		if(  (st->discard >= AVDISCARD_NONKEY && !((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_KEY ||         codec_type))
			||(st->discard >= AVDISCARD_BIDIR  &&  ((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_DISP_INTER && !codec_type))
			|| st->discard >= AVDISCARD_ALL
			){
				url_fseek(s->pb, next, SEEK_SET);
				continue;
		}
		break;
 }

    // if not streamed and no duration from metadata then seek to end to find the duration from the timestamps
    if(!url_is_streamed(s->pb) && s->duration==AV_NOPTS_VALUE){
        int size;
        const int64_t pos= url_ftell(s->pb);
        const int64_t fsize= url_fsize(s->pb);
        url_fseek(s->pb, fsize-4, SEEK_SET);
        size= get_be32(s->pb);
        url_fseek(s->pb, fsize-3-size, SEEK_SET);
        if(size == get_be24(s->pb) + 11){
            s->duration= get_be24(s->pb) * (int64_t)AV_TIME_BASE / 1000;
        }
        url_fseek(s->pb, pos, SEEK_SET);
    }

    if(codec_type == 1){
        if(!st->channels || !st->sample_rate || !st->bits_per_coded_sample) {
            st->channels = (flags & MSV_AUDIO_CHANNEL_MASK) == MSV_STEREO ? 2 : 1;
            st->sample_rate = (44100 << ((flags & MSV_AUDIO_SAMPLERATE_MASK) >> MSV_AUDIO_SAMPLERATE_OFFSET) >> 3);
            st->bits_per_coded_sample = (flags & MSV_AUDIO_SAMPLESIZE_MASK) ? 16 : 8;
        }
        if(!st->codec_id){
            msv_set_audio_codec(s, st, flags & MSV_AUDIO_CODECID_MASK);
        }
    }else if(codec_type == 0){
        size -= msv_set_video_codec(s, st, flags & MSV_VIDEO_CODECID_MASK);
    }

    ret= av_get_packet(s->pb, pkt, size);
    if (ret <= 0) {
        return AVERROR(5);//AVERROR(EIO);
    }
    /* note: we need to modify the packet size here to handle the last
       packet */
    pkt->size = ret;
    pkt->dts = dts;
    pkt->pts = pts == AV_NOPTS_VALUE ? dts : pts;
	pkt->stream_id    = st->id;

    if (codec_type== 1 || ((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_KEY))
        pkt->flags |= PKT_FLAG_KEY;

    return ret;
}


int msv_read_metadata(AVFormatContext *s)
{
    MSVContext *msv = (MSVContext *)s->priv_data;
    int  type, size, flags,  stream_id;
    int64_t next, pos;
    int64_t dts;//, pts = AV_NOPTS_VALUE;
	unsigned int sync = -1;
	// AVStream *st = NULL;
	assert(s);//IAN
	assert(msv);//IAN
//printf("msv_read_metadata sync\n");
	for(;;)
	{
		pos = url_ftell(s->pb);
		url_fskip(s->pb, 4); /* size of previous packet */
#ifdef USESYNC
		sync = get_be32(s->pb);
//printf("msv_read_metadata sync 0x%x SYNChead != sync=%d\n",sync,SYNChead != sync);
		if(SYNChead != sync )
		{
			if(findSYNChead(s->pb, &type) == AVERROR(32))
				return AVERROR(32);
		} else {
			type = get_byte(s->pb);
		}
#else
		type = get_byte(s->pb);
#endif
		size = get_be24(s->pb);
		dts = get_be24(s->pb);
		dts |= (uint32_t)get_byte(s->pb) << 24;
		if (url_feof(s->pb))
			return AVERROR(32);//AVERROR_EOF;
		stream_id = get_be24(s->pb);/* stream id, always 0 */
		flags = 0;

		if(size == 0)
			continue;

		next= size + url_ftell(s->pb);

		if (type == MSV_TAG_TYPE_META){
			if (type == MSV_TAG_TYPE_META && size > 13+1+4)
				msv_read_metabody(s, next);
		}
		else
		{
			url_fseek(s->pb, next, SEEK_SET);
			continue;
		}

		break;
 }
	return 0;
}

 //获取文件信息
int msv_read_info(char* filename, int64_t* start_time, int64_t* duration)
{
	if(filename == NULL)
		return 0;

	//需要用到的变量
	AVFormatContext Context = {0},*pInFormat = &Context;

	//设置参数
#ifdef CONFIG_WIN32
	_snprintf(pInFormat->filename, sizeof(pInFormat->filename), "%s", filename);
#else
	snprintf(pInFormat->filename, sizeof(pInFormat->filename), "%s", filename);
#endif
	MSVContext msv = {0}, *pmsv = &msv;
	pInFormat->priv_data = pmsv;	//上下文使用

	if (url_fopen(&pInFormat->pb, pInFormat->filename, URL_RDONLY) < 0)
	{
		return 0;
	}

	//read head
    int offset, flags;
    int audio_enc = 0, video_enc = 0, data_enc = 0;
	//int i;

    url_fskip(pInFormat->pb, 4);
    flags = get_byte(pInFormat->pb);

    if (flags) {
		video_enc = flags / MSV_HEADER_FLAG_HASAUDIO;
		audio_enc = (flags % MSV_HEADER_FLAG_HASAUDIO) / MSV_HEADER_FLAG_HASVIDEO;
		data_enc = flags % MSV_HEADER_FLAG_HASVIDEO;
    }

    offset = get_be32(pInFormat->pb);
    url_fseek(pInFormat->pb, offset, SEEK_SET);

	//read body
	int ret = 0;
	ret = msv_read_metadata(pInFormat);

	if(pInFormat->start_time != 0 && start_time != NULL)
	{
		*start_time = pInFormat->start_time;
	}

	if(pInFormat->duration != 0 && duration != NULL)
	{
		*duration = pInFormat->duration;
	}

	msv_read_release(pInFormat);

	if(pInFormat->pb)
		url_fclose(pInFormat->pb);

	return 0;
}
 
#define FALSE 0
#define TRUE 1

int msv_seek_frame(AVFormatContext *s, uint64_t timestamp, int flags)
{
	int res = FALSE;
	printf("%s line %d,timestamp %lld\n",__FUNCTION__,__LINE__,timestamp);
	if(timestamp>= 0) //有效的时间戳
	{
		int size, stream_id;
		int type;
		int64_t next, seekpos = 0;
		const int64_t pos= url_ftell(s->pb);
		uint64_t pts=0;
		unsigned int sync = -1;
		
		url_fskip(s->pb, 4); /* size of previous packet  and  type, fixed by sam */
#ifdef USESYNC
		sync = get_be32(s->pb);
		//printf("sync 0x%08x\n",sync);
		if(SYNChead != sync )
		{
			if(findSYNChead(s->pb, &type) == AVERROR(32))
				return AVERROR(32);
		}else {
			type = get_byte(s->pb);
		}
#else
		type = get_byte(s->pb);
#endif

		if((type == MSV_TAG_TYPE_AUDIO) || (type == MSV_TAG_TYPE_VIDEO) || (type == MSV_TAG_TYPE_META) || (type == MSV_TAG_TYPE_DATA))
		{
			size = get_be24(s->pb);
			pts = get_be24(s->pb);
			pts |= (uint32_t)get_byte(s->pb) << 24;
			if (!url_feof(s->pb) )
			{
				//s->duration= pts /** (int64_t)AV_TIME_BASE*/ / 1000;
				next= size + 3 + url_ftell(s->pb); // 3 为stream id
				url_fseek(s->pb, next, SEEK_SET);

				if(pts < timestamp) //前进搜索
				{
					for(;;)
					{
						url_fskip(s->pb, 4); /* size of previous packet  and  type, fixed by sam */

#ifdef USESYNC			
						sync = get_be32(s->pb);
						//printf("sync 0x%08x\n",sync);
						if(SYNChead != sync )
						{
							if(findSYNChead(s->pb, &type) == AVERROR(32))
								return AVERROR(32);
						}else {
							type = get_byte(s->pb);
						}
#else
						type = get_byte(s->pb);
#endif
						if((type != MSV_TAG_TYPE_AUDIO) && (type != MSV_TAG_TYPE_VIDEO) && (type != MSV_TAG_TYPE_META) && (type != MSV_TAG_TYPE_DATA))
							break;
						size = get_be24(s->pb);
						pts = get_be24(s->pb);
						pts |= (uint32_t)get_byte(s->pb) << 24;
						if (url_feof(s->pb) )
							break;//AVERROR_EOF;
						stream_id = get_be24(s->pb);/* stream id, always 0 */
						if(size == 0 || size > 1000000)
							continue;

						if((pts > timestamp && res == TRUE)||(pts > (timestamp + 2000000)))//2Sec
							break;

						if (type == MSV_TAG_TYPE_VIDEO) 
						{
							flags = get_byte(s->pb);
							size--;
							if(flags == MSV_FRAME_KEY)
							{
								seekpos = url_ftell(s->pb) - 16 - SYNCLEN;
								res = TRUE;
							}
						}
						next= size + url_ftell(s->pb);
						url_fseek(s->pb, next, SEEK_SET);
					}
				}
				else		//后退搜索
				{
					int oldsize = 0;
					for(;;)
					{
						next = url_ftell(s->pb);
						size= get_be32(s->pb);
						
						next = next-size-4;	//next previous tag
						if(next <= 0)
							break;		//已经到文件头

#ifdef USESYNC
						sync = get_be32(s->pb);
						if(SYNChead != sync )
						{
							if(findSYNChead(s->pb, &type) == AVERROR(32))
								return AVERROR(32);
						}
						else
						{
							type = get_byte(s->pb);
						}
#else
						type = get_byte(s->pb);
#endif
						int packetsixe = get_be24(s->pb);
						if(oldsize ==  packetsixe+ 11 +SYNCLEN)
						{
							pts = get_be24(s->pb);
							pts |= (uint32_t)get_byte(s->pb) << 24;
							stream_id = get_be24(s->pb);/* stream id, always 0 */
							if (type == MSV_TAG_TYPE_VIDEO) 
							{
								flags = get_byte(s->pb);
								if(flags == MSV_FRAME_KEY && pts <= timestamp)
								{
									seekpos = url_ftell(s->pb) - 16 - SYNCLEN;
									res = TRUE;
									break;
								}
							}

						}

						if(size == 0)
							break;		//已经到一个数据帧
						oldsize = size;
						url_fseek(s->pb, next, SEEK_SET);
					}
				}	//搜索结束
			}
		}

		if(res == FALSE)
			url_fseek(s->pb, pos, SEEK_SET);
		else
			url_fseek(s->pb, seekpos, SEEK_SET);
	}
	return res;
}

int msv_seek_frame_Q(AVFormatContext *s, uint64_t timestamp, int flags, int64_t file_size, int64_t duration)
{
	int res = FALSE;
	uint64_t pts=0;
	int64_t next, seekpos = 0, nearFileSize = 0;
	const int64_t pos= url_ftell(s->pb);

	printf("%s line %d,timestamp %lld\n",__FUNCTION__,__LINE__,timestamp);
	printf("%s line %d,file_size %lld\n",__FUNCTION__,__LINE__,file_size);
	printf("%s line %d,duration %lld\n",__FUNCTION__,__LINE__,duration);
	if(timestamp>= 0) //有效的时间戳
	{
		int size, stream_id, sync;
		int type;
		
		url_fskip(s->pb, 4); /* size of previous packet  and  type, fixed by sam */
#ifdef USESYNC
		if(file_size <= 0)
		file_size = url_fsize(s->pb);
		if(file_size > 0 && duration > timestamp)
		{
			nearFileSize = file_size*(timestamp)/duration;
			url_fseek(s->pb, nearFileSize, SEEK_SET);
			printf("nearFileSize %lld,pos %lld\n",nearFileSize,url_ftell(s->pb));
		}
		else {
			printf("%s line %d Error AVERROR(32):file_size %lld duration %lld ? seek timestamp %lld \n",__FUNCTION__,__LINE__,file_size,duration,timestamp);
			return AVERROR(32);

		}
		sync = get_be32(s->pb);
		if(SYNChead != sync )
		{
			if(findSYNChead(s->pb, &type) == AVERROR(32)){
				printf("%s line %d Error AVERROR(32)\n",__FUNCTION__,__LINE__);
				return AVERROR(32);
			}
		}
		else
		{
			type = get_byte(s->pb);
		}
#else
		type = get_byte(s->pb);
#endif

		if((type == MSV_TAG_TYPE_AUDIO) || (type == MSV_TAG_TYPE_VIDEO) || (type == MSV_TAG_TYPE_META) || (type == MSV_TAG_TYPE_DATA))
		{
			size = get_be24(s->pb);
			pts = get_be24(s->pb);
			pts |= (uint32_t)get_byte(s->pb) << 24;
			//printf("%s line %d,pts %lld\n",__FUNCTION__,__LINE__,pts);
			if (!url_feof(s->pb) )
			{
				//s->duration= pts /** (int64_t)AV_TIME_BASE*/ / 1000;
				next= size + 3 + url_ftell(s->pb); // 3 为stream id
				url_fseek(s->pb, next, SEEK_SET);

				if(pts < timestamp) //前进搜索
				{
					for(;;)
					{
						url_fskip(s->pb, 4); /* size of previous packet  and  type, fixed by sam */
				#ifdef USESYNC
						sync = get_be32(s->pb);
						if(SYNChead != sync )
						{
							if(findSYNChead(s->pb, &type) == AVERROR(32)){
								printf("%s line %d Error AVERROR(32)\n",__FUNCTION__,__LINE__);
								return AVERROR(32);
							}
						}
						else
						{
							type = get_byte(s->pb);
						}
				#else
						type = get_byte(s->pb);
				#endif
						if((type != MSV_TAG_TYPE_AUDIO) && (type != MSV_TAG_TYPE_VIDEO) && (type != MSV_TAG_TYPE_META) && (type != MSV_TAG_TYPE_DATA)){
							printf("%s line %d Error\n",__FUNCTION__,__LINE__);
							break;
						}
						size = get_be24(s->pb);
						pts = get_be24(s->pb);
						pts |= (uint32_t)get_byte(s->pb) << 24;
						if (url_feof(s->pb) ){
							printf("%s line %d Error:AVERROR_EOF\n",__FUNCTION__,__LINE__);
							break;//AVERROR_EOF;
						}
						stream_id = get_be24(s->pb);/* stream id, always 0 */
						if(size == 0 || size > 1000000)
							continue;

						//if(pts > timestamp)
						//if((pts > timestamp && res == TRUE)||(pts > (timestamp + 2000000))||(pts < (timestamp - 20000000))){//2Sec
						if((res == TRUE)&&((pts > timestamp) ||(pts > (timestamp + 2000000))||(pts < (timestamp - 20000000)))){//2Sec
							printf("%s line %d Found\n",__FUNCTION__,__LINE__);
							break;
						}

						if (type == MSV_TAG_TYPE_VIDEO) 
						{
							flags = get_byte(s->pb);
							size--;
							if(flags == MSV_FRAME_KEY)
							{
								seekpos = url_ftell(s->pb) - 16 - SYNCLEN;
								res = TRUE;
							}
						}
						next= size + url_ftell(s->pb);
						url_fseek(s->pb, next, SEEK_SET);
					}
				}
				else		//后退搜索
				{
					int oldsize = 0;
					for(;;)
					{
						next = url_ftell(s->pb);
						size= get_be32(s->pb);
						
						next = next-size-4;	//next previous tag
						if(next <= 0){
							printf("%s line %d Error\n",__FUNCTION__,__LINE__);
							break;		//已经到文件头
						}

				#ifdef USESYNC
						sync = get_be32(s->pb);
						if(SYNChead != sync )
						{
							if(findSYNChead(s->pb, &type) == AVERROR(32)){
								printf("%s line %d Error AVERROR(32)\n",__FUNCTION__,__LINE__);
								return AVERROR(32);
							}
						}
						else
						{
							type = get_byte(s->pb);
						}
				#else
						type = get_byte(s->pb);
				#endif
						int packetsixe = get_be24(s->pb);
						if(oldsize ==  packetsixe+ 11 + SYNCLEN)
						{
							pts = get_be24(s->pb);
							pts |= (uint32_t)get_byte(s->pb) << 24;
							//printf("%s line %d,pts %lld\n",__FUNCTION__,__LINE__,pts);
							stream_id = get_be24(s->pb);/* stream id, always 0 */
							if (type == MSV_TAG_TYPE_VIDEO) 
							{
								flags = get_byte(s->pb);
								if(flags == MSV_FRAME_KEY /*&& pts <= timestamp*/)
								{
									seekpos = url_ftell(s->pb) - 16 - SYNCLEN;
									res = TRUE;
									printf("%s line %d Found\n",__FUNCTION__,__LINE__);
									break;
								}
							}
							//if(((pts <= timestamp) && res == TRUE)||(pts <= (timestamp - 10000000))||(pts > (timestamp + 20000000))){//2Sec
							if((res == TRUE)&&((pts <= timestamp) ||(pts <= (timestamp - 10000000))||(pts > (timestamp + 20000000)))){//2Sec
								printf("%s line %d Found\n",__FUNCTION__,__LINE__);
								break;
							}

						}

						if(size == 0 || size > 1000000){
							printf("%s line %d Error\n",__FUNCTION__,__LINE__);
							break;		//已经到一个数据帧
						}
						oldsize = size;
						url_fseek(s->pb, next, SEEK_SET);
					}
				}	//搜索结束
			}
		}

		if(res == FALSE)
			url_fseek(s->pb, pos, SEEK_SET);
		else
			url_fseek(s->pb, seekpos, SEEK_SET);
	}
	printf("%s line %d res %d,pts %lld,pos %lld,seekpos %lld\n",__FUNCTION__,__LINE__,res,pts,pos,seekpos);
	return res;
}


int msv_read_pre_packet(AVFormatContext *s, AVPacket *pkt)
{
    //MSVContext *msv = (MSVContext *)s->priv_data;
    int ret, i, type, size, flags, codec_type, stream_id;
    int64_t next, pos;
    int64_t dts=0, pts = AV_NOPTS_VALUE;
    AVStream *st = NULL;
    unsigned int sync = -1;
    int64_t prepacketNext;

	for(;;)
	{
		pos = url_ftell(s->pb);
        size= get_be32(s->pb);
		prepacketNext= pos - size-4;
        if(prepacketNext <= 0)
        {
           printf("file eof\n");
           return AVERROR(32);
        }
#ifdef USESYNC
resync_fun:

		sync = get_be32(s->pb);
		//my_printf(LEVEL_DEBUG,"sync 0x%08x\n",sync);
		if(SYNChead != sync )
		{
			if(findSYNChead(s->pb, &type) == AVERROR(32))
				return AVERROR(32);
		}else {
			type = get_byte(s->pb);
		}
#else
		type = get_byte(s->pb);
#endif

//my_printf(LEVEL_DEBUG,"%s type %x\n",__FUNCTION__,type);
		size = get_be24(s->pb);
#ifdef USESYNC
		if(size > MAX_PACKET_LEN)
			goto resync_fun;
#endif
//my_printf(LEVEL_DEBUG,"%s type %x\n",__FUNCTION__,size);
		dts = get_be24(s->pb);
		dts |= (uint32_t)get_byte(s->pb) << 24;
//my_printf(LEVEL_DEBUG,"%s dts %x\n",__FUNCTION__,dts);
		if (url_feof(s->pb))
			return AVERROR(32);//AVERROR_EOF;
		stream_id = get_be24(s->pb);/* stream id, always 0 */
//my_printf(LEVEL_DEBUG,"%s stream_id %x\n",__FUNCTION__,stream_id);

		
		//pkt->frameCnt = get_byte(s->pb);

		flags = 0;

		if(size == 0)
			continue;

		//next= size + url_ftell(s->pb);
		next= url_ftell(s->pb) - size - 4;

		if (type == MSV_TAG_TYPE_DATA) {
			codec_type = 2;
			flags = get_byte(s->pb);
			size--;
		}else  if (type == MSV_TAG_TYPE_AUDIO) {
			codec_type=1;
			flags = get_byte(s->pb);
			size--;
		} else if (type == MSV_TAG_TYPE_VIDEO) {
			codec_type=0;
			flags = get_byte(s->pb);
			size--;
			if ((flags & 0xf0) == 0x50) /* video info / command frame */
				goto skip;
		} else {
			if (type == MSV_TAG_TYPE_META && size > 13+1+4)
				msv_read_metabody(s, next);
			printf("%s next %d\n",__FUNCTION__,next);
skip:
			url_fseek(s->pb, next, SEEK_SET);
			continue;
		}
//my_printf(LEVEL_DEBUG,"dec flags %x\n",flags);
		/* skip empty data packets */
		if (!size)
			continue;

		/* now find stream */
		for(i=0;i<s->nb_streams;i++) {
			st = s->streams[i];
			if (st->id == stream_id)
				break;
		}
		if(i == s->nb_streams){
			st= create_stream(s, i);
			if(codec_type == 2)
				st->codec_type = CODEC_TYPE_DATA;
			else
				st->codec_type = codec_type ? CODEC_TYPE_AUDIO : CODEC_TYPE_VIDEO;
			s->ctx_flags &= ~AVFMTCTX_NOHEADER;
		}
		if(  (st->discard >= AVDISCARD_NONKEY && !((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_KEY ||         codec_type))
			||(st->discard >= AVDISCARD_BIDIR  &&  ((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_DISP_INTER && !codec_type))
			|| st->discard >= AVDISCARD_ALL
			){
				url_fseek(s->pb, next, SEEK_SET);
				continue;
		}
		break;
 }

    // if not streamed and no duration from metadata then seek to end to find the duration from the timestamps
    if(!url_is_streamed(s->pb) && s->duration==AV_NOPTS_VALUE){
        int size;
        const int64_t pos= url_ftell(s->pb);
        const int64_t fsize= url_fsize(s->pb);
        url_fseek(s->pb, fsize-4, SEEK_SET);
        size= get_be32(s->pb);
        url_fseek(s->pb, fsize-3-size, SEEK_SET);
        if(size == get_be24(s->pb) + 11){
            s->duration= get_be24(s->pb) * (int64_t)AV_TIME_BASE / 1000;
        }
        url_fseek(s->pb, pos, SEEK_SET);
    }

    if(codec_type == 1){
        if(!st->channels || !st->sample_rate || !st->bits_per_coded_sample) {
            st->channels = (flags & MSV_AUDIO_CHANNEL_MASK) == MSV_STEREO ? 2 : 1;
            st->sample_rate = (44100 << ((flags & MSV_AUDIO_SAMPLERATE_MASK) >> MSV_AUDIO_SAMPLERATE_OFFSET) >> 3);
            st->bits_per_coded_sample = (flags & MSV_AUDIO_SAMPLESIZE_MASK) ? 16 : 8;
        }
        if(!st->codec_id){
            msv_set_audio_codec(s, st, flags & MSV_AUDIO_CODECID_MASK);
        }
    }else if(codec_type == 0){
        size -= msv_set_video_codec(s, st, flags & MSV_VIDEO_CODECID_MASK);
    }

   
    ret= av_get_packet(s->pb, pkt, size);
    if (ret <= 0) {
        return AVERROR(5);//AVERROR(EIO);
    }

    
    /* note: we need to modify the packet size here to handle the last
       packet */
    pkt->size = ret;
    pkt->dts = dts;
    pkt->pts = pts == AV_NOPTS_VALUE ? dts : pts;
	pkt->stream_id    = st->id;

    if (codec_type== 1 || ((flags & MSV_VIDEO_FRAMETYPE_MASK) == MSV_FRAME_KEY))
        pkt->flags |= PKT_FLAG_KEY;

    url_fseek(s->pb, prepacketNext, SEEK_SET);

    return ret;
}

 
