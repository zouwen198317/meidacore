#include"common.h"
//if you want to use drawDot or drawpixel ,you should use initGraphParm_yuyv first

void initGraphParm_yuyv(uint w,uint h);

void drawpixel_yuyv(uchar *yuvBuf,
								uint x,
								uint y,
								YUV yuv);
void drawDot_yuyv(uchar *yuvBuf,
							 uint x,
							 uint y,
							 uint size,
							 YUV yuv);


void drawline_yuyv(uchar *yuvBuf,
							uint nWidth,
							uint nHeight,
							uint x0,
							uint y0,
							uint x1,
							uint y1,
							uint size,
							rgb color);

void drawRect_yuyv(uchar *yuvBuf,
									 uint nWidth,
									 uint nHeight,
									 uint x0,uint y0,
									 uint x1,uint y1,
									 uint size,
									 rgb color);
