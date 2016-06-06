#include "ffmpeg/avcodec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int avi_read_header(AVFormatContext *s, AVFormatParameters *ap);
int avi_read_packet(AVFormatContext *s, AVPacket *pkt);
int avi_read_close(AVFormatContext *s);
int avi_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
int avi_read_videoFrames(AVFormatContext *s);


#ifdef __cplusplus
}
#endif


