#ifndef _MOTION_DETECTION_TECHNOLOGY_H_
#define _MOTION_DETECTION_TECHNOLOGY_H_
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "mediacore.h"
#include "mediablock.h"
#include "sensoropcode.h"
#include "shmringqueue.h"
#include "libthread/threadpp.h"

class MyMotionDetectionTechnology : public Thread
{
public:
    enum
    {
        YUV422
    };
    MyMotionDetectionTechnology(vidCapParm *pParm);
    ~MyMotionDetectionTechnology();
    int GetWidth() {return m_width;}
    int GetHeight() {return m_height;}
    bool YUV422_IsLargeDifference(unsigned char* buf1,unsigned char* buf2);
    //int parkingColisionDetection(gSensorData_t* gSensorData);

    int exit();
    int pause();
    int resume();

private:
    struct myPoint
    {
        int x;
        int y;
    };

    struct myPointNode
    {
        myPointNode *next;
        myPoint point;
    };
    void run();

    int notify(int msgopc);
    int notifyItself(int msgopc);

    int motionDetection(ImgBlk *pImgBlk);

    int Contrast_YUV422_Image(unsigned char* buf1,unsigned char* buf2,int w,int h);
    int GetLargestLabelRegionSize(unsigned char **inImage,int w,int h,int connected);
    unsigned char** Malloc_matrix(int h,int w);
    void Free_matrix(unsigned char** p,int h);
    
    int m_width;
    int m_height;
    int m_format;
    int m_fps;
    int mRunning;
    int mPause;
    u8* image1;
    u8* image2;
    int mStaticTime;
    int mStaticCount;
    //gSensorData_t mSensorData;
    
    unsigned char** m_bwImage;
    unsigned char** m_imageStorage;
    myPointNode m_head;
    ShmRingQueue<ImgBlk> * mpCamRingQueue;

};

int Simplify_YUV422_Image(unsigned char* inbuf,int w,int h,unsigned char*outbuf,int w_multiple,int h_multiple);

//unsigned char** Contrast_YUV422_Image(unsigned char* buf1,unsigned char* buf2,int w,int h);

//int GetLargestLabelRegionSize(unsigned char **inImage,int w,int h,int connected);

//unsigned char** Malloc_matrix(int h,int w);

//void Free_matrix(unsigned char** p,int h);

//unsigned char ** label_region(unsigned char **inImage,int w,int h,int connected);

#endif
