#include "debug/errlog.h"
#include "log/log.h"
#include "platform/camcapture.h"
#include "math.h"
#include "recfilejob.h"
#include "videoprepencodejob.h"
#include "motion_detection_technology.h"
#include "videoencodejob.h"
#include "ini_common.h"

#define RATIO_THRESHOLD 0.125 //变化面积的比例阈值
#define DIFF_THRESHOLD 30 //变化差值阈值
#define IMG_SCALE 4
#define STATIC_TIME 30  //sec 
#define DEBUG_TIME 15  //sec 

#define DETECTION_FREQUENCY 2  //detection nth per sencond
#define SENSOR_THRESHOLD 0.15

extern mediaStatus gMediaStatus;
extern MsgQueue *gpMotionDetectMsgQueue;
extern MsgQueue *gpRecFileMsgQueue;
extern RecFileJob *gpRecFileJob;
extern VideoPrepEncJob *gpVideoPrepEncJob;
extern VideoEncodeJob *gpVidEncJob;
extern RecFileIniParm gRecIniParm;

extern bool gDebugMode;
MyMotionDetectionTechnology::MyMotionDetectionTechnology(vidCapParm *pParm)
{
    m_width = pParm->width/IMG_SCALE;
    m_height = pParm->height/IMG_SCALE;
    m_format = pParm->fourcc;
    m_fps = pParm->fps;
    m_bwImage = Malloc_matrix(m_height,m_width);
    m_imageStorage = Malloc_matrix(m_height,m_width);
    m_head.next = NULL;
    
    //mSensorData.x = 0.0;
    //mSensorData.y = 0.0;
    //mSensorData.z = 0.0;
    mpCamRingQueue = new ShmRingQueue<ImgBlk>((key_t)RING_BUFFER_CAPTURE_TO_MOTIONDETECT,CAM_BUFFER_NUM,sizeof(ImgBlk));
    mpCamRingQueue->clear();
    
    mStaticTime = gDebugMode ?  DEBUG_TIME : STATIC_TIME * DETECTION_FREQUENCY;
}

MyMotionDetectionTechnology::~MyMotionDetectionTechnology()
{
    Free_matrix(m_bwImage,m_height);
    Free_matrix(m_imageStorage,m_height);
    myPointNode *current;
    myPointNode *next;
    current = m_head.next;
    while(current != NULL)
    {
        next = current->next;               
        free(current);
        current = next;
    }
      if(mpCamRingQueue) {
        delete mpCamRingQueue;
        mpCamRingQueue = NULL;
      }
    if(image1) {
        free(image1);
        image1 = NULL;
    }
    if(image2) {
        free(image2);
        image2 =NULL;
    }

}


//==================================================================   
//功能：判断两个YUV422图像是否相差过大 (仅用Y比较)  
//输入参数: buf1 输入图像1的data指针
//              buf2 输入图像2的data指针
//返回值： 是否相差过大
//==================================================================
bool MyMotionDetectionTechnology::YUV422_IsLargeDifference(unsigned char* buf1,unsigned char* buf2)
{ 
    int nSum = Contrast_YUV422_Image(buf1,buf2,m_width,m_height);
    int nLargestlabelregionsize = GetLargestLabelRegionSize(m_bwImage,m_width,m_height,8);
    //printf("---%d/%d---\n",nSum,m_width*m_height);
    //printf("---%d---\n",nLargestlabelregionsize);
    if(nSum > m_width*m_height*RATIO_THRESHOLD*2 ||
            (nSum > m_width*m_height*RATIO_THRESHOLD    && nLargestlabelregionsize > nSum/3))
        return true;
    return false;   
}


//==================================================================   
//功能：得到两个YUV422图像的差值二值图（二维数组） 
//输入参数: buf1 输入图像1的data指针
//              buf2 输入图像2的data指针
//          w 图片宽
//              h 图片高
//返回值： 差值二值图亮点数
//==================================================================
int MyMotionDetectionTechnology::Contrast_YUV422_Image(unsigned char* buf1,unsigned char* buf2,int w,int h)
{ 
    //printf("motionDetectionJob::Contrast_YUV422_Image()\n");
    int sum = 0;
    for(int i = 0;i < h;i++)
    {
        for(int j = 0;j < w;j++)
        {
            int pos = i*w*2 + j*2;

            int diff = buf1[pos]-buf2[pos];
            if(diff > DIFF_THRESHOLD || diff < -DIFF_THRESHOLD )
            {
                sum++;
                m_bwImage[i][j]= 1;
            }
            else
                m_bwImage[i][j]= 0;
        }
    }
    return sum;  
}

//==================================================================   
//功能：得到二值图的最大连通域的面积
//输入参数: inImage 二值图（二维数组）
//          w 图片宽
//              h 图片高
//              connected 连通数 （4或8）
//返回值： 最大连通域的面积
//==================================================================
int MyMotionDetectionTechnology::GetLargestLabelRegionSize(unsigned char **inImage,int w,int h,int connected)
{
    //printf("motionDetectionJob::GetLargestLabelRegionSize()\n");
    int ret_size = 0;
    //unsigned char **tmpStorage = Malloc_matrix(h,w);

    int label=0;
    int *neighbour;
    int neighbour_eight[16] = {-1,-1,0,-1,1,-1,1,0,1,1,0,1,-1,1,-1,0};
    int neighbour_four[8] = {0,-1,1,0,0,1,-1,0};
    if (connected == 8) 
        neighbour = neighbour_eight;
    else        
        neighbour = neighbour_four;

    for (int i=0;i<h;i++) 
    {
        for (int j=0;j<w;j++) 
            {
                m_imageStorage[i][j]=0;
        }
    }

    for (int i = 1; i < h - 1; i++)
    {
        for (int j = 1; j < w - 1; j++)
        {
            if (inImage[i][j] != 0 && m_imageStorage[i][j] == 0)
            {
                //label++;
                label = 1;
                m_imageStorage[i][j] = label;
                             
                m_head.point.y = i;
                m_head.point.x = j;
                
                myPointNode *regionNow = &m_head;
                myPointNode *regionTail = &m_head;
                int size = 1;
                do{
                    myPoint pix = regionNow->point;
                    myPoint nearPix;
                    for (int k = 0; k < connected; k++)
                    {
                        nearPix.x = pix.x + neighbour[2 * k];
                        nearPix.y = pix.y + neighbour[2 * k + 1];
                        if (nearPix.x > 0 && nearPix.x < w - 1 && nearPix.y > 0 && nearPix.y < h - 1)
                        {
                            if (inImage[nearPix.y][nearPix.x] != 0 && m_imageStorage[nearPix.y][nearPix.x] == 0)
                            {
                                m_imageStorage[nearPix.y][nearPix.x] = label;
                                size++;
                                
                                if(regionTail->next == NULL)
                                {
                                    regionTail->next = (myPointNode *)malloc(sizeof(myPointNode));
                                    regionTail->next->next = NULL;
                                }
                                regionTail = regionTail->next;
                                regionTail->point = nearPix;
                            }
                        }
                    }
                    regionNow = regionNow->next;                    
                }while(regionNow != regionTail->next);
                ret_size = ret_size>size?ret_size:size;
            }
        }
    }   
    return ret_size;
}

int MyMotionDetectionTechnology::exit()
{
    int ret = -1;
    if(mRunning) {
        log_print(_DEBUG,"MyMotionDetectionTechnology exit()\n");
        notifyItself(MSG_OP_EXIT);
        ret =0;
    }
    return ret;
}

int MyMotionDetectionTechnology::pause()
{
    notifyItself(MSG_OP_PAUSE);
    return 0;
}

int MyMotionDetectionTechnology::resume()
{
    notifyItself(MSG_OP_RESUME);
    return 0;
}

int MyMotionDetectionTechnology::notify(int msgopc)
{
    int msgNum=0;
    msgbuf_t msg;
    msg.type=MSGQUEU_TYPE_NORMAL;
    msg.len=0;
    msg.opcode=msgopc;//MSG_OP_NORMAL;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpRecFileMsgQueue) {
        msgNum = gpRecFileMsgQueue->msgNum();
        if(msgNum > MSGQUEUE_MSG_LIMIT_NUM) {
            log_print(_ERROR, "motionDetectionJob::notify() gpRecFileMsgQueue %d\n", msgNum);
        } else {
            gpRecFileMsgQueue->send(&msg);
        }
    }
    return 0;
}

int MyMotionDetectionTechnology::notifyItself(int msgopc)
{
    int msgNum=0;
    log_print(_DEBUG,"MyMotionDetectionTechnology notifyItself() msgopc %d\n",msgopc);
    msgbuf_t msg;
    msg.type=MSGQUEU_TYPE_NORMAL;
    msg.len=0;
    msg.opcode = msgopc;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpMotionDetectMsgQueue) {
        msgNum = gpMotionDetectMsgQueue->msgNum();
        if(msgNum > MSGQUEUE_MSG_LIMIT_NUM) {
            log_print(_ERROR, "motionDetectionJob::notify() gpMotionDetectMsgQueue %d\n", msgNum);
        }
        gpMotionDetectMsgQueue->send(&msg);
    }
    return 0;
}
/*
int MyMotionDetectionTechnology::parkingColisionDetection(gSensorData_t* gSensorData)
{
    static bool flag = false;
    if(flag && (fabs(gSensorData->x - mSensorData.x) > SENSOR_THRESHOLD
        ||fabs(gSensorData->y - mSensorData.y) > SENSOR_THRESHOLD
        ||fabs(gSensorData->z - mSensorData.z) > SENSOR_THRESHOLD))
        printf("colision???\n");
        //notify(MSG_OP_MOTION_DET);
    mSensorData.x = gSensorData->x;
    mSensorData.y = gSensorData->y;
    mSensorData.z = gSensorData->z;
    flag = true;
    return 0;
}
*/
int MyMotionDetectionTechnology::motionDetection(ImgBlk *pImgBlk)
{
    if(image1 == NULL) {
        image1 = (u8*)malloc(m_width * m_height * 2 * sizeof(u8)); 
        Simplify_YUV422_Image((u8*)pImgBlk->vaddr, pImgBlk->width, pImgBlk->height, image1, IMG_SCALE, IMG_SCALE);
    } else {
        if(image2 == NULL) {
            image2 = (u8*)malloc(m_width*m_height * 2 * sizeof(u8)); 
        }
        Simplify_YUV422_Image((u8*)pImgBlk->vaddr, pImgBlk->width, pImgBlk->height, image2, IMG_SCALE, IMG_SCALE);
        if(YUV422_IsLargeDifference(image1, image2)) {
            log_print(_INFO, "MotionDetectionJob YUV422_IsLargeDifference() motion detection\n");
            #if 0
            if(gDriveRecord == 0) {
                notify(MSG_OP_MOTION_DET);
                printf("MotionDetectionJob run() notify MSG_OP_MOTION_DET\n");
            } else {
        #endif
                notify(MSG_OP_MOTION_DET);
                gpVidEncJob->setSkipping(false);
                mStaticCount = 0;
            //}
        } else {
            //if(gRecIniParm.RecFile.bootRecord == 1) {
                if(!gpRecFileJob->isSkipping() && gpRecFileJob->getRecType()!=EVENT_EMC) {
                    //printf("mStaticCount, mStaticTime %d, %d\n", mStaticCount, mStaticTime);
                    if(++mStaticCount > mStaticTime) {
                        notify(MSG_OP_FRAME_SKIPPING);
                        gpVidEncJob->setSkipping(true);
                    }
                }
            //}
        }
        u8* tmp = image1;
        image1 = image2;
        image2 = tmp;
    }

}
void  MyMotionDetectionTechnology::run()
{
    msgbuf_t msg;
    int ret = -1;
    int frameCount = 0; 
    int frameThrehold = m_fps / DETECTION_FREQUENCY;
    mRunning = true;
    mPause = false;
    mStaticCount = 0;
    struct timeval startTime,endTime;
    long useTime;

    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    printf("\n-----MotionDetectionJob run()-----\n");
    
    while( mRunning ) {
        gpMotionDetectMsgQueue->recv(&msg);
        //log_print(_DEBUG,"MotionDetectionJob run() recv opcode %d\n",msg.opcode);

        switch(msg.opcode) {
            case MSG_OP_NORMAL:
                ImgBlk inputImgBlk;
                if(mPause) break;
                if(gMediaStatus==MEDIA_RECORD || gMediaStatus==MEDIA_IDLE_MAIN) {
                    ret = mpCamRingQueue->pop();
                    if(ret < 0) {
                        log_print(_ERROR, "MotionDetectionJob processImg mpCamRingQueue no data input %d\n",ret);
                        break;
                    }
                    if(++frameCount >= frameThrehold) {
                        inputImgBlk = mpCamRingQueue->front();
                            //gettimeofday(&startTime, NULL);
                        motionDetection(&inputImgBlk); //
                            //gettimeofday(&endTime, NULL);
                            //useTime = (endTime.tv_sec - startTime.tv_sec) * 1000 + (endTime.tv_usec - startTime.tv_usec) / 1000;
                            //log_print(_INFO, "MotionDetectionJob run() motionDetection() time %ldms\n", useTime);   
                        frameCount = 0;
                    }
                }
                break;
            case MSG_OP_RESUME:
                log_print(_INFO, "MotionDetectionJob run() MSG_OP_RESUME recv\n");
                mPause = false;
                break;
            case MSG_OP_START:
                
                break;
            case MSG_OP_PAUSE:
                gpVidEncJob->setSkipping(false);
                mStaticCount = 0;

                log_print(_INFO, "MotionDetectionJob run() MSG_OP_PAUSE recv\n");
                mPause = true;
                break;
            case MSG_OP_STOP:
                mRunning = false;
                break;
            case MSG_OP_EXIT:
                mRunning = false;
                break;
            default:
                break;
        } //switch()
    } //while()
    notify(MSG_OP_MOTION_DET);
    gpVidEncJob->setSkipping(false);
    log_print(_ERROR, "MyMotionDetectionTechnology run() exit\n");
    
}

//==================================================================   
//功能：申请二维数组的内存
//输入参数: h 数组高
//          w 数组宽         
//返回值： 二维指针
//==================================================================
unsigned char** MyMotionDetectionTechnology::Malloc_matrix(int h,int w)
{
    unsigned char **ret_p;
    ret_p=(unsigned char **)malloc(h*sizeof(unsigned char *));
   if (NULL == ret_p) 
    return NULL;
   for (int i=0;i<h;i++) 
   {
    ret_p[i]=(unsigned char *)malloc(w*sizeof(unsigned char));
    if (NULL==ret_p[i]) 
    return NULL;
   }
   for (int i=0;i<h;i++) 
   {
    for (int j=0;j<w;j++) 
    {
        ret_p[i][j]=0;
    }
   }
   return ret_p;
}


//==================================================================   
//功能：释放二维数组的内存
//输入参数: p 二维数组
//          h 数组高     
//返回值： 
//==================================================================
void MyMotionDetectionTechnology::Free_matrix(unsigned char** p,int h)
{
    for (int i=0;i<h;i++) 
    {
        free(p[i]);
    }
    free(p);
}

//==================================================================   
//功能：插值法，均值法混合缩放YUV422图像   
//输入参数: inbuf 输入图像的data指针
//              w 图片宽
//              h 图片高
//              outbuf 输入图像的data指针
//              w_multiple 宽缩放比例
//          h_multiple 高缩放比例
//          注：这两个比例是插值法时的缩放比例，实际缩放比例还有乘上均值法的固定2倍
//返回值： 输出图像的大小
//==================================================================
int Simplify_YUV422_Image(unsigned char* inbuf,int w,int h,unsigned char* outbuf,int w_multiple,int h_multiple)
{
    if(inbuf == NULL || w < w_multiple  || h < h_multiple)
    {
        printf("Reduced_YUV422_Image() param mismatch\n");
        return -1;
    }   
    int num = 0;
    for(int i = 0;i < h/h_multiple;i++)
   {
    for(int j = 0;j < w/w_multiple;j++)
    {
        int pos = i*h_multiple*w*2 + j*w_multiple*2;

        int sum = inbuf[pos]+inbuf[pos+w_multiple]+inbuf[pos+w*h_multiple]
                    +inbuf[pos+w*h_multiple+w_multiple];
        outbuf[num*2]=sum/4;
        outbuf[num*2+1]=0;
        num++;
    }
   }
   return num;
}


//==================================================================   
//功能：得到二值图的连通域图像
//输入参数: inImage 二值图（二维数组）
//          w 图片宽
//              h 图片高
//              connected 连通数 （4或8）
//返回值： 二值图的连通域图像
//==================================================================
/*
unsigned char ** label_region(unsigned char **inImage,int w,int h,int connected)
{
    unsigned char **tmpStorage = Malloc_matrix(h,w);

    int label=0;
    int *neighbour;
    int neighbour_eight[16] = {-1,-1,0,-1,1,-1,1,0,1,1,0,1,-1,1,-1,0};
    int neighbour_four[8] = {0,-1,1,0,0,1,-1,0};
    if (connected == 8) 
        neighbour = neighbour_eight;
    else        
        neighbour = neighbour_four;

    for (int i = 1; i < h - 1; i++)
    {
        for (int j = 1; j < w - 1; j++)
        {
            if (inImage[i][j] != 0 && tmpStorage[i][j] == 0)
            {
                label++;
                tmpStorage[i][j] = label;
                 
                myPointNode region;          
                region.point.y = i;
                region.point.x = j;
                region.next = NULL;
                
                myPointNode *regionNow = &region;
                myPointNode *regionTail = &region;
                
                do{
                    myPoint pix = regionNow->point;
                    myPoint nearPix;
                    for (int k = 0; k < connected; k++)
                    {
                        nearPix.x = pix.x + neighbour[2 * k];
                        nearPix.y = pix.y + neighbour[2 * k + 1];
                        if (nearPix.x > 0 && nearPix.x < w - 1 && nearPix.y > 0 && nearPix.y < h - 1)
                        {
                            if (inImage[nearPix.y][nearPix.x] != 0 && tmpStorage[nearPix.y][nearPix.x] == 0)
                            {
                                tmpStorage[nearPix.y][nearPix.x] = label;
                                   
                                regionTail->next = (myPointNode *)malloc(sizeof(myPointNode));
                                regionTail = regionTail->next;
                                regionTail->point = nearPix;
                                regionTail->next = NULL;
                            }
                        }
                    }
                    regionNow = regionNow->next;
                }while(regionNow != NULL);
                 
                myPointNode *current;
                myPointNode *next;
                current = region.next;
                while(current != NULL)
                {
                    next = current->next;               
                    free(current);
                    current = next;
                }
  
            }
        }
    }
    return tmpStorage;
}
*/
