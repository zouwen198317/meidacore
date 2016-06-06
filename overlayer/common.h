#ifndef COMMON_H
#define COMMON_H
#include <string>
#include"log/log.h"
#include<stdlib.h>
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;
typedef struct RGB
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;

	RGB(unsigned char _red,unsigned char _green,unsigned char _blue)
	{
		red = _red;
		green = _green;
		blue = _blue;
	}
	RGB(){red = 0x0;green = 0x0;blue = 0x0;}
} rgb;

typedef struct _YUV
{
	unsigned char y;
	unsigned char u;
	unsigned char v;
	_YUV(unsigned char _y,unsigned char _u,unsigned char _v)
		: y(_y),u(_u),v(_v)
	{
	}
	_YUV(): y(0),u(0),v(0){}
}YUV;


struct font_info
{
	std::string str;//字符串
	uint width;//描述字体的宽（与字体文件对应）
	uint height;//描述字体的高（与字体文件对应）
	rgb background;//背景颜色
	rgb font_color;//字体颜色
	std::string path;//字体文件的路径
	bool hasBackground;
font_info(const std::string &_str,uint w,uint h,rgb back,rgb f_c,const std::string &p)
	: str(_str),width(w),height(h),background(back),font_color(f_c),path(p),hasBackground(true)
{
	
}
font_info(const std::string &_str,uint w,uint h,rgb back,rgb f_c,const std::string &p,bool flag)
	: str(_str),width(w),height(h),background(back),font_color(f_c),path(p),hasBackground(flag)
{
	
}
font_info():
	width(0),height(0),background(255,255,255),font_color(0,0,0),hasBackground(true)
{
	
}
	
} ;

inline int max(int x,int y)
{
	return ((x>y)?x:y);
}
inline int min(int x,int y)
{
	return ((x<y)?x:y);
}

//inline int abs(int diff){ return (diff<0?(-diff):diff); }
inline void swap(uint &a,uint &b){ a ^= b; b ^= a; a ^= b; }


inline YUV rgb2yuv(rgb color)
{
	int tmp_y,tmp_u,tmp_v;

	tmp_y = ( ( 66 * color.red + 129 * color.green +  25 * color.blue + 128) >> 8) + 16  ;       
	tmp_u = ( ( -38 * color.red -  74 * color.green + 112 * color.blue + 128) >> 8) + 128 ;          
	tmp_v = ( ( 112 * color.red -  94 * color.green -  18 * color.blue + 128) >> 8) + 128 ;

	YUV yuv;
	yuv.y = max(0,min(tmp_y,255));
	yuv.u = max(0,min(tmp_u,255));
	yuv.v = max(0,min(tmp_v,255));
	
	return yuv;
}


#endif
