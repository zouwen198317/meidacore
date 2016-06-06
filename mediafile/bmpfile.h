#ifndef _BMP_FILE_H_
#define _BMP_FILE_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*λͼͷ�ļ����ݽṹ*/
typedef struct tagBITMAPFILEHEADER {
  unsigned char  bfType[2];   //λͼ�ļ����ͣ�����Ϊ'B' 'M'
  unsigned long  bfSize;   //λͼ�ļ���С�����ֽ�Ϊ��λ��
  unsigned short bfReserved1;  //λͼ�ļ������֣�����Ϊ0
  unsigned short bfReserved2;  //λͼ�ļ������֣�����Ϊ0
  unsigned long  bfOffBits;   //λͼ���ݵ���ʼλ�ã������λͼ�ļ�ͷ��ƫ��������λ�ֽڣ�
}__attribute__((packed)) BITMAPFILEHEADER ;  //��14Byte
/*λͼ��Ϣ���ݽṹ*/
typedef struct tagBITMAPINFOHEADER{
  unsigned long  biSize;          //���ṹ��ռ���ֽ���
  long           biWidth;             //λͼ��ȣ�������Ϊ��λ
  long           biHeight;             //λͼ�߶ȣ�������Ϊ��λ
  unsigned short biPlanes;      //Ŀ���豸���𣬱���Ϊ1
  unsigned short biBitCount;    //ÿ�����������λ����1��˫ɫ����4��16ɫ����8��256ɫ����24�����ɫ��
  unsigned long  biCompression;  //λͼ��ѹ�����ͣ�������0��ʾ��ѹ������1(BI_RLE8ѹ������)��2(BI_RLE4ѹ������)֮һ
  unsigned long  biSizeImage;   //λͼ��С���ֽ�Ϊ��λ
  long           biXPixPerMeter;    //ͼ��ˮƽ�ֱ��ʣ�ÿ��������
  long           biYPixPerMeter;   //ͼ��ֱ�ֱ��ʣ�ÿ��������
  unsigned long  biClrUsed;      //λͼʵ��ʹ�õ���ɫ���е���ɫ��
  unsigned long  biClrImporant;   //λͼ��ʾ��������Ҫ����ɫ��
}__attribute__((packed)) BITMAPINFOHEADER;  //��40Byte

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
