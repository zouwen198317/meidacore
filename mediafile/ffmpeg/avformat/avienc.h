#include "ffmpeg/avcodec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int avi_write_header(AVFormatContext *s);
int avi_write_packet(AVFormatContext *s, AVPacket *pkt);
int avi_write_trailer(AVFormatContext *s);

#ifdef __cplusplus
}
#endif


