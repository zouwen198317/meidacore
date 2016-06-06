#ifndef STRING_OVERLAY_I420_H
#define STRING_OVERLAY_I420_H

#include<vector>
#include"common.h"
#include"dot_rgb.h"
using namespace std;
/**
	str: 点阵字体数据。str[i][j],i表示第i个字符，j表示第j段数据（16*32字体，2段表示1行）
	f_info: 有关字体的信息
	yuvBuf: 被覆盖的yuv数据
	width、height: 该图像的寬高
	X、Y: 覆盖点坐标的位置
	
	传入点阵字体数据，直接在yuv指定位置上做覆盖操作，即贴图。
*/
class OverlayerI420
{
public:
	OverlayerI420();
	OverlayerI420(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag);
	~OverlayerI420();
	void initialParms(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag);
	void initialDotData(font_info* f_info);
	int initialOverlay(uchar*);
	int overlay(uchar*);
private:
	vector<uint> strokeVec;
	vector<uint> pointVec;	
	bool mNeedStroke;
	uint mX;
	uint mY;
	uint mBgWidth;
	uint mBgHeight;
	uchar* mpBgBuf;
	uchar** mpDotData;
	YUV yuv_bg;
	YUV yuv_overlay;
	font_info *mpStringInfo;
	
};

int dot_overlay_I420(uchar *str[],
							const font_info f_info,
								uchar *yuvBuf,
								uint width,
								uint height,
								uint X,
								uint Y,
								bool needStroke=false);
//点阵字体数据 转 I420
int dot_to_I420(uchar *str[],
							const font_info f_info,
								uchar *yuvBuf);
#endif
