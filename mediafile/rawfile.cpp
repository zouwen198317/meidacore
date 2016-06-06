#include "rawfile.h"

RawFile::RawFile(char *filename)
{
	mpFile = fopen(filename,"r+");
	if(mpFile == NULL)
		mpFile = fopen(filename,"w+");
		
}

RawFile::~RawFile()
{
	if( mpFile )
		fclose(mpFile);
	mpFile= NULL;
}

int RawFile::write(char *buf, int size)
{
	int ret =-1;
	if( mpFile )
		 ret = fwrite(buf,1,size,mpFile);
	return ret;
}

int RawFile::read(char *buf, int size)
{
	int ret =-1;
	if( mpFile )
		 ret = fread(buf,1,size,mpFile);
	return ret;

}

