#ifndef DVR_RESAMPLE_H
#define DVR_RESAMPLE_H

#include "avcodec.h"
struct AVResampleContext;
struct ReSampleContext;
//typedef struct ReSampleContext ReSampleContext;
#ifdef __cplusplus
extern "C" {
#endif

ReSampleContext *audio_resample_init(int output_channels, int input_channels,
                                     int output_rate, int input_rate);
int audio_resample(ReSampleContext *s, short *output, short *input, int nb_samples);
void audio_resample_close(ReSampleContext *s);

#ifdef __cplusplus
}
#endif

#endif

