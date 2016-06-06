#include<stack>
#include<vector>
#include<sys/time.h>
#include"dot_rgb.h"
#include"drawGraph_yuyv.h"
#include"string_overlay_yuyv.h"
using namespace std;

OverlayerYUYV::OverlayerYUYV()
{
	mpBgBuf = NULL;
	mpDotData = NULL;
	mpStringInfo = NULL;
}
OverlayerYUYV::OverlayerYUYV(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag)
	:mBgWidth(bg_w),mBgHeight(bg_h),mX(X),mY(Y),mNeedStroke(StrokeFlag)
{
	mpBgBuf = NULL;
	mpDotData = NULL;
	mpStringInfo = NULL;
	if(mX > mBgWidth || mY > mBgHeight){
		log_print(_ERROR,"In OverlayerYUYV, X > width || Y > height\n");
	}
}
OverlayerYUYV::~OverlayerYUYV()
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

void OverlayerYUYV::initialParms(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag)
{
	mBgWidth = bg_w;
	mBgHeight = bg_h;
	mX = X;
	mY = Y;
	mNeedStroke = StrokeFlag;
}

void OverlayerYUYV::initialDotData(font_info * f_info)
{
	if(f_info == NULL){
		log_print(_ERROR,"OverlayerYUYV::initialDotData f_info null ptr.\n");
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

int OverlayerYUYV::initialOverlay(uchar *yuvBuf)
{
	if(mpDotData== NULL){
		log_print(_ERROR,"In OverlayerYUYV,dot_overlay_yuyv: NULL str\n");
		return -1;
	}
	if(yuvBuf == NULL){
		log_print(_ERROR,"In OverlayerYUYV,dot_overlay_yuyv: NULL yuvBuf\n");
		return -2;
	}
	if(mX > mBgWidth || mY > mBgHeight){
		log_print(_ERROR,"In OverlayerYUYV,dot_overlay_yuyv: X > width || Y > height\n");
	}
	if(mpStringInfo->str.size() == 0){
		log_print(_ERROR,"In OverlayerYUYV,dot_overlay_yuyv: empty string\n");
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

	//rgb颜色转yuv
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

	uint cur_X;
//timeval begin,end;
//gettimeofday(&begin,NULL);
	for(int j = 0,col = 0;j < font_bytes;j += seg,++col)//j表示处理第j段数据
	{
		if(col + mY >= mBgHeight)break;
		int i;
		uint y_pos = (mBgWidth*(mY+col)+mX)<<1;
		uint u_pos = y_pos + 1;
		uint v_pos = y_pos + 3;
		cur_X = mX;
		for(i = 0;i < str_size;++i)//i表示第i个字符
		{
			for(int bytes = 0;bytes < seg;++bytes)//例如，16*32字体是2段表示一行，bytes表示正处理的段
			{
				for(int bit = 0;bit < 8;++bit)//bit表示处理的位
				{
					if(mpDotData[i][j] & mask[bit])
					{
							//xstack.push((y_pos>>1)-width*(Y+col));
							//ystack.push(Y+col);
						pointVec.push_back((y_pos>>1)-mBgWidth*(mY+col));
						pointVec.push_back(mY+col);
						yuvBuf[y_pos] = yuv_overlay.y; y_pos+=2;
						if((bit&1)!=0)//每两个取一次样
						{
							yuvBuf[u_pos] = yuv_overlay.u; u_pos+=4;
							yuvBuf[v_pos] = yuv_overlay.v; v_pos+=4;
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
							y_pos+=2;
							if((bit&1)!=0)
							{
								u_pos+=4;
								v_pos+=4;
							}
						}else{
							yuvBuf[y_pos] = yuv_bg.y; y_pos+=2;
							if(bit%2==0)
							{
								yuvBuf[u_pos] = yuv_bg.u; u_pos+=4;
								yuvBuf[v_pos] = yuv_bg.v; v_pos+=4;
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
endloop1: 	;
	}
	//gettimeofday(&end,NULL);
	//printf("core interval: %ld\n",(end.tv_sec-begin.tv_sec)*1000000+end.tv_usec-begin.tv_usec);
	//gettimeofday(&begin,NULL);
	if(mNeedStroke)
	{
		if(pointVec.empty()){
			log_print(_ERROR,"dot_overlay_yuyv: cannot find the string pixel.\n");
			return -1;
		}
		initGraphParm_yuyv(mBgWidth,mBgHeight);
		for(uint i=0;i < pointVec.size();i+=2)
		{
			int tmp_pos = (pointVec[i] + (pointVec[i+1])*mBgWidth)<<1;
			if(pointVec[i+1] > 0 && mpBgBuf[tmp_pos - (mBgWidth<<1)] != yuv_overlay.y){//up
				drawpixel_yuyv(mpBgBuf,pointVec[i],pointVec[i+1]-1,yuv_bg);
				strokeVec.push_back(pointVec[i]);
				strokeVec.push_back(pointVec[i+1]-1);
			}
			if(pointVec[i+1]<mBgHeight-1 && mpBgBuf[tmp_pos + (mBgWidth<<1)] != yuv_overlay.y){//down
				drawpixel_yuyv(mpBgBuf,pointVec[i],pointVec[i+1]+1,yuv_bg);
				strokeVec.push_back(pointVec[i]);
				strokeVec.push_back(pointVec[i+1]+1);
			}
			if(pointVec[i]>0 && mpBgBuf[tmp_pos - 2] != yuv_overlay.y){//left
				drawpixel_yuyv(mpBgBuf,pointVec[i]-1,pointVec[i+1],yuv_bg);
				strokeVec.push_back(pointVec[i]-1);
				strokeVec.push_back(pointVec[i+1]);
			}
			if(pointVec[i]<mBgWidth-1 && mpBgBuf[tmp_pos + 2] != yuv_overlay.y){//right
				drawpixel_yuyv(mpBgBuf,pointVec[i]+1,pointVec[i+1],yuv_bg);
				strokeVec.push_back(pointVec[i]+1);
				strokeVec.push_back(pointVec[i+1]);
			}
		}
	}
	//gettimeofday(&end,NULL);
	//printf("stroke interval: %ld\n",(end.tv_sec-begin.tv_sec)*1000000+end.tv_usec-begin.tv_usec);
	return 0;
}
int OverlayerYUYV::overlay(uchar* bgBuf)
{
	if(pointVec.empty()){
		log_print(_ERROR,"dot_overlay_yuyv: cannot find the string pixel.\n");
		return -1;
	}
	initGraphParm_yuyv(mBgWidth,mBgHeight);
	for(uint i=0;i < pointVec.size();i+=2)
	{
		drawpixel_yuyv(bgBuf,pointVec[i],pointVec[i+1],yuv_overlay);
	}
	if(mNeedStroke)
	{
		if(strokeVec.empty()){
			log_print(_ERROR,"dot_overlay_yuyv: cannot find the stroke pixel.\n");
			return -1;
		}
		for(uint i=0;i < strokeVec.size();i+=2)
		{
			drawpixel_yuyv(bgBuf,strokeVec[i],strokeVec[i+1],yuv_bg);
		}
	}
	return 0;
}

/*
int dot_to_yuyv(uchar *str[],
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
	uint m = 1;
	uint n = 3;

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
						yuvBuf[k] = y[1]; k+=2;
						if(bit%2==0)
						{
							yuvBuf[m] = u[1]; m+=4;
							yuvBuf[n] = v[1]; n+=4;
						}
					}
					else
					{
						yuvBuf[k] = y[0]; k+=2;
						if(bit%2==0)
						{
							yuvBuf[m] = u[0]; m+=4;
							yuvBuf[n] = v[0]; n+=4;
						}
					}
				}
				++j;
			}
			j -= seg;
		}
	}
}*/
/*
int yuyv_overlay(uchar* yuvBg,uint nWidth,uint nHeight,uchar* yuvOverlay,uint w,uint h,uint X,uint Y)
{
	if(yuvBg == NULL || yuvOverlay == NULL )
	{
		log_print(_INFO,"In string_overlay_yuyv,yuyv_overlay: ptr NULL\n");
		return -1;
	
	}
	if(X > width || Y > height){
		log_print(_INFO,"In string_overlay_yuyv,yuyv_overlay: X > width || Y > height\n");
		return -3;	
	}
	if(X&1)++X;
	if(Y&1)++Y;
	
	for(uint i = Y;i < Y+h;++i){
		if(nWidth <= i)break;
		int h_offset = i-Y;
		for(uint j = X;j < X+w;++j){
			if(nHeight <= j)break;
			int y_pos = i*nHeight+j;
			int y_pos_overlay = h_offset*w+j-X;
			yuvBg[i*nHeight+j] = yuvOverlay[y_pos_overlay];
			if(j%2==0)
			{
				yuvBg[y_pos + 1] = yuvOverlay[y_pos_overlay + 1];
				yuvBg[y_pos + 3] = yuvOverlay[y_pos_overlay + 3];
			}
		}

	}
	return 0;
}*/
