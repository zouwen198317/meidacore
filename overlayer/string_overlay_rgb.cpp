#include"string_overlay_rgb.h"
using namespace std;

int dot_overlay_rgb(uchar *str[],
							const font_info f_info,
								uchar *rgbBuf,
								uint width,
								uint height,
								uint X,
								uint Y)
{
	if(str == NULL){
		log_print(_ERROR,"In string_overlay_rgb,dot_overlay_rgb: NULL str\n");
		return -1;
	}
	if(rgbBuf == NULL){
		log_print(_ERROR,"In string_overlay_rgb,dot_overlay_rgb: NULL rgbBuf\n");
		return -2;
	}
	if(X > width || Y > height){
		log_print(_ERROR,"In string_overlay_rgb,dot_overlay_rgb: X > width || Y > height\n");
		return -3;
	}
	if(f_info.str.size() == 0){
		log_print(_ERROR,"In string_overlay_rgb,dot_overlay_rgb: empty string\n");
		return -4;
	}

	static uchar mask[] = {128,64,32,16,8,4,2,1};
	const uint seg = f_info.width / 8;
	const uint font_bytes = f_info.height * seg;
	const int str_size = f_info.str.size();

	if(X%2!=0)++X;
	if(Y%2!=0)++Y;

	uint Y_pos = (height-(Y+f_info.height))*width*3;
	uint Y_offset = width*3;
	uint X_offset = X*3;
	uint cur_X;

	for(int j = font_bytes-1,col = f_info.height;j >= 0;j -= seg,--col)//j表示处理第j段数据
	{
		if(col + Y >= height)continue;
		int i;
		int pos = Y_pos+X_offset;
		cur_X=X;
				
		for(i = 0;i < str_size;++i)//i表示第i个字符
		{
			j -=(seg-1);
			for(int bytes = 0;bytes < seg;++bytes)//bytes表示正处理的段
			{
				for(int bit = 0;bit < 8;++bit)//bit表示处理的位
				{
					if(str[i][j] & mask[bit])
					{
						rgbBuf[pos++] = f_info.font_color.blue;
						rgbBuf[pos++] = f_info.font_color.green;
						rgbBuf[pos++] = f_info.font_color.red;
						if(++cur_X >= width)
						{
							j+=(seg-bytes-1);
							goto endloop1;
						}
					}
					else
					{
						rgbBuf[pos++] = f_info.background.blue;
						rgbBuf[pos++] = f_info.background.green;
						rgbBuf[pos++] = f_info.background.red;
						if(++cur_X >= width)
						{
							j+=(seg-bytes-1);
							goto endloop1;
						}
					}
				}
				++j;
			}
			--j;
		}
endloop1:	Y_pos+=Y_offset;
	}
	return 0;
}
