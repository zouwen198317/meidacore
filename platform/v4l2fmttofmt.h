#ifndef  _V4L2FMTTOFMT_H_
#define _V4L2FMTTOFMT_H_

#include <linux/videodev2.h>
#include "mediacore.h"


inline u32 AppFmtToV4l2Fourcc(u32 infmt)
{
	u32 pixelformat = V4L2_PIX_FMT_YUV420M;
	switch(infmt){
	    case PIX_FMT_YUV420P:   ///< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
	    		pixelformat = V4L2_PIX_FMT_YUV420;
	    break;
	    case PIX_FMT_NV12:      ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = V4L2_PIX_FMT_NV12;
	    break;
	    case PIX_FMT_NV21:      ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = V4L2_PIX_FMT_NV21;
	    break;
	    //case PIX_FMT_YVU420P:   ///< Planar YUV 4:2:0 (1 Cb & Cr sample per 2x2 Y samples) (YV12)
	    		//pixelformat = V4L2_PIX_FMT_YVU420M;
	    break;
	    case PIX_FMT_YUV422:    ///< Packed pixel, Y0 Cb Y1 Cr 
	    		pixelformat = V4L2_PIX_FMT_YUYV;
	    break;
	    case PIX_FMT_RGB24:     ///< Packed pixel, 3 bytes per pixel, RGBRGB...
	    		pixelformat = V4L2_PIX_FMT_RGB24;
	    break;
	    case PIX_FMT_BGR24:     ///< Packed pixel, 3 bytes per pixel, BGRBGR...
	    		pixelformat = V4L2_PIX_FMT_BGR24;
	    break;
	    case PIX_FMT_YUV422P:   ///< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV422P;
	    break;
	    case PIX_FMT_YUV444P:  ///< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV444;
	    break;
	   //  case PIX_FMT_RGBA32,    ///< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
	   // case PIX_FMT_BGRA32:    ///< Packed pixel, 4 bytes per pixel, ARGBARGB...
	   // case PIX_FMT_ARGB32,    ///< Packed pixel, 4 bytes per pixel, ABGRABGR..., stored in cpu endianness
	   // case PIX_FMT_ABGR32,    ///< Packed pixel, 4 bytes per pixel, RGBARGBA...
	    case PIX_FMT_RGB32:    ///< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
	    		pixelformat = V4L2_PIX_FMT_RGB32;
	    break;
	    //case PIX_FMT_xRGB32,    ///< Packed pixel, 4 bytes per pixel, xBGRxBGR..., stored in cpu endianness
	    case PIX_FMT_BGR32:    ///< Packed pixel, 4 bytes per pixel, xRGBxRGB...
	    		pixelformat = V4L2_PIX_FMT_BGR32;
	    break;
	    //case PIX_FMT_BGRx32,    ///< Packed pixel, 4 bytes per pixel, RGBxRGBx...
	    case PIX_FMT_YUV410P:   ///< Planar YUV 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV410;
	    break;
	    case PIX_FMT_YVU410P:   ///< Planar YVU 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YVU410;
	    break;
	    case PIX_FMT_YUV411P:   ///< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV411P;
	    break;
	    //case PIX_FMT_Y800,      ///< 8 bit Y plane (range [16-235])
	    case PIX_FMT_Y16:       ///< 16 bit Y plane (little endian)
	    		pixelformat = V4L2_PIX_FMT_Y16;
	    break;
	    case PIX_FMT_RGB565:    ///< always stored in cpu endianness 
	    		pixelformat = V4L2_PIX_FMT_RGB565;
	    break;
	    case PIX_FMT_RGB555:    ///< always stored in cpu endianness, most significant bit to 1 
	    		pixelformat = V4L2_PIX_FMT_RGB555;
	    break;
	    case PIX_FMT_GRAY8:
	    		pixelformat = V4L2_PIX_FMT_GREY;
	    break;
	    //case PIX_FMT_GRAY16_L,
	    //case PIX_FMT_GRAY16_B,
	    //case PIX_FMT_MONOWHITE, ///< 0 is white 
	    //case PIX_FMT_MONOBLACK, ///< 0 is black 
	    case PIX_FMT_PAL8:      ///< 8 bit with RGBA palette 
		    		pixelformat = V4L2_PIX_FMT_PAL8;
	    break;
    
	    //case PIX_FMT_YUVJ420P,  ///< Planar YUV 4:2:0 full scale (jpeg)
	    //case PIX_FMT_YUVJ422P,  ///< Planar YUV 4:2:2 full scale (jpeg)
	    //case PIX_FMT_YUVJ444P,  ///< Planar YUV 4:4:4 full scale (jpeg)
	    //case PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing(xvmc_render.h)
	    //case PIX_FMT_XVMC_MPEG2_IDCT,
	    case PIX_FMT_UYVY422:   ///< Packed pixel, Cb Y0 Cr Y1 
		    		pixelformat = V4L2_PIX_FMT_UYVY;
	    break;
	    case PIX_FMT_YVYU422:   ///< Packed pixel, Y0 Cr Y1 Cb 
		    		pixelformat = V4L2_PIX_FMT_YVYU;
	    break;
	    //case PIX_FMT_UYVY411,   ///< Packed pixel, Cb Y0 Y1 Cr Y2 Y3
	    //case PIX_FMT_V308,      ///< Packed pixel, Y0 Cb Cr

	    //case PIX_FMT_AYUV4444,  ///< Packed pixel, A0 Y0 Cb Cr
	    //case PIX_FMT_YUVA420P,   ///< Planar YUV 4:4:2:0 (1 Cr & Cb sample per 2x2 Y & A samples) (A420)
	    default: //PIX_FMT_YUV420P
	    	printf("fmt 0x%x not define, default to V4L2_PIX_FMT_YUV420M\n",infmt);
		//mFourcc = PIX_FMT_YUV420P;
		return pixelformat;
	    break;
			
	}
	//mFourcc = fmt;
	return pixelformat;
}

inline u32 V4l2FourccToAppFmt(u32 fmt)
{
	u32 pixelformat = PIX_FMT_YUV420P;
	switch(fmt){
	    case V4L2_PIX_FMT_YUV420M:///< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
	    		pixelformat = PIX_FMT_YUV420P;
	    break;
	    case V4L2_PIX_FMT_NV12:  ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = PIX_FMT_NV12;
	    break;
	    case V4L2_PIX_FMT_NV21: ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = PIX_FMT_NV21;
	    break;
	    //case PIX_FMT_YVU420P:   ///< Planar YUV 4:2:0 (1 Cb & Cr sample per 2x2 Y samples) (YV12)
	    		//pixelformat = V4L2_PIX_FMT_YVU420M;
	    break;
	    case V4L2_PIX_FMT_YUYV:    ///< Packed pixel, Y0 Cb Y1 Cr 
	    		pixelformat = PIX_FMT_YUV422;
	    break;
	    case V4L2_PIX_FMT_RGB24:     ///< Packed pixel, 3 bytes per pixel, RGBRGB...
	    		pixelformat = PIX_FMT_RGB24;
	    break;
	    case V4L2_PIX_FMT_BGR24:     ///< Packed pixel, 3 bytes per pixel, BGRBGR...
	    		pixelformat = PIX_FMT_BGR24;
	    break;
	    case V4L2_PIX_FMT_YUV422P:   ///< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
	    		pixelformat = PIX_FMT_YUV422P;
	    break;
	    case V4L2_PIX_FMT_YUV444:  ///< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
	    		pixelformat = PIX_FMT_YUV444P;
	    break;
	   //  case PIX_FMT_RGBA32,    ///< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
	   // case PIX_FMT_BGRA32:    ///< Packed pixel, 4 bytes per pixel, ARGBARGB...
	   // case PIX_FMT_ARGB32,    ///< Packed pixel, 4 bytes per pixel, ABGRABGR..., stored in cpu endianness
	   // case PIX_FMT_ABGR32,    ///< Packed pixel, 4 bytes per pixel, RGBARGBA...
	    case V4L2_PIX_FMT_RGB32:    ///< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
	    		pixelformat = PIX_FMT_RGB32;
	    break;
	    //case PIX_FMT_xRGB32,    ///< Packed pixel, 4 bytes per pixel, xBGRxBGR..., stored in cpu endianness
	    case V4L2_PIX_FMT_BGR32:    ///< Packed pixel, 4 bytes per pixel, xRGBxRGB...
	    		pixelformat = PIX_FMT_BGR32;
	    break;
	    //case PIX_FMT_BGRx32,    ///< Packed pixel, 4 bytes per pixel, RGBxRGBx...
	    case V4L2_PIX_FMT_YUV410:   ///< Planar YUV 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = PIX_FMT_YUV410P;
	    break;
	    case V4L2_PIX_FMT_YVU410:   ///< Planar YVU 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = PIX_FMT_YVU410P;
	    break;
	    case V4L2_PIX_FMT_YUV411P:   ///< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
	    		pixelformat = PIX_FMT_YUV411P;
	    break;
	    //case PIX_FMT_Y800,      ///< 8 bit Y plane (range [16-235])
	    case V4L2_PIX_FMT_Y16:       ///< 16 bit Y plane (little endian)
	    		pixelformat = PIX_FMT_Y16;
	    break;
	    case V4L2_PIX_FMT_RGB565:    ///< always stored in cpu endianness 
	    		pixelformat = PIX_FMT_RGB565;
	    break;
	    case V4L2_PIX_FMT_RGB555:    ///< always stored in cpu endianness, most significant bit to 1 
	    		pixelformat = PIX_FMT_RGB555;
	    break;
	    case V4L2_PIX_FMT_GREY:
	    		pixelformat = PIX_FMT_GRAY8;
	    break;
	    //case PIX_FMT_GRAY16_L,
	    //case PIX_FMT_GRAY16_B,
	    //case PIX_FMT_MONOWHITE, ///< 0 is white 
	    //case PIX_FMT_MONOBLACK, ///< 0 is black 
	    case V4L2_PIX_FMT_PAL8:      ///< 8 bit with RGBA palette 
		    		pixelformat = PIX_FMT_PAL8;
	    break;
    
	    //case PIX_FMT_YUVJ420P,  ///< Planar YUV 4:2:0 full scale (jpeg)
	    //case PIX_FMT_YUVJ422P,  ///< Planar YUV 4:2:2 full scale (jpeg)
	    //case PIX_FMT_YUVJ444P,  ///< Planar YUV 4:4:4 full scale (jpeg)
	    //case PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing(xvmc_render.h)
	    //case PIX_FMT_XVMC_MPEG2_IDCT,
	    case V4L2_PIX_FMT_UYVY:   ///< Packed pixel, Cb Y0 Cr Y1 
		    		pixelformat = PIX_FMT_UYVY422;
	    break;
	    case V4L2_PIX_FMT_YVYU:   ///< Packed pixel, Y0 Cr Y1 Cb 
		    		pixelformat = PIX_FMT_YVYU422;
	    break;
	    //case PIX_FMT_UYVY411,   ///< Packed pixel, Cb Y0 Y1 Cr Y2 Y3
	    //case PIX_FMT_V308,      ///< Packed pixel, Y0 Cb Cr

	    //case PIX_FMT_AYUV4444,  ///< Packed pixel, A0 Y0 Cb Cr
	    //case PIX_FMT_YUVA420P,   ///< Planar YUV 4:4:2:0 (1 Cr & Cb sample per 2x2 Y & A samples) (A420)
	    default: //PIX_FMT_YUV420P
	    	printf("fmt 0x%x not define, default to V4L2_PIX_FMT_YUV420M\n",fmt);
		//mFourcc = PIX_FMT_YUV420P;
		return pixelformat;
	    break;
			
	}
	//mFourcc = fmt;
	return pixelformat;
}


#endif
