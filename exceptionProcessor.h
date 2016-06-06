#ifndef EXCEPTION_PROCESSOR_H
#define EXCEPTION_PROCESSOR_H
#include<pthread.h>
#include<stdlib.h>

struct mount_entry;
class OverrideJob;
#define MAX_SIZE_PER_MIN 90
//use in main thread
class sdcardProcessor
{
public:
    sdcardProcessor();
    ~sdcardProcessor();
    
    /**
        Sdcard插拔事件后的处理流程
    */
    void loadSdcard();
    void unloadSdcard();
    void refreshSdcardStorage();
    /**
        写文件标识，用以甄别文件完整
    */
    int writeFileFlag(const char*);
    int removeFileFlag(const char*);
    
    /**
        控制线程overridejob开始
    */
    void startOverrideJob();
    void stopOverrideJob();
    void turnOnOverride(bool);
    
    unsigned long long getFreeSpace();
    unsigned long long getTotalSpace();
    unsigned long long getUsedSpace();
    
    //bool isLoad(){return mbLoaded;}
    bool IsPassCheck(){return mbPassPrimaryCheck;}//test
    bool IsExist(){return mbStorageExist;}
    int getSdcardStatus(){return mRecStatus;};
    int processKernelMsg();
    void printFlag();//debug
    int format();
    int getFd(){return mFd;}
private:
    /**
        开机后的检查流程
    */
    void bootDetection(bool needCheckRestore = true);
    void clearFileFlag(const char*);
    void clearFSCKfile();//clear file named FSCK*.REC
    void initFlag();//test
    void init();
    /**
        基本的检查流程
    */
    void initSocket();
    void notifySdcardEvent();
    int checkFileFlag();
    int restoreSdcard();
    void primaryDetection();
    bool storageFullDetection();
    bool writableDetection();
    void refreshSdcardInfo();
    void updateMountList();
    int mountDevice(char*);//test
    int umountDevice(char*);//test
    int checkIsMounted(char*);//test
    void stopWriting();//test
    void restoreWriting(int);
    int checkDevExist(char*);
    int checkIsWritable(char*);
    bool mbStorageExist;
    bool mbStorageWritable;
    bool mbStorageFull;
    bool mbNeedFormat;
    bool mbNeedRestore;
    bool mbPassPrimaryCheck;
    bool mbPassBootCheck;
    bool mbLoaded;
    bool mbInserted; //checkDevExist
    unsigned long long mFreeSpace;//bytes
    unsigned long long mTotalSpace;
    unsigned long long mUsedSpace;
    int mFd;
    //char mMountedPath[128];
    //char mDevPath[128];
    //char mPartitionPath[128];
    char mKernelMsgBuf[1024];
    struct mount_entry* mpMountList;
    pthread_mutex_t mThreadLock;
    pthread_mutex_t mUnloadLock;
    int mUsrCnt;
    int mRecStatus;
    OverrideJob *mpOverrideJob;
};

class ExceptionProcessor
{//设计给需要异常检测的部分调用
public:
    static ExceptionProcessor* GetInstance();
    static void releaseInstance();
private:
    ExceptionProcessor();
    ~ExceptionProcessor();
    static ExceptionProcessor* pInstance;
    
public:
    sdcardProcessor* sdcard;    
};
#endif
