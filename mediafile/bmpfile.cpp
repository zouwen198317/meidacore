#include "bmpfile.h"

BmpFile::BmpFile(char *name, int width, int height, int rgbBits)
{

	if((mFp= fopen(name, "wb")) ==NULL){
		printf("BmpFile file %s cannot open\n",name);
	}

	/*定义位图头文件数据结构*/
	mFileHeader.bfType[0] = 'B';
	mFileHeader.bfType[1] = 'M';
	mFileHeader.bfReserved1 = 0x0000;
	mFileHeader.bfReserved2 = 0x0000;
	//位图起始位置
	mFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	if(rgbBits == 16)
	{
		mFileHeader.bfOffBits = mFileHeader.bfOffBits + 12;
	}

	/*定义位图信息数据结构*/
	mInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	mInfoHeader.biWidth = (long)(width); 
	mInfoHeader.biHeight = (long)(height); 
	mInfoHeader.biPlanes = 0x0001;
	mInfoHeader.biBitCount = rgbBits;//0x0018;  //24位图
	mInfoHeader.biCompression = 0x0000;  //不压缩
	if(rgbBits == 16)
	{
		mInfoHeader.biCompression = 0x0003;
	}

	/*每像素由RGB24位构成，即3字节*/
	mInfoHeader.biSizeImage = width*height * rgbBits/8;
	mInfoHeader.biXPixPerMeter = 0;  /*96dpi*/
	mInfoHeader.biYPixPerMeter = 0;  /*96dpi*/
	mInfoHeader.biClrUsed = 0;   
	mInfoHeader.biClrImporant = 0;  

	/*位图文件大小*/
	mFileHeader.bfSize = mFileHeader.bfOffBits + mInfoHeader.biSizeImage;

	/*位图头文件结构写入文件*/
	if(mFp)   
		fwrite((unsigned char *)&mFileHeader, sizeof(BITMAPFILEHEADER), 1, mFp);
	/*位图信息结构写入文件*/
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
  	/*RGB颜色写入文件*/
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

