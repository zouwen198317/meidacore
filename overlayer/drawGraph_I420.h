#include"common.h"

//if you want to use drawDot or drawpixel ,you should use initGraphParm_I420 first
void initGraphParm_I420(uint w,uint h);

void drawpixel(uchar *yuvBuf,
								uint x,
								uint y,
								YUV yuv);
void drawDot(uchar *yuvBuf,
							 uint x,
							 uint y,
							 uint size,
							 YUV yuv);


void drawline_I420(uchar *yuvBuf,
							uint nWidth,
							uint nHeight,
							uint x0,
							uint y0,
							uint x1,
							uint y1,
							uint size,
							rgb color);

void drawRect_I420(uchar *yuvBuf,
									 uint nWidth,
									 uint nHeight,
									 uint x0,uint y0,
									 uint x1,uint y1,
									 uint size,
									 rgb color);
