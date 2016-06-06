/**
	dot_rgb.h
	提供 字符串转点阵字函数string_to_dot_matrix()，点阵字转RGB数据函数dot_matrix_to_rgb_buffer()
	 
*/

#ifndef DOT_RGB_H
#define DOT_RGB_H
#include"common.h"


uchar* get_bmp_buffer(const char *path,unsigned long &size,unsigned long &width,unsigned long &height);


/**
	path : 字体文件路径
	font_bytes ： 单个字符字节数
	str : 字符串
	p : 由用户创建的buffer，p[字符数][font_bytes]
	
*/
int string_to_dot_matrix(const char *path,uint font_bytes,const char* str,uchar *p[]);


void dot_matrix_to_rgb_buffer(uchar* p[],
				int str_size,
				uint font_w,
				uint font_h,
				uchar* buffer,
				rgb font_color = rgb(0,0,0),
				rgb background_color = rgb(0xff,0xff,0xff));

bool rgb_buffer_overlay(const uchar* overlay,
			uint width,
			uint height,
			uchar* dest,
			uint bg_width,
			uint bg_height,
			uint x,
			uint y);



void rgb_with_dot_to_yuv(uchar* raw_buf,uint buf_len,
				uint nwidth,
				uint nheight,
				int x,int y,
				uchar* dest,
				font_info f_info = font_info("2014/10/11",16,32,rgb(255,255,255),rgb(0,0,0),"ASC32"));
#endif
