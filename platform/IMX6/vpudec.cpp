
#include "vpudec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "log/log.h"



#define PS_SAVE_SIZE 0x80000
#define MAX_BUF_NUM 32
#define Dec_RingBuf_Size 0xA00000

struct frame_buf fbpool[MAX_BUF_NUM];

VpuDec::VpuDec(videoParms * parms):CVideoDec(parms)
{	
	dec_fb = NULL;
	dec_pfbpool = NULL;
	regfbcount = 0;
	switch(parms->codec){
		case CODEC_ID_H264:
		log_print(_DEBUG,"VpuDec:: VPU Code CODEC_ID_H264 STD_AVC %d\n",STD_AVC);
		dec_format = STD_AVC;
		break;
		case CODEC_ID_MJPEG:
		log_print(_DEBUG,"VpuEnc:: VPU Code CODEC_ID_MJPEG STD_MJPG\n");
		dec_format = STD_MJPG;
		break;
		default:
			log_print(_ERROR,"VpuEnc::invalid codec(0x%x),change to VPU Code CODEC_ID_MJPEG STD_MJPG\n",parms->codec);
			dec_format = STD_MJPG;
			break;
	}
	
	disp_clr_index = -1;
	dis_index = -1;
	frame_id = 0;
	
	memset(&mem_desc,0,sizeof(mem_desc));
	memset(&ps_mem_desc,0,sizeof(ps_mem_desc));
	memset(&slice_mem_desc,0,sizeof(slice_mem_desc));
	
	set_DecOpenParam();			
}

VpuDec::~VpuDec()
{
	RetCode ret;
	ret = vpu_DecClose(handle);
	if (ret == RETCODE_FRAME_NOT_COMPLETE) {
		vpu_SWReset(handle, 0);
		ret = vpu_DecClose(handle);
		if (ret != RETCODE_SUCCESS)
			printf("vpu_DecClose failed\n");
	}
	printf("vpu_DecClose success\n");


	for (int i = 0; i < regfbcount; i++) {
		framebuf_free(&fbpool[i]);
	}


	if(dec_format == STD_AVC){
		IOFreePhyMem(&slice_mem_desc);
		IOFreePhyMem(&ps_mem_desc);
	}

	freeHwBuffer(&Dec_bufZone);

	if (dec_fb) {
		free(dec_fb);
		dec_fb = NULL;
	}

	if (dec_pfbpool) {
		free(dec_pfbpool);
		dec_pfbpool = NULL;
	}

}

int VpuDec::config(videoParms * parms)
{
	return 0;
}

int VpuDec::decode(VencBlk * src, ImgBlk * dest)//blocking
{	
	int ret = 1; 
	int loop_id = 0;
	int imageSize;
	DecOutputInfo outinfo = {0};
	DecParam decparam = {0};

	if(dec_fb == NULL || dec_pfbpool == NULL){
		if (write_Src_Ringbuf(src) == -1 || get_DecInitialInfo(src) == -1 || calloc_FrameBuff(src) == -1){
			printf("Config VPU error:VencBlk codec 0x%x, %dx%d, frameSize %d, frameType %d\n",src->codec,src->width,src->height,src->frameSize,src->frameType);			
			return -1;
		}
		decparam.dispReorderBuf = 0;
		decparam.skipframeMode = 0;
		decparam.skipframeNum = 0;
		decparam.iframeSearchEnable = 0;

		if(dec_format == STD_MJPG)
			vpu_DecGiveCommand(handle, SET_ROTATOR_STRIDE, &src->width);
		
	}else{
		ret = write_Src_Ringbuf(src);
			if (ret < 0) {
				printf("read data failed\n");
				return -1;
			}

	}
	if(dec_format == STD_MJPG){
			decparam.chunkSize = src->frameSize;
			decparam.phyJpgChunkBase = Dec_bufZone.pStartAddr;
			decparam.virtJpgChunkBase = (unsigned char*)Dec_bufZone.vStartaddr;
			vpu_DecGiveCommand(handle, SET_ROTATOR_OUTPUT,(void *)&dec_fb[0]);
	}

	imageSize = src->width * src->height * 3 / 2;
	
	ret = vpu_DecStartOneFrame(handle, &decparam);
	if (ret != RETCODE_SUCCESS) {
			printf("DecStartOneFrame failed, ret=%d,LINE:%d\n", ret,__LINE__);
			return -1;
	}

			loop_id = 0;

	while (vpu_IsBusy()) {
			
			if (loop_id == 50) {
				ret = vpu_SWReset(handle, 0);
				return -1;
			}

			vpu_WaitForInt(100);
			loop_id ++;
	}

	ret = vpu_DecGetOutputInfo(handle, &outinfo);
	if (ret != RETCODE_SUCCESS) {
			printf("vpu_DecGetOutputInfo failed Err code is %d\n"
				"\tframe_id = %d\n", ret, (int)frame_id);
			return -1;
		}
	
	if (cpu_is_mx6x() && (outinfo.decodingSuccess & 0x10)) {
			printf("vpu needs more bitstream in rollback mode\n"
				"\tframe_id = %d\n", (int)frame_id);

			return -1;
	}

	if (outinfo.notSufficientPsBuffer) {
		printf("PS Buffer overflow,LINE:%d\n",__LINE__);
		return -1;
	}

	if (outinfo.notSufficientSliceBuffer) {
		printf("Slice Buffer overflow,LINE:%d\n",__LINE__);
		return -1;
	}

	if (outinfo.indexFrameDisplay == -1){
			printf("VPU has no more display data,LINE:%d\n",__LINE__);
			return -1;
	}

	if ((outinfo.indexFrameDisplay == -3) ||(outinfo.indexFrameDisplay == -2)) {
		printf("VPU doesn't have picture to be displayed.\n"
				"\toutinfo.indexFrameDisplay = %d\n",
					outinfo.indexFrameDisplay);

			if (disp_clr_index >= 0) {
				ret = vpu_DecClrDispFlag(handle, disp_clr_index);
				if (ret)
					printf("vpu_DecClrDispFlag failed Error code"
								" %d\n", ret);
			}
			disp_clr_index = outinfo.indexFrameDisplay;		
		}
		
	dis_index = outinfo.indexFrameDisplay;
	dest->vaddr = (void *)(dec_pfbpool[dis_index]->addrY + (dec_pfbpool[dis_index]->desc.virt_uaddr- dec_pfbpool[dis_index]->desc.phy_addr)); 
	dest->paddr = dec_pfbpool[dis_index]->desc.phy_addr; //physical address
	dest->size = imageSize;
	dest->height = src->height;
	dest->width = src->width;
	dest->fourcc = PIX_FMT_YUV420P;//src->codec;
	dest->pts = src->pts;

	//printf("dis_index:%d,frame_id:%d,disp_clr_index:%d\n",dis_index,frame_id,disp_clr_index);

	if(dec_format == STD_AVC){
		if (disp_clr_index >= 0) {
			ret = vpu_DecClrDispFlag(handle,disp_clr_index);
			if (ret)
				printf("vpu_DecClrDispFlag failed Error code%d,LINE:%d\n", ret,__LINE__);
		}
	}
	
	disp_clr_index = outinfo.indexFrameDisplay;
	frame_id++;
	return 1;

}

bufZone* VpuDec::allocHwBuffer(u32 size)
{
	int ret = 0;
	
	mem_desc.size = size;
	Dec_bufZone.bufSize = mem_desc.size;
	ret = IOGetPhyMem(&mem_desc);
	if (ret) {
		printf("Unable to obtain physical memory,LINE:%d\n",__LINE__);
		return 0;
	}
	Dec_bufZone.pStartAddr = mem_desc.phy_addr;

	Dec_bufZone.vStartaddr = (void *)IOGetVirtMem(&mem_desc);
	if (Dec_bufZone.vStartaddr <= 0) {
		printf("Unable to map physical memory,LINE:%d\n",__LINE__);
		return 0;
	}
	
	if(dec_format == STD_AVC)
		Dec_bufZone.isRingBuf = 1;
	else 
		Dec_bufZone.isRingBuf = 0;
	
	return &Dec_bufZone;
}

void VpuDec::freeHwBuffer(bufZone*)
{	
	IOFreeVirtMem(&mem_desc);
	IOFreePhyMem(&mem_desc);
}

int VpuDec::set_DecOpenParam()
{
	int ret = -1;
	memset(&handle,0,sizeof(DecHandle));
	DecOpenParam oparam;// = {0};
	memset(&oparam, 0, sizeof(DecOpenParam)); //must to clean to 0
	
	allocHwBuffer(Dec_RingBuf_Size);
	
	oparam.bitstreamFormat = (CodStd)dec_format;
	oparam.bitstreamBuffer = Dec_bufZone.pStartAddr;
	oparam.bitstreamBufferSize = Dec_bufZone.bufSize;
	oparam.pBitStream = (Uint8 *)Dec_bufZone.vStartaddr;
	oparam.reorderEnable = 1;
	oparam.mp4DeblkEnable = 0;
	oparam.chromaInterleave = 0;
	oparam.mp4Class = 0;
	if (cpu_is_mx6x())
		oparam.avcExtension = 0;
	oparam.mjpg_thumbNailDecEnable = 0;
	oparam.mapType = 0;
	oparam.tiled2LinearEnable = 0;
	oparam.bitstreamMode = 1;
	oparam.jpgLineBufferMode = 1;
	
	if (dec_format == STD_AVC) {
		ps_mem_desc.size = PS_SAVE_SIZE;
		ret = IOGetPhyMem(&ps_mem_desc);
		if (ret){
			printf("Unable to obtain physical ps save mem,LINE%d\n",__LINE__);
			return -1;
		}
		
		oparam.psSaveBuffer = ps_mem_desc.phy_addr;
		oparam.psSaveBufferSize = PS_SAVE_SIZE;
	}
		//printf("bitstreamFormat %x, bitstreamBuffer %x,bitstreamBufferSize %d, pBitStream %x\n",oparam.bitstreamFormat,
			//oparam.bitstreamBuffer,oparam.bitstreamBufferSize,oparam.pBitStream);

	ret = vpu_DecOpen(&handle, &oparam);
	if (ret != RETCODE_SUCCESS) {
		printf("vpu_DecOpen failed, ret:%d\n", ret);
		return -1;
	}
	
	return 0;
}

int VpuDec::write_Src_Ringbuf(VencBlk * src)
{	

	if(dec_format == STD_MJPG){
	
		memcpy(Dec_bufZone.vStartaddr,(void *)((u32)src->buf.vStartaddr+src->offset),src->frameSize);

		return 0;
	}

	u32 space;
	int room;
	RetCode ret;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	u32 target_addr;
	
	ret = vpu_DecGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr,&space);
	if (ret != RETCODE_SUCCESS) {
		printf("vpu_DecGetBitstreamBuffer failed\n");
		return -1;
	}
	
	//printf("&pa_read_ptr:%x, &pa_write_ptr:%x,space:%x,vstart:%x\n",pa_read_ptr, pa_write_ptr,space,Dec_bufZone.vStartaddr+(pa_write_ptr-Dec_bufZone.pStartAddr));
	if (space <= 0) {
		printf("space %lu <= 0\n", space);
		return -1;
	}

	if (src->frameSize > space) {
		printf("space less than src size:%x < %x\n",space,(unsigned int)src->frameSize);
		return -1;
	}
	
	target_addr = (u32)Dec_bufZone.vStartaddr + (pa_write_ptr - Dec_bufZone.pStartAddr);
	if ( (target_addr + src->frameSize) > ((u32)Dec_bufZone.vStartaddr+Dec_RingBuf_Size)) {
		room = ((u32)Dec_bufZone.vStartaddr+Dec_RingBuf_Size) - target_addr;
		ret = vpu_DecUpdateBitstreamBuffer(handle, room);
		if (ret != RETCODE_SUCCESS) {
			printf("vpu_DecUpdateBitstreamBuffer failed\n");
			return -1;
		}

		ret = vpu_DecGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr,&space);
		if (ret != RETCODE_SUCCESS) {
			printf("vpu_DecGetBitstreamBuffer failed\n");
			return -1;
		}
		target_addr = (u32)Dec_bufZone.vStartaddr + (pa_write_ptr - Dec_bufZone.pStartAddr);
	}

	memcpy((void *)target_addr,(void *)((u32)src->buf.vStartaddr+src->offset),src->frameSize);

	ret = vpu_DecUpdateBitstreamBuffer(handle, src->frameSize);
		if (ret != RETCODE_SUCCESS) {
			printf("vpu_DecUpdateBitstreamBuffer failed,LINE:%d\n",__LINE__);
			return -1;
		}

	return 0;
}

int VpuDec::get_DecInitialInfo(VencBlk * src)
{	
	int ret = -1;
	DecInitialInfo initinfo = {0};
	
	/* Parse bitstream and get width/height/framerate etc */
	vpu_DecSetEscSeqInit(handle, 1);
	ret = vpu_DecGetInitialInfo(handle, &initinfo);
	vpu_DecSetEscSeqInit(handle, 0);
	if (ret != RETCODE_SUCCESS) {
		printf("vpu_DecGetInitialInfo failed, ret:%d, errorcode:%ld\n",
		         ret, initinfo.errorcode);
		return -1;
	}

	/*if (cpu_is_mx6x())
		printf("Decoder: width = %d, height = %d, frameRateRes = %lu, frameRateDiv = %lu, count = %u\n",
			initinfo.picWidth, initinfo.picHeight,
			initinfo.frameRateRes, initinfo.frameRateDiv,
			initinfo.minFrameBufferCount);*/

	/*if(initinfo.picWidth != src->width || initinfo.picHeight != src->height){
		printf("initinfo.picWidth or initinfo.picHeight is not equal the src\tLINE:%d\n",__LINE__);
		return -1;
	}*/
	
	src->width = initinfo.picWidth;
	src->height = initinfo.picHeight;
	printf("Decoder: width = %d, height = %d\n",initinfo.picWidth,initinfo.picHeight);

	regfbcount = initinfo.minFrameBufferCount +2;
	slice_mem_desc.size = initinfo.worstSliceSize * 1024;
	
	return 0;	
	
}

int VpuDec::calloc_FrameBuff(VencBlk * src)
{	
	int mvCol = 1;
	int ret = -1;
	int i;
	DecBufInfo bufinfo;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;

	if (dec_format == STD_MJPG)
		mvCol = 0;
	
	fb = dec_fb = (FrameBuffer *)calloc(regfbcount, sizeof(FrameBuffer));
	if (fb == NULL) {
		printf("Failed to allocate fb,LINE:%d\n",__LINE__);
		return -1;
	}

	pfbpool = dec_pfbpool = (frame_buf **)calloc(regfbcount, sizeof(struct frame_buf *));
	if (pfbpool == NULL) {
		printf("Failed to allocate pfbpool,LINE:%d\n",__LINE__);
		free(dec_fb);
		dec_fb = NULL;
		return -1;
	}

	for (i = 0; i < regfbcount; i++) {
		pfbpool[i] = framebuf_alloc(&fbpool[i], STD_AVC, 0, src->width, src->height, mvCol);
		if (pfbpool[i] == NULL)
			return -1;
	}

	for (i = 0; i < regfbcount; i++) {
			fb[i].myIndex = i;
			fb[i].bufY = pfbpool[i]->addrY;
			fb[i].bufCb = pfbpool[i]->addrCb;
			fb[i].bufCr = pfbpool[i]->addrCr;
			fb[i].bufMvCol = pfbpool[i]->mvColBuf;
	}

	if (dec_format == STD_AVC) {
		ret = IOGetPhyMem(&slice_mem_desc);
			if (ret) {
				printf("Unable to obtain physical slice save mem,LINE:%d\n",__LINE__);
				return -1;
			}

		bufinfo.avcSliceBufInfo.bufferBase = slice_mem_desc.phy_addr;
		bufinfo.avcSliceBufInfo.bufferSize = slice_mem_desc.size;
	}

	bufinfo.maxDecFrmInfo.maxMbX = src->width / 16;
	bufinfo.maxDecFrmInfo.maxMbY = src->height / 16;
	bufinfo.maxDecFrmInfo.maxMbNum = src->width * src->height / 256;

	ret = vpu_DecRegisterFrameBuffer(handle, fb, regfbcount, src->width, &bufinfo);
	if (ret != RETCODE_SUCCESS) {
		printf("Register frame buffer failed, ret=%d,LINE:%d\n", ret,__LINE__);
		return -1;
	}
	
	return 0;

}

int VpuDec::get_fbpool_info(fbuffer_info* info)
{
		for(int i=0;i < regfbcount;i++)
		{
			info->addr[i]= dec_pfbpool[i]->desc.phy_addr;
		}
		info->length = dec_pfbpool[0]->desc.size;
		info->num = regfbcount;
		return 0;
}

CVideoDec::CVideoDec(videoParms * parms)
{

}

CVideoDec::~CVideoDec()
{

}


