
/**
 * Add a new stream to a media file.
 *
 * Can only be called in the read_header() function. If the flag
 * AVFMTCTX_NOHEADER is in the format context, then new streams
 * can be added in read_packet too.
 *
 * @param s media file handle
 * @param id file format dependent stream id
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
#include "common_t.h"

#ifdef CONFIG_WIN32
__declspec( dllexport ) AVStream *av_new_stream(AVFormatContext *s, int id)
#else
AVStream *av_new_stream(AVFormatContext *s, int id)
#endif
{
    AVStream *st;

    if (s->nb_streams >= MAX_STREAMS)
        return NULL;

    st = (AVStream *)malloc(sizeof(AVStream));
    if (!st)
        return NULL;
	memset(st, 0, sizeof(AVStream));

    st->AVCodecHDL= NULL;
    if (s->iformat) {
        /* no default bitrate if decoding */
        st->bit_rate = 0;
    }
    st->id = s->nb_streams;
    st->start_time = AV_NOPTS_VALUE;
    st->duration = AV_NOPTS_VALUE;

    /* default pts settings is MPEG like */
    av_set_pts_info(st, 33, 1, 90000);

    s->streams[s->nb_streams++] = st;
    return st;
}

/**
 * Set the pts for a given stream.
 *
 * @param s stream
 * @param pts_wrap_bits number of bits effectively used by the pts
 *        (used for wrap control, 33 is the value for MPEG)
 * @param pts_num numerator to convert to seconds (MPEG: 1)
 * @param pts_den denominator to convert to seconds (MPEG: 90000)
 */
void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                     int pts_num, int pts_den)
{
    s->time_base.num = pts_num;
    s->time_base.den = pts_den;
}

void av_destruct_packet_nofree(AVPacket *pkt)
{
    pkt->data = NULL; pkt->size = 0;
}

/* initialize optional fields of a packet */
void av_init_packet(AVPacket *pkt)
//static inline void av_init_packet(AVPacket *pkt)
{
    pkt->pts   = AV_NOPTS_VALUE;
    pkt->dts   = AV_NOPTS_VALUE;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->flags = 0;
    pkt->stream_id = 0;
    pkt->destruct= av_destruct_packet_nofree;
}

/**
 * Default packet destructor.
 */
void av_destruct_packet(AVPacket *pkt)
{
    free(pkt->data);
    pkt->data = NULL; pkt->size = 0;
}

/**
 * Allocate the payload of a packet and intialized its fields to default values.
 *
 * @param pkt packet
 * @param size wanted payload size
 * @return 0 if OK. AVERROR_xxx otherwise.
 */
int av_new_packet(AVPacket *pkt, int size)
{
	if(size > pkt->maxsize)
	{
		void *data;
		if(pkt->data)
		{
			data = pkt->data;
			pkt->data = NULL;
			free(data);
		}
		if((unsigned)size > (unsigned)size + FF_INPUT_BUFFER_PADDING_SIZE)
			return AVERROR_NOMEM;
		data = malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);
		if (!data)
			return AVERROR_NOMEM;
		memset((char*)data + size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

		av_init_packet(pkt);
		pkt->data = (uint8_t *)data;
		pkt->size = size;
		pkt->destruct = av_destruct_packet;

		pkt->maxsize = size;
	}
	else
	{
		av_init_packet(pkt);
		pkt->destruct = av_destruct_packet;
	}
    return 0;
}

/**
 * Free a packet
 *
 * @param pkt packet to free
 */
#ifdef CONFIG_WIN32
//static inline void av_free_packet(AVPacket *pkt)
#else 
void av_free_packet(AVPacket *pkt)
#endif
{
    if (pkt && pkt->destruct) {
        pkt->destruct(pkt);
    }
}

/**
 * Allocate and read the payload of a packet and intialized its fields to default values.
 *
 * @param pkt packet
 * @param size wanted payload size
 * @return >0 (read size) if OK. AVERROR_xxx otherwise.
 */
int av_get_packet(ByteIOContext *s, AVPacket *pkt, int size)
{
    int ret= av_new_packet(pkt, size);

    if(ret<0)
        return ret;

    pkt->pos= url_ftell(s);

    ret= get_buffer(s, pkt->data, size);
    if(ret<=0)
        av_free_packet(pkt);
    else
        pkt->size= ret;

    return ret;
}



void *av_fast_realloc(void *ptr, unsigned int *size, unsigned int min_size)
{
    if(min_size < *size)
        return ptr;

    *size= FFMAX_T(17*min_size/16 + 32, min_size);

	//printf("av_fast_realloc \n");//IAN
    ptr= av_realloc_t(ptr, *size);
    if(!ptr) //we could set this to the unmodified min_size but this is safer if the user lost the ptr and uses NULL now
        *size= 0;

    return ptr;
}


int av_add_index_entry(AVStream *st,
                            int64_t pos, int64_t timestamp, int size, int distance, int flags)
{
    AVIndexEntry *entries, *ie;
    int index;

    if((unsigned)st->nb_index_entries + 1 >= UINT_MAX / sizeof(AVIndexEntry))
        return -1;

    entries = av_fast_realloc(st->index_entries,
                              &st->index_entries_allocated_size,
                              (st->nb_index_entries + 1) *
                              sizeof(AVIndexEntry));
    if(!entries)
        return -1;

    st->index_entries= entries;

    index= av_index_search_timestamp(st, timestamp, AVSEEK_FLAG_ANY);

    if(index<0){
        index= st->nb_index_entries++;
        ie= &entries[index];
        assert(index==0 || ie[-1].timestamp < timestamp);
    }else{
        ie= &entries[index];
        if(ie->timestamp != timestamp){
            if(ie->timestamp <= timestamp)
                return -1;
            memmove(entries + index + 1, entries + index, sizeof(AVIndexEntry)*(st->nb_index_entries - index));
            st->nb_index_entries++;
        }else if(ie->pos == pos && distance < ie->min_distance) //do not reduce the distance
            distance= ie->min_distance;
    }

    ie->pos = pos;
    ie->timestamp = timestamp;
    ie->min_distance= distance;
    ie->size= size;
    ie->flags = flags;

    return index;
}

int av_index_search_timestamp(AVStream *st, int64_t wanted_timestamp,
                              int flags)
{
    AVIndexEntry *entries= st->index_entries;
    int nb_entries= st->nb_index_entries;
    int a, b, m;
    int64_t timestamp;

    a = - 1;
    b = nb_entries;

    //optimize appending index entries at the end
    if(b && entries[b-1].timestamp < wanted_timestamp)
        a= b-1;

    while (b - a > 1) {
        m = (a + b) >> 1;
//printf("entries[%d].timestamp %lld\n",m,entries[m].timestamp);
        timestamp = entries[m].timestamp;
        if(timestamp >= wanted_timestamp)
            b = m;
        if(timestamp <= wanted_timestamp)
            a = m;
    }
    m= (flags & AVSEEK_FLAG_BACKWARD) ? a : b;
//printf("m:%d\n", m);

    if(!(flags & AVSEEK_FLAG_ANY)){
        while(m>=0 && m<nb_entries && !(entries[m].flags & AVINDEX_KEYFRAME)){
            m += (flags & AVSEEK_FLAG_BACKWARD) ? -1 : 1;
        }
    }

    if(m == nb_entries)
        return -1;
    return  m;
}

