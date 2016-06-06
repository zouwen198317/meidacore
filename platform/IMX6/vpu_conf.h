/*
 * Copyright 2004-2013 Freescale Semiconductor, Inc.
 *
 * Copyright (c) 2006, Chips & Media.  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _VPU_CONF_H
#define _VPU_CONF_H

#include "vpu_lib.h"
#include "vpu_io.h"

#undef u32
#undef u16
#undef u8
#undef s32
#undef s16
#undef s8
#undef u64
typedef unsigned long long u64;
typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long s32;
typedef short s16;
typedef char s8;

#define STREAM_BUF_SIZE		0xFFFC00

enum {
    MODE420 = 0,
    MODE422 = 1,
    MODE224 = 2,
    MODE444 = 3,
    MODE400 = 4
};

struct frame_buf {
	int addrY;
	int addrCb;
	int addrCr;
	int strideY;
	int strideC;
	int mvColBuf;
	vpu_mem_desc desc;
};

#define MAX_BUF_NUM	32

#define MAX_PATH	256
struct cmd_line {
	char input[MAX_PATH];	/* Input file name */
	char output[MAX_PATH];  /* Output file name */
	int src_scheme;
	int dst_scheme;
	int video_node;
	int video_node_capture;
	int src_fd;
	int dst_fd;
	u32 width;
	u32 height;
	u32 enc_width;
	u32 enc_height;
	int loff;
	int toff;
	u32 format;
	int deblock_en;
	int dering_en;
	int rot_en; /* Use VPU to do rotation */
	int ext_rot_en; /* Use IPU/GPU to do rotation */
	int rot_angle;
	int mirror;
	int chromaInterleave;
	int bitrate;
	int gop;
	int save_enc_hdr;
	int count;
	int prescan;
	int bs_mode;
	char *nbuf; /* network buffer */
	int nlen; /* remaining data in network buffer */
	int noffset; /* offset into network buffer */
	int seq_no; /* seq numbering to detect skipped frames */
	u16 port; /* udp port number */
	u16 complete; /* wait for the requested buf to be filled completely */
	int iframe;
	int mp4_h264Class;
	char vdi_motion;	/* VDI motion algorithm */
	int fps;
	int mapType;
	int quantParam;
};

struct encode_ {
	EncHandle handle;		/* Encoder handle */
	PhysicalAddress phy_bsbuf_addr; /* Physical bitstream buffer */
	u32 virt_bsbuf_addr;		/* Virtual bitstream buffer */
	int enc_picwidth;	/* Encoded Picture width */
	int enc_picheight;	/* Encoded Picture height */
	int src_picwidth;        /* Source Picture width */
	int src_picheight;       /* Source Picture height */
	int totalfb;	/* Total number of framebuffers allocated */
	int src_fbid;	/* Index of frame buffer that contains YUV image */
	FrameBuffer *fb; /* frame buffer base given to encoder */
	struct frame_buf **pfbpool; /* allocated fb pointers are stored here */
	ExtBufCfg scratchBuf;
	int mp4_dataPartitionEnable;
	int ringBufferEnable;
	int mjpg_fmt;
	int mvc_paraset_refresh_en;
	int mvc_extension;
	int linear2TiledEnable;
	int minFrameBufferCount;

        EncReportInfo mbInfo;
        EncReportInfo mvInfo;
        EncReportInfo sliceInfo;

	struct cmd_line *cmdl; /* command line */
	u8 * huffTable;
	u8 * qMatTable;

	struct frame_buf fbpool[MAX_BUF_NUM];
};

struct frame_buf *framebuf_alloc(struct frame_buf *fb, int stdMode, int format, int strideY, int height, int mvCol);
void framebuf_free(struct frame_buf *fb);

#endif
