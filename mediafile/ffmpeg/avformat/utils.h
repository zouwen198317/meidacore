#ifndef AVFORMAT_UTILS_H
#define AVFORMAT_UTILS_H

#include "avcodec_common.h"
#include "avio.h"
#ifdef __cplusplus
extern "C" {
#endif

AVStream *av_new_stream(AVFormatContext *s, int id);
void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                     int pts_num, int pts_den);
void av_destruct_packet_nofree(AVPacket *pkt);
void av_init_packet(AVPacket *pkt);
void av_destruct_packet(AVPacket *pkt);
int av_new_packet(AVPacket *pkt, int size);
void av_free_packet(AVPacket *pkt);
int av_get_packet(ByteIOContext *s, AVPacket *pkt, int size);

void *av_fast_realloc(void *ptr, unsigned int *size, unsigned int min_size);
int av_add_index_entry(AVStream *st,
                            int64_t pos, int64_t timestamp, int size, int distance, int flags);

int av_index_search_timestamp(AVStream *st, int64_t wanted_timestamp,
                              int flags);
#ifdef __cplusplus
}
#endif

#endif

