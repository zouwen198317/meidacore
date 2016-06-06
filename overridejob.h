#ifndef _OVERRIDE_JOB_H_
#define _OVERRIDE_JOB_H_

#include<queue>
#include<vector>
#include<string>
#include"libthread/threadpp.h"
#include"exceptionProcessor.h"

struct cmp
{
    bool operator()(std::string p1, std::string p2)
    {
        return p1 > p2;
    }
};

#define EMC_FILE_EXPIRY_DATE 2 //two month.
#define THRESHOLD 307200  //300MB
//#define THRESHOLD 9133536 //9G for test

class OverrideJob : public Thread
{
public:
    OverrideJob(sdcardProcessor* sdcard);
    ~OverrideJob();
    bool isRunning() const {return mRunning;}
    bool isOverride() const {return mOverride;}
    bool canOverride();// const {return !tobeDeletedQueue.empty() && mOverride;} 
    void setOverride(bool onoff);
    void exit();
    void pause();
    void resume();
    void wakeup();
    void refresh();//you should use it when pause
private:
    void run();
    void buildQueueSortedbyTime(char* rootpath);
    void buildCycleRecQueue(char* rootpath, char *expiryDate);

    bool checkAvFileSubfix(char *name);
    bool isDirEmpty(const char*);
    //static unsigned long long getCurAvailableDisk();
    void PopPath();
    void PushPath(char* path);
    //static unsigned long long get_file_size(const char *path);
    void Sleep(int sec);
    
    std::priority_queue<std::string,std::vector<std::string>,cmp> tobeDeletedQueue;
    bool mRunning;
    bool mOverride;
    bool mPause;
    bool mIntr;//interrupt flag
    bool isSleeping;
    int recBitRate;
    sdcardProcessor *mpSdcard;
    //pthread_t pid;

    //availableDisk
    //unsigned long long availableDisk;
};


#endif
