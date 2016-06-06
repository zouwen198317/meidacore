#include "imageprocessunit.h"


#define PAGE_ALIGN(x) (((x) + 4095) & ~4095)

int ctrl_c_rev = 0;

#define MAX_PATH 128

#define deb_msg 0
#if deb_msg
#define deb_printf printf
#else
#define deb_printf
#endif


CIpu::CIpu()
{
    struct ipu_task *t = &mIpuTask;
    /*default settings*/
    t->priority = 0;
    t->task_id = 0;
    t->timeout = 1000;

    t->input.width = 320;
    t->input.height = 240;
    t->input.format = IPU_PIX_FMT_YUV420P;
    t->input.crop.pos.x = 0;
    t->input.crop.pos.y = 0;
    t->input.crop.w = 0;
    t->input.crop.h = 0;
    t->input.deinterlace.enable = 0;
    t->input.deinterlace.motion = 0;

    t->overlay_en = 0;
    t->overlay.width = 320;
    t->overlay.height = 240;
    t->overlay.format = IPU_PIX_FMT_YUV420P;
    t->overlay.crop.pos.x = 0;
    t->overlay.crop.pos.y = 0;
    t->overlay.crop.w = 0;
    t->overlay.crop.h = 0;
    t->overlay.alpha.mode = 0;
    t->overlay.alpha.gvalue = 0;
    t->overlay.colorkey.enable = 0;
    t->overlay.colorkey.value = 0x555555;

    t->output.width = 320;
    t->output.height = 240;
    t->output.format = IPU_PIX_FMT_UYVY;
    t->output.rotate = 0;
    t->output.crop.pos.x = 0;
    t->output.crop.pos.y = 0;
    t->output.crop.w = 0;
    t->output.crop.h = 0;


    mIpuFd = open("/dev/mxc_ipu", O_RDWR, 0);
    if (mIpuFd < 0) {
        printf("open ipu dev fail\n");
    }

}

CIpu::~CIpu()
{
	close(mIpuFd);
}

int CIpu::resize(ImgBlk* pSrc, ImgBlk* pDest)
{
    int ret = 1;

    struct ipu_task tmpIpuTask = mIpuTask;
    struct ipu_task *t = &tmpIpuTask;

    if (fmt_to_ipufmt(pSrc->format,&t->input.format) < 0 ||
            fmt_to_ipufmt(pDest->format,&t->output.format) < 0)
    {
        return -1;
    }

    t->input.width = pSrc->width;
    t->input.height = pSrc->height;
    t->input.paddr = pSrc->paddr;

    t->output.width = pDest->width;
    t->output.height = pDest->height;
    t->output.paddr = pDest->paddr;
    
    //printf("input.w=%d\ninput.h=%d\ninput.fmt=0x%x\noutput.w=%d\noutput.h=%d\noutput.fmt=0x%x\n",
//t->input.width,t->input.height,t->input.format,t->output.width,t->output.height,t->output.format);
    ret=ioctl(mIpuFd,IPU_QUEUE_TASK,t);
    return ret;
}

int CIpu::colorCovert(ImgBlk* pSrc, ImgBlk* pDest)
{
    return resize(pSrc,pDest);
}


int CIpu::overlay(ImgBlk* pSrc, ImgBlk* pDest, ImgBlk* pOverlap, ImgBlk* pAlpha)
{
    int ret = 0;
    struct ipu_task tmpIpuTask = mIpuTask;
    struct ipu_task *t = &tmpIpuTask;
    t->overlay_en = 1;
    t->overlay.alpha.mode = 1;
    t->overlay.alpha.gvalue = 255;
    t->overlay.colorkey.enable = 0;
    t->overlay.colorkey.value = 0xffffff;
    t->overlay.alpha.loc_alp_paddr = pAlpha->paddr;
    if (fmt_to_ipufmt(pSrc->format,&t->input.format) < 0 ||
            fmt_to_ipufmt(pDest->format,&t->output.format) < 0 ||
	    fmt_to_ipufmt(pOverlap->format,&t->overlay.format) < 0)
    {
        return -1;
    }
    
    t->input.width = pSrc->width;
    t->input.height = pSrc->height;
    t->input.paddr = pSrc->paddr;
    
    t->overlay.width = pOverlap->width;
    t->overlay.height = pOverlap->height;
    t->overlay.paddr = pOverlap->paddr;

    t->output.width = pDest->width;
    t->output.height = pDest->height;
    t->output.paddr = pDest->paddr;

    ret=ioctl(mIpuFd,IPU_QUEUE_TASK,t);
    return ret;
}

ImgBlk* CIpu::allocHwBuf(u32 width,u32 height,u32 format)
{
    if(width < 0 || height < 0)
    {
        printf("allocHwBuf fail\n");
        return NULL;
    }
    ImgBlk* tmp = (ImgBlk *)malloc(sizeof(ImgBlk));
    int ret = 0;
//    if (IPU_PIX_FMT_TILED_NV12F == format) {
//            tmp->size = PAGE_ALIGN(width * height/2) +
//                PAGE_ALIGN(width * height/4);
//            tmp->size  = tmp->size * 2;
//        } else
            tmp->size = width * height* fmt_to_bpp(format)/8;
    printf("bbp=%d",fmt_to_bpp(format));
    tmp->paddr = tmp->size;
    ret=ioctl(mIpuFd, IPU_ALLOC, &tmp->paddr);
    if (ret < 0) {
        printf("ioctl IPU_ALLOC fail\n");
        return NULL;
    }
	
    tmp->vaddr = mmap(0, tmp->size, PROT_READ | PROT_WRITE,
        MAP_SHARED, mIpuFd, tmp->paddr);
    if (!tmp->vaddr) {
        printf("mmap fail\n");
        return NULL;
    }
    printf("paddr %x\n vaddr %x\n",tmp->paddr,tmp->vaddr);
    tmp->format = format;
    tmp->height = height;
    tmp->width = width;
    return tmp;
}

void CIpu::freeHwBuf(ImgBlk* img)
{
	if (img->vaddr)
		munmap(img->vaddr, img->size);
	if (img->paddr)
		ioctl(mIpuFd, IPU_FREE, img->paddr);
	free(img);
}

unsigned int CIpu::fmt_to_bpp(unsigned int pixelformat)
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

int CIpu::fmt_to_ipufmt(unsigned int pixelformat,u32 *format)
{
    int ret = 0;

    switch (pixelformat)
    {
        case PIX_FMT_GRAY8:
            *format = IPU_PIX_FMT_GREY;
            break;
        case PIX_FMT_NV12:      //< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
            *format = IPU_PIX_FMT_NV12;
            break;
        case PIX_FMT_YUV420P:   //< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
            *format = IPU_PIX_FMT_YUV420P;
            break;
        case PIX_FMT_YVU420P:   //< Planar YUV 4:2:0 (1 Cb & Cr sample per 2x2 Y samples) (YV12)
            *format = IPU_PIX_FMT_YVU420P;
            break;
        case PIX_FMT_YUV411P:   //< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
            *format = IPU_PIX_FMT_Y41P;
            break;
        case PIX_FMT_YUV422:    //< Packed pixel, Y0 Cb Y1 Cr
            *format = IPU_PIX_FMT_YUYV;
            break;
        case PIX_FMT_YUV422P:   //< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
            *format = IPU_PIX_FMT_YUV422P;
            break;
        case PIX_FMT_Y16:       //< 16 bit Y plane (little endian)

        case PIX_FMT_RGB565:    //< always stored in cpu endianness
            *format = IPU_PIX_FMT_RGB565;
            break;
        case PIX_FMT_RGB555:    //< always stored in cpu endianness, most significant bit to 1
            *format = IPU_PIX_FMT_RGB555;
            break;
        case PIX_FMT_UYVY422:   //< Packed pixel, Cb Y0 Cr Y1
            *format = IPU_PIX_FMT_UYVY;
            break;
        case PIX_FMT_YVYU422:   //< Packed pixel, Y0 Cr Y1 Cb
            *format = IPU_PIX_FMT_YVYU;
            break;
        case PIX_FMT_RGB24:     //< Packed pixel, 3 bytes per pixel, RGBRGB...
            *format = IPU_PIX_FMT_RGB24;
            break;
        case PIX_FMT_BGR24:     //< Packed pixel, 3 bytes per pixel, BGRBGR...
            *format = IPU_PIX_FMT_BGR24;
            break;
        case PIX_FMT_YUV444P:   //< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
            *format = IPU_PIX_FMT_YUV444P;
            break;
        case PIX_FMT_RGBA32:    //< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
            *format = IPU_PIX_FMT_RGBA32;
            break;
        case PIX_FMT_BGRA32:    //< Packed pixel, 4 bytes per pixel, ARGBARGB...
            *format = IPU_PIX_FMT_BGRA32;
            break;
        case PIX_FMT_ABGR32:    //< Packed pixel, 4 bytes per pixel, RGBARGBA...
            *format = IPU_PIX_FMT_ABGR32;
            break;
        case PIX_FMT_RGB32:     //< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
            *format = IPU_PIX_FMT_RGB32;
            break;
        case PIX_FMT_BGR32:     //< Packed pixel, 4 bytes per pixel, xRGBxRGB...
            *format = IPU_PIX_FMT_BGR32;
            break;
        default:
            printf("can't found ipuformat\n");
            ret = -1;
            break;
    }
    return ret;
}
