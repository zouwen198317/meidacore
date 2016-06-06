/*
 * AVI muxer
 * Copyright (c) 2000 Fabrice Bellard
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
//#include "avformat.h"
#include "avcodec_common.h"
#include "msv.h"
#include "assert.h"

#include "avi.h"
#include "riff.h"
#include "list/list.h"
#include "intreadwrite.h"
#include "metadata.h"
#include "rational.h"
#include <math.h>
//#include "dvripcpacket.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>


/*
 * TODO:
 *  - fill all fields if non streamed (nb_frames for example)
 */

typedef struct AVIIentry {
    unsigned int flags, pos, len;
} AVIIentry;

#define AVI_INDEX_CLUSTER_SIZE 16384

typedef struct AVIIndex {
    int64_t     indx_start;
    int         entry;
    int         ents_allocated;
    AVIIentry** cluster;
} AVIIndex;

/*
typedef struct {
    int64_t riff_start, movi_list, odml_list;
    int64_t frames_hdr_all;
    int riff_id;
} AVIContext;
*/
typedef struct  {
    int64_t frames_hdr_strm;
    int audio_strm_length;
    int packet_count;
    int entry;

    AVIIndex indexes;
} AVIStream ;



static unsigned char* putbyte(unsigned char *s, unsigned int b)
{
	*s++ = (unsigned char)b;
	return s;
}

static unsigned char* putle32(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val);
    s= putbyte(s, val >> 8);
    s= putbyte(s, val >> 16);
    s= putbyte(s, val >> 24);
    return s;
}

static unsigned char* puttag(unsigned char *s, const char *tag)
{
    while (*tag) {
        s = putbyte(s, *tag++);
    }
    return s;
}

#if 0

static unsigned char* putbe32(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 24);
    s= putbyte(s, val >> 16);
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}

static unsigned char* putle16(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val);
    s= putbyte(s, val >> 8);
    return s;
}


static unsigned char* putbe16(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}


static unsigned char* putle24(unsigned char *s, unsigned int val)
{
    s= putle16(s, val & 0xffff);
    s= putbyte(s, val >> 16);
    return s;
}


static unsigned char* putbe24(unsigned char *s, unsigned int val)
{
    s= putbe16(s, val >> 8);
    s= putbyte(s, val);
    return s;
}



static unsigned char* putamf_bool(unsigned char* ptr, const int bVal)
{
	*ptr = 1;
	*(ptr+1) = bVal ? 1 : 0;
	return ptr + 2;
}

static unsigned char* putamf_string(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    *ptr = len >> 8;
    *(ptr+1) = len;
    memcpy(ptr+2, str, len);

    return ptr + len + 2;
}

#define AMF_DATA_TYPE_STRING 0x02

static unsigned char* putamf_string_content(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    ptr = putbyte(ptr, AMF_DATA_TYPE_STRING);
    ptr = putbe16(ptr, len);
    memcpy(ptr, str, len);
	return  ptr+len;
}


static unsigned char* putbe64(unsigned char* ptr, uint64_t val)
{
	ptr =putbe32(ptr, (uint32_t)(val >> 32));
	ptr =putbe32(ptr, (uint32_t)(val & 0xffffffff));
	return ptr;
}

static int64_t dbl2int(double d){
    int e;
    if     ( !d) return 0;
    else if(d-d) return 0x7FF0000000000000LL + ((int64_t)(d<0)<<63) + (d!=d);
    d= frexp(d, &e);
    return (int64_t)(d<0)<<63 | (e+1022LL)<<52 | (int64_t)((fabs(d)-0.5)*(1LL<<53));
}

static unsigned char* putamf_double(unsigned char* ptr, double d)
{
    ptr = putbyte(ptr, 0);
    ptr = putbe64(ptr, dbl2int(d));
	return ptr;
}

#endif






static inline AVIIentry* avi_get_ientry(AVIIndex* idx, int ent_id)
{
    int cl = ent_id / AVI_INDEX_CLUSTER_SIZE;
    int id = ent_id % AVI_INDEX_CLUSTER_SIZE;
    return &idx->cluster[cl][id];
}

static int64_t avi_start_new_riff(AVFormatContext *s, ByteIOContext *pb,
                                  const char* riff_tag, const char* list_tag)
{
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
    AVIContext *avi= s->priv_data;
    int64_t loff;
    int i;
	//printf("%s line %d\n",__FUNCTION__,__LINE__);

    avi->riff_id++;
    for (i=0; i<s->nb_streams; i++){
		//printf("%s line %d\n",__FUNCTION__,__LINE__);
        AVIStream *avist= s->streams[i]->priv_data;
        avist->indexes.entry = 0;
    }
	//printf("%s line %d\n",__FUNCTION__,__LINE__);

    avi->riff_start = ff_start_tag(pb, "RIFF");
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
    put_tag(pb, riff_tag);
    loff = ff_start_tag(pb, "LIST");
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
    put_tag(pb, list_tag);
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
    return loff;
}

static char* avi_stream2fourcc(char* tag, int index, enum AVMediaType type)
{
    tag[0] = '0';
    tag[1] = '0' + index;
    if (type == AVMEDIA_TYPE_VIDEO) {
        tag[2] = 'd';
        tag[3] = 'c';
    } else if (type == AVMEDIA_TYPE_SUBTITLE) {
        // note: this is not an official code
        tag[2] = 's';
        tag[3] = 'b';
    } else {
        tag[2] = 'w';
        tag[3] = 'b';
    }
    tag[4] = '\0';
    return tag;
}

static void avi_write_info_tag(ByteIOContext *pb, const char *tag, const char *str)
{
    int len = strlen(str);
    if (len > 0) {
        len++;
        put_tag(pb, tag);
        put_le32(pb, len);
        put_strz(pb, str);
        if (len & 1)
            put_byte(pb, 0);
    }
}

static int avi_write_counters(AVFormatContext* s, int riff_id)
{
    ByteIOContext *pb = s->pb;
    AVIContext *avi = s->priv_data;
    int n, au_byterate, au_ssize, au_scale, nb_frames = 0;
    int64_t file_size;
    AVCodecContext* stream;

    file_size = url_ftell(pb);
    for(n = 0; n < s->nb_streams; n++) {
        AVIStream *avist= s->streams[n]->priv_data;

        assert(avist->frames_hdr_strm);
        stream = s->streams[n]->codec;
        url_fseek(pb, avist->frames_hdr_strm, SEEK_SET);
        ff_parse_specific_params(stream, &au_byterate, &au_ssize, &au_scale);
        if(au_ssize == 0) {
            put_le32(pb, avist->packet_count);
        } else {
            put_le32(pb, avist->audio_strm_length / au_ssize);
        }
        if(stream->codec_type == AVMEDIA_TYPE_VIDEO)
            nb_frames = FFMAX_T(nb_frames, avist->packet_count);
    }
    if(riff_id == 1) {
        assert(avi->frames_hdr_all);
        url_fseek(pb, avi->frames_hdr_all, SEEK_SET);
        put_le32(pb, nb_frames);
    }
    url_fseek(pb, file_size, SEEK_SET);

    return 0;
}

int avi_write_header(AVFormatContext *s)
{
    AVIContext *avi = s->priv_data;
    ByteIOContext *pb = s->pb;
    int bitrate, n, i, nb_frames, au_byterate, au_ssize, au_scale;
    AVCodecContext *stream, *video_enc;
    int64_t list1, list2, strh, strf;
    AVMetadataTag *t = NULL;
//	printf("%s line %d avi %x\n",__FUNCTION__,__LINE__,avi);

    for(n=0;n<s->nb_streams;n++) {
        s->streams[n]->priv_data= av_mallocz_t(sizeof(AVIStream));
        if(!s->streams[n]->priv_data)
            return AVERROR(ENOMEM);
    }
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* header list */
    avi->riff_id = 0;
//	printf("%s line %d\n",__FUNCTION__,__LINE__);
    list1 = avi_start_new_riff(s, pb, "AVI ", "hdrl");
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* avi header */
    put_tag(pb, "avih");
    put_le32(pb, 14 * 4);
    bitrate = 0;

    video_enc = NULL;
    for(n=0;n<s->nb_streams;n++) {
        stream = s->streams[n]->codec;
//		printf("%s line %d bit_rate %d,time_base.num %d,time_base.den %d\n",__FUNCTION__,__LINE__,stream->bit_rate,stream->time_base.num,stream->time_base.den);
        bitrate += stream->bit_rate;
        if(stream->codec_type == AVMEDIA_TYPE_VIDEO)
            video_enc = stream;
    }

    nb_frames = 0;

    if(video_enc){
//		printf("video_enc->time_base.num %d ,video_enc->time_base.den %d, ms time %d\n ",video_enc->time_base.num,video_enc->time_base.den,(uint32_t)(INT64_C(1000000) * video_enc->time_base.num / video_enc->time_base.den));
        put_le32(pb, (uint32_t)(INT64_C(1000000) * video_enc->time_base.num / video_enc->time_base.den));
    } else {
        put_le32(pb, 0);
    }
//	printf("%s line %d time_base.num %d,time_base.den %d\n",__FUNCTION__,__LINE__,video_enc->time_base.num,video_enc->time_base.den);
    put_le32(pb, bitrate / 8); /* XXX: not quite exact */
    put_le32(pb, 0); /* padding */
    if (url_is_streamed(pb))
        put_le32(pb, AVIF_TRUSTCKTYPE | AVIF_ISINTERLEAVED); /* flags */
    else
        put_le32(pb, AVIF_TRUSTCKTYPE | AVIF_HASINDEX | AVIF_ISINTERLEAVED); /* flags */
    avi->frames_hdr_all = url_ftell(pb); /* remember this offset to fill later */
    put_le32(pb, nb_frames); /* nb frames, filled later */
    put_le32(pb, 0); /* initial frame */
    put_le32(pb, s->nb_streams); /* nb streams */
    put_le32(pb, 1024 * 1024); /* suggested buffer size */
//	printf("%s line %d\n",__FUNCTION__,__LINE__);
    if(video_enc){
        put_le32(pb, video_enc->width);
        put_le32(pb, video_enc->height);
    } else {
        put_le32(pb, 0);
        put_le32(pb, 0);
    }
    put_le32(pb, 0); /* reserved */
    put_le32(pb, 0); /* reserved */
    put_le32(pb, 0); /* reserved */
    put_le32(pb, 0); /* reserved */
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* stream list */
    for(i=0;i<n;i++) {
        AVIStream *avist= s->streams[i]->priv_data;
        list2 = ff_start_tag(pb, "LIST");
        put_tag(pb, "strl");

        stream = s->streams[i]->codec;

        /* stream generic header */
        strh = ff_start_tag(pb, "strh");
        switch(stream->codec_type) {
	        case AVMEDIA_TYPE_SUBTITLE:
	            // XSUB subtitles behave like video tracks, other subtitles
	            // are not (yet) supported.
	            if (stream->codec_id != CODEC_ID_XSUB) break;
	        case AVMEDIA_TYPE_VIDEO: put_tag(pb, "vids"); break;
	        case AVMEDIA_TYPE_AUDIO: put_tag(pb, "auds"); break;
	//        case AVMEDIA_TYPE_TEXT : put_tag(pb, "txts"); break;
	        case AVMEDIA_TYPE_DATA : put_tag(pb, "dats"); break;
		default:
			break;
        }
        if(stream->codec_type == AVMEDIA_TYPE_VIDEO ||
           stream->codec_id == CODEC_ID_XSUB)
            put_le32(pb, stream->codec_tag);
        else
            put_le32(pb, 1);
        put_le32(pb, 0); /* flags */
        put_le16(pb, 0); /* priority */
        put_le16(pb, 0); /* language */
        put_le32(pb, 0); /* initial frame */

        ff_parse_specific_params(stream, &au_byterate, &au_ssize, &au_scale);
//		printf("%s line %d au_scale %d, au_byterate %d,au_ssize %d\n ",__FUNCTION__,__LINE__,au_scale,au_byterate,au_ssize);

        put_le32(pb, au_scale); /* scale */
        put_le32(pb, au_byterate); /* rate */
        av_set_pts_info(s->streams[i], 64, au_scale, au_byterate);

        put_le32(pb, 0); /* start */
        avist->frames_hdr_strm = url_ftell(pb); /* remember this offset to fill later */
        if (url_is_streamed(pb))
            put_le32(pb, AVI_MAX_RIFF_SIZE); /* FIXME: this may be broken, but who cares */
        else
            put_le32(pb, 0); /* length, XXX: filled later */

        /* suggested buffer size */ //FIXME set at the end to largest chunk
        if(stream->codec_type == AVMEDIA_TYPE_VIDEO)
            put_le32(pb, 1024 * 1024);
        else if(stream->codec_type == AVMEDIA_TYPE_AUDIO)
            put_le32(pb, 12 * 1024);
        else
            put_le32(pb, 0);
        put_le32(pb, -1); /* quality */
        put_le32(pb, au_ssize); /* sample size */
//        put_le32(pb, 0);
//        put_le16(pb, stream->width);
//        put_le16(pb, stream->height);
        ff_end_tag(pb, strh);
//		printf("%s line %d\n",__FUNCTION__,__LINE__);

      if(stream->codec_type != AVMEDIA_TYPE_DATA){
        strf = ff_start_tag(pb, "strf");
        switch(stream->codec_type) {
	        case AVMEDIA_TYPE_SUBTITLE:
	            // XSUB subtitles behave like video tracks, other subtitles
	            // are not (yet) supported.
	            if (stream->codec_id != CODEC_ID_XSUB) break;
	        case AVMEDIA_TYPE_VIDEO:
	            ff_put_bmp_header(pb, stream, ff_codec_bmp_tags, 0);
	            break;
	        case AVMEDIA_TYPE_AUDIO:
	            if (ff_put_wav_header(pb, stream) < 0) {
	                return -1;
	            }
	            break;
	        default:
	            return -1;
        }
        ff_end_tag(pb, strf);
        if ((t = av_metadata_get(s->streams[i]->metadata, "strn", NULL, 0))) {
            avi_write_info_tag(s->pb, t->key, t->value);
            t = NULL;
        }
        //FIXME a limitation of metadata conversion system
        else if ((t = av_metadata_get(s->streams[i]->metadata, "INAM", NULL, 0))) {
            avi_write_info_tag(s->pb, "strn", t->value);
            t = NULL;
        }
      }
//	  printf("%s line %d\n",__FUNCTION__,__LINE__);

        if (!url_is_streamed(pb)) {
            unsigned char tag[5];
            int j;

            /* Starting to lay out AVI OpenDML master index.
             * We want to make it JUNK entry for now, since we'd
             * like to get away without making AVI an OpenDML one
             * for compatibility reasons.
             */
            avist->indexes.entry = avist->indexes.ents_allocated = 0;
            avist->indexes.indx_start = ff_start_tag(pb, "JUNK");
            put_le16(pb, 4);        /* wLongsPerEntry */
            put_byte(pb, 0);        /* bIndexSubType (0 == frame index) */
            put_byte(pb, 0);        /* bIndexType (0 == AVI_INDEX_OF_INDEXES) */
            put_le32(pb, 0);        /* nEntriesInUse (will fill out later on) */
            put_tag(pb, avi_stream2fourcc((char *)(&tag[0]), i, stream->codec_type));
                                    /* dwChunkId */
            put_le64(pb, 0);        /* dwReserved[3]
            put_le32(pb, 0);           Must be 0.    */
            for (j=0; j < AVI_MASTER_INDEX_SIZE * 2; j++)
                 put_le64(pb, 0);
            ff_end_tag(pb, avist->indexes.indx_start);
        }
//		printf("%s line %d\n",__FUNCTION__,__LINE__);

        if(   stream->codec_type == AVMEDIA_TYPE_VIDEO
           && s->streams[i]->sample_aspect_ratio.num>0
           && s->streams[i]->sample_aspect_ratio.den>0){
            int vprp= ff_start_tag(pb, "vprp");
			AVRational c = {stream->width, stream->height};
			AVRational b = s->streams[i]->sample_aspect_ratio;
            //AVRational dar = av_mul_q(s->streams[i]->sample_aspect_ratio,tmp);
                                      //(AVRational){stream->width, stream->height});
			av_reduce(&b.num, &b.den, b.num * (int64_t)c.num, b.den * (int64_t)c.den, INT_MAX);
			AVRational dar = b;

            int num, den;
            av_reduce(&num, &den, dar.num, dar.den, 0xFFFF);

            put_le32(pb, 0); //video format  = unknown
            put_le32(pb, 0); //video standard= unknown
            put_le32(pb, lrintf(1.0/av_q2d(stream->time_base)));
            put_le32(pb, stream->width );
            put_le32(pb, stream->height);
            put_le16(pb, den);
            put_le16(pb, num);
            put_le32(pb, stream->width );
            put_le32(pb, stream->height);
            put_le32(pb, 1); //progressive FIXME

            put_le32(pb, stream->height);
            put_le32(pb, stream->width );
            put_le32(pb, stream->height);
            put_le32(pb, stream->width );
            put_le32(pb, 0);
            put_le32(pb, 0);

            put_le32(pb, 0);
            put_le32(pb, 0);
            ff_end_tag(pb, vprp);
        }

        ff_end_tag(pb, list2);
    }
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    if (!url_is_streamed(pb)) {
        /* AVI could become an OpenDML one, if it grows beyond 2Gb range */
        avi->odml_list = ff_start_tag(pb, "JUNK");
        put_tag(pb, "odml");
        put_tag(pb, "dmlh");
        put_le32(pb, 248);
        for (i = 0; i < 248; i+= 4)
             put_le32(pb, 0);
        ff_end_tag(pb, avi->odml_list);
    }

    ff_end_tag(pb, list1);
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    list2 = ff_start_tag(pb, "LIST");
    put_tag(pb, "INFO");
    for (i = 0; *ff_avi_tags[i]; i++) {
        if ((t = av_metadata_get(s->metadata, ff_avi_tags[i], NULL, AV_METADATA_MATCH_CASE)))
            avi_write_info_tag(s->pb, t->key, t->value);
    }
    ff_end_tag(pb, list2);
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    /* some padding for easier tag editing */
    list2 = ff_start_tag(pb, "JUNK");
    for (i = 0; i < 1016; i += 4)
        put_le32(pb, 0);
    ff_end_tag(pb, list2);
//	printf("%s line %d\n",__FUNCTION__,__LINE__);

    avi->movi_list = ff_start_tag(pb, "LIST");
    put_tag(pb, "movi");

//	printf("%s line %d\n",__FUNCTION__,__LINE__);
    ff_end_tag(pb, avi->riff_start);

    put_flush_packet(pb);

    return 0;
}

static int avi_write_ix(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    AVIContext *avi = s->priv_data;
    char tag[5];
    char ix_tag[] = "ix00";
    int i, j;

    assert(!url_is_streamed(pb));

    if (avi->riff_id > AVI_MASTER_INDEX_SIZE)
        return -1;

    for (i=0;i<s->nb_streams;i++) {
        AVIStream *avist= s->streams[i]->priv_data;
         int64_t ix, pos;

         avi_stream2fourcc(&tag[0], i, s->streams[i]->codec->codec_type);
         ix_tag[3] = '0' + i;

         /* Writing AVI OpenDML leaf index chunk */
         ix = url_ftell(pb);
         put_tag(pb, &ix_tag[0]);     /* ix?? */
         put_le32(pb, avist->indexes.entry * 8 + 24);
                                      /* chunk size */
         put_le16(pb, 2);             /* wLongsPerEntry */
         put_byte(pb, 0);             /* bIndexSubType (0 == frame index) */
         put_byte(pb, 1);             /* bIndexType (1 == AVI_INDEX_OF_CHUNKS) */
         put_le32(pb, avist->indexes.entry);
                                      /* nEntriesInUse */
         put_tag(pb, &tag[0]);        /* dwChunkId */
         put_le64(pb, avi->movi_list);/* qwBaseOffset */
         put_le32(pb, 0);             /* dwReserved_3 (must be 0) */

         for (j=0; j<avist->indexes.entry; j++) {
             AVIIentry* ie = avi_get_ientry(&avist->indexes, j);
             put_le32(pb, ie->pos + 8);
             put_le32(pb, ((uint32_t)ie->len & ~0x80000000) |
                          (ie->flags & 0x10 ? 0 : 0x80000000));
         }
         //put_flush_packet(pb);//IAN
         pos = url_ftell(pb);

         /* Updating one entry in the AVI OpenDML master index */
         url_fseek(pb, avist->indexes.indx_start - 8, SEEK_SET);
         put_tag(pb, "indx");                 /* enabling this entry */
         url_fskip(pb, 8);
         put_le32(pb, avi->riff_id);          /* nEntriesInUse */
         url_fskip(pb, 16*avi->riff_id);
         put_le64(pb, ix);                    /* qwOffset */
         put_le32(pb, pos - ix);              /* dwSize */
         put_le32(pb, avist->indexes.entry); /* dwDuration */

         url_fseek(pb, pos, SEEK_SET);
    }
    return 0;
}

static int avi_write_idx1(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
//    AVIContext *avi = s->priv_data;
    int64_t idx_chunk;
    int i;
    char tag[5];

    if (!url_is_streamed(pb)) {
        AVIStream *avist;
        AVIIentry* ie = 0, *tie;
        int empty, stream_id = -1;

        idx_chunk = ff_start_tag(pb, "idx1");
		if(s->aviIdxBuf.buffer == NULL || s->aviIdxBuf.len == 0 || s->aviIdxBuf.bufSize == 0){
	        for(i=0; i<s->nb_streams; i++){
	            avist= s->streams[i]->priv_data;
	            avist->entry=0;
	        }

	        do {
	            empty = 1;
	            for (i=0; i<s->nb_streams; i++) {
	                avist= s->streams[i]->priv_data;
	                 if (avist->indexes.entry <= avist->entry)
	                     continue;

	                 tie = avi_get_ientry(&avist->indexes, avist->entry);
	                 if (empty || tie->pos < ie->pos) {
	                     ie = tie;
	                     stream_id = i;
	                 }
	                 empty = 0;
	            }
	            if (!empty) {
	                avist= s->streams[stream_id]->priv_data;
	                avi_stream2fourcc(&tag[0], stream_id,
	                                  s->streams[stream_id]->codec->codec_type);
	                put_tag(pb, &tag[0]);
	                put_le32(pb, ie->flags);
	                put_le32(pb, ie->pos);
	                put_le32(pb, ie->len);
	                avist->entry++;
	            }
	        } while (!empty);
		} else {
			//write idx buffer //IAN
			printf("@@@@@@@@Write idx buffer\n");
			if((s->aviIdxBuf.buffer != NULL) && (s->aviIdxBuf.len >0)){
				put_buffer(pb,(const unsigned char*)s->aviIdxBuf.buffer,s->aviIdxBuf.len);
			}
			
		}
        ff_end_tag(pb, idx_chunk);

        //avi_write_counters(s, avi->riff_id);
    }
    return 0;
}


static int __avi_write_algm(ByteIOContext *pb, char * tag, AlgChk *pAlgChk)
{
    int64_t algm_chk;
    List *pos_entry, *n_entry;
    if(!pAlgChk  || list_empty(&pAlgChk->listEntry)) {
        printf("avi_write_algm() no %s data to write.\n", tag);
        return -1;
    }

    algm_chk = ff_start_tag(pb, tag);
    list_for_each_safe(pos_entry, n_entry, &pAlgChk->listEntry) {
        AlgChk* entry = list_entry(pos_entry, AlgChk, listEntry);
        if (entry) {
            //printf("avi_write_algm() set data\n");
            put_le32(pb, entry->tmPoint);
            put_le32(pb, entry->Duration);
            list_del_init(&entry->listEntry);
            free(entry);
        }
    }
    ff_end_tag(pb, algm_chk);
    return 0;
}

// note: this is not an official code
// fcws size timestamp duration timestamp duration ..
static int avi_write_algm(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    unsigned char flag = 0;
    int i=0;
    int64_t algm_chk;
    
    //!!must correspond to AlgChkType that was defined in algchunk.h 
    const char* algChkTag[ALGCHK_TYPE_NB] = {"fcws", "ldws","mdws"};

    //if no any alg chunk to write, than not wirting "algm" chunk
    for(i=0; i<ALGCHK_TYPE_NB; i++) {
        if(s->algChkHead[i]!=NULL && !list_empty(&s->algChkHead[i]->listEntry)) {
            flag = 1;
            break;
        }
    }
    if(flag == 0) {
        printf("avi_write_algm() no data to write.\n");
        return -1;
    }
    if (!url_is_streamed(pb)) {
        algm_chk = ff_start_tag(pb, "algm");
        for(i=0; i<ALGCHK_TYPE_NB; i++) {
        	__avi_write_algm(pb, algChkTag[i], s->algChkHead[i]);
        }
        ff_end_tag(pb, algm_chk);
    }
    return 0;
}

/*
// note: this is not an official code
// fcws size timestamp duration timestamp duration ..
static int avi_write_algm(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    AlgChk *fcwHead = s->fcwChk_head;
    AlgChk *ldwHead = s->ldwChk_head;
    AlgChk *mdwHead = s->mdwChk_head;
    
    int64_t algm_chk;
    if( (fcwHead!=NULL  || list_empty(&fcwHead->listEntry)) &&
	  (!ldwHead  || list_empty(&ldwHead->listEntry)) &&
	  (!mdwHead  || list_empty(&mdwHead->listEntry)))
    {
        printf("avi_write_algm() no data to write.\n");
        return -1;
    }
    
    if (!url_is_streamed(pb)) {
        algm_chk = ff_start_tag(pb, "algm");
        __avi_write_algm(pb, "fcws", fcwHead);
        __avi_write_algm(pb, "ldws", ldwHead);
	 __avi_write_algm(pb, "mdws", mdwHead);
        ff_end_tag(pb, algm_chk);
    }
    return 0;
}
*/

/*
void av_destruct_packet_nofree(AVPacket *pkt)
{
    pkt->data = NULL; pkt->size = 0;
}
*/
/*
static void av_init_packet(AVPacket *pkt)
{
    pkt->pts   = AV_NOPTS_VALUE;
    pkt->dts   = AV_NOPTS_VALUE;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->flags = 0;
    pkt->stream_id = 0;
    pkt->destruct= av_destruct_packet_nofree;
}
*/

int avi_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVIContext *avi = s->priv_data;
    ByteIOContext *pb = s->pb;
    unsigned char tag[5];
    unsigned int flags=0;
    const int stream_index= pkt->stream_index;
    AVIStream *avist= s->streams[stream_index]->priv_data;
    AVCodecContext *enc= s->streams[stream_index]->codec;
    int size= pkt->size;
	unsigned char *pIdxBuf= NULL;
//    av_log(s, AV_LOG_DEBUG, "%"PRId64" %d %d\n", pkt->dts, avi->packet_count[stream_index], stream_index);
    while(enc->block_align==0 && pkt->dts != AV_NOPTS_VALUE && pkt->dts > avist->packet_count){
        AVPacket empty_packet;
printf("%s line %d block_align %d,dts %lld,packet_count %d\n",__FUNCTION__,__LINE__,enc->block_align,pkt->dts,avist->packet_count);

        av_init_packet(&empty_packet);
        empty_packet.size= 0;
        empty_packet.data= NULL;
        empty_packet.stream_index= stream_index;
        avi_write_packet(s, &empty_packet);
//        av_log(s, AV_LOG_DEBUG, "dup %"PRId64" %d\n", pkt->dts, avi->packet_count[stream_index]);
    }
    avist->packet_count++;


	if(avist->packet_count == 10) {
	        ff_end_tag(pb, avi->movi_list);
		 //printf("avi_write_packet update movi_list\n");
	}

    // Make sure to put an OpenDML chunk when the file size exceeds the limits
    if (!url_is_streamed(pb) &&
        (url_ftell(pb) - avi->riff_start > AVI_MAX_RIFF_SIZE)) {

        avi_write_ix(s);
        ff_end_tag(pb, avi->movi_list);

        if (avi->riff_id == 1)
            avi_write_idx1(s);

        ff_end_tag(pb, avi->riff_start);
        avi->movi_list = avi_start_new_riff(s, pb, "AVIX", "movi");
    }

    avi_stream2fourcc((char *)(&tag[0]), stream_index, enc->codec_type);
    if(pkt->flags&AV_PKT_FLAG_KEY)
        flags = 0x10;
    if (enc->codec_type == AVMEDIA_TYPE_AUDIO) {
       avist->audio_strm_length += size;
    }
    if (enc->codec_type == AVMEDIA_TYPE_VIDEO || enc->codec_type == AVMEDIA_TYPE_AUDIO) {
	    if (!url_is_streamed(s->pb)) {
	        AVIIndex* idx = &avist->indexes;
	        int cl = idx->entry / AVI_INDEX_CLUSTER_SIZE;
	        int id = idx->entry % AVI_INDEX_CLUSTER_SIZE;
	        if (idx->ents_allocated <= idx->entry) {
				//printf("ents_allocated size %d\n",(cl+1)*sizeof(void*));
	            idx->cluster = av_realloc_t(idx->cluster, (cl+1)*sizeof(void*));
	            if (!idx->cluster)
	                return -1;
			//	printf("cluster[cl] size %d\n",cl,AVI_INDEX_CLUSTER_SIZE*sizeof(AVIIentry));
	            idx->cluster[cl] = av_malloc_t(AVI_INDEX_CLUSTER_SIZE*sizeof(AVIIentry));
	            if (!idx->cluster[cl])
	                return -2;
	            idx->ents_allocated += AVI_INDEX_CLUSTER_SIZE;
	        }

	        idx->cluster[cl][id].flags = flags;
	        idx->cluster[cl][id].pos = url_ftell(pb) - avi->movi_list;
	        idx->cluster[cl][id].len = size;
	        idx->entry++;
			if((s->aviIdxBuf.buffer != NULL) && (s->aviIdxBuf.len < s->aviIdxBuf.bufSize)){
				pIdxBuf = (unsigned char *)(s->aviIdxBuf.buffer) + s->aviIdxBuf.len;
				pIdxBuf=puttag((unsigned char *)pIdxBuf, (const char *)&(tag[0]));
				pIdxBuf=putle32((unsigned char *)pIdxBuf, idx->cluster[cl][id].flags);
				pIdxBuf=putle32((unsigned char *)pIdxBuf, idx->cluster[cl][id].pos);
				pIdxBuf=putle32((unsigned char *)pIdxBuf, idx->cluster[cl][id].len);
				s->aviIdxBuf.len += pIdxBuf - ((unsigned char *) (s->aviIdxBuf.buffer) + s->aviIdxBuf.len);
			}
	    }
    }
	//printf("%s line %d\n",__FUNCTION__,__LINE__);

    put_buffer(pb, tag, 4);
    put_le32(pb, size);
    put_buffer(pb, pkt->data, size);
    if (size & 1)
        put_byte(pb, 0);

	//printf("%s line %d\n",__FUNCTION__,__LINE__);
	s->buffer_data_len += size +8;
	
    //put_flush_packet(pb);
	//printf("%s line %d\n",__FUNCTION__,__LINE__);
   	if(pb->error != 0) 
		return -3;
    return pb->error;//0

	//return 0;
}

#define DEBUG_WRITE_TRAILER 0

int avi_write_trailer(AVFormatContext *s)
{
    AVIContext *avi = s->priv_data;
    ByteIOContext *pb = s->pb;
    int res = 0;
    int i, j, n, nb_frames;
    int64_t file_size;
#if DEBUG_WRITE_TRAILER
struct timeval tv; 	
 gettimeofday(&tv, NULL);
 printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);
#endif
	#if 1
    if (!url_is_streamed(pb)){
#if DEBUG_WRITE_TRAILER
		 gettimeofday(&tv, NULL);
		 printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);
#endif
        if (avi->riff_id == 1) {
#if DEBUG_WRITE_TRAILER
			 gettimeofday(&tv, NULL);
			 printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);
#endif
            ff_end_tag(pb, avi->movi_list);
            res = avi_write_idx1(s);
            avi_write_algm(s);
            ff_end_tag(pb, avi->riff_start);
#if DEBUG_WRITE_TRAILER
			 gettimeofday(&tv, NULL);
			 printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);
#endif
			avi_write_counters(s, avi->riff_id);
        } else {
            avi_write_ix(s);
            ff_end_tag(pb, avi->movi_list);
            ff_end_tag(pb, avi->riff_start);

            file_size = url_ftell(pb);
            url_fseek(pb, avi->odml_list - 8, SEEK_SET);
            put_tag(pb, "LIST"); /* Making this AVI OpenDML one */
            url_fskip(pb, 16);

            for (n=nb_frames=0;n<s->nb_streams;n++) {
	//			printf("%s line %d\n",__FUNCTION__,__LINE__);
                AVCodecContext *stream = s->streams[n]->codec;
                AVIStream *avist= s->streams[n]->priv_data;

                if (stream->codec_type == AVMEDIA_TYPE_VIDEO) {
                    if (nb_frames < avist->packet_count)
                        nb_frames = avist->packet_count;
                } else {
                    if (stream->codec_id == CODEC_ID_MP2 || stream->codec_id == CODEC_ID_MP3) {
                        nb_frames += avist->packet_count;
                    }
                }
            }
            put_le32(pb, nb_frames);
            url_fseek(pb, file_size, SEEK_SET);

            avi_write_counters(s, avi->riff_id);
        }
    }
	#endif
	//gettimeofday(&tv, NULL);
	//printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);
    put_flush_packet(pb);

    for (i=0; i<s->nb_streams; i++) {
//		printf("%s line %d\n",__FUNCTION__,__LINE__);
         AVIStream *avist= s->streams[i]->priv_data;
		 //printf("%s line %d\n times %d\n",__FUNCTION__,__LINE__,avist->indexes.ents_allocated/AVI_INDEX_CLUSTER_SIZE);
         for (j=0; j<avist->indexes.ents_allocated/AVI_INDEX_CLUSTER_SIZE; j++)
              av_free_t(avist->indexes.cluster[j]);
         av_freep_t(&avist->indexes.cluster);
//		 printf("%s line %d\n",__FUNCTION__,__LINE__);
         avist->indexes.ents_allocated = avist->indexes.entry = 0;
    }
	//gettimeofday(&tv, NULL);
	//printf("%s line %d time %u:%u\n",__FUNCTION__,__LINE__,tv.tv_sec, tv.tv_usec);

    return res;
}
/*
AVOutputFormat avi_muxer = {
    "avi",
    NULL_IF_CONFIG_SMALL("AVI format"),
    "video/x-msvideo",
    "avi",
    sizeof(AVIContext),
    CODEC_ID_ADPCM_MS,
    CODEC_ID_H264,
    avi_write_header,
    avi_write_packet,
    avi_write_trailer,
    .codec_tag= (const AVCodecTag* const []){ff_codec_bmp_tags, ff_codec_wav_tags, 0},
    .flags= AVFMT_VARIABLE_FPS,
    .metadata_conv = ff_avi_metadata_conv,
};
*/
