#ifndef _RAW_FILE_H_
#define _RAW_FILE_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class RawFile
{
public:
	RawFile(char *filename);
	~RawFile();
	int write(char *buf, int size);
	int read(char *buf, int size);
private:
	FILE* mpFile;
};

#endif
