#include<iostream>
#include<stack>
#include"dot_rgb.h"
#include"drawGraph_I420.h"
#include"string_overlay_I420.h"
using namespace std;

static stack<uint> xstack;
static stack<uint> ystack;


OverlayerI420::OverlayerI420()
{
	mpBgBuf = NULL;
	mpDotData = NULL;
	mpStringInfo = NULL;
}
OverlayerI420::OverlayerI420(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag)
	:mBgWidth(bg_w),mBgHeight(bg_h),mX(X),mY(Y),mNeedStroke(StrokeFlag)
{
	mpBgBuf = NULL;
	mpDotData = NULL;
	mpStringInfo = NULL;
	if(mX > mBgWidth || mY > mBgHeight){
		log_print(_ERROR,"In OverlayerI420, X > width || Y > height\n");
	}
}
OverlayerI420::~OverlayerI420()
{
	mpBgBuf = NULL;
	if(mpDotData != NULL)
	{
		for(int i = 0;i < mpStringInfo->str.size();++i)
				delete [] mpDotData[i];
		delete [] mpDotData;
		mpDotData = NULL;
	}
	if(mpStringInfo != NULL)
	{
		delete mpStringInfo;
		mpStringInfo = NULL;
	}
	pointVec.clear();
	strokeVec.clear();
}

void OverlayerI420::initialParms(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag)
{
	mBgWidth = bg_w;
	mBgHeight = bg_h;
	mX = X;
	mY = Y;
	mNeedStroke = StrokeFlag;
}

void OverlayerI420::initialDotData(font_info * f_info)
{
	if(f_info == NULL){
		log_print(_ERROR,"OverlayerI420::initialDotData f_info null ptr.\n");
		return;
	}
	if(mpDotData != NULL)
	{
		for(int i = 0;i < mpStringInfo->str.size();++i)
				delete [] mpDotData[i];
		delete [] mpDotData;
		mpDotData = NULL;
	}
	if(mpStringInfo != NULL)
	{
		delete mpStringInfo;
		mpStringInfo = NULL;
	}
	mpStringInfo = new font_info(f_info->str,f_info->width,f_info->height,
									f_info->background,f_info->font_color,f_info->path,false);
	mpDotData= new uchar*[mpStringInfo->str.size()];
	uint font_bytes = mpStringInfo->width/8*mpStringInfo->height;
	for(int i=0;i < mpStringInfo->str.size();++i)
		mpDotData[i] = new uchar[font_bytes];
	string_to_dot_matrix(mpStringInfo->path.data(),font_bytes,mpStringInfo->str.data(),mpDotData);
}

int OverlayerI420::initialOverlay(uchar *yuvBuf)
{
	if(mpDotData== NULL){
		log_print(_ERROR,"In OverlayerI420,initialOverlay: NULL str\n");
		return -1;
	}
	if(yuvBuf == NULL){
		log_print(_ERROR,"In OverlayerI420,initialOverlay: NULL yuvBuf\n");
		return -2;
	}
	if(mX > mBgWidth || mY > mBgHeight){
		log_print(_ERROR,"In OverlayerI420,initialOverlay: X > width || Y > height\n");
	}
	if(mpStringInfo->str.size() == 0){
		log_print(_ERROR,"In OverlayerI420,initialOverlay: empty string\n");
		return -4;
	}
	
	//if(needStroke){
	//	while(!xstack.empty())
	//		xstack.pop();
	//	while(!ystack.empty())
	//ystack.pop();
	//}
	pointVec.clear();
	strokeVec.clear();
	mpBgBuf = yuvBuf;

	static uchar mask[] = {128,64,32,16,8,4,2,1};
	const uint seg = mpStringInfo->width / 8;
	const uint font_bytes = mpStringInfo->height * seg;
	const int str_size = mpStringInfo->str.size();

	const uint overlay_w = mpStringInfo->width * str_size;
	const uint overlay_h = mpStringInfo->height;

	if(mX%2!=0)++mX;
	if(mY%2!=0)++mY;

	//uchar y[2],u[2],v[2];
	int tmp_y0,tmp_u0,tmp_v0;
	int tmp_y1,tmp_u1,tmp_v1;
	tmp_y0 = ( ( 66 * mpStringInfo->background.red + 129 * mpStringInfo->background.green +  25 * mpStringInfo->background.blue + 128) >> 8) + 16  ;       
	tmp_u0 = ( ( -38 * mpStringInfo->background.red -  74 * mpStringInfo->background.green + 112 * mpStringInfo->background.blue + 128) >> 8) + 128 ;          
	tmp_v0 = ( ( 112 * mpStringInfo->background.red -  94 * mpStringInfo->background.green -  18 * mpStringInfo->background.blue + 128) >> 8) + 128 ;				
	tmp_y1 = ( ( 66 * mpStringInfo->font_color.red + 129 * mpStringInfo->font_color.green +  25 * mpStringInfo->font_color.blue + 128) >> 8) + 16  ;   
	tmp_u1 = ( ( -38 * mpStringInfo->font_color.red -  74 * mpStringInfo->font_color.green + 112 * mpStringInfo->font_color.blue + 128) >> 8) + 128 ;  
	tmp_v1 = ( ( 112 * mpStringInfo->font_color.red -  94 * mpStringInfo->font_color.green -  18 * mpStringInfo->font_color.blue + 128) >> 8) + 128 ;

	yuv_bg.y = max(0,min(tmp_y0,255));
	yuv_overlay.y= max(0,min(tmp_y1,255));
	yuv_bg.u = max(0,min(tmp_u0,255));
	yuv_overlay.u = max(0,min(tmp_u1,255));
	yuv_bg.v = max(0,min(tmp_v0,255));
	yuv_overlay.v = max(0,min(tmp_v1,255));

	const uint u_offset = mBgWidth*mBgHeight;
	const uint v_offset = mBgWidth*mBgHeight/4;
	uint cur_X;//记录当前x坐标，用来作为贴图是否越界的标准

	for(int j = 0,col = 0;j < font_bytes;j += seg,++col)//j表示处理第j段数据
	{
		if(col + mY >= mBgHeight)break;
		int i;
		uint y_offset = mBgWidth* (mY+col);
		uint y_pos = y_offset + mX;
		uint u_pos = u_offset + (y_offset>>2) + (mX>>1);
		uint v_pos = u_pos + v_offset;
		cur_X = mX;//重设X值
		for(i = 0;i < str_size;++i)//i表示第i个字符
		{
			for(int bytes = 0;bytes < seg;++bytes)//bytes表示正处理的段
			{
				for(int bit = 0;bit < 8;++bit)//bit表示处理的位
				{
					if(mpDotData[i][j] & mask[bit])
					{
						if(mNeedStroke){
							pointVec.push_back(y_pos-y_offset);
							pointVec.push_back(mY+col);
						}
						yuvBuf[ y_pos++ ] = yuv_overlay.y;
						if(col%2==0&&bit%2==0)//每4个y取一次样
						{
							yuvBuf[ u_pos++ ] = yuv_overlay.u;
							yuvBuf[ v_pos++ ] = yuv_overlay.v;
						}
						if(++cur_X >= mBgWidth)
						{
							j -= bytes;
							goto endloop1;
						}
					}
					else
					{
						if(mpStringInfo->hasBackground == false){
							++y_pos;
							if(col%2==0&&bit%2==0)
							{
								++u_pos;
								++v_pos;
							}
						}else{
							yuvBuf[ y_pos++ ] = yuv_bg.y;
							if(col%2==0&&bit%2==0)
							{
								yuvBuf[ u_pos++ ] = yuv_bg.u;
								yuvBuf[ v_pos++ ] = yuv_bg.v;
							}
						}
						if(++cur_X >= mBgWidth)
						{
							j -= bytes;
							goto endloop1;
						}
					}
				}
				++j;
			}
			j -= seg;
		}
endloop1:	;
	}
	
	if(mNeedStroke)
	{
		if(pointVec.empty()){
			log_print(_ERROR,"dot_overlay_yuyv: cannot find the string pixel.\n");
			return -1;
		}
		initGraphParm_I420(mBgWidth,mBgHeight);
		for(uint i=0;i < pointVec.size();i+=2)
		{
			int tmp_pos = (pointVec[i] + (pointVec[i+1])*mBgWidth);
			if(pointVec[i+1] > 0 && mpBgBuf[tmp_pos - mBgWidth] != yuv_overlay.y){//up
				drawpixel(mpBgBuf,pointVec[i],pointVec[i+1]-1,yuv_bg);
				strokeVec.push_back(pointVec[i]);
				strokeVec.push_back(pointVec[i+1]-1);
			}
			if(pointVec[i+1]<mBgHeight-1 && mpBgBuf[tmp_pos + mBgWidth] != yuv_overlay.y){//down
				drawpixel(mpBgBuf,pointVec[i],pointVec[i+1]+1,yuv_bg);
				strokeVec.push_back(pointVec[i]);
				strokeVec.push_back(pointVec[i+1]+1);
			}
			if(pointVec[i]>0 && mpBgBuf[tmp_pos - 1] != yuv_overlay.y){//left
				drawpixel(mpBgBuf,pointVec[i]-1,pointVec[i+1],yuv_bg);
				strokeVec.push_back(pointVec[i]-1);
				strokeVec.push_back(pointVec[i+1]);
			}
			if(pointVec[i]<mBgWidth-1 && mpBgBuf[tmp_pos + 1] != yuv_overlay.y){//right
				drawpixel(mpBgBuf,pointVec[i]+1,pointVec[i+1],yuv_bg);
				strokeVec.push_back(pointVec[i]+1);
				strokeVec.push_back(pointVec[i+1]);
			}
		}
	}
	//gettimeofday(&end,NULL);
	//printf("stroke interval: %ld\n",(end.tv_sec-begin.tv_sec)*1000000+end.tv_usec-begin.tv_usec);
	return 0;
}
int OverlayerI420::overlay(uchar* bgBuf)
{
	if(pointVec.empty()){
		log_print(_ERROR,"OverlayerI420::overlay cannot find the string pixel.\n");
		return -1;
	}
	initGraphParm_I420(mBgWidth,mBgHeight);
	for(uint i=0;i < pointVec.size();i+=2)
	{
		drawpixel(bgBuf,pointVec[i],pointVec[i+1],yuv_overlay);
	}
	if(mNeedStroke)
	{
		if(strokeVec.empty()){
			log_print(_ERROR,"dot_overlay_yuyv: cannot find the stroke pixel.\n");
			return -1;
		}
		for(uint i=0;i < strokeVec.size();i+=2)
		{
			drawpixel(bgBuf,strokeVec[i],strokeVec[i+1],yuv_bg);
		}
	}
	return 0;
}


int dot_overlay_I420(uchar *str[],
							const font_info f_info,
								uchar *yuvBuf,
								uint width,
								uint height,
								uint X,
								uint Y,
								bool needStroke)
{
	if(str == NULL){
		log_print(_ERROR,"In string_overlay_I420,dot_overlay_I420: NULL str\n");
		return -1;
	}
	if(yuvBuf == NULL){
		log_print(_ERROR,"In string_overlay_I420,dot_overlay_I420: NULL yuvBuf\n");
		return -2;
	}
	if(X > width || Y > height){
		log_print(_ERROR,"In string_overlay_I420,dot_overlay_I420: X > width || Y > height\n");
		return -3;
	}
	if(f_info.str.size() == 0){
		log_print(_ERROR,"In string_overlay_I420,dot_overlay_I420: empty string\n");
		return -4;
	}

	if(needStroke){
		while(!xstack.empty())
			xstack.pop();
		while(!ystack.empty())
			ystack.pop();
	}

	static uchar mask[] = {128,64,32,16,8,4,2,1};
	const uint seg = f_info.width / 8;
	const uint font_bytes = f_info.height * seg;
	const int str_size = f_info.str.size();

	//const uint overlay_w = f_info.width * str_size;
	//const uint overlay_h = f_info.height;

	if(X%2!=0)++X;
	if(Y%2!=0)++Y;

	uchar y[2],u[2],v[2];
	int tmp_y0,tmp_u0,tmp_v0;
	int tmp_y1,tmp_u1,tmp_v1;
	tmp_y0 = ( ( 66 * f_info.background.red + 129 * f_info.background.green +  25 * f_info.background.blue + 128) >> 8) + 16  ;       
	tmp_u0 = ( ( -38 * f_info.background.red -  74 * f_info.background.green + 112 * f_info.background.blue + 128) >> 8) + 128 ;          
	tmp_v0 = ( ( 112 * f_info.background.red -  94 * f_info.background.green -  18 * f_info.background.blue + 128) >> 8) + 128 ;				
	tmp_y1 = ( ( 66 * f_info.font_color.red + 129 * f_info.font_color.green +  25 * f_info.font_color.blue + 128) >> 8) + 16  ;   
	tmp_u1 = ( ( -38 * f_info.font_color.red -  74 * f_info.font_color.green + 112 * f_info.font_color.blue + 128) >> 8) + 128 ;  
	tmp_v1 = ( ( 112 * f_info.font_color.red -  94 * f_info.font_color.green -  18 * f_info.font_color.blue + 128) >> 8) + 128 ;

	y[0] = max(0,min(tmp_y0,255));
	y[1] = max(0,min(tmp_y1,255));
	u[0] = max(0,min(tmp_u0,255));
	u[1] = max(0,min(tmp_u1,255));
	v[0] = max(0,min(tmp_v0,255));
	v[1] = max(0,min(tmp_v1,255));

	const uint u_offset = width*height;
	const uint v_offset = width*height/4;
	uint cur_X;//记录当前x坐标，用来作为贴图是否越界的标准

	for(int j = 0,col = 0;j < font_bytes;j += seg,++col)//j表示处理第j段数据
	{
		if(col + Y >= height)break;
		int i;
		uint y_offset = width * (Y+col);
		uint y_pos = y_offset + X;
		uint u_pos = u_offset + (y_offset>>2) + (X>>1);
		uint v_pos = u_pos + v_offset;
		cur_X = X;//重设X值
		for(i = 0;i < str_size;++i)//i表示第i个字符
		{
			for(int bytes = 0;bytes < seg;++bytes)//bytes表示正处理的段
			{
				for(int bit = 0;bit < 8;++bit)//bit表示处理的位
				{
					if(str[i][j] & mask[bit])
					{
						if(needStroke){
							xstack.push(y_pos-y_offset);
							ystack.push(Y+col);
						}
						yuvBuf[ y_pos++ ] = y[1];
						if(col%2==0&&bit%2==0)//每4个y取一次样
						{
							yuvBuf[ u_pos++ ] = u[1];
							yuvBuf[ v_pos++ ] = v[1];
						}
						if(++cur_X >= width)
						{
							j -= bytes;
							goto endloop1;
						}
					}
					else
					{
						if(f_info.hasBackground == false){
							++y_pos;
							if(col%2==0&&bit%2==0)
							{
								++u_pos;
								++v_pos;
							}
						}else{
							yuvBuf[ y_pos++ ] = y[0];
							if(col%2==0&&bit%2==0)
							{
								yuvBuf[ u_pos++ ] = u[0];
								yuvBuf[ v_pos++ ] = v[0];
							}
						}
						if(++cur_X >= width)
						{
							j -= bytes;
							goto endloop1;
						}
					}
				}
				++j;
			}
			j -= seg;
		}
endloop1:	;
	}
	if(needStroke)
	{
		if(xstack.empty() || ystack.empty()){
			log_print(_ERROR,"dot_overlay_I420: cannot find the string pixel.\n");
			return -1;
		}
		uint tmp_x,tmp_y;
		initGraphParm_I420(width,height);
		YUV yuv(y[0],u[0],v[0]);
		while(!xstack.empty()){
			tmp_x = xstack.top();
			tmp_y = ystack.top();
			xstack.pop();
			ystack.pop();
			drawpixel(yuvBuf,tmp_x,tmp_y,yuv);

		}		
	}
	return 0;
}


int dot_to_I420(uchar *str[],
							const font_info f_info,
								uchar *yuvBuf)
{
	static uchar mask[] = {128,64,32,16,8,4,2,1};
	const uint seg = f_info.width / 8;
	const uint font_bytes = f_info.height * seg;
	const int str_size = f_info.str.size();

	uchar y[2],u[2],v[2];
	int tmp_y0,tmp_u0,tmp_v0;
	int tmp_y1,tmp_u1,tmp_v1;
	tmp_y0 = ( ( 66 * f_info.background.red + 129 * f_info.background.green +  25 * f_info.background.blue + 128) >> 8) + 16  ;       
	tmp_u0 = ( ( -38 * f_info.background.red -  74 * f_info.background.green + 112 * f_info.background.blue + 128) >> 8) + 128 ;          
	tmp_v0 = ( ( 112 * f_info.background.red -  94 * f_info.background.green -  18 * f_info.background.blue + 128) >> 8) + 128 ;				
	tmp_y1 = ( ( 66 * f_info.font_color.red + 129 * f_info.font_color.green +  25 * f_info.font_color.blue + 128) >> 8) + 16  ;   
	tmp_u1 = ( ( -38 * f_info.font_color.red -  74 * f_info.font_color.green + 112 * f_info.font_color.blue + 128) >> 8) + 128 ;  
	tmp_v1 = ( ( 112 * f_info.font_color.red -  94 * f_info.font_color.green -  18 * f_info.font_color.blue + 128) >> 8) + 128 ;

	y[0] = max(0,min(tmp_y0,255));
	y[1] = max(0,min(tmp_y1,255));
	u[0] = max(0,min(tmp_u0,255));
	u[1] = max(0,min(tmp_u1,255));
	v[0] = max(0,min(tmp_v0,255));
	v[1] = max(0,min(tmp_v1,255));
	

	uint k = 0;
	uint u_pos = str_size*f_info.width*f_info.height;
	uint v_pos = u_pos /4*5;

	for(int j = 0,col=0;j < font_bytes;j += seg,++col)
	{
		int i;
		for(i = 0;i < str_size;++i)
		{
			for(int bytes = 0;bytes < seg;++bytes)
			{
				for(int bit = 0;bit < 8;++bit)
				{
					if(str[i][j] & mask[bit])
					{
						yuvBuf[ k++ ] = y[1];
						if(col%2==0&&bit%2==0)
						{
							yuvBuf[u_pos++] = u[1];
							yuvBuf[v_pos++] = v[1];
						}
					}
					else
					{
						yuvBuf[ k++ ] = y[0];
						if(col%2==0&&bit%2==0)
						{
							yuvBuf[u_pos++] = u[0];
							yuvBuf[v_pos++] = v[0];
						}
					}
				}
				++j;
			}
			j -= seg;
		}
	}
	return 0;
}
