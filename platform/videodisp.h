#ifndef _VIDEO_DISP_H_
#define _VIDEO_DISP_H_
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
#include "avcodec.h"
#include "v4l2fmttofmt.h"
#include "mediacore.h"

//#include "mediacore.h"
//#define u32 unsigned int

//#define VIDEO_DISP_DEV "/dev/video17"

struct dispbuffer{        
	unsigned char *start;        
	size_t offset;        
	unsigned int length;
};


#define DISP_BUFFER_MAX_NUM 10
#define DISP_BUFFER_NUM 3


class VidDisp
{
public:
    	VidDisp(vidDispParm Parm ,int memoryType);
	~VidDisp();
	int configure(vidCapParm *pParm);
	int getDispImgBlk();
	int configure(vidDispParm *pParm);
	int addBuffer(unsigned char * vaddr,unsigned int paddr,size_t length);
	int switchBuffers(int amount,ImgBlk *newBuffers[],vidDispParm *pParm = NULL);//
	int display(ImgBlk *newBuffers);
	int startDisplay();
	int stopStoplay();
	int getFd()const { return mV4l2Fd; }
	vidDispParm* getParm() {return &mVidDispParm;}

        
private:
	int m_num_buffers;  
	int m_num_enqbuf; //the number of buffers in V4L2 outgoing queue.
	int fliterImg;
	int m_mem_type;//V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;    
	int openDev();
	int closeDev();
	int fb_setup(int alpha,bool colorkey_enable = false,u32 colorkey = 0x000000);
	int mV4l2Fd;
	bool isStarted;
	char devPath[MAX_PATH_NAME_LEN];
	//u32 mFourcc;
	//int mWidth;
	//int mHeight;
	//struct v4l2_requestbuffers req;  
	vidDispParm mVidDispParm;
	struct v4l2_format fmt;
    	struct v4l2_requestbuffers buf_req;     
	struct dispbuffer buffers[DISP_BUFFER_MAX_NUM];
	struct v4l2_buffer buf;
};

#endif

