#ifndef _CAM_CAPTURE_H_
#define _CAM_CAPTURE_H_
/* Standard Include Files */
#include <errno.h>

/* Verification Test Environment Include Files */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <malloc.h>

#include "mediacore.h"

#include <queue>

struct cambuffer{        
	unsigned char *start;        
	size_t offset;        
	unsigned int length;
};

enum camType_t{
	CAM_NULL,
	CAM_OV10635,
	CAM_UVC,
	CAM_MAX,
};

#define CAM_BUFFER_NUM 3

class CamCapture
{
public:
	CamCapture();
	~CamCapture();
	int configure(vidCapParm *pParm);
	int capture(ImgBlk *img);
	int startCapture();
	int stopCapture();
	int getFd()const { return mV4l2Fd; }
	fbuffer_info* get_buffer_info(fbuffer_info* info);
	int getCamType() const { return m_camType; }
	time_t getStartPts() { return mStarTv.tv_sec; }
private:
	int openDev();
	int closeDev();
	u32 AppFmtToV4l2Fourcc(u32 fmt);
	u64 getBufferPts(struct v4l2_buffer * buf);
	int mV4l2Fd;
	enum camType_t m_camType;
	bool isStarted;
	bool isConfigured;
	bool needQBuf;
	u32 mFourcc;
	int mWidth;
	int mHeight;
	struct v4l2_requestbuffers req;       
	struct cambuffer buffers[CAM_BUFFER_NUM];
	struct v4l2_buffer buf;
	struct  timeval    mStarTv;

	std::queue<struct v4l2_buffer> mBufIdQueue;
};

#endif

