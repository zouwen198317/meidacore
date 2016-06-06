#include<iostream>
#include<fstream>
#include<iterator>
#include<string>
#include<algorithm>
#include"createSingleColorBg.h"
using namespace std;


void createBg_i420(unsigned int width,unsigned int height,RGB color,unsigned char* buf)
{
	YUV yuv = rgb2yuv(color);

	unsigned int y_pos=0;
	unsigned int u_pos=width*height;
	unsigned int v_pos=u_pos + width*height/4;	
	for(int j = 0;j < height;++j)
	{
		for(int i = 0;i < width;++i)
		{
			buf[y_pos++]=yuv.y;
			if(i%2==0&&j%2==0)
			{
				buf[u_pos++]=yuv.u;
				buf[v_pos++]=yuv.v;
			}
		}
	}
}

void createBg_yuyv(unsigned int width,unsigned int height,RGB color,unsigned char* buffer)
{
	YUV yuv = rgb2yuv(color);

	for(int j = 0;j < height;++j)
	{
		int pos = j*width*2;
		for(int i = 0;i < width;i+=2)
		{
			buffer[pos++] = yuv.y;
			buffer[pos++] = yuv.u;
			buffer[pos++] = yuv.y;
			buffer[pos++] = yuv.v;
		}
	}	
}

