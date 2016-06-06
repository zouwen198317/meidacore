#include<iostream>
#include"common.h"
#include"drawGraph_yuyv.h"

using namespace std;

static uint width;//存储操作的图像的宽
static uint height;//存储操作的图像的高

void initGraphParm_yuyv(uint w,uint h)
{
	width = w;
	height = h;
}

void drawpixel_yuyv(uchar *yuvBuf,
								uint x,
								uint y,
								YUV yuv)
{
	if(x > width || y > height)return;

	uint y_offset = (width*y)<<1;//坐标Y位置跨越的长度
	uint y_pos = y_offset + (x<<1);//y（yuv）的位置


	yuvBuf[y_pos] = yuv.y;
	if(x%2==0)//根据当前位置确定是否取样
	{
		yuvBuf[y_pos + 1] = yuv.u;
		yuvBuf[y_pos + 3] = yuv.v;
	}
}

void drawDot_yuyv(uchar *yuvBuf,
							 uint x,
							 uint y,
							 uint size,
							 YUV yuv)
{
	uint start_y = y-(size>>1);
	uint start_x = x-(size>>1);
	
	//画（x，y）周围的点
	for(int j=0;j < size;++j)
	{
		for(int i=0;i < size;++i)
		{
			drawpixel_yuyv(yuvBuf,start_x + i,start_y + j,yuv);
		}
	}
}

//布雷森汉姆算法
/**伪代码
function line(x0, x1, y0, y1)
     boolean steep := abs(y1 - y0) > abs(x1 - x0)
     if steep then
         swap(x0, y0)
         swap(x1, y1)
     if x0 > x1 then
         swap(x0, x1)
         swap(y0, y1)
     int deltax := x1 - x0
     int deltay := abs(y1 - y0)
     int error := deltax / 2
     int ystep
     int y := y0
     if y0 < y1 then ystep := 1 else ystep := -1
     for x from x0 to x1
         if steep then plot(y,x) else plot(x,y)
         error := error - deltay
         if error < 0 then
             y := y + ystep
             error := error + deltax
*/
void drawline_yuyv(uchar *yuvBuf,
							uint nWidth,
							uint nHeight,
							uint x0,
							uint y0,
							uint x1,
							uint y1,
							uint size,
							rgb color)
{
	//if(x0 > nWidth || x1 > nWidth || y0 > nHeight || y1 > nHeight)
	//	return;
	if(size > 10)return;
	//调整传入点的位置以适应图像大小
	if(x0 > nWidth)x0 = nWidth;
	if(y0 > nHeight)y0 = nHeight;
	if(x1 > nWidth)x1 = nWidth;
	if(y1 > nHeight)y1 = nHeight;

	initGraphParm_yuyv(nWidth,nHeight);
	
	YUV yuv = rgb2yuv(color);

	bool steep = abs(y1-y0) > abs(x1 -x0);
	if(steep)
	{
		swap(x0,y0);
		swap(x1,y1);
	}
	if(x0>x1)
	{
		swap(x0,x1);
		swap(y0,y1);
	}
	int deltax = x1 - x0;
	int deltay = abs(y1-y0);
	int error = deltax/2;
	int ystep;
	int y = y0;
	if(y0<y1)
		ystep=1;
	else ystep=-1;

	for(int x = x0;x <= x1;++x)
	{
		if(steep)
			drawDot_yuyv(yuvBuf,y,x,size,yuv);
		else
			drawDot_yuyv(yuvBuf,x,y,size,yuv);
		error = error -deltay;
		if(error<0)
		{
			y = y + ystep;
			error = error + deltax;
		}
	}
}


void drawRect_yuyv(uchar *yuvBuf,
                    uint nWidth,
                    uint nHeight,
                    uint x0,uint y0,
                    uint x1,uint y1,
                    uint size,
                    rgb color)
{
	if(x0 > nWidth || x1 > nWidth || y0 > nHeight || y1 > nHeight)
		return;
	if(size > 10)return;

	initGraphParm_yuyv(nWidth,nHeight);

	YUV yuv = rgb2yuv(color);

	if(x0>x1)swap(x0,x1);
	if(y0>y1)swap(y0,y1);

	for(int x = x0;x <= x1;x += size)//horizontal
	{
		drawDot_yuyv(yuvBuf,x,y0,size,yuv);
		drawDot_yuyv(yuvBuf,x,y1,size,yuv);
	}
	for(int y = y0 + size;y < y1;y += size)//vertical
	{
		drawDot_yuyv(yuvBuf,x0,y,size,yuv);
		drawDot_yuyv(yuvBuf,x1,y,size,yuv);
	}
}

