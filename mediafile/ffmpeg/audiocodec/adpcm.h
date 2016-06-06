#ifndef AV_ADPCM_H
#define AV_ADPCM_H
#include "avcodec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ADPCMChannelStatus {
    int predictor;
    short int step_index;
    int step;
    /* for encoding */
    int prev_sample;

    /* MS version */
    short sample1;
    short sample2;
    int coeff1;
    int coeff2;
    int idelta;
} ADPCMChannelStatus;

typedef struct ADPCMContext {
    ADPCMChannelStatus status[6];
} ADPCMContext;

int adpcm_encode_init(AVCodecContext *avctx);
int adpcm_encode_frame(AVCodecContext *avctx,
						unsigned char *frame, int buf_size, void *data);
int adpcm_encode_close(AVCodecContext *avctx);
int adpcm_decode_init(AVCodecContext * avctx);
int adpcm_decode_frame(AVCodecContext *avctx,
                            void *data, int *data_size,
                            AVPacket *avpkt);

#ifdef __cplusplus
}
#endif



#endif

