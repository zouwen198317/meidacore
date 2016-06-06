#ifndef _JPG_FILE_H_
#define _JPG_FILE_H_

#include <stdio.h>

struct Segment_type{
	unsigned char identifier;
	unsigned char type;	
};


struct _JPG_Header{

	struct Segment_type SOI;
	struct Segment_type APP0;
	unsigned char APP0_length_h;
	unsigned char APP0_length_l;
	unsigned char APP0_identifies[5];
	unsigned char APP0_version_h;
	unsigned char APP0_version_l; 
	unsigned char APP0_units;
	unsigned char APP0_Xdensity_h;
	unsigned char APP0_Xdensity_l;
	unsigned char APP0_YDensity_h;
	unsigned char APP0_YDensity_l; 
	unsigned char APP0_Xthumbnail; 
	unsigned char APP0_Ythunbnail;
		
	struct Segment_type DRI;
	unsigned char DRI_length_h;
	unsigned char DRI_length_l;
	unsigned char DRI_interval_h;
	unsigned char DRI_interval_l;

	struct Segment_type luma_DQT;
	unsigned char luma_DQT_length_h;
	unsigned char luma_DQT_length_l;
	unsigned char luma_DQT_table_id;
	unsigned char luma_DQT_table[64];

	struct Segment_type chroma_DQT;
	unsigned char chroma_DQT_length_h;
	unsigned char chroma_DQT_length_l;
	unsigned char chroma_DQT_table_id;
	unsigned char chroma_DQT_table[64];
	unsigned char DHT[420];

	struct Segment_type SOF;
	unsigned char SOF_length_h;
	unsigned char SOF_length_l;
	unsigned char SOF_precision;
	unsigned char SOF_height_h;
	unsigned char SOF_height_l;
	unsigned char SOF_width_h;
	unsigned char SOF_width_l;
	unsigned char SOF_color_components;
	unsigned char SOF_Y;
	unsigned char SOF_Y_sample;
	unsigned char SOF_Y_DQT_table_id;
	unsigned char SOF_Cr;
	unsigned char SOF_Cr_sample;
	unsigned char SOF_Cr_DQT_table_id;
	unsigned char SOF_Cb;
	unsigned char SOF_Cb_sample;
	unsigned char SOF_Cb_DQT_table_id;
		
};

class JpgFile
{
public:
	JpgFile(char *name, int width, int height);
	~JpgFile();
	int write(char *buf , int size);

private:
	FILE * dst_fd;
	struct _JPG_Header JPG_Header;
	
};

#endif

