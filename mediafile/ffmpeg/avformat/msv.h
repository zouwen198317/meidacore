/**
 * @file msv.h
 * MSV common header
 *
 * Copyright (c) 2006 The FFmpeg Project.
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

#ifndef AVFORMAT_MSV_H
#define AVFORMAT_MSV_H

#include "avio.h"
#include "avcodec_common.h"
#include "flv_ffmpeg.h"

#include "utils.h"

#define USESYNC 1

#define FRAME_POSLIST 0

/* offsets for packed values */
#define MSV_AUDIO_SAMPLESSIZE_OFFSET 1
#define MSV_AUDIO_SAMPLERATE_OFFSET  2
#define MSV_AUDIO_CODECID_OFFSET     4

#define MSV_VIDEO_FRAMETYPE_OFFSET   4

/* bitmasks to isolate specific values */
#define MSV_AUDIO_CHANNEL_MASK    0x01
#define MSV_AUDIO_SAMPLESIZE_MASK 0x02
#define MSV_AUDIO_SAMPLERATE_MASK 0x0c
#define MSV_AUDIO_CODECID_MASK    0xf0

#define MSV_VIDEO_CODECID_MASK    0x0f
#define MSV_VIDEO_FRAMETYPE_MASK  0xf0

#if 0
#define AMF_END_OF_OBJECT         0x09

#define AVFMT_GLOBALHEADER  0x0040 /* format wants global header */
#define AVFMT_VARIABLE_FPS  0x0400 /**< Format allows variable fps. */

#define AVFMTCTX_NOHEADER      0x0001 /* signal that no header is present
  (streams are added dynamically) */
#endif

#define AVERROR_IO          AVERROR(EIO)     /**< I/O error */
#define AVERROR_NOMEM       AVERROR(ENOMEM)  /**< not enough memory */

/**
 * Required number of additionally allocated bytes at the end of the input bitstream for decoding.
 * This is mainly needed because some optimized bitstream readers read
 * 32 or 64 bit at once and could read over the end.<br>
 * Note: If the first 23 bits of the additional bytes are not 0, then damaged
 * MPEG bitstreams could cause overread and segfault.
 */
//#define FF_INPUT_BUFFER_PADDING_SIZE 8


#define AVPROBE_SCORE_MAX 100

enum {
    MSV_HEADER_FLAG_HASDATA = 1,
    MSV_HEADER_FLAG_HASVIDEO = 4,
    MSV_HEADER_FLAG_HASAUDIO = 32,
};

enum {
    MSV_TAG_TYPE_AUDIO = 0x08,
    MSV_TAG_TYPE_VIDEO = 0x09,
    MSV_TAG_TYPE_META  = 0x12,
    MSV_TAG_TYPE_DATA  = 0x16,
};

enum {
    MSV_MONO   = 0,
    MSV_STEREO = 1,
};

enum {
    MSV_SAMPLESSIZE_8BIT  = 0,
    MSV_SAMPLESSIZE_16BIT = 1 << MSV_AUDIO_SAMPLESSIZE_OFFSET,
};

enum {
    MSV_SAMPLERATE_SPECIAL = 0, /**< signifies 5512Hz and 8000Hz in the case of NELLYMOSER */
    MSV_SAMPLERATE_11025HZ = 1 << MSV_AUDIO_SAMPLERATE_OFFSET,
    MSV_SAMPLERATE_22050HZ = 2 << MSV_AUDIO_SAMPLERATE_OFFSET,
    MSV_SAMPLERATE_44100HZ = 3 << MSV_AUDIO_SAMPLERATE_OFFSET,
};

enum {
    MSV_CODECID_PCM                  = 0,
    MSV_CODECID_ADPCM                = 1 << MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_MP3                  = 2 << MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_PCM_LE               = 3 << MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_NELLYMOSER_8KHZ_MONO = 5 << MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_NELLYMOSER           = 6 << MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_AAC                  = 10<< MSV_AUDIO_CODECID_OFFSET,
    MSV_CODECID_SPEEX                = 11<< MSV_AUDIO_CODECID_OFFSET,
};

enum {
    MSV_CODECID_H263    = 2,
    MSV_CODECID_SCREEN  = 3,
    MSV_CODECID_VP6     = 4,
    MSV_CODECID_VP6A    = 5,
    MSV_CODECID_SCREEN2 = 6,
    MSV_CODECID_H264    = 7,
};

enum {
    MSV_FRAME_KEY        = 1 << MSV_VIDEO_FRAMETYPE_OFFSET,
    MSV_FRAME_INTER      = 2 << MSV_VIDEO_FRAMETYPE_OFFSET,
    MSV_FRAME_DISP_INTER = 3 << MSV_VIDEO_FRAMETYPE_OFFSET,
};

/*
typedef enum {
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;

*/


/**
 * Rational number num/den.
 */



/*
typedef struct AVPacket {
    uint64_t pts;                            ///< presentation time stamp in time_base units
    uint64_t dts;                            ///< decompression time stamp in time_base units
	//uint8_t frameCnt;
    uint8_t *data;
    int   size;
    int   stream_id;						///流ID
    int   flags;							///是否关键帧
    int   duration;                         ///< presentation duration in time_base units (0 if not available)
    void  (*destruct)(struct AVPacket *);
    void  *priv;
    int64_t pos;                            ///< byte position in stream, -1 if unknown
	int   maxsize;
} AVPacket;
*/

//#define PKT_FLAG_KEY   0x0001
struct AVFormatContext;

typedef struct AVCodecContext_b {
    
	  /**
     * the average bitrate
     * - encoding: Set by user; unused for constant quantizer encoding.
     * - decoding: Set by libavcodec. 0 or some bitrate if this info is available in the stream.
     */
    int bit_rate;

    /**
     * number of bits the bitstream is allowed to diverge from the reference.
     *           the reference can be CBR (for CBR pass1) or VBR (for pass2)
     * - encoding: Set by user; unused for constant quantizer encoding.
     * - decoding: unused
     */
    int bit_rate_tolerance;

    /**
     * CODEC_FLAG_*.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int flags;

	/**
     * some codecs need / can use extradata like Huffman tables.
     * mjpeg: Huffman tables
     * rv10: additional flags
     * mpeg4: global headers (they can be in the bitstream or here)
     * The allocated memory should be FF_INPUT_BUFFER_PADDING_SIZE bytes larger
     * than extradata_size to avoid prolems if it is read with the bitstream reader.
     * The bytewise contents of extradata must not depend on the architecture or CPU endianness.
     * - encoding: Set/allocated/freed by libavcodec.
     * - decoding: Set/allocated/freed by user.
     */
    uint8_t *extradata;
    int extradata_size;

	/**
     * This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identically 1.
     * - encoding: MUST be set by user.
     * - decoding: Set by libavcodec.
     */
    AVRational time_base;

    /* video only */
    /**
     * picture width / height.
     * - encoding: MUST be set by user.
     * - decoding: Set by libavcodec.
     * Note: For compatibility it is possible to set this instead of
     * coded_width/height before decoding.
     */
    int width, height;
    /**
     * bits per sample/pixel from the demuxer (needed for huffyuv).
     * - encoding: Set by libavcodec.
     * - decoding: Set by user.
     */
     int bits_per_sample;
	 int bits_per_coded_sample;

	
    /* audio only */
    int sample_rate; ///< samples per second
    int channels;    ///< number of audio channels

    enum CodecType codec_type; /* see CODEC_TYPE_xxx */
    enum CodecID codec_id; /* see CODEC_ID_xxx */

    /**
     * fourcc (LSB first, so "ABCD" -> ('D'<<24) + ('C'<<16) + ('B'<<8) + 'A').
     * This is used to work around some encoder bugs.
     * A demuxer should set this to what is stored in the field used to identify the codec.
     * If there are multiple such fields in a container then the demuxer should choose the one
     * which maximizes the information about the used codec.
     * If the codec tag field in a container is larger then 32 bits then the demuxer should
     * remap the longer ID to 32 bits with a table or other structure. Alternatively a new
     * extra_codec_tag + size could be added but for this a clear advantage must be demonstrated
     * first.
     * - encoding: Set by user, if not then the default based on codec_id will be used.
     * - decoding: Set by user, will be converted to uppercase by libavcodec during init.
     */
    unsigned int codec_tag;
   
} AVCodecContext_b;

#define MSV_POSLISTNUM 1024
typedef struct MSVContext {
    int reserved;
    int64_t duration_offset;
    int64_t filesize_offset;
//#if FRAME_POSLIST	
    int64_t frameListPosition_offset;
    int64_t frameListPos;
	unsigned int posListNum;
	int64_t posList[MSV_POSLISTNUM];
//#endif	
	unsigned int last_pts;
    int64_t duration;
    int delay; ///< first dts delay for AVC
    int wrong_dts; ///< wrong dts due to negative cts
	char busnum[24];

} MSVContext;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rational to double conversion.
 * @param a rational to convert
 * @return (double) a
 */
static inline double av_q2d(AVRational a){
    return a.num / (double) a.den;
}

void av_set_pts_info(AVStream *s, int pts_wrap_bits, int pts_num, int pts_den);

AVStream *av_new_stream(AVFormatContext *s, int id);
int av_get_packet(ByteIOContext *s, AVPacket *pkt, int size);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//编码函数
int msv_write_header(AVFormatContext *s);
int msv_write_packet(AVFormatContext *s, AVPacket *pkt);
//int msv_write_video_packet(AVFormatContext *s, AVPacket *pkt,VENC_STREAM_S *pststream);

int msv_write_trailer(AVFormatContext *s);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//解码函数
int msv_read_header(AVFormatContext *s, AVFormatParameters *ap); 
int msv_read_packet(AVFormatContext *s, AVPacket *pkt);
int msv_read_release(AVFormatContext *s);
int msv_read_info(char* filename, int64_t* start_time, int64_t* duration);
int msv_seek_frame(AVFormatContext *s, uint64_t timestamp, int flags);
int msv_seek_frame_Q(AVFormatContext *s, uint64_t timestamp, int flags, int64_t file_size, int64_t duration);
int msv_read_pre_packet(AVFormatContext *s, AVPacket *pkt);

int msv_read_header_allInfo(AVFormatContext *s);
int msv_update_info(AVFormatContext *s);
#ifdef __cplusplus
}
#endif

#endif /* AVFORMAT_MSV_H */
