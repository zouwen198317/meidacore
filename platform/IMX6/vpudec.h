#include "platform/HAL/videodecoder.h"
#include "vpu_conf.h"
#include "ffmpeg/avcodec_common.h"
#include "mediacore.h"

class VpuDec : public CVideoDec
{
public:
	VpuDec(videoParms * parms);
	~VpuDec();
	int config(videoParms * parms);
	int decode(VencBlk * src, ImgBlk * dest);//blocking
	
	bufZone* allocHwBuffer(u32 size); 
	void freeHwBuffer(bufZone*);

	int get_fbpool_info(fbuffer_info* info);

private:
	int regfbcount;
	int disp_clr_index ;
	int dis_index;
	int frame_id;
	int dec_format;
	DecHandle handle;
	bufZone Dec_bufZone;
	FrameBuffer * dec_fb;
	frame_buf ** dec_pfbpool;
	vpu_mem_desc	mem_desc;
	vpu_mem_desc ps_mem_desc;
	vpu_mem_desc slice_mem_desc;

	int set_DecOpenParam();
	int get_DecInitialInfo(VencBlk * src);
	int write_Src_Ringbuf(VencBlk * src);
	int calloc_FrameBuff(VencBlk * src);

};

