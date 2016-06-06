#include<stdio.h>
#include<string.h>
#include<string>
#include"string_overlay_yuyv.h"
#include"dot_rgb.h"
#include"common.h"
//g++ -o test_class_yuyv test_class_yuyv.cpp string_overlay_yuyv.cpp drawGraph_yuyv.cpp dot_rgb.cpp ../../common/log/log.c -I../../common
int main()
{
	unsigned char yuvBuf[1280*720*2]={0};
	OverlayerYUYV overlayer(1280,720,300,300,true);
	font_info f_info;
	f_info.width = 8;
	f_info.height = 16;
	f_info.path = "./ASC16";
	f_info.hasBackground = false;
	f_info.str = "hello world!";
	f_info.background = rgb(0,0,0);
	f_info.font_color = rgb(255,255,255);
  	int stringLen = strlen("hello world!");
	uchar ** dotBuf = new uchar*[stringLen];
	uint font_bytes = f_info.width/8*f_info.height;
	for(int i=0;i < stringLen;++i)
		dotBuf[i] = new uchar[font_bytes];
	string_to_dot_matrix(f_info.path.data(),font_bytes,f_info.str.data(),dotBuf);
	overlayer.dot_overlay_yuyv(dotBuf,f_info,yuvBuf);
	FILE *fp;
	fp = fopen("yuv720p","w+");
	if(fp == NULL)printf("open error\n");
	fwrite(yuvBuf,sizeof(unsigned char),1280*720*2,fp);
	fclose(fp);
}
