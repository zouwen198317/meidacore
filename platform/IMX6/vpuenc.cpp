#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vpuenc.h"
#include "vpu_jpegtable.h"
#include "log/log.h"
VpuEnc::VpuEnc(videoParms* parms):CVideoEnc(parms)
{
	int ret = -1;
	memset(&mem_desc,0,sizeof(mem_desc));
	memset(&cmd,0,sizeof(cmd));

	enc = (struct encode_ *)calloc(1, sizeof(struct encode_));
	if (enc == NULL)
		log_print(_ERROR,"VpuEnc Failed to allocate encode structure\n");
	
	frame_num =0;
//	mSkipping = false;
	ret = set_EncOpenParam(parms);
	ret = set_EncInitialInfo();
	ret = calloc_FrameBuff();
	
	if(ret<0)
		log_print(_ERROR,"VpuEnc Failed to Init VPU\n");
	else
		log_print(_DEBUG,"VpuEnc::VpuEnc Create VPU\n");
		
}

VpuEnc::~VpuEnc()
{
	free_Vpu_All_Space();

	if (enc) 
		free(enc);

}

int VpuEnc::config(videoParms* parms)
{
	int ret = -1;
	frame_num =0;

	free_Vpu_All_Space();
	memset(enc,0,sizeof(struct encode_));
	memset(&mem_desc,0,sizeof(mem_desc));
	memset(&cmd,0,sizeof(cmd));
	ret = set_EncOpenParam(parms);
	ret = set_EncInitialInfo();
	ret = calloc_FrameBuff();
 
	return ret;
}

int VpuEnc::encode(ImgBlk* src, VencBlk* dest)//retuen -1, frame type 0, 1 for I frame
{
	return encode(src,dest,false);	
}

int VpuEnc::encode(ImgBlk* src, VencBlk* dest, bool keyframe)//force to encode I frame
{
    //log_print(_DEBUG,"VpuEnc::encode height %d width %d pts %llu \n", src->height ,  src->width ,src->pts);
    if(src->height == enc->src_picheight && src->width == enc->src_picwidth){
		EncParam  enc_param = {0};
		EncOpenParam encop = {0};
		EncOutputInfo outinfo = {0};
		EncHeaderParam enchdr_param = {0};
		int src_fbid = enc->src_fbid;
		int ret = -1;
		int img_size, frame_id = 0;
		int src_scheme = enc->cmdl->src_scheme;
		int count = enc->cmdl->count;
		int loop_id = 0;
		PhysicalAddress phy_bsbuf_start = enc->phy_bsbuf_addr;
		u32 virt_bsbuf_start = enc->virt_bsbuf_addr;
		u32 virt_bsbuf_end = virt_bsbuf_start + STREAM_BUF_SIZE;
		int is_waited_int = 0;

		if ((cpu_is_mx6x() && (enc->cmdl->format == (int)CODEC_ID_H264))) {
			if(frame_num == 0 || keyframe || frame_num >= enc->cmdl->gop){
				if(frame_num >= enc->cmdl->gop) frame_num = 0;
				//ret = encoder_fill_headers(enc);
			//	if(!mSkipping) {
					enchdr_param.headerType = SPS_RBSP;
					vpu_EncGiveCommand(enc->handle, ENC_PUT_AVC_HEADER, &enchdr_param);

					enchdr_param.headerType = PPS_RBSP;
					vpu_EncGiveCommand(enc->handle, ENC_PUT_AVC_HEADER, &enchdr_param);

			//	}
			}

		}/*else if (enc->cmdl->format == (int)CODEC_ID_MJPEG) {
			if (cpu_is_mx6x()) {
				EncParamSet enchdr_param = {0};
				enchdr_param.size = STREAM_BUF_SIZE;
				enchdr_param.pParaSet = malloc(STREAM_BUF_SIZE);
				if (enchdr_param.pParaSet) {
					vpu_EncGiveCommand(enc->handle,ENC_GET_JPEG_HEADER, &enchdr_param);
					//vpu_write(enc->cmdl, (void *)enchdr_param.pParaSet, enchdr_param.size);
					write(enc->cmdl->dst_fd, (void *)enchdr_param.pParaSet, enchdr_param.size);
					free(enchdr_param.pParaSet);
				} else {
					printf("memory allocate failure\n");
					return -1;
				}
			}
		}*/

		enc_param.sourceFrame = &enc->fb[src_fbid];
		enc_param.quantParam = 23;
		enc_param.skipPicture = 0;
		enc_param.enableAutoSkip = 1;
		enc_param.encLeftOffset = 0;
		enc_param.encTopOffset = 0;

		if(keyframe)
		{
			enc_param.forceIPicture = 1;
			frame_num = 0;//Synchronous generation I frame and PPS & SPS parameter set
		}
		else
			enc_param.forceIPicture = 0;

		img_size = enc->src_picwidth * enc->src_picheight * 3 / 2;

		/* The main encoding */

		ret = set_EncSrc_addr(&enc->fbpool[src_fbid],src->width, src->height, src,enc->fb,enc->src_fbid);
		if(ret < 0) 
			log_print(_ERROR,"VpuEnc::encode set_EncSrc_addr fail\n");

		ret = vpu_EncStartOneFrame(enc->handle, &enc_param);
		if (ret != RETCODE_SUCCESS) {
			log_print(_ERROR,"VpuEnc::encode vpu_EncStartOneFrame failed Err code:%d\n",ret);
			return -1;
		}

		while (vpu_IsBusy()) {
			if (loop_id == 20) {
				ret = vpu_SWReset(enc->handle, 0);
				log_print(_ERROR,"VpuEnc::encode vpu_SWReset failed Err code: %d\n",ret);
				return -1;
			}

			vpu_WaitForInt(200);
			loop_id ++;
		}

		ret = vpu_EncGetOutputInfo(enc->handle, &outinfo);
		if (ret != RETCODE_SUCCESS) {
			log_print(_ERROR,"VpuEnc::encode vpu_EncGetOutputInfo failed Err code: %d\n",ret);
			return -1;
		}

		if (outinfo.skipEncoded)
			log_print(_ERROR,"VpuEnc::encode Skip encoding one Frame!\n");
		
		frame_num++;
		
		set_vencBlk_addr(phy_bsbuf_start,dest);
							
		dest->codec = enc->cmdl->format;//src->fourcc;
		dest->height = src->height;
		dest->width = src->width;
		dest->pts = src->pts;

		dest->buf.vStartaddr = Enc_bufZone.vStartaddr; //virtual address
		dest->buf.pStartAddr = Enc_bufZone.pStartAddr ; //physical address
		dest->buf.bufSize = Enc_bufZone.bufSize;
		dest->buf.isRingBuf = Enc_bufZone.isRingBuf;
		dest->frameType = outinfo.picType;
		//log_print(_DEBUG,"VpuEnc::encode  encoding one Frame Finish!\n");
		
		return dest->frameType;
	}else{
	 		log_print(_ERROR,"VpuEnc::encode:src format is not match\n");
		return -1;
	 }
}

bufZone* VpuEnc::allocHwBuffer(u32 size)//for encode data output, ring buffer
{
	int ret = 0;
    	log_print(_DEBUG,"VpuEnc::allocHwBuffer size %d\n",size);

	Enc_bufZone.bufSize = mem_desc.size = STREAM_BUF_SIZE;
	ret = IOGetPhyMem(&mem_desc);
	if (ret) {
		log_print(_ERROR,"VpuEnc::allocHwBuffer Unable to obtain physical memory\n");
		return NULL;
	}
	Enc_bufZone.pStartAddr = mem_desc.phy_addr;
	enc->phy_bsbuf_addr = Enc_bufZone.pStartAddr;

	/* mmap that physical buffer */
	enc->virt_bsbuf_addr = IOGetVirtMem(&mem_desc);
	Enc_bufZone.vStartaddr =(void*) (enc->virt_bsbuf_addr);
	if (enc->virt_bsbuf_addr <= 0) {
		log_print(_ERROR,"VpuEnc::allocHwBuffer Unable to map physical memory\n");
		return NULL;
	}
	
	if(enc->ringBufferEnable == 1)
		Enc_bufZone.isRingBuf = 1;
	else 	
		Enc_bufZone.isRingBuf = 0;

	return &Enc_bufZone;
}

void VpuEnc::VpuInit()
{
	int err=0;
	vpu_versioninfo ver;
	memset(&ver,0,sizeof(ver));

	err = vpu_Init(NULL);
	if (err) 
		log_print(_ERROR,"VPU Init Failure.\n");
	
	err = vpu_GetVersionInfo(&ver);
	if (err) {
		log_print(_ERROR,"Cannot get version info, err:%d\n", err);
		vpu_UnInit();
	}
		
	log_print(_INFO,"VPU firmware version: %d.%d.%d_r%d\n", ver.fw_major, ver.fw_minor,ver.fw_release, ver.fw_code);
	log_print(_INFO,"VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor,ver.lib_release);
}

void VpuEnc::VpuUnInit()
{

	vpu_UnInit();
}

void VpuEnc::freeHwBuffer(bufZone* zone)
{
	IOFreeVirtMem(&mem_desc);
	IOFreePhyMem(&mem_desc);
}

int VpuEnc::set_vencBlk_addr(u32 bs_pa_startaddr,VencBlk * dest)
{
	RetCode ret;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	u32 size;

	ret = vpu_EncGetBitstreamBuffer(enc->handle, &pa_read_ptr, &pa_write_ptr,
					(Uint32 *)&size);
	if (ret != RETCODE_SUCCESS) {
		log_print(_ERROR,"VpuEnc::set_vencBlk_addr EncGetBitstreamBuffer failed\n");
		return -1;
	}

	dest->offset = pa_read_ptr - bs_pa_startaddr; // the offset of frame in bufZone.pStartAddr
	dest->frameSize = size;

	//printf("pa_read_ptr:%x, pa_write_ptr:%x,size:%d,dest->offset:%x\n",pa_read_ptr, pa_write_ptr,size,dest->offset);

		ret = vpu_EncUpdateBitstreamBuffer(enc->handle, size);
		if (ret != RETCODE_SUCCESS) {
			log_print(_ERROR,"VpuEnc::set_vencBlk_addr EncUpdateBitstreamBuffer failed\n");
			return -1;
		}

	return size;
}

int VpuEnc::set_EncSrc_addr(struct frame_buf *fb, int strideY, int height,ImgBlk* src,FrameBuffer *fb_addr,int src_fbid)
{
	//strideY = (enc->src_picwidth + 15) & ~15;
	//height = (enc->src_picheight + 15) & ~15;

	if (fb == NULL)
		return -1;

	memset(&(fb->desc), 0, sizeof(vpu_mem_desc));
	fb->desc.size = (strideY * height * 3 / 2);
	
	fb->desc.phy_addr=src->paddr;
	fb->desc.virt_uaddr = (u32)src->vaddr;

	fb->addrY = fb->desc.phy_addr;
	fb->addrCb = fb->addrY + strideY * height;
	fb->addrCr = fb->addrCb + strideY * height / 4;
	fb->strideY = strideY;
	fb->strideC =  strideY / 2;

	fb_addr[src_fbid].myIndex = src_fbid;
	fb_addr[src_fbid].bufY = fb->addrY;
	fb_addr[src_fbid].bufCb = fb->addrCb;
	fb_addr[src_fbid].bufCr = fb->addrCr;
	fb_addr[src_fbid].strideY = fb->strideY;
	fb_addr[src_fbid].strideC = fb->strideC;

	return 0;
}

int VpuEnc::set_EncOpenParam(videoParms* parms)
{
	int ret = -1;
	EncOpenParam encop = {0};

//********************start EncOpenParam*******************
	enc->cmdl = &cmd;
	enc->cmdl->format = parms->codec;
	enc->cmdl->mapType = 0;
	enc->ringBufferEnable = 1;
	enc->cmdl->chromaInterleave = 0;
	enc->src_picwidth = enc->cmdl->width = parms->width;
	enc->src_picheight = enc->cmdl->height = parms->height;
	enc->enc_picwidth = enc->src_picwidth;
	enc->enc_picheight = enc->src_picheight;
	enc->cmdl->fps = parms->fps;
	
	allocHwBuffer(STREAM_BUF_SIZE);	

	encop.bitstreamBuffer = enc->phy_bsbuf_addr;
	encop.bitstreamBufferSize = Enc_bufZone.bufSize;
	switch(enc->cmdl->format){
		case CODEC_ID_H264:
		log_print(_DEBUG,"VpuEnc::set_EncOpenParam VPU Code CODEC_ID_H264 STD_AVC %d\n",STD_AVC);
		encop.bitstreamFormat = STD_AVC;
		break;
		case CODEC_ID_MJPEG:
		log_print(_DEBUG,"VpuEnc::set_EncOpenParam VPU Code CODEC_ID_MJPEG STD_MJPG\n");
		encop.bitstreamFormat = STD_MJPG;
		break;
		default:
			log_print(_DEBUG,"VpuEnc::set_EncOpenParam VPU Code CODEC_ID_MJPEG STD_MJPG\n");
			encop.bitstreamFormat = STD_MJPG;
			break;
	}
	
	encop.mapType = enc->cmdl->mapType;
	encop.linear2TiledEnable = enc->linear2TiledEnable;
	encop.picWidth = enc->enc_picwidth;
	encop.picHeight = enc->enc_picheight;

	/*Note: Frame rate cannot be less than 15fps per H.263 spec */
	encop.frameRateInfo = enc->cmdl->fps;
	encop.bitRate = enc->cmdl->bitrate = parms->bitRate;
	encop.gopSize = enc->cmdl->gop = parms->keyFrameInterval;
	encop.slicemode.sliceMode = 0;	/* 0: 1 slice per picture; 1: Multiple slices per picture */
	encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
	encop.slicemode.sliceSize = 4000;  /* Size of a slice in bits or MB numbers */

	encop.initialDelay = 0;
	encop.vbvBufferSize = 0;        /* 0 = ignore 8 */
	encop.intraRefresh = 0;
	encop.sliceReport = 0;
	encop.mbReport = 0;
	encop.mbQpReport = 0;
	encop.rcIntraQp = -1;
	encop.userQpMax = 0;
	encop.userQpMin = 0;
	encop.userQpMinEnable = 0;
	encop.userQpMaxEnable = 0;

	encop.IntraCostWeight = 0;
	encop.MEUseZeroPmv  = 0;
	/* (3: 16x16, 2:32x16, 1:64x32, 0:128x64, H.263(Short Header : always 3) */
	encop.MESearchRange = 3;

	encop.userGamma = (u32)(0.75*32768);         /*  (0*32768 <= gamma <= 1*32768) */
	encop.RcIntervalMode= 1;        /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
	encop.MbInterval = 0;
	encop.avcIntra16x16OnlyModeEnable = 0;

	encop.ringBufferEnable = enc->ringBufferEnable = 1;
	encop.dynamicAllocEnable = 0;
	encop.chromaInterleave = enc->cmdl->chromaInterleave;

	if(enc->cmdl->format == (int)CODEC_ID_H264) {
		encop.EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
		encop.EncStdParam.avcParam.avc_disableDeblk = 0;
		encop.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
		encop.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;
		encop.EncStdParam.avcParam.avc_chromaQpOffset = 10;
		encop.EncStdParam.avcParam.avc_audEnable = 0;
		if(cpu_is_mx6x()) {
			encop.EncStdParam.avcParam.interview_en = 0;
			encop.EncStdParam.avcParam.paraset_refresh_en = enc->mvc_paraset_refresh_en = 0;
			encop.EncStdParam.avcParam.prefix_nal_en = 0;
			encop.EncStdParam.avcParam.mvc_extension = enc->cmdl->mp4_h264Class;
			enc->mvc_extension = enc->cmdl->mp4_h264Class;
			encop.EncStdParam.avcParam.avc_frameCroppingFlag = 0;
			encop.EncStdParam.avcParam.avc_frameCropLeft = 0;
			encop.EncStdParam.avcParam.avc_frameCropRight = 0;
			encop.EncStdParam.avcParam.avc_frameCropTop = 0;
			encop.EncStdParam.avcParam.avc_frameCropBottom = 0;
		}
	}else if (enc->cmdl->format == (int)CODEC_ID_MJPEG) {
		encop.EncStdParam.mjpgParam.mjpg_sourceFormat = enc->mjpg_fmt; /* encConfig.mjpgChromaFormat */
		encop.EncStdParam.mjpgParam.mjpg_restartInterval = 60;
		encop.EncStdParam.mjpgParam.mjpg_thumbNailEnable = 0;
		encop.EncStdParam.mjpgParam.mjpg_thumbNailWidth = 0;

		encop.EncStdParam.mjpgParam.mjpg_thumbNailHeight = 0;
		if (cpu_is_mx6x()) {
			copy_JPEG_table(&encop.EncStdParam.mjpgParam);

		}
		
	}
	log_print(_DEBUG,"VpuEnc::set_EncOpenParam VpuEnc Param format 0x%x, bitstreamFormat %d, %d x %d, fps %d, bitrate %d,gop %d\n",
		enc->cmdl->format,encop.bitstreamFormat  ,encop.picWidth ,encop.picHeight,encop.frameRateInfo,encop.bitRate,encop.gopSize  );
	
	ret = vpu_EncOpen(&enc->handle, &encop);
	if (ret != RETCODE_SUCCESS) {
		log_print(_ERROR,"VpuEnc::set_EncOpenParam Encoder open failed %d\n", ret);
		return -1;
		
	}
//********************end EncOpenParam*******************

	return 0;
}

int VpuEnc::set_EncInitialInfo()
{
	RetCode ret;
	int intraRefreshMode = 1;
	EncInitialInfo initinfo = {0};

//*******************start EncInitialInfo*******************	
	vpu_EncGiveCommand(enc->handle, ENC_SET_INTRA_REFRESH_MODE, &intraRefreshMode);

	ret = vpu_EncGetInitialInfo(enc->handle, &initinfo);
	if (ret != RETCODE_SUCCESS) {
		log_print(_ERROR,"VpuEnc::set_EncInitialInfo Encoder GetInitialInfo failed\n");
		return -1;
	}

	enc->minFrameBufferCount = initinfo.minFrameBufferCount;
	enc->mbInfo.enable = 0;
	enc->mvInfo.enable = 0;
	enc->sliceInfo.enable = 0;
//*******************end EncInitialInfo*******************
	
	return 0;
}

int VpuEnc::calloc_FrameBuff()
{
	int i, enc_stride, src_stride, src_fbid;
	int enc_fbwidth, enc_fbheight, src_fbwidth, src_fbheight;
	RetCode ret;
	FrameBuffer *fb;
	PhysicalAddress subSampBaseA = 0, subSampBaseB = 0;
	struct frame_buf **pfbpool;
	EncExtBufInfo extbufinfo = {0};
	int totalfb, minfbcount, srcfbcount, extrafbcount;

//******************calloc FrameBuff**********************
	minfbcount = enc->minFrameBufferCount;
	srcfbcount = 1;

	enc_fbwidth = (enc->enc_picwidth + 15) & ~15;
	enc_fbheight = (enc->enc_picheight + 15) & ~15;
	src_fbwidth = (enc->src_picwidth + 15) & ~15;
	src_fbheight = (enc->src_picheight + 15) & ~15;

	if (cpu_is_mx6x()) {
		if (enc->cmdl->format == (int)CODEC_ID_H264 && enc->mvc_extension) /* MVC */
			extrafbcount = 2 + 2; /* Subsamp [2] + Subsamp MVC [2] */
		else if (enc->cmdl->format == (int)CODEC_ID_MJPEG)
			extrafbcount = 0;
		else
			extrafbcount = 2; /* Subsamp buffer [2] */
	} else
		extrafbcount = 0;

	enc->totalfb = totalfb = minfbcount + extrafbcount + srcfbcount;
	log_print(_DEBUG,"VpuEnc::calloc_FrameBuff enc->totalfb:%d,enc->minFrameBufferCount:%d\n",enc->totalfb,enc->minFrameBufferCount);
	/* last framebuffer is used as src frame in the test */
	enc->src_fbid = src_fbid = totalfb - 1;
	
	fb = enc->fb = (FrameBuffer *)calloc(totalfb, sizeof(FrameBuffer));
	if (fb == NULL) {
		log_print(_ERROR,"VpuEnc::calloc_FrameBuff Failed to allocate enc->fb\n");
		return -1;
	}

	pfbpool = enc->pfbpool = (frame_buf **)calloc(totalfb,sizeof(struct frame_buf *));
	if (pfbpool == NULL) {
		log_print(_ERROR,"VpuEnc::calloc_FrameBuff Failed to allocate enc->pfbpool\n");
		free(enc->fb);
		enc->fb = NULL;
		return -1;
	}

	if (enc->cmdl->mapType == LINEAR_FRAME_MAP) {
		/* All buffers are linear */
		for (i = 0; i < minfbcount + extrafbcount; i++) {
			pfbpool[i] = framebuf_alloc(&enc->fbpool[i], enc->cmdl->format, enc->mjpg_fmt,
						    enc_fbwidth, enc_fbheight, 0);
			if (pfbpool[i] == NULL) {
				log_print(_ERROR,"VpuEnc::calloc_FrameBuff pfbpool[i] == NULL\n");
				goto err1;
			}
		}
	}

	for (i = 0; i < minfbcount + extrafbcount; i++) {
		fb[i].myIndex = i;
		fb[i].bufY = pfbpool[i]->addrY;
		fb[i].bufCb = pfbpool[i]->addrCb;
		fb[i].bufCr = pfbpool[i]->addrCr;
		fb[i].strideY = pfbpool[i]->strideY;
		fb[i].strideC = pfbpool[i]->strideC;
	}

	if (cpu_is_mx6x() && (enc->cmdl->format != (int)CODEC_ID_MJPEG)) {
		subSampBaseA = fb[minfbcount].bufY;
		subSampBaseB = fb[minfbcount + 1].bufY;
	}

	enc_stride = (enc->enc_picwidth + 15) & ~15;
	src_stride = (enc->src_picwidth + 15 ) & ~15;

	extbufinfo.scratchBuf = enc->scratchBuf;
	ret = vpu_EncRegisterFrameBuffer(enc->handle, fb, minfbcount, enc_stride, src_stride,subSampBaseA, subSampBaseB, &extbufinfo);
	if (ret != RETCODE_SUCCESS) {
		log_print(_ERROR,"VpuEnc::calloc_FrameBuff Register frame buffer failed\n");
		goto err1;
	}

	enc->pfbpool[src_fbid]=&enc->fbpool[src_fbid];

	return 0;
		
//*************end FrameBuff********************

err1:
	for (i = 0; i < totalfb; i++) {
		framebuf_free(pfbpool[i]);
	}

	free(enc->fb);
	free(enc->pfbpool);
	enc->fb = NULL;
	enc->pfbpool = NULL;
	return -1;

}

void VpuEnc::free_Vpu_All_Space()
{
	//free frameBuff
	int i;
	if (enc->pfbpool) {
		for (i = 0; i < enc->totalfb-1; i++) {
			framebuf_free(enc->pfbpool[i]);
		}
	}

	if (enc->fb) {
		free(enc->fb);
		enc->fb = NULL;
	}
	if (enc->pfbpool) {
		free(enc->pfbpool);
		enc->pfbpool = NULL;
	}

	//CloseEnc
	RetCode ret;
	ret = vpu_EncClose(enc->handle);
	if (ret == RETCODE_FRAME_NOT_COMPLETE) {
		vpu_SWReset(enc->handle, 0);
		vpu_EncClose(enc->handle);
	}
	if(ret==RETCODE_SUCCESS)
	log_print(_DEBUG," VpuEnc::free_Vpu_All_Space close Enc success\n");
	
	//free EncBuf
	freeHwBuffer(&Enc_bufZone);
	
}


void VpuEnc::copy_JPEG_table(EncMjpgParam *param)
{
	//void jpgGetHuffTable(EncMjpgParam *param)
	int i=0;
	/* Rearrange and insert pre-defined Huffman table to deticated variable. */
	memcpy(param->huffBits[DC_TABLE_INDEX0], lumaDcBits, 16);   /* Luma DC BitLength */
	memcpy(param->huffVal[DC_TABLE_INDEX0], lumaDcValue, 16);   /* Luma DC HuffValue */

	memcpy(param->huffBits[AC_TABLE_INDEX0], lumaAcBits, 16);   /* Luma DC BitLength */
	memcpy(param->huffVal[AC_TABLE_INDEX0], lumaAcValue, 162);  /* Luma DC HuffValue */

	memcpy(param->huffBits[DC_TABLE_INDEX1], chromaDcBits, 16); /* Chroma DC BitLength */
	memcpy(param->huffVal[DC_TABLE_INDEX1], chromaDcValue, 16); /* Chroma DC HuffValue */

	memcpy(param->huffBits[AC_TABLE_INDEX1], chromaAcBits, 16); /* Chroma AC BitLength */
	memcpy(param->huffVal[AC_TABLE_INDEX1], chromaAcValue, 162); /* Chorma AC HuffValue */


	//void jpgGetQMatrix(EncMjpgParam *param)

	/* Rearrange and insert pre-defined Q-matrix to deticated variable. */
	memcpy(param->qMatTab[DC_TABLE_INDEX0], lumaQ2, 64);
	memcpy(param->qMatTab[AC_TABLE_INDEX0], chromaBQ2, 64);

	memcpy(param->qMatTab[DC_TABLE_INDEX1], param->qMatTab[DC_TABLE_INDEX0], 64);
	memcpy(param->qMatTab[AC_TABLE_INDEX1], param->qMatTab[AC_TABLE_INDEX0], 64);


	//void jpgGetCInfoTable(EncMjpgParam *param)

	int format = param->mjpg_sourceFormat;
	memcpy(param->cInfoTab, cInfoTable[format], 6 * 4);

}



