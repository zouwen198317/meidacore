//extern unsigned char lumaQ2[64];
//extern unsigned char chromaBQ2[64];
#include "vpu_jpegtable.h"

#include "jpgfile.h"

#include <string.h>

static char s_default_dht[] = { 
  
         0xff,0xc4, /* DHT (Define Huffman Table) identifier */ 
         0x01,0xa2, /* section size: 0x01a2, from this two bytes to the end, inclusive */   
  
         0x00,                                                                                                                                  /* DC Luminance */ 
         0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* BITS         */ 
         0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,                                   /* HUFFVALS     */ 
  
         0x10,                                                                                                                                  /* AC Luminance */ 
         0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7d, /* BITS         */ 
         0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07, /* HUFFVALS     */ 
         0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0, 
         0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28, 
         0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49, 
         0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69, 
         0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89, 
         0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7, 
         0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5, 
         0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2, 
         0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
         0xf9,0xfa, 
  
         0x01,                                                                                                                                  /* DC Chrominance */ 
         0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00, /* BITS           */ 
         0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,                                   /* HUFFVALS       */ 
  
         0x11,                                                                                                                                  /* AC Chrominance */ 
         0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77, /* BITS           */ 
         0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71, /* HUFFVALS       */ 
         0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0, 
         0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26, 
         0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48, 
         0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68, 
         0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87, 
         0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5, 
         0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3, 
         0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda, 
         0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8, 
         0xf9,0xfa 
};

JpgFile::JpgFile(char *name, int width, int height)
{
	if((dst_fd = fopen(name, "ab+")) ==NULL)
		printf("Open %s fail\n",name);

	memset(&JPG_Header,0,sizeof(JPG_Header));
	
	/*图像开始标记*/
	JPG_Header.SOI.identifier = 0xFF;
	JPG_Header.SOI.type = 0xD8;

	/*应用程序保留标记*/
	JPG_Header.APP0.identifier = 0xFF;
	JPG_Header.APP0.type = 0xE0;
	JPG_Header.APP0_length_h = 0x00;
	JPG_Header.APP0_length_l = 0x10;
	memcpy(JPG_Header.APP0_identifies,"JFIF",strlen("JFIF"));//固定的字符串"JFIF"
	JPG_Header.APP0_version_h = 0x01;
	JPG_Header.APP0_version_l = 0x01; //版本号 
	JPG_Header.APP0_units = 0x01;//像素单位pixel/inch
	JPG_Header.APP0_Xdensity_h = 0x00;
	JPG_Header.APP0_Xdensity_l = 0x5A;//水平像素
	JPG_Header.APP0_YDensity_h = 0x00;
	JPG_Header.APP0_YDensity_l = 0x5A;//垂直像素 
	JPG_Header.APP0_Xthumbnail = 0x00; 
	JPG_Header.APP0_Ythunbnail = 0x00;//缩略图像素
	
	/*定义差分编码累计复位的间隔*/	
	JPG_Header.DRI.identifier = 0xFF;
	JPG_Header.DRI.type = 0xDD;
	JPG_Header.DRI_length_h = 0x00;
	JPG_Header.DRI_length_l = 0x04;
	JPG_Header.DRI_interval_h = 0x00;
	JPG_Header.DRI_interval_l = 0x3C;

	/*量化表*/
	JPG_Header.luma_DQT.identifier = 0xFF;
	JPG_Header.luma_DQT.type = 0xDB;
	JPG_Header.luma_DQT_length_h = (sizeof(lumaQ2)+3) >> 8;
	JPG_Header.luma_DQT_length_l = (sizeof(lumaQ2)+3) & 0xFF;
	JPG_Header.luma_DQT_table_id = 0x00;
	memcpy(JPG_Header.luma_DQT_table,lumaQ2,sizeof(lumaQ2));

	JPG_Header.chroma_DQT.identifier = 0xFF;
	JPG_Header.chroma_DQT.type = 0xDB;
	JPG_Header.chroma_DQT_length_h = (sizeof(chromaBQ2)+3) >> 8;
	JPG_Header.chroma_DQT_length_l = (sizeof(chromaBQ2)+3) & 0xFF;
	JPG_Header.chroma_DQT_table_id = 0x01;
	memcpy(JPG_Header.chroma_DQT_table,chromaBQ2,sizeof(chromaBQ2));

	/*霍夫曼表*/
	memcpy(JPG_Header.DHT,s_default_dht,sizeof(s_default_dht));

	/*帧图像开始*/
	JPG_Header.SOF.identifier = 0xFF;
	JPG_Header.SOF.type = 0xC0;
	JPG_Header.SOF_length_h = 0x00;
	JPG_Header.SOF_length_l = 0x11 ;
	JPG_Header.SOF_precision = 0x08;//每个颜色分量每个像素的位数
	JPG_Header.SOF_height_h = height >> 8;
	JPG_Header.SOF_height_l = height & 0xFF;
	JPG_Header.SOF_width_h = width >> 8;
	JPG_Header.SOF_width_l = width & 0xFF;
	JPG_Header.SOF_color_components = 0x3;
	JPG_Header.SOF_Y = 0x01;
	JPG_Header.SOF_Y_sample = 0x22;//样本因子
	JPG_Header.SOF_Y_DQT_table_id = 0x00;
	JPG_Header.SOF_Cr = 0x02;
	JPG_Header.SOF_Cr_sample = 0x11;
	JPG_Header.SOF_Cr_DQT_table_id = 0x01;
	JPG_Header.SOF_Cb = 0x03;
	JPG_Header.SOF_Cb_sample = 0x11;
	JPG_Header.SOF_Cb_DQT_table_id = 0x01;

	if(dst_fd)
		fwrite((unsigned char *)&JPG_Header,1,sizeof(JPG_Header),dst_fd);
		//printf("sizeof JPG_Header:%d\n",sizeof(JPG_Header));
}

JpgFile::~JpgFile()
{
	if(dst_fd)
		fclose(dst_fd);
}

int JpgFile::write(char *buf , int size)
{
	int ret = -1,i = -1;
	char tmp_str=0x00;
	char EOI[2] = {0xFF,0XD9};

	if(dst_fd){
		ret = fwrite(buf,1,size,dst_fd);
		/*if SOE not exist add SOE mark*/
		do{	
			//printf("tmp_str:%x,i:%d\n",tmp_str,i);
			if(tmp_str == 0x00){
				fseek(dst_fd,i,SEEK_END);
				fread(&tmp_str,1,1,dst_fd);
				i--;
			}else{
				if(tmp_str == 0xD9){
					fseek(dst_fd,i,SEEK_END);
					fread(&tmp_str,1,1,dst_fd);
					if(tmp_str == 0xFF)
						break;
				}
					fseek(dst_fd,i+2,SEEK_END);
					fwrite(EOI,1,sizeof(EOI),dst_fd);
					break;
			
			}

		}while(1);

		return ret;	
	}

	
}


