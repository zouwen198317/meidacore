#ifndef STRING_OVERLAY_RGB_H
#define STRING_OVERLAY_RGB_H
#include"common.h"
#include"dot_rgb.h"
int dot_overlay_rgb(uchar *str[],
               const font_info f_info,
                 uchar *rgbBuf,
                 uint width,
                 uint height,
                 uint X,
                 uint Y);
#endif
