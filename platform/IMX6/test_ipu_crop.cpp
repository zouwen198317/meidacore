#include"imageprocessunit.h"
#include"imageblock.h"
#include"avcodec_common.h"
#include<stdio.h>

//$arm-poky-linux-gnueabi-g++ -o test_ipu_crop ../HAL/imageprocess.cpp ../../../common/log/log.c test_ipu_crop.cpp imageprocessunit.cpp -I../../mediafile/ffmpeg/ -I../../../include -I/work/fsl-release-bsp/build-ai-x11/tmp/work/imx6qsabreauto-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include/uapi -I/work/fsl-release-bsp/build-ai-x11/tmp/work/imx6qsabreauto-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include -I../../ -I../../mediafile -I../../../common

int main()
{
	CIpu ipu;
	ImgBlk* blk=NULL;
	FILE *fp;
	printf("here okay.\n");
	blk = ipu.allocHwBuf(1280,720,PIX_FMT_YUV422);
	if(blk==NULL)printf("NULL ptr.\n");
	fp = fopen("yuv720p","r");
	if(fp==NULL){printf("NULL ptr.\n");return 1;}
	fread(blk->vaddr,sizeof(char),1280*720*2,fp);
	fclose(fp);
	ipu.crop(blk,blk);
	fopen("output.yuyv","w+");
	fwrite(blk->vaddr,sizeof(char),1280*720*2,fp);
	fclose(fp);
	
}
