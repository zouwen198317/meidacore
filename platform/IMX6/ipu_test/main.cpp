#include <iostream>
#include "imageprocessunit.h"

using namespace std;


int fd_fb;



int main(int argc, char *argv[])
{
    struct timeval begin, end;
    CIpu *ipu = new CIpu();
    ImgBlk *imginput = NULL;
    ImgBlk *imgoutput = NULL;
    ImgBlk *imgoverlay = NULL;
    ImgBlk *imgalpha = NULL;
    ImgBlk *imgalpha2 = NULL;
    imginput = ipu->allocHwBuf(1024,768,PIX_FMT_RGB565);//PIX_FMT_YUV420P
    imgoutput = ipu->allocHwBuf(640,480,PIX_FMT_RGB565);//PIX_FMT_YUV422P
    imgoverlay = ipu->allocHwBuf(640,480,PIX_FMT_RGB565);
    imgalpha = ipu->allocHwBuf(640,480,PIX_FMT_GRAY8);
    imgalpha2 = ipu->allocHwBuf(640,480,PIX_FMT_GRAY8);
    memset(imgalpha->vaddr, 0x00, imgalpha->size);
    //memset(imgalpha2->vaddr, 0x00, imgalpha->size);
    //memset(imgalpha->vaddr, 0xff, imgalpha->size/8);
    //memset(imgalpha->vaddr+imgalpha->size/4, 0x00, imgalpha->size/8);
    //memset(imgalpha->vaddr+imgalpha->size/2, 0xff, imgalpha->size/8);
    //memset(imgalpha->vaddr+imgalpha->size*3/4, 0xff, imgalpha->size/8);

    FILE * file_overalpha = NULL;	
    file_overalpha = fopen("/mnt/te/12.raw", "rb");
    fread(imgalpha->vaddr,imgalpha->size,1,file_overalpha);
    FILE * file_overalpha2 = NULL;	
    file_overalpha2 = fopen("/mnt/te/22.raw", "rb");
    fread(imgalpha2->vaddr,imgalpha2->size,1,file_overalpha2);
/*
    for (u32 m = 1;m < 481; m++ )
        {
	for (u32 n = 0;n < 640; n++ )
	{
	    fread(imgalpha->vaddr+imgalpha->size-m*640+n,1,1,file_overalpha);
	    //fseek(file_overalpha,2,1);
	}
    }
    */

    fclose(file_overalpha);
    fclose(file_overalpha2);

    FILE * file_in = NULL;
    FILE * file_overlay = NULL;
    file_in = fopen(argv[1], "rb");
    file_overlay = fopen(argv[argc-1], "rb");
    if (file_in == NULL){
	printf("there is no such file for reading %s\n", argv[argc-1]);
    }
    int ret = 0;
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;

    char outfile[128];
    memcpy(outfile,"ipu0-1st-ovfb",13);

    int found = 0, i;
    char fb_dev[] = "/dev/fb0";
    char fb_name[16];

    //if (!strcmp(outfile, "ipu0-1st-ovfb"))
        memcpy(fb_name, "DISP3 FG", 9);
    //if (!strcmp(outfile, "ipu0-2nd-fb"))
    //    memcpy(fb_name, "DISP3 BG - DI1", 15);
    //if (!strcmp(outfile, "ipu1-1st-ovfb"))
     //   memcpy(fb_name, "DISP4 FG", 9);
    //if (!strcmp(outfile, "ipu1-2nd-fb"))
     //   memcpy(fb_name, "DISP4 BG - DI1", 15);

    for (i=0; i<5; i++) {
        fb_dev[7] = '0';
        fb_dev[7] += i;
        fd_fb = open(fb_dev, O_RDWR, 0);
        if (fd_fb > 0) {
            ioctl(fd_fb, FBIOGET_FSCREENINFO, &fb_fix);
            if (!strcmp(fb_fix.id, fb_name)) {
                printf("found fb dev %s\n", fb_dev);
                found = 1;
                break;
            } else
                close(fd_fb);
        }
    }
    if (!found) {
        printf("can not find fb dev %s\n", fb_name);
    }
    
    u32 tmpnonstd;
    ipu->fmt_to_ipufmt(imgoutput->format,&tmpnonstd);

    printf("h:%d,w%d\n", imgoutput->height,imgoutput->width);
    ioctl(fd_fb, FBIOGET_VSCREENINFO, &fb_var);
    fb_var.xres = imgoutput->width;
    fb_var.xres_virtual = fb_var.xres;
    fb_var.yres = imgoutput->height;
    fb_var.yres_virtual = fb_var.yres;
    fb_var.activate |= FB_ACTIVATE_FORCE;
    fb_var.vmode |= FB_VMODE_YWRAP;
    fb_var.nonstd = tmpnonstd;
    fb_var.bits_per_pixel = ipu->fmt_to_bpp(imgoutput->format);

    ret = ioctl(fd_fb, FBIOPUT_VSCREENINFO, &fb_var);
    if (ret < 0) {
        printf("fb ioctl FBIOPUT_VSCREENINFO fail\n");
    }
    ioctl(fd_fb, FBIOGET_VSCREENINFO, &fb_var);
    ioctl(fd_fb, FBIOGET_FSCREENINFO, &fb_fix);
    int blank;
    blank = FB_BLANK_UNBLANK;
    ioctl(fd_fb, FBIOBLANK, blank);

    imgoutput->paddr = fb_fix.smem_start;
    ret = fread(imginput->vaddr, 1, imginput->size, file_in);
    if (ret < imginput->size) {
		printf("Can not read enough data from input file\n");
	}
    ret = fread(imgoverlay->vaddr, 1, imgoverlay->size, file_overlay);
    if (ret < imgoverlay->size) {
		printf("Can not read enough data from overlay file\n");
	}
gettimeofday(&begin, NULL);
    for(int total_cut = 0;total_cut<1000;total_cut++)
    {
    	//ret = ipu->overlay(imginput,imgoutput,imgoverlay,imgalpha);
    	ret = ipu->overlay(imginput,imgoutput,imgoverlay,imgalpha2);

        //ret = ipu->resize(imginput,imgoutput);
    }
int sec;
int usec;
int run_time;
gettimeofday(&end, NULL);
sec = end.tv_sec - begin.tv_sec;
		usec = end.tv_usec - begin.tv_usec;

		if (usec < 0) {
			sec--;
			usec = usec + 1000000;
		}
		run_time = (sec * 1000000) + usec;
printf("time %d \n",run_time/1000);
    //ret = ipu->resize(imginput,imgoutput);

   // for(int i = 0; i<1000000000; i++ )
    //{
	
    //}

    if (ret < 0) {
        printf("ioct IPU_QUEUE_TASK fail\n");
    }
	//FILE *file_fd2;
    //file_fd2 = fopen("/mnt/te/RGB565_1280*720", "w");
	//ret=fwrite(imgoutput->vaddr,imgoutput->size, 1, file_fd2);
	//printf("file %d\n",ret);
    //ret = ioctl(fd_fb, FBIOPAN_DISPLAY, &fb_var);
	///printf("%d\n",ret);
    //if (ret < 0) {
    //    printf("fb ioct FBIOPAN_DISPLAY fail\n");
    //}
	blank = FB_BLANK_POWERDOWN;
		ioctl(fd_fb, FBIOBLANK, blank);
   //fclose(file_fd2);
    close(fd_fb);

    return 0;
}





