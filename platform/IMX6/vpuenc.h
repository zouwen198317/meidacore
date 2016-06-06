#ifndef _VPU_ENC_H
#define _VPU_ENC_H
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>

//#include "mxc_ipudev_test.h"
#include "platform/HAL/videoencoder.h"

#include "ffmpeg/avcodec_common.h"

#include "vpu_conf.h"

class VpuEnc : public CVideoEnc
{
public:
	VpuEnc(videoParms * parms);
	~VpuEnc();
	int config(videoParms * parms);
	int encode(ImgBlk * src, VencBlk * dest);//retuen -1, frame type 0, 1 for I frame
	int encode(ImgBlk * src, VencBlk * dest, bool keyframe);//force to encode I frame
	//int setSkipping(bool flag){mSkipping = flag; return 0;}
	static void VpuInit();	
	static void VpuUnInit();
	bufZone* allocHwBuffer(u32 size); //for encode data output, ring buffer
	void freeHwBuffer(bufZone* zone);
private:
	//EncHandle *mpEncHdl;
	bufZone Enc_bufZone;	
	struct encode_	*enc;
	struct cmd_line cmd;
	vpu_mem_desc	mem_desc;
	unsigned int frame_num;
	//bool mSkipping;


	int set_EncSrc_addr(struct frame_buf *fb, int strideY, int height,ImgBlk* src,FrameBuffer *fb_addr,int src_fbid);
	int set_vencBlk_addr(u32 bs_pa_startaddr,VencBlk * dest);
	int set_EncOpenParam(videoParms* parms);
	int set_EncInitialInfo();
	int calloc_FrameBuff();
	void free_Vpu_All_Space();
	void copy_JPEG_table(EncMjpgParam *param);
};

#endif

