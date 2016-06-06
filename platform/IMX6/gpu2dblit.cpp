#include "gpu2dblit.h"


CGpu2D::CGpu2D()
{
	mBuf_num = 0;
	int ret = g2d_open(&mHandle);
	if (ret < 0)
	{
		printf("CGpu2D::CGpu2D() g2d_open fail\n");
	}
}

CGpu2D::~CGpu2D()
{
	g2d_close(mHandle);
}

int CGpu2D::resize(ImgBlk* pSrc, ImgBlk* pDest)
{
	int ret = -1;
	struct g2d_surface src;
	struct g2d_surface dst;
	u32 tmpformat;
	ret = fmt_to_g2dfmt(pSrc->fourcc,&tmpformat);
	if (ret < 0)
	{
		return -1;
	}
	
	src.format = (g2d_format)tmpformat;
	ret = set_surface_planes(&src,pSrc);
	if (ret < 0)
	{
		return -1;
	}
	src.left = 0;
	src.top = 0;
	src.right = pSrc->width;
	src.bottom = pSrc->height;
	src.stride = pSrc->width;
	src.width = pSrc->width;
	src.height = pSrc->height;
	src.rot    = G2D_ROTATION_0;
	
	ret = fmt_to_g2dfmt(pDest->fourcc,&tmpformat);
	if (ret < 0)
	{
		return -1;
	}

	dst.format = (g2d_format)tmpformat;
	ret = set_surface_planes(&dst,pDest);
	if (ret < 0)
	{
		return -1;
	}
	//dst.planes[0] = pDest->paddr;
	dst.left = 0;
	dst.top = 0;
	dst.right = pDest->width;
	dst.bottom = pDest->height;
	dst.stride = pDest->width;
	dst.width = pDest->width;
	dst.height = pDest->height;
	dst.rot    = G2D_ROTATION_0;
	
	ret = g2d_blit(mHandle, &src, &dst);
	if(ret < 0)
	{
		printf("g2d_blit fail\n");
	}
	ret = g2d_finish(mHandle);
	if(ret < 0)
	{
		printf("g2d_finish fail\n");
	}
	return 0;

}

int CGpu2D::colorCovert(ImgBlk* pSrc, ImgBlk* pDest)
{
	return resize(pSrc,pDest);
}

int CGpu2D::rotate(ImgBlk* pSrc, ImgBlk* pDest, u32 rotation)
{
	int ret = -1;
	struct g2d_surface src;
	struct g2d_surface dst;
	u32 tmpformat;
	ret = fmt_to_g2dfmt(pSrc->fourcc,&tmpformat);
	if (ret < 0)
	{
		return -1;
	}
	
	src.format = (g2d_format)tmpformat;

	ret = set_surface_planes(&src,pSrc);
	if (ret < 0)
	{
		return -1;
	}

	src.left = 0;
	src.top = 0;
	src.right = pSrc->width;
	src.bottom = pSrc->height;
	src.stride = pSrc->width;
	src.width = pSrc->width;
	src.height = pSrc->height;
	src.rot    = G2D_ROTATION_0;

	ret = fmt_to_g2dfmt(pDest->fourcc,&tmpformat);
	if (ret < 0)
	{
		return -1;
	}

	dst.format = (g2d_format)tmpformat;

	ret = set_surface_planes(&dst,pDest);
	if (ret < 0)
	{
		return -1;
	}

	dst.left = 0;
	dst.top = 0;
	dst.right = pDest->width;
	dst.bottom = pDest->height;
	dst.stride = pDest->width;
	dst.width = pDest->width;
	dst.height = pDest->height;
	dst.rot    = (g2d_rotation)rotation;
	
	ret = g2d_blit(mHandle, &src, &dst);
	if(ret < 0)
	{
		printf("g2d_blit fail\n");
	}
	ret = g2d_finish(mHandle);
	if(ret < 0)
	{
		printf("g2d_finish fail\n");
	}
	return 0;
}

int CGpu2D::overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha)
{
}


ImgBlk* CGpu2D::allocHwBuf(u32 width,u32 height,u32 format)
{
	if(mBuf_num>=BUF_MAX)
	{
		printf("CGpu2D::allocHwBuf alloc to much buff\n");
		return NULL;
	}
	ImgBlk* rc = (ImgBlk *)malloc(sizeof(ImgBlk));
	struct g2d_buf *buf;
	buf = g2d_alloc(width*height*fmt_to_bpp(format)/8, 0);
	rc->fourcc = format;
	rc->height = height;
	rc->width = width;
	rc->paddr = buf->buf_paddr;
	rc->vaddr = buf->buf_vaddr;
	rc->size = buf->buf_size;
	mBufs[mBuf_num] = buf;
	mBuf_num++;
	return rc;	
}

void CGpu2D::freeHwBuf(ImgBlk* img)
{
	if(mBuf_num > 0)
	{
		for(int i = 0;i < mBuf_num;i++)
		{
			if(mBufs[i]->buf_paddr == img->paddr)
			{
				g2d_free(mBufs[i]);
				while(i+1<mBuf_num)
				{
					mBufs[i]=mBufs[i+1];
					i++;
				}
				mBuf_num--;
				break;
			}
		}
		
	}
}

unsigned int CGpu2D::fmt_to_bpp(unsigned int pixelformat)
{
    unsigned int bpp;

    switch (pixelformat)
    {
        case PIX_FMT_Y800:      //< 8 bit Y plane (range [16-235])
        case PIX_FMT_GRAY8:
        case PIX_FMT_PAL8:     //< 8 bit with RGBA palette
        case PIX_FMT_MONOWHITE: //< 0 is white
        case PIX_FMT_MONOBLACK: //< 0 is black
        bpp = 8;
        break;
        case PIX_FMT_NV12:      //< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
        case PIX_FMT_NV21:     //< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
        case PIX_FMT_YUV420P:   //< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
        case PIX_FMT_YVU420P:   //< Planar YUV 4:2:0 (1 Cb & Cr sample per 2x2 Y samples) (YV12)
        case PIX_FMT_YUV411P:   //< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
        case PIX_FMT_YUVJ420P:  //< Planar YUV 4:2:0 full scale (jpeg)
        case PIX_FMT_UYVY411:   //< Packed pixel, Cb Y0 Y1 Cr Y2 Y3
            bpp = 12;
            break;
        case PIX_FMT_YUV422:    //< Packed pixel, Y0 Cb Y1 Cr
        case PIX_FMT_YUV422P:   //< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
        case PIX_FMT_Y16:       //< 16 bit Y plane (little endian)
        case PIX_FMT_RGB565:    //< always stored in cpu endianness
        case PIX_FMT_RGB555:    //< always stored in cpu endianness, most significant bit to 1
        case PIX_FMT_GRAY16_L:
        case PIX_FMT_GRAY16_B:
        case PIX_FMT_YUVJ422P:  //< Planar YUV 4:2:2 full scale (jpeg)
        case PIX_FMT_UYVY422:   //< Packed pixel, Cb Y0 Cr Y1
        case PIX_FMT_YVYU422:   //< Packed pixel, Y0 Cr Y1 Cb
            bpp = 16;
            break;
        case PIX_FMT_RGB24:     //< Packed pixel, 3 bytes per pixel, RGBRGB...
        case PIX_FMT_BGR24:     //< Packed pixel, 3 bytes per pixel, BGRBGR...
        case PIX_FMT_YUV444P:   //< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
        case PIX_FMT_YUVJ444P:  //< Planar YUV 4:4:4 full scale (jpeg)
        case PIX_FMT_V308:      //< Packed pixel, Y0 Cb Cr
            bpp = 24;
            break;
        case PIX_FMT_RGBA32:    //< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
        case PIX_FMT_BGRA32:    //< Packed pixel, 4 bytes per pixel, ARGBARGB...
        case PIX_FMT_ARGB32:    //< Packed pixel, 4 bytes per pixel, ABGRABGR..., stored in cpu endianness
        case PIX_FMT_ABGR32:    //< Packed pixel, 4 bytes per pixel, RGBARGBA...
        case PIX_FMT_RGB32:     //< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
        case PIX_FMT_xRGB32:    //< Packed pixel, 4 bytes per pixel, xBGRxBGR..., stored in cpu endianness
        case PIX_FMT_BGR32:     //< Packed pixel, 4 bytes per pixel, xRGBxRGB...
        case PIX_FMT_BGRx32:    //< Packed pixel, 4 bytes per pixel, RGBxRGBx...
        case PIX_FMT_AYUV4444:  //< Packed pixel, A0 Y0 Cb Cr
            bpp = 32;
            break;
        default:
            printf("can't found format\n");
            bpp = 8;
            break;
    }
    return bpp;

}

int CGpu2D::fmt_to_g2dfmt(unsigned int pixelformat,u32 *format)
{
    int ret = 0;

    switch (pixelformat)
    {
        case PIX_FMT_NV12:      //< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
            *format = G2D_NV12;
            break;
        case PIX_FMT_YUV420P:   //< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
            *format = G2D_I420;
            break;
        case PIX_FMT_YUV422:    //< Packed pixel, Y0 Cb Y1 Cr
            *format = G2D_YUYV;
            break;
        case PIX_FMT_RGB565:    //< always stored in cpu endianness
            *format = G2D_RGB565;
            break;
        case PIX_FMT_UYVY422:   //< Packed pixel, Cb Y0 Cr Y1
            *format = G2D_UYVY;
            break;
        case PIX_FMT_RGBA32:    //< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
            *format = G2D_RGBA8888;
            break;
        case PIX_FMT_BGRA32:    //< Packed pixel, 4 bytes per pixel, ARGBARGB...
            *format = G2D_BGRA8888;
            break;
        case PIX_FMT_RGB32:     //< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
            *format = G2D_RGBX8888;
            break;
        case PIX_FMT_BGRx32:     //< Packed pixel, 4 bytes per pixel, xRGBxRGB...
            *format = G2D_BGRX8888;
            break;
        default:
            printf("can't found g2dformat\n");
            ret = -1;
            break;
    }
    return ret;
}

int CGpu2D::set_surface_planes(g2d_surface* surface,ImgBlk* iBlk)
{
	switch (surface->format) {
	case G2D_RGB565:
	case G2D_RGBA8888:
	case G2D_RGBX8888:
	case G2D_BGRA8888:
	case G2D_BGRX8888:
	case G2D_BGR565:
	case G2D_YUYV:
	case G2D_UYVY:
		surface->planes[0] = iBlk->paddr;
		break;
	case G2D_NV12:
		surface->planes[0] = iBlk->paddr;
		surface->planes[1] = iBlk->paddr; + iBlk->width* iBlk->height;
		break;
	case G2D_I420:
		surface->planes[0] = iBlk->paddr;
		surface->planes[1] = iBlk->paddr + iBlk->width* iBlk->height;
		surface->planes[2] = surface->planes[1]  + iBlk->width* iBlk->height / 4;
		break;
	case G2D_NV16:
		surface->planes[0] = iBlk->paddr;
		surface->planes[1] = iBlk->paddr; + iBlk->width* iBlk->height;
                break;
	default:
		printf("Unsupport image format in the example code\n");
		return -1;
	}
	return surface->format;
}






























