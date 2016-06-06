#include<fstream>
#include<iterator>
#include<algorithm>
#include<string>
#include<string.h>
#include"dot_rgb.h"
#include"common.h"
#include<iostream>
using namespace std;
#define BMP_FILE_LENTH_POS 2
#define BMP_DATA_START_POS 54
#define BMP_WIDTH_POS 0x12
#define BMP_HEIGHT_POS 0x16



uchar* get_bmp_buffer(const char *path,
		  unsigned long &size,
		  unsigned long &width,
		  unsigned long &height)
{
	ifstream bmp(path,ios::binary);
	bmp.seekg(0,ios::end);
	streampos end = bmp.tellg();

	uchar *buf = new uchar[end];
	bmp.seekg(0,ios::beg);
	bmp.unsetf(ios::skipws);
	copy(istream_iterator<uchar>(bmp),istream_iterator<uchar>(),buf);
	size = buf[BMP_FILE_LENTH_POS + 3]<<24 | 
			buf[BMP_FILE_LENTH_POS + 2]<<16 | 
			buf[BMP_FILE_LENTH_POS + 1]<<8 | 
			buf[BMP_FILE_LENTH_POS];
	width = buf[BMP_WIDTH_POS + 3]<<24 | 
			buf[BMP_WIDTH_POS + 2]<<16 | 
			buf[BMP_WIDTH_POS + 1]<<8 | 
			buf[BMP_WIDTH_POS];
	height = buf[BMP_HEIGHT_POS + 3]<<24 | 
			buf[BMP_HEIGHT_POS + 2]<<16 | 
			buf[BMP_HEIGHT_POS + 1]<<8 | 
			buf[BMP_HEIGHT_POS];	
	bmp.close();
	return buf;
}


int string_to_dot_matrix(const char *path,uint font_bytes,const char* str,uchar *p[])
{
	ifstream font(path,ios::binary);
	if(!font)
	{
		return -1;
	}
	int len;
	for(len = 0;str[len] != '\0';++len)
	{
		long offset = font_bytes * str[len];//字符字节*ASCII码值=偏移位置
		font.seekg(offset,ios::beg);
		font.read((char*)p[len],font_bytes);
	}
	font.close();
}

void dot_matrix_to_rgb_buffer(uchar* p[],
			      int str_size,
			      uint font_w,
			      uint font_h,
			      uchar* buffer,
			      rgb font_color,
			      rgb background_color)
{
	static unsigned char mask[] = {128,64,32,16,8,4,2,1};

	uint seg = font_w / 8;
	uint font_bytes = font_h * seg;

	int k = 0;//buffer pos
	for(int j = font_bytes - 1;j >= 0;j -= seg)
	{
		int i;
		for(i = 0;i < str_size;++i)
		{
			j -=(seg - 1);
			for(int bytes = 0;bytes < seg;++bytes)
			{
				for(int bit = 0;bit < 8;++bit)
				{
					if(p[i][j] & mask[bit])
					{
						buffer[k++] = font_color.blue;
						buffer[k++] = font_color.green;
						buffer[k++] = font_color.red;
					}
					else
					{
						buffer[k++] = background_color.blue;
						buffer[k++] = background_color.green;
						buffer[k++] = background_color.red;
					}
				}
				++j;
			}
			j--;
		}
	}
	return;
}

bool rgb_buffer_overlay(const uchar* overlay,
			uint width,
			uint height,
			uchar* dest,
			uint bg_width,
			uint bg_height,
			uint x,
			uint y)
{
	if(width + x > bg_width){cout<<"font width out of range"<<endl;return false;}
	if(height + y > bg_height){cout<<"font height out of range"<<endl;return false;}

	int w = width,h = height;
	int x_pos = 3 * x;
	int y_pos = 3 * (bg_height - y - h) * bg_width;
	int x_end = x_pos + 3 * w;
	int y_offset = bg_width * 3;
	int k = 0;//overlay pos
	int cur_height = y_pos;
	
	while(k < w*h*3)
	{
		for(int i = x_pos;i < x_end;++i,++k)
		{
			dest[ i + cur_height] = overlay[k];
		}
		cur_height += y_offset;
	}
	return true;
}






/**
void rgb_with_dot_to_yuv(uchar* raw_buf,uint buf_len,
				uint nwidth,
				uint nheight,
				int x,int y,
				uchar* yuv_buf,
				font_info f_info)
{
	const uint font_w = f_info.width;
	const uint font_h = f_info.height;
	const uint seg = font_w / 8;
	const uint font_bytes = font_h * seg;

	string str = f_info.str;

	uchar **dot_buf = new uchar*[str.size()];
	for(int i = 0;i < str.size();++i)
		dot_buf[i] = new uchar[font_bytes];
	
	string_to_dot_matrix(f_info.path.data(),font_bytes,str.data(),dot_buf);

	uchar *overlay = new uchar[font_w * str.size() * font_h * 3];

	//贴图
	dot_matrix_to_rgb_buffer(dot_buf,str.size(),font_w,font_h,overlay);
cout<<"enter overlay"<<endl;
	rgb_buffer_overlay(overlay,font_w * str.size(),font_h,raw_buf,nwidth,nheight,x,y);


	
	ulong yuv_size = buf_len/2;
	if(!rgb_to_yuv(raw_buf,nwidth,nheight,yuv_buf,&yuv_size))
		cout<<"rgb to yuv failed!"<<endl;

	



//delete
	delete [] overlay;
	for(int i = 0;i < str.size();++i)
		delete [] dot_buf[i];
	delete [] dot_buf;
}
*/
