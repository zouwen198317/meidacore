#ifndef CREATESINGLECOLORBG_H
#define CREATESINGLECOLORBG_H

#include "common.h"


void createBg_i420(unsigned int width,unsigned int height,RGB rgb,unsigned char* buf);
void createBg_yuyv(unsigned int width,unsigned int height,RGB rgb,unsigned char* buffer);


#endif
