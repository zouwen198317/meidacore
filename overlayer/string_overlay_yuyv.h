#ifndef STRING_OVERLAY_YUYV_H
#define STRING_OVERLAY_YUYV_H

#include"common.h"
#include"dot_rgb.h"
#include<vector>
using namespace std;

/**
	str: 点阵字体数据。str[i][j],i表示第i个字符，j表示第j段数据（16*32字体，2段表示1行）
	f_info: 有关字体的信息
	yuvBuf: 被覆盖的yuv数据
	width、height: 该图像的寬高
	X、Y: 覆盖点坐标的位置
	
	传入点阵字体数据，直接在yuv指定位置上做覆盖操作，即贴图。
*/
class OverlayerYUYV
{
public:
	OverlayerYUYV();
	OverlayerYUYV(uint bg_w,uint bg_h,uint X,uint Y,bool StrokeFlag);
	~OverlayerYUYV();
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

//点阵字体数据 转 yuyv

int dot_to_yuyv(uchar *str[],
							const font_info f_info,
								uchar *yuvBuf);

#endif
