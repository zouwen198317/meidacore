#ifndef _BMP_FILE_H_
#define _BMP_FILE_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*位图头文件数据结构*/
typedef struct tagBITMAPFILEHEADER {
  unsigned char  bfType[2];   //位图文件类型，必须为'B' 'M'
  unsigned long  bfSize;   //位图文件大小（以字节为单位）
  unsigned short bfReserved1;  //位图文件保留字，必须为0
  unsigned short bfReserved2;  //位图文件保留字，必须为0
  unsigned long  bfOffBits;   //位图数据的起始位置，以相对位图文件头的偏移量（单位字节）
}__attribute__((packed)) BITMAPFILEHEADER ;  //共14Byte
/*位图信息数据结构*/
typedef struct tagBITMAPINFOHEADER{
  unsigned long  biSize;          //本结构所占用字节数
  long           biWidth;             //位图宽度，以像素为单位
  long           biHeight;             //位图高度，以像素为单位
  unsigned short biPlanes;      //目标设备级别，必须为1
  unsigned short biBitCount;    //每个像素所需的位数，1（双色），4（16色），8（256色），24（真彩色）
  unsigned long  biCompression;  //位图的压缩类型，必须是0（示不压缩），1(BI_RLE8压缩类型)，2(BI_RLE4压缩类型)之一
  unsigned long  biSizeImage;   //位图大小以字节为单位
  long           biXPixPerMeter;    //图像水平分辨率，每米像素数
  long           biYPixPerMeter;   //图像垂直分辨率，每米像素数
  unsigned long  biClrUsed;      //位图实际使用的颜色表中的颜色数
  unsigned long  biClrImporant;   //位图显示过程中重要的颜色数
}__attribute__((packed)) BITMAPINFOHEADER;  //共40Byte

class BmpFile
{
public:
	BmpFile(char *name, int width, int height, int rgbBits);
	~BmpFile();
	int write(char *buf, int size);
private:
	 FILE *mFp;

	BITMAPFILEHEADER mFileHeader;
	BITMAPINFOHEADER mInfoHeader;
};

#endif
