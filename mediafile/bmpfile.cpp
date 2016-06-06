#include "bmpfile.h"

BmpFile::BmpFile(char *name, int width, int height, int rgbBits)
{

	if((mFp= fopen(name, "wb")) ==NULL){
		printf("BmpFile file %s cannot open\n",name);
	}

	/*����λͼͷ�ļ����ݽṹ*/
	mFileHeader.bfType[0] = 'B';
	mFileHeader.bfType[1] = 'M';
	mFileHeader.bfReserved1 = 0x0000;
	mFileHeader.bfReserved2 = 0x0000;
	//λͼ��ʼλ��
	mFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	if(rgbBits == 16)
	{
		mFileHeader.bfOffBits = mFileHeader.bfOffBits + 12;
	}

	/*����λͼ��Ϣ���ݽṹ*/
	mInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	mInfoHeader.biWidth = (long)(width); 
	mInfoHeader.biHeight = (long)(height); 
	mInfoHeader.biPlanes = 0x0001;
	mInfoHeader.biBitCount = rgbBits;//0x0018;  //24λͼ
	mInfoHeader.biCompression = 0x0000;  //��ѹ��
	if(rgbBits == 16)
	{
		mInfoHeader.biCompression = 0x0003;
	}

	/*ÿ������RGB24λ���ɣ���3�ֽ�*/
	mInfoHeader.biSizeImage = width*height * rgbBits/8;
	mInfoHeader.biXPixPerMeter = 0;  /*96dpi*/
	mInfoHeader.biYPixPerMeter = 0;  /*96dpi*/
	mInfoHeader.biClrUsed = 0;   
	mInfoHeader.biClrImporant = 0;  

	/*λͼ�ļ���С*/
	mFileHeader.bfSize = mFileHeader.bfOffBits + mInfoHeader.biSizeImage;

	/*λͼͷ�ļ��ṹд���ļ�*/
	if(mFp)   
		fwrite((unsigned char *)&mFileHeader, sizeof(BITMAPFILEHEADER), 1, mFp);
	/*λͼ��Ϣ�ṹд���ļ�*/
	if(mFp)   
		fwrite((unsigned char *)&mInfoHeader, sizeof(BITMAPINFOHEADER), 1, mFp);
	if(rgbBits == 16)
	{
		unsigned int rgba[3] = {0x0000F800,0x000007E0,0x0000001F};
		fwrite(&rgba[0],1,sizeof(int),mFp);
     		fwrite(&rgba[1],1,sizeof(int),mFp);
     		fwrite(&rgba[2],1,sizeof(int),mFp);
	}

 
}

BmpFile::~BmpFile()
{

 	if(mFp) 
		fclose(mFp);

}

int BmpFile::write(char *buf, int size)
{
  	/*RGB��ɫд���ļ�*/
  	int ret =-1;
	char tmp;
	if(mFp) 
	{
		if(mInfoHeader.biBitCount == 24)
  		{
			for(int i = 0;i < size/3;i++)
			{
				tmp = buf[i*3];
				buf[i*3] = buf[i*3+2];
				buf[i*3+2] = tmp; 
			}
			for(int i = mInfoHeader.biHeight-1;i >= 0 ;i--)
			{
				ret =fwrite(buf+i*mInfoHeader.biWidth*3,1,mInfoHeader.biWidth*3,mFp);
			}
  		}
		else
		{
			ret =fwrite(buf,1,size,mFp);
		}
	}
	return ret;
}

