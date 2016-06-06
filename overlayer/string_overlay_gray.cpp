#include"string_overlay_gray.h"
#include"drawGraph_I420.h"
#include"createSingleColorBg.h"
int dot_overlay_gray(uchar *str[],
							const font_info f_info,
								uchar *grayBuf,
								uint width,
								uint height,
								uint X,
								uint Y)
{
	if(str == NULL){
		log_print(_ERROR,"In string_overlay_gray,dot_overlay_gray: NULL str\n");
		return -1;
	}
	if(grayBuf == NULL){
		log_print(_ERROR,"In string_overlay_gray,dot_overlay_gray: NULL grayBuf\n");
		return -2;
	}
	if(X > width || Y > height){
		log_print(_ERROR,"In string_overlay_gray,dot_overlay_gray: X > width || Y > height\n");
		return -3;
	}
	if(f_info.str.size() == 0){
		log_print(_ERROR,"In string_overlay_gray,dot_overlay_gray: empty string\n");
		return -4;
	}
	
	
	static uchar mask[] = {128,64,32,16,8,4,2,1};
	const uint seg = f_info.width / 8;
	const uint font_bytes = f_info.height * seg;
	const int str_size = f_info.str.size();

	if(X%2!=0)++X;
	if(Y%2!=0)++Y;

	uint Y_pos = Y*width;
	uint Y_offset = width;
	uint X_offset = X;
	uint cur_X;

	for(int j = 0,col = 0;j < font_bytes;j += seg,++col)//j表示处理第j段数据
	{
		if(col + Y >= height)break;
		int i;
		int pos = Y_pos+X_offset;
		cur_X=X;
				
		for(i = 0;i < str_size;++i)//i表示第i个字符
		{
			for(int bytes = 0;bytes < seg;++bytes)//bytes表示正处理的段
			{
				for(int bit = 0;bit < 8;++bit)//bit表示处理的位
				{
					if(str[i][j] & mask[bit])
					{
						grayBuf[pos++] = f_info.font_color.red;
						if(++cur_X >= width)
						{
							j-= bytes;
							goto endloop1;
						}
					}
					else
					{
						grayBuf[pos++] = f_info.background.red;
						if(++cur_X >= width)
						{
							j-= bytes;
							goto endloop1;
						}
					}
				}
				++j;
			}
			j -= seg;
		}
endloop1:	Y_pos+=Y_offset;
	}
	return 0;
}

#include<stack>
//find the point which has specified value from grayBuf
int overlayAlpha_collect_specified_point(const uchar* grayBuf,
											const uint width,const uint height,
											uint x0,uint y0,
											uint x1,uint y1,
											const uchar grayValue,
											std::stack<uint>& XStack,std::stack<uint>& YStack)
{
	if(grayBuf == NULL){
		log_print(_ERROR,"overlayAlpha_collect_specified_point empty buffer.\n");
		return -1;
	}
	for(uint h=y0;h < y1;++h){//printf("\n");
		for(uint w=x0;w < x1;++w){
			uint pos = h*width+w;
			if(grayBuf[pos] == grayValue){//printf("*");
				XStack.push(w);
				YStack.push(h);
			}//else{printf(" ");}
		}
	}
	return 0;
}

//this function would not chech the parameters
//x1 should larger than x0
//y1 should larger than y0
int refreshSpecifiedArea_unsafe(uchar *I420Buf,
						const uint width,const uint height,
						 uint x0,uint y0,
						 uint x1,uint y1,
						 const rgb color)
{
	initGraphParm_I420(width,height);
	YUV yuv = rgb2yuv(color);
	for(int y=y0;y < y1;++y){
		for(int x=x0;x < x1;++x){
			drawpixel(I420Buf,x,y,yuv);
		}
	}
}

int overlayAlpha_Stroke(uchar *I420Buf,	//I420
						uchar *grayAlpha,					//gray8
						const uint width,const uint height,
						uint x0,uint y0,
						uint x1,uint y1,
						const font_info f_info,
						const rgb borderColor)
{
	if(I420Buf == NULL || grayAlpha == NULL){
		log_print(_ERROR,"overlayAlpha_Stroke grayBg or grayAlpha is NULL.\n");
		return -1;
	}
	if(x0 == x1 || y0 == y1){
		log_print(_ERROR,"overlayAlpha_Stroke x0==x1 || y0==y1\n");
		return -1;
	}
	if(x0 > x1)swap(x0,x1);
	if(y0 > y1)swap(y0,y1);

	if(x1 > width)x1 = width;
	if(y1 > height)y1 = height;

	YUV yuv = rgb2yuv(borderColor);
	//YUV yuv2 = rgb2yuv(rgb(0,0,0));

	std::stack<uint> xstack,ystack;
	
	//find the pixel which belong to font
	overlayAlpha_collect_specified_point(grayAlpha,
											width,height,
											x0,y0,x1,y1,
											f_info.font_color.red,
											xstack,ystack);
	if(xstack.empty() || ystack.empty()){
		log_print(_ERROR,"overlayAlpha_Stroke: cannot find the string pixel.\n");
		return -1;
	}

	
	//then draw its adjoining pixels with f_info.font_color that will rewrite grayAlpha.
	//meanwhile,to draw the corresponding pixels with borderColor on grayBg
	uint tmp_x,tmp_y;//printf("\n");
	initGraphParm_I420(width,height);
	refreshSpecifiedArea_unsafe(I420Buf,width,height,x0,y0,x1,y1,f_info.background);
	
	while(!xstack.empty()){
		tmp_x = xstack.top();
		tmp_y = ystack.top();
		xstack.pop();
		ystack.pop();
	//drawpixel(I420Buf,tmp_x,tmp_y,yuv2);
		uint pos = tmp_x + tmp_y * width;
		if(tmp_y>0 && grayAlpha[pos - width] != f_info.font_color.red){//up
			grayAlpha[pos - width] = f_info.font_color.red;
			drawpixel(I420Buf,tmp_x,tmp_y-1,yuv);
		}
		if(tmp_y<height-1 && grayAlpha[pos + width] != f_info.font_color.red){//down
			grayAlpha[pos + width] = f_info.font_color.red;
			drawpixel(I420Buf,tmp_x,tmp_y+1,yuv);
		}
		if(tmp_x>0 && grayAlpha[pos - 1] != f_info.font_color.red){//left
			grayAlpha[pos - 1] = f_info.font_color.red;
			drawpixel(I420Buf,tmp_x-1,tmp_y,yuv);
		}
		if(tmp_x<width-1 && grayAlpha[pos + 1] != f_info.font_color.red){//right
			grayAlpha[pos + 1] = f_info.font_color.red;
			drawpixel(I420Buf,tmp_x+1,tmp_y,yuv);
		}

	}		
		
	return 0;
	
}