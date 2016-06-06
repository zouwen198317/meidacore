#include<stdio.h>
#include<memory.h>
#include<string.h>
#include<math.h>
#include "AviAMFData.h"
#include "mediablock.h"
#include "mediacore.h"

extern mediaStatus gMediaStatus;

typedef signed char  int8_t;
typedef signed short int16_t;
typedef signed int   int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef signed long long   int64_t;
typedef unsigned long long uint64_t;


unsigned char* putbyte(unsigned char *s, unsigned int b)
{
    *s++ = (unsigned char)b;
    return s;
}

unsigned char* putbe32(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 24);
    s= putbyte(s, val >> 16);
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}

unsigned char* putbe16(unsigned char *s, unsigned int val)
{
    s= putbyte(s, val >> 8);
    s= putbyte(s, val);
    return s;
}

unsigned char* putamf_string(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    *ptr = len >> 8;
    *(ptr+1) = len;
    memcpy(ptr+2, str, len);

    return ptr + len + 2;
}

#define AMF_DATA_TYPE_STRING 0x02

unsigned char* putamf_string_content(unsigned char* ptr, const char *str)
{
    size_t len = strlen(str);
    ptr = putbyte(ptr, AMF_DATA_TYPE_STRING);
    ptr = putbe16(ptr, len);
    memcpy(ptr, str, len);
    return  ptr+len;
}

unsigned char* putbe64(unsigned char* ptr, uint64_t val)
{
    ptr =putbe32(ptr, (uint32_t)(val >> 32));
    ptr =putbe32(ptr, (uint32_t)(val & 0xffffffff));
    return ptr;
}

static int64_t dbl2int(double d){
    int e;
    if     ( !d) return 0;
    else if(d-d) return 0x7FF0000000000000LL + ((int64_t)(d<0)<<63) + (d!=d);
    d= frexp(d, &e);
    return (int64_t)(d<0)<<63 | (e+1022LL)<<52 | (int64_t)((fabs(d)-0.5)*(1LL<<53));
}

unsigned char* putamf_double(unsigned char* ptr, double d)
{
    ptr = putbyte(ptr, 0);
    ptr = putbe64(ptr, dbl2int(d));
    return ptr;
}

#define AMF_DATA_TYPE_MIXEDARRAY 0x08
#define AMF_END_OF_OBJECT 0x09

int GpsAmfData(unsigned char *pb,gpsData_t *pData)
{
 
 unsigned char *pstart = pb;
 if(pb == NULL || pData == NULL) 
  return -1;
    /*first event name as a string*/
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_STRING);
    pb = putamf_string(pb, (const char *)"GPS"); // 12 bytes

    /* first event name as a string */// store DVR CAR Info
    //pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_STRING);
   // pb = putamf_string(pb, (const char *)dvrIniConf.carInfo.plateNumber); // 12 bytes

    /* mixed array (hash) with size and string/type/data tuples */
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_MIXEDARRAY);
    pb = putbe32(pb, 10); // 

    pb = putamf_string(pb, (const char *)"carInfo");//carInfo
    pb = putamf_string_content(pb, (const char *)""); // 12 bytes dvrIniConf.carInfo.plateNumber

    pb = putamf_string(pb, (const char *)"mode");//latitude
    pb = putamf_double(pb, (double)pData->mode);
 
    pb = putamf_string(pb, (const char *)"sat_used");//satellites_used
    pb = putamf_double(pb, (double)pData->satellites_used);

    pb = putamf_string(pb, (const char *)"sat_visible");//satellites_visible
    pb = putamf_double(pb, (double)pData->satellites_visible);

    pb = putamf_string(pb, (const char *)"lat");//latitude
    pb = putamf_double(pb, (double)pData->lat);

    pb = putamf_string(pb, (const char *)"lon");//longitude
    pb = putamf_double(pb, (double)pData->lon); 

    pb = putamf_string(pb, (const char *)"alt");//altitude
    pb = putamf_double(pb, (double)pData->alt); 
 
    pb = putamf_string(pb, (const char *)"speed");//speed over ground
    pb = putamf_double(pb, (double)pData->speed); 
    pb = putamf_string(pb, (const char *)"course");//course over ground
    pb = putamf_double(pb, (double)pData->course); 

    pb = putamf_string(pb, (const char *)"time");//time
    pb = putamf_double(pb, (double)pData->time); 

    pb = putamf_string(pb, (const char *)"");
    pb = putbyte(pb, (unsigned int)AMF_END_OF_OBJECT);
    log_print(_DEBUG,"GPS:lat %f,lon %f,alt %f,speed %f,course %f\n",
    pData->lat,pData->lon,pData->alt,pData->speed,pData->course);
    return pb - pstart;
}

int GSensorAmfData(unsigned char *pb, gSensorData_t *pData)
{
    unsigned char *pstart = pb;
    if(pb == NULL || pData == NULL) 
        return -1;
    /* first event name as a string */
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_STRING);
    pb = putamf_string(pb, (const char *)"GSensor"); // 12 bytes

    /* mixed array (hash) with size and string/type/data tuples */
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_MIXEDARRAY);
    pb = putbe32(pb, (unsigned int)3); // 

    pb = putamf_string(pb, (const char *)"x");//latitude
    pb = putamf_double(pb, (double)pData->x);

    pb = putamf_string(pb, (const char *)"y");//longitude
    pb = putamf_double(pb, (double)pData->y); 

    pb = putamf_string(pb, (const char *)"z");//altitude
    pb = putamf_double(pb, (double)pData->z); 
    
    pb = putamf_string(pb, (const char *)"");
    pb = putbyte(pb, (unsigned int)AMF_END_OF_OBJECT);
    //log_print(_DEBUG,"GSensor:x %f,y %f,z %f\n",pData->x,pData->y,pData->z);
 
    return pb - pstart;
}

int otherSensorAmfData(unsigned char *pb, otherSensorData_t *pData)
{
    unsigned char *pstart = pb;
    if(pb == NULL || pData == NULL) 
        return -1;
    /* first event name as a string */
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_STRING);
    pb = putamf_string(pb, (const char *)"otherSensors"); // 12 bytes

    /* mixed array (hash) with size and string/type/data tuples */
    pb = putbyte(pb, (unsigned int)AMF_DATA_TYPE_MIXEDARRAY);
    pb = putbe32(pb, (unsigned int)1); // 

    pb = putamf_string(pb, (const char *)"speed");//kmh
    pb = putamf_double(pb, (double)pData->speed);
    
    pb = putamf_string(pb, (const char *)"temperature");//
    pb = putamf_double(pb, (double)pData->temperature);

    pb = putamf_string(pb, (const char *)"");
    pb = putbyte(pb, (unsigned int)AMF_END_OF_OBJECT);
    log_print(_DEBUG,"otherSensors:speed %f temperature %f\n",pData->speed,pData->temperature);
    return pb - pstart;

}



#include<sys/time.h>
#include"shmringqueue.h"
#include "msgqueueopcode.h"
#include "ipc/msgqueue.h"
#include "ipc/ipcclient.h"

#define MEDIA_DATA_BUFFER_NUM 100
#define DATA_BUFF_LEN 256

static char* gGpsData = NULL; 
static char* gGSensorData = NULL;
static char* gOtherSensorData = NULL;
static ShmRingQueue<MediaBlk> *gpMediaRingQueue = NULL;
static int gGpsDataId=0;
static int gGSensorDataId=0;
static int gOtherSensorDataId=0;
static struct timeval gStartTime;

extern MsgQueue *gpRecFileMsgQueue;

void initAviAmfWriter()
{
    gGpsData = new char[DATA_BUFF_LEN * MEDIA_DATA_BUFFER_NUM];
    gGSensorData = new char[DATA_BUFF_LEN * MEDIA_DATA_BUFFER_NUM];
    gOtherSensorData = new char[DATA_BUFF_LEN * MEDIA_DATA_BUFFER_NUM];
    gpMediaRingQueue = new ShmRingQueue<MediaBlk>((key_t)RING_BUFFER_AVENC_TO_MEDIA_BUF,RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM,sizeof(MediaBlk));
    gettimeofday(&gStartTime,NULL);
}
void uninitAviAmfWriter()
{
    if(gGpsData != NULL)
        delete gGpsData;
    gGpsData = NULL;
    if(gGSensorData != NULL)
        delete gGSensorData;
    gGSensorData = NULL;
    if(gOtherSensorData != NULL)
        delete gOtherSensorData;
    gOtherSensorData = NULL;
    if(gpMediaRingQueue != NULL)
        delete gpMediaRingQueue;
    gpMediaRingQueue = NULL;
}

int notify(int msgopc)
{
    /*
    if(gMediaStatus != MEDIA_RECORD)
        return 0;

    int msgNum=0;
    msgbuf_t msg;
    msg.type=MSGQUEU_TYPE_NORMAL;
    msg.len=0;
    msg.opcode=msgopc;
    memset(msg.data,0,MSGQUEUE_BUF_SIZE);
    if(gpRecFileMsgQueue) {
        msgNum = gpRecFileMsgQueue->msgNum();
        //log_print(_INFO,"AviAmfWriter notify queue num %d\n",msgNum);
        //if(msgNum < ( RING_BUFFER_AVENC_TO_MEDIA_BUF_NUM>>2)) // write file may blocking too many frame
        if(msgNum < MSGQUEUE_MSG_LIMIT_NUM) //if msgNum >61, msgsnd() may be block
            gpRecFileMsgQueue->send(&msg);
        //else 
            //log_print(_DEBUG,"AviAmfWriter notify ignore, too more queue num %d\n", msgNum);
    }
*/
}

void generateGPSBlk(gpsData_t * gps)
{
    if(gps != NULL)
    {
        int gps_data_len = GpsAmfData((unsigned char*)gGpsData + gGpsDataId * DATA_BUFF_LEN,gps);
        MediaBlk mediaBlk;
        struct timeval currTime;
        gettimeofday(&currTime,NULL);
        mediaBlk.blkType = SENSOR_BLK;
        mediaBlk.blk.sensor.buf.vStartaddr = (void*)gGpsData;
        mediaBlk.blk.sensor.buf.bufSize = MEDIA_DATA_BUFFER_NUM * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.offset = gGpsDataId * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.frameSize = gps_data_len;
        mediaBlk.blk.sensor.pts = (currTime.tv_sec-gStartTime.tv_sec)*1000+(currTime.tv_usec-gStartTime.tv_usec)/1000;
        //log_print(_INFO,"generateGPSBlk .\n");

        gpMediaRingQueue->push(mediaBlk);
        notify(MSG_OP_NORMAL);
        ++gGpsDataId;
        if(gGpsDataId >= MEDIA_DATA_BUFFER_NUM)gGpsDataId=0;
    }
    else
        log_print(_ERROR,"generateGPSBlk NULL.\n");
}

void generateGSensorBlk(gSensorData_t* gsensor)
{

    if(gsensor != NULL)
    {
        int gsensor_data_len = GSensorAmfData((unsigned char*)gGSensorData + gGSensorDataId*DATA_BUFF_LEN,gsensor);
        MediaBlk mediaBlk;
        struct timeval currTime;
        gettimeofday(&currTime,NULL);
        mediaBlk.blkType = SENSOR_BLK;
        mediaBlk.blk.sensor.buf.vStartaddr = (void*)gGSensorData;
        mediaBlk.blk.sensor.buf.bufSize = MEDIA_DATA_BUFFER_NUM * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.offset = gGSensorDataId * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.frameSize = gsensor_data_len;
        mediaBlk.blk.sensor.pts = (currTime.tv_sec-gStartTime.tv_sec)*1000+(currTime.tv_usec-gStartTime.tv_usec)/1000;
        #if 0
        FILE *fp;
        fp = fopen("dump_sensor","ab");
        fwrite(mGSensorData[mGSensorDataId],1,gsensor_data_len,fp);
        printf("##gsensor data: %s\t%d\n",mGSensorData + mGSensorDataId,gsensor_data_len);
        fclose(fp);
        #endif
        //log_print(_INFO,"generateGSensorBlk .\n");
        gpMediaRingQueue->push(mediaBlk);
        notify(MSG_OP_NORMAL);
        ++gGSensorDataId;
        if(gGSensorDataId >= MEDIA_DATA_BUFFER_NUM)gGSensorDataId=0;
    }
    else
        log_print(_ERROR,"generateGSensorBlk NULL.\n");
}

void generateOtherSensorBlk(otherSensorData_t* otherSensor)
{
    if(otherSensor != NULL)
    {
        int otherSensor_data_len = otherSensorAmfData((unsigned char*)gOtherSensorData + gOtherSensorDataId * DATA_BUFF_LEN, otherSensor);
        MediaBlk mediaBlk;
        struct timeval currTime;
        gettimeofday(&currTime,NULL);
        mediaBlk.blkType = SENSOR_BLK;
        mediaBlk.blk.sensor.buf.vStartaddr = (void*)gOtherSensorData;
        mediaBlk.blk.sensor.buf.bufSize = MEDIA_DATA_BUFFER_NUM * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.offset = gOtherSensorDataId * DATA_BUFF_LEN;
        mediaBlk.blk.sensor.frameSize = otherSensor_data_len;
        mediaBlk.blk.sensor.pts = (currTime.tv_sec-gStartTime.tv_sec)*1000+(currTime.tv_usec-gStartTime.tv_usec)/1000;

        gpMediaRingQueue->push(mediaBlk);
        notify(MSG_OP_NORMAL);
        ++gOtherSensorDataId;
        if(gOtherSensorDataId >= MEDIA_DATA_BUFFER_NUM)gOtherSensorDataId=0;
    }
    else
        log_print(_ERROR,"generateOtherSensorBlk NULL.\n");
}



