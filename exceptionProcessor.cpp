#include <stdio.h>
#include <fcntl.h>             // 提供open()函数  
#include <unistd.h>  
#include <stdio.h>  
#include <dirent.h>            // 提供目录流操作函数  
#include <string>
#include <string.h>  
#include <errno.h>
#include <sys/stat.h>        // 提供属性操作函数  
#include <sys/statfs.h> 
#include <sys/types.h>         // 提供mode_t 类型    
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>    
#include <linux/netlink.h>  
#include "exceptionProcessor.h"
#include "log/log.h"
#include "parms_dump.h"
#include "overridejob.h"
#include "ipcopcode.h"
#include "mediacore.h"
#include "mediaopcode.h"
#include "playbackjob.h"
#include "opccommand.h"

#define DEV_PATH "/dev/mmcblk0"
#define PARTITION_PATH "/dev/mmcblk0p1"
#define MOUNTED_PATH "/mnt/sdcard"

extern RecFileIniParm gRecIniParm;
extern CaptureIniParm gCapIniParm;
extern PlayBackJob *gpPlayBackJob;
extern mediaStatus gMediaStatus;

extern RecFileIniParm gRecIniParm;
extern CaptureIniParm gCapIniParm;

extern bool isReadyToRec();

extern int sendEndFileEvent(int endFlag);

//return value 
//0 : ok
//<0 : error 
//>0 : status from func

class mount_entry
{
public:
    char *devname;      /* Device node pathname, including "/dev/". */
    char *mountdir;     /* Mount point directory pathname. */
    char *type;     /* "nfs", "4.2", etc. */
    char *property;  /* "rw,nosuid,nodev,noexec,relatime", etc */
    unsigned int dummy; /* Nonzero for dummy filesystems. */
    unsigned int remote;    /* Nonzero for remote fileystems. */
    struct mount_entry *next;
    mount_entry()
    {
        this->devname = NULL;
        this->mountdir = NULL;
        this->type = NULL;
        this->property = NULL;
        this->next = NULL;
    }
    ~mount_entry()
    {   
        if(devname)
            delete devname;
        if(mountdir)
            delete mountdir;
        if(type)
            delete type;
        if(property)
            delete property;
        
        devname = NULL;
        mountdir = NULL;
        type = NULL;
        property = NULL;
    }
    void init()
    {
        this->devname = new char [256];
        this->mountdir = new char [256];
        this->type = new char [256];
        this->property = new char [256];
        this->next = NULL;
    }
};

mount_entry* readMountList()
{
    struct mount_entry *mountList = NULL;
    struct mount_entry *ptr;
    FILE *fp;
    fp = fopen("/proc/self/mounts","r");
    if(fp == NULL) {
        perror("fopen /proc/self/mounts");
        return NULL;
    }

    mountList = new mount_entry;
    ptr = mountList;
    ptr->init();
    
    while(true) {
        if(fscanf(fp, "%s %s %s %s %u %u", ptr->devname, ptr->mountdir, ptr->type, ptr->property,
                                    &(ptr->dummy), &(ptr->remote)) == 6) {
            ptr->next = new mount_entry;
            ptr = ptr->next;
            ptr->init();
        } else break;
    }
    fclose(fp);
    return mountList;
    
}

void releaseMountList(mount_entry* list)
{
    if(list == NULL)return;
    mount_entry *p = list;
    mount_entry *q = p->next;
    while(p) {
        delete p;
        p = q;
        if(q)q=q->next;
    }
    list=NULL;
}

mount_entry* findMountedDev(char* devPath,mount_entry* mountList)
{
    mount_entry* p = mountList;
    while(p != NULL && strncmp(p->devname, devPath, strlen(devPath))) {
        p = p->next;
    }
    return p;
}

int System(char* cmdBuf)
{
    int status = system(cmdBuf);
    int ret;
    if(-1 != status) {//
        if(WIFEXITED(status)) {//
            if(0 == WEXITSTATUS(status)) {//shell return
                ret = 0;
                log_print(_DEBUG,"System cmd \"%s\" success.\n",cmdBuf);
            } else {
                ret = WEXITSTATUS(status);
                log_print(_ERROR,"System cmd \"%s\" return non-zero value.\n",cmdBuf);
            }
        }else{
            log_print(_ERROR,"System cmd \"%s\" failed.\n",cmdBuf);
            ret = -11;
        }
    } else {
        log_print(_ERROR,"System func error\n");
        ret = -10;
    }
    return ret;
}


////////ExceptionProcessor
ExceptionProcessor::ExceptionProcessor()
{
    sdcard = new sdcardProcessor;
}

ExceptionProcessor::~ExceptionProcessor()
{
    if(sdcard) {
        delete sdcard;
        sdcard = NULL;
    }
}

ExceptionProcessor* ExceptionProcessor::pInstance = NULL;

ExceptionProcessor* ExceptionProcessor::GetInstance()
{
    if(pInstance == NULL) {
        pInstance = new ExceptionProcessor;
    }
    return pInstance;
}
void ExceptionProcessor::releaseInstance()
{
    if(pInstance) {
        delete pInstance;
        pInstance = NULL;
    }
}

////////sdcardProcessor
sdcardProcessor::sdcardProcessor()
{
    mFd = -1;
    mbNeedFormat = false;
    mbPassBootCheck = false;
    mbPassPrimaryCheck = false;
    mbStorageExist = false;
    mbStorageWritable = false;
    mbStorageFull = false;
    mbNeedRestore = false;
    mbLoaded = false;
    mbInserted = false;
    mTotalSpace = 0;
    mFreeSpace = 0;
    mUsedSpace = 0;
    mUsrCnt = 0;
    mRecStatus = 0;
    mpMountList = NULL;
    mpOverrideJob = NULL;
    //strcpy(mDevPath, DEV_PATH);
    //strcpy(mPartitionPath, PARTITION_PATH);
    //strcpy(mMountedPath, MOUNTED_PATH);
    if(pthread_mutex_init(&mThreadLock, NULL) != 0)
        log_print(_ERROR, "sdcardProcessor pthread_mutex_init ret < 0\n");
    if(pthread_mutex_init(&mUnloadLock, NULL) != 0)
        log_print(_ERROR, "sdcardProcessor pthread_mutex_init ret < 0\n"); 

    init();
}

sdcardProcessor::~sdcardProcessor()
{
    releaseMountList(mpMountList);
    if(mpOverrideJob != NULL) {
        delete mpOverrideJob;
        mpOverrideJob = NULL;
    }
}

void sdcardProcessor::initFlag()
{
    mbNeedFormat = false;
    mbPassBootCheck = false;
    mbPassPrimaryCheck = false;
    mbStorageExist = false;
    mbStorageWritable = false;
    mbStorageFull = false;
    mbNeedRestore = false;
}

void sdcardProcessor::initSocket()
{
    struct sockaddr_nl snl;// socket address
    const unsigned int uiRecvBuffSize = sizeof( mKernelMsgBuf);// size of msg buffer
    int ret;

    // 1.fill in socket addr
    snl.nl_family = AF_NETLINK;
    snl.nl_pad = 0;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    // 2.create socket
    // NETLINK_KOBJECT_UEVENT - msg from kernel to user Linux 2.6.10
    mFd = socket( PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT ); //PF_NETLINK: use netlink
    if ( -1 == mFd ) {
        log_print(_ERROR,"sdcardProcessor::initSocket() create socket error.\n");
    }

    // 3.set size of socket recv buffer
    setsockopt( mFd, SOL_SOCKET, SO_RCVBUF, &uiRecvBuffSize, sizeof( uiRecvBuffSize ) );//SO_RCVBUF: set recv buffer

    // 4.add socket to specified multicast group
    ret = bind( mFd, (struct sockaddr*)&snl, sizeof( snl ) );
    if ( -1 == ret ) {
        log_print(_ERROR,"sdcardProcessor::initSocket() bind socket error.\n");
    }
    log_print(_DEBUG,"sdcardProcessor::initSocket() OKAY.\n");
}

void sdcardProcessor::init()
{
    initSocket();
    if(mpOverrideJob == NULL)
        mpOverrideJob = new OverrideJob(this);
}

int sdcardProcessor::processKernelMsg()
{
    // recv msg from kernel
    recv(mFd,&mKernelMsgBuf,sizeof( mKernelMsgBuf ),0);
    log_print(_INFO, "sdcardProcessor::recvKernelMsg() Kernel Message:\n%s\n", mKernelMsgBuf );
    
    if( 0 == memcmp( mKernelMsgBuf,"add@",4)
        && NULL != strstr( mKernelMsgBuf,"mmcblk0" )
        && NULL == strstr( mKernelMsgBuf,"mmcblk0p" ))
    {
        log_print(_INFO,"##################sdcardProcessor::processKernelMsg() Add sdcard Device\n");
        usleep(1000);
          loadSdcard();
    }

    if( 0 == memcmp( mKernelMsgBuf,"remove@",7) 
        && NULL != strstr( mKernelMsgBuf, "mmcblk0" )
        && NULL == strstr( mKernelMsgBuf,"mmcblk0p" )) 
    {
        log_print(_INFO,"#################sdcardProcessor::processKernelMsg() Remove sdcard Device\n");
          unloadSdcard();
    }
    return 0;
}

/**
            Sdcard插拔事件后的处理流程
        */
void sdcardProcessor::loadSdcard()
{
    if(mbLoaded == false) {
        umountDevice(MOUNTED_PATH);//必须解除上一次mount，或者解除mount在该目录的设备
        mountDevice(PARTITION_PATH);
        bootDetection();
        clearFSCKfile();
        if(mbNeedRestore) {
            int ret = restoreSdcard();
            if(ret > 0) {
                //TODO: something to do after restore sdcard failed
            }
        }
        notifySdcardEvent();
        if(IsPassCheck()) {
            //mbLoaded = true;
            //startOverrideJob();//it already be included in commandRecOnoff() but bootRecord won't start it
            //restoreWriting(gRecIniParm.RecFile.bootRecord);//call by GUI
            restoreWriting(isReadyToRec());
        } else {
            stopWriting();
            //stopOverrideJob();//it already be included in commandRecOnoff()
            //mbLoaded = false;
        }
    }
}
//#include "recfilejob.h"
//extern RecFileJob *gpRecFileJob;
void sdcardProcessor::unloadSdcard()
{
    pthread_mutex_lock(&mUnloadLock);
    if(mbLoaded == true) {
        stopWriting();
        stopOverrideJob();
        while(umountDevice(MOUNTED_PATH) != 0) {
            sleep(1);           
        }
        mbLoaded = false;
    }
    bootDetection(false);
    printf("#########unloadSdcard mbPassPrimaryCheck %d\n",mbPassPrimaryCheck);
    notifySdcardEvent();
    pthread_mutex_unlock(&mUnloadLock);
}

void sdcardProcessor::refreshSdcardStorage()
{
    if(mbStorageExist && mbStorageWritable)
        mbPassPrimaryCheck = storageFullDetection();
    
    if(mbPassPrimaryCheck == false) {
        //stopOverrideJob();
        stopWriting();
    }
    notifySdcardEvent();
}


/**
            开机后的检查流程
        */
void sdcardProcessor::bootDetection(bool needCheckRestore)
{
    ++mUsrCnt;
    pthread_mutex_lock(&mThreadLock);
    primaryDetection();
    pthread_mutex_unlock(&mThreadLock);
    
    if(mbPassPrimaryCheck){
        pthread_mutex_lock(&mThreadLock);
        if(mbPassPrimaryCheck){
            pthread_mutex_unlock(&mThreadLock);
            if(needCheckRestore){
                int ret = checkFileFlag();
                if(ret == 0) {
                    mbNeedRestore = false;
                    log_print(_DEBUG,"don't need restore\n");
                } else if(ret == 1) {
                    mbNeedRestore = true;
                    log_print(_INFO,"need restore\n");
                } else {
                    log_print(_ERROR,"bootDetection from checkFileFlag: unhandle fault!\n");
                }
                mbPassBootCheck = !mbNeedRestore;
            } else {
                mbPassBootCheck = mbPassPrimaryCheck;
                mbNeedRestore = false;
            } //if(needCheckRestore)
        } else {
            pthread_mutex_unlock(&mThreadLock);
        }
    } else {
        mbPassBootCheck = false;
        mbNeedRestore = false;
    }
    pthread_mutex_lock(&mThreadLock);
    if(--mUsrCnt != 0){//somewhere else still running detection
        pthread_mutex_unlock(&mThreadLock);
        return;
    }
    pthread_mutex_unlock(&mThreadLock);
    //notifySdcardEvent();
}

void sdcardProcessor::notifySdcardEvent()
{
    if(mbPassPrimaryCheck)
    {   
        if(mRecStatus != REC_STORAGE_OKAY)
        {
            mRecStatus = REC_STORAGE_OKAY;
            NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_OKAY);
            log_print(_INFO,"##NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_OKAY);\n");
        }
    }
    else
    {
        if(mbStorageExist)
        {
            if((mbStorageWritable == false) && (mRecStatus != REC_STORAGE_UNWRITABLE))
            {
                mRecStatus = REC_STORAGE_UNWRITABLE;
                NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_UNWRITABLE);
                log_print(_ERROR,"##NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_UNWRITABLE);\n");
            } 
            else if(mbStorageFull && (mRecStatus != REC_STORAGE_FULL))//only when overridejob has no more items to delete or mOverride is disabled will go into this branch
            {
                mRecStatus = REC_STORAGE_FULL;
                log_print(_ERROR,"##NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_FULL);\n");
                NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_FULL);
            }
        }
        else
        {
            if(mbNeedFormat && (mRecStatus != REC_STORAGE_ERROR)) {
                mRecStatus = REC_STORAGE_ERROR;
                NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_ERROR);
                log_print(_ERROR,"##NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_STORAGE_ERROR);\n");
            }else if(mRecStatus != REC_NO_STORAGE) {
                if(gMediaStatus == MEDIA_PLAYBACK)// stop playing immediately when pull out the sdcard at the time of playback.
                {
                    gpPlayBackJob->stopPlay(false);  
                    sendEndFileEvent(PLAYBACK_NORMAN_END);
                }
                mRecStatus = REC_NO_STORAGE;
                NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_NO_STORAGE);
                log_print(_ERROR,"##NotifyEventToGui(OPC_REC_STORAGE_ERROR_EVENT,REC_NO_STORAGE);\n");
            }
        }
    }
}


void sdcardProcessor::printFlag()
{
    printf("&&&&&&mbStorageExist %d,mbStorageWritable %d,mbStorageFull %d,mbNeedFormat %d,mbNeedRestore %d,mbPassPrimaryCheck %d,mbPassBootCheck %d\n",
        mbStorageExist,mbStorageWritable,mbStorageFull,mbNeedFormat,mbNeedRestore,mbPassPrimaryCheck,mbPassBootCheck);
}

/**
            写文件标识，用以甄别文件完整
        */
int sdcardProcessor::writeFileFlag(const char* filePath)
{
    if(filePath == NULL)
        return -1;
    int ret = -1;
    if(IsPassCheck()){
        char tmpFile[128];
        sprintf(tmpFile,"%s~",filePath);
        log_print(_INFO, "sdcardProcessor::writeFileFlag() %s\n", tmpFile); 
        FILE *fp = fopen(tmpFile,"w+");
        if(fp != NULL) {
            ret = 0;
            fclose(fp);
        }
        else ret = -1;
    }
    return ret;
}

int sdcardProcessor::removeFileFlag(const char* filePath)
{
    if(filePath == NULL)
        return -1;
    int ret = -1;
    if(IsPassCheck()) {
        char tmpFile[128];
        sprintf(tmpFile,"%s~",filePath);
        log_print(_INFO, "sdcardProcessor::removeFileFlag() %s\n", tmpFile);    
        ret = remove(tmpFile);
    }
    return ret;
}

void sdcardProcessor::clearFileFlag(const char* path)
{
    if(path == NULL) return;
    log_print(_INFO,"sdcardProcessor::clearFileFlag.\n");
    if(IsPassCheck()) {
        char cmdBuf[128];
        sprintf(cmdBuf,"find %s -name \"*~\" -type f -exec rm -f {} \\;",gRecIniParm.RecFile.path);
        log_print(_INFO, "gRecIniParm.RecFile.path %s\n",gRecIniParm.RecFile.path);
        //sprintf(cmdBuf,"find %s -name \"*~\" -type f -exec rm -f {} \\;",MOUNTED_PATH);
        System(cmdBuf);
    }
    
}

void sdcardProcessor::clearFSCKfile()
{
    log_print(_INFO, "sdcardProcessor::clearFSCKfile.\n");
    if(IsPassCheck()) {
        char cmdBuf[128];
        sprintf(cmdBuf, "find %s -name \"FSCK*.REC\" -type f -exec rm -f {} \\;", MOUNTED_PATH);
        System(cmdBuf);
    }
}
    
void sdcardProcessor::stopWriting()
{
    //commandRecEmergencyOnOff(0);
    if(gMediaStatus == MEDIA_RECORD) {
        gMediaStatus = MEDIA_IDLE_MAIN;
        commandRecOnOff(3);
    } else if(gMediaStatus == MEDIA_IDLE) {
        commandRecOnOff(0);
    }
}

void sdcardProcessor::restoreWriting(int continueRec)
{
    log_print(_INFO, "sdcardProcessor::restoreWriting continueRec %d MediaStatus %d\n", continueRec, gMediaStatus);
    if(gMediaStatus == MEDIA_IDLE_MAIN) {
        if(continueRec)
            commandRecOnOff(1);
        else
            commandRecOnOff(3);
    }
}

int sdcardProcessor::mountDevice(char* devPath)
{
    int ret = -1;
    if(checkDevExist(devPath) == 0){
        log_print(_ERROR,"####################sdcardProcessor::mountDevice() device not exists.\n");
        return ret = -1;
    }
    if((ret = mount(devPath,MOUNTED_PATH,"vfat",MS_SYNCHRONOUS, NULL)) != 0){
        mbLoaded = false;
        perror("#################sdcardProcessor::mountDevice() mount");
        log_print(_ERROR,"##################sdcardProcessor::mountDevice() mount failed mount %s %s.\n",devPath,MOUNTED_PATH);      
    }
    else {
        mbLoaded = true;
        log_print(_INFO,"#################sdcardProcessor::mountDevice() mount success.\n");
    }
    
    return ret;
}
int sdcardProcessor::umountDevice(char *devPath)
{
    log_print(_INFO, "#################umountDevice\n");
    int ret = -1;
    #if 0
    if(umount(devPath) != 0)
            perror("sdcardProcessor::umountDevice() umount"); 
    #endif
    char cmdBuf[128];
    if(gMediaStatus == MEDIA_PLAYBACK) // lazy umount when pull out the sdcard at the time of playback.
        sprintf(cmdBuf, "umount -l %s", devPath);
    else
        sprintf(cmdBuf, "umount -f %s", devPath);
    ret = System(cmdBuf);
    return ret;
}
int sdcardProcessor::format()
{
    //if sdcard not exists,return
    if(checkDevExist(DEV_PATH) == 0) {
        log_print(_ERROR, "sdcardProcessor::format() sdcard not exists.\n");
        return -1;
    }
    if(mbLoaded) {
        unloadSdcard();
    }
    #if 0
    //if sdcard is mounted,umount it
    if(checkIsMounted(DEV_PATH) == 1){
    
        int ret=-1;
        stopWriting();
        usleep(1000);
        char devPath[128];
        sprintf(devPath,"%sp*",DEV_PATH);
        for(int i=0;i<3;++i)
            if((ret = umountDevice(devPath)) == 0)break;
            else usleep(5000);
        if(ret != 0){
            log_print(_INFO,"sdcardProcessor::format() umountDevice failed,ret %d.\n",ret);
            return -1;
        }
    
    }
    #endif
    char cmdBuf[128];
    int ret = -1;
    unsigned long long capacity = getTotalSpace();
    if(capacity <= 0) {
        log_print(_ERROR,"sdcardProcessor::format() capacity <= 0.\n");
        return -2;
    }
    sprintf(cmdBuf, "./fdisk.sh %s >> /dev/null ", DEV_PATH);
    
    ret = System(cmdBuf);//re-partition and change file system
    if(ret == 0) {
        if((capacity>>30) >= 7)
         sprintf(cmdBuf, "mkdosfs -F 32 -s 128 %s", PARTITION_PATH);
        else if((capacity>>30) >= 3)
         sprintf(cmdBuf, "mkdosfs -F 32 -s 64 %s", PARTITION_PATH);
        else if((capacity>>30) >= 1)
         sprintf(cmdBuf, "mkdosfs -F 32 -s 32 %s", PARTITION_PATH);
        else
         sprintf(cmdBuf,"mkdosfs %s",PARTITION_PATH);
        ret = System(cmdBuf);//build DOS file system
    }
    if(ret != 0) {
        log_print(_ERROR, "sdcardProcessor::format() %s failed.\n", cmdBuf);
        return ret;
    }
    usleep(1000);
    
    loadSdcard();

    return 0;
        
}
int sdcardProcessor::checkFileFlag()
{
    //if read file in sdcard,should check mPassPrimaryCheck first
    int ret = 0;
    if(mbPassPrimaryCheck) {
        char cmdBuf[128];
        sprintf(cmdBuf,"find %s -type f | grep \".*~\"",gRecIniParm.RecFile.path);
        //sprintf(cmdBuf,"find %s -type f | grep \".*~\"",MOUNTED_PATH);
        int status = System(cmdBuf);
        if(status == 1) {
            ret = 0;
        } else if(status == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
    } else {
        log_print(_ERROR,"sdcardProcessor::checkFileFlag mbPassPrimaryCheck false.\n");
        ret = 404;
    }
    log_print(_INFO,"#####sdcardProcessor::checkFileFlag. ret %d\n",ret);
    return ret;
}
int sdcardProcessor::restoreSdcard()
{
    //if restore sdcard,should check mPassPrimaryCheck first
    int ret = -1;
    if(mbPassPrimaryCheck) {
        char cmdBuf[128];
        sprintf(cmdBuf, "dosfsck -a %s", PARTITION_PATH);
        log_print(_INFO, "#########sdcardProcessor::restoreSdcard.\n");
        printf("\n-----dosfsck start-----\n");
        ret = System(cmdBuf);
        printf("-----dosfsck end-----\n\n");

        clearFileFlag(MOUNTED_PATH);

        mbNeedRestore = false;
        mbPassBootCheck = true;
    }
    return ret;
}
void sdcardProcessor::refreshSdcardInfo()
{
    unsigned long long blocksize;
    if(mbInserted) {
        if(checkIsMounted(PARTITION_PATH) == 1) {
            struct statfs diskInfo;
            if(statfs(MOUNTED_PATH, &diskInfo) != 0) {
                log_print(_ERROR, "sdcardProcessor::refreshSdcardInfo() statfs:%s\n", strerror(errno));
                mFreeSpace = 0;
                mTotalSpace = 0;
            } else {
                blocksize = diskInfo.f_bsize;
                mFreeSpace = diskInfo.f_bfree * blocksize;
                mTotalSpace = diskInfo.f_blocks * blocksize;
            }
            //assert(mTotalSpace > mFreeSpace);
            mUsedSpace = mTotalSpace - mFreeSpace;
            log_print(_DEBUG, "sdcardProcessor::refreshSdcardInfo() mounted:free:%llu,total:%llu.\n", mFreeSpace, mTotalSpace);
        } else {
            int fd = open(DEV_PATH, O_RDONLY);
            if(fd >= 0) {
                if (ioctl(fd, BLKGETSIZE64, &mTotalSpace) != 0) {
                    log_print(_ERROR,"sdcardProcessor::refreshSdcardInfo() ioctl BLKGETSIZE64 failed.\n");
                }
                close(fd);
            }
            mUsedSpace = 0;
            mFreeSpace = 0;
        }
    } else {
        mFreeSpace = 0;
        mTotalSpace = 0;
        mUsedSpace = 0;
    }
    if((mFreeSpace>>10) > THRESHOLD) mbStorageFull = false;
    else mbStorageFull = true;
    //printf("free %lld,total %lld,used: %lld\n",mFreeSpace,mTotalSpace,mUsedSpace);
}
unsigned long long sdcardProcessor::getFreeSpace()
{
    refreshSdcardInfo();
    return mFreeSpace;
}
unsigned long long sdcardProcessor::getTotalSpace()
{
    refreshSdcardInfo();
    return mTotalSpace;
}
unsigned long long sdcardProcessor::getUsedSpace()
{
    refreshSdcardInfo();
    return mUsedSpace;
}
void sdcardProcessor::updateMountList()
{
    if(mpMountList != NULL)
        releaseMountList(mpMountList);
    mpMountList = readMountList();
}
int sdcardProcessor::checkIsMounted(char* devPath)//return 1 when dev is mounted
{
    int ret=-1;
    updateMountList();
    mount_entry* p = findMountedDev(devPath, mpMountList);
    if(p == NULL) {
        log_print(_ERROR,"sdcardProcessor::checkIsMounted(): sdcard hasn't been mounted yet.\n");
        return 0;
    }
    if(p && strcmp(p->mountdir,MOUNTED_PATH) != 0) {
        log_print(_ERROR, "sdcardProcessor::checkIsMounted(): sdcard was mounted on wrong path.%s %s\n", p->mountdir, MOUNTED_PATH);
        return 2;
    }
    ret = 1;
    
    return ret;
}

int sdcardProcessor::checkIsWritable(char* devPath)
{
    updateMountList();
    updateMountList();
    mount_entry* p = findMountedDev(devPath, mpMountList);
    if(p == NULL) {
        log_print(_ERROR,"sdcardProcessor::checkIsWritable(): sdcard hasn't been mounted yet.\n");
        return -2;
    }
    if(p && strstr(p->property, "rw") == NULL) {
        log_print(_ERROR, "sdcardProcessor::checkIsWritable(): mounted unwritable.%s\n", p->property);
        return 0;
    }
    return 1;
    
}

int sdcardProcessor::checkDevExist(char *devPath)//return 1 when dev exists
{
    int ret = 0;
    int fd = open(devPath, O_RDONLY);
    log_print(_INFO, "sdcardProcessor::checkDevExist open %s fd %d.\n", devPath, fd);
    if(fd >= 0) {
        ret = 1;
        int val = close(fd);
        log_print(_DEBUG, "sdcardProcessor::checkDevExist close fd %d ret %d.\n", fd, val);
        
    } else perror("sdcardProcessor::checkDevExist open ");
    return ret;
}
void sdcardProcessor::primaryDetection()
{
    #if 1
    int ret = checkDevExist(DEV_PATH);
    mbInserted = (ret == 1) ? true : false;

    if(mbInserted == false) {
        mbStorageExist = false;
        mbStorageFull = false;
        mbNeedFormat = false;
        mbPassPrimaryCheck = false;
        return;
    }
    #endif
//check mount
    bool isMounted = false;
    ret = checkIsMounted(PARTITION_PATH);
    if(ret == 1)
        isMounted = true;
    if(isMounted == false) {
        mbStorageExist = false;
    } else {
        mbStorageExist = true;
    }
    if(mbStorageExist) {
        if(writableDetection() == false)
            mbPassPrimaryCheck = false;
    } else {
        if(mbInserted) {
            mbNeedFormat = true;
        } else {
            mbNeedFormat = false;
        }
    }
    if(!mbStorageExist || !mbStorageWritable) {
        mbPassPrimaryCheck = false;
    } else {
        mbPassPrimaryCheck = storageFullDetection();
    } 
}

bool sdcardProcessor::writableDetection()
{
    #if 1
        FILE *fp = NULL;
        char tmpfile[128];
        sprintf(tmpfile, "%s/testWritableFile", MOUNTED_PATH);
        fp = fopen(tmpfile,"w+");
        if(fp == NULL) {
            mbStorageWritable = false;
            log_print(_ERROR, "sdcardProcessor::writableDetection() cannot write.\n");
        } else {
            mbStorageWritable = true;
            mkdir(gRecIniParm.RecFile.path, 0777);
            mkdir(gCapIniParm.PicCapParm.path, 0777);
            fclose(fp);
            remove(tmpfile);
        }
        return mbStorageWritable;
    #else
        if(checkIsWritable(PARTITION_PATH) != 1)
            mbStorageWritable = false;
        else{
            mbStorageWritable = true;
            mkdir(gRecIniParm.RecFile.path,0777);
            mkdir(gCapIniParm.PicCapParm.path,0777);
        }
        return mbStorageWritable;
    #endif
}

bool sdcardProcessor::storageFullDetection()
{
    refreshSdcardInfo();
    if(mbStorageFull) {
        if(mTotalSpace == 0) {
            log_print(_ERROR, "sdcardProcessor::storageFullDetection() cannot read mmcblk storage.\n");
            return false;
        }
        log_print(_INFO, "sdcardProcessor::storageFullDetection() storage full,canOverride %d.\n", mpOverrideJob->canOverride());
        if(mpOverrideJob->canOverride() == false)
            return false;
    }
    return true;
}
/**
            控制线程overridejob开始
        */
void sdcardProcessor::startOverrideJob()
{
    //if(mbLoaded == false) {
    //if(mbPassPrimaryCheck == false) {
        //log_print(_ERROR,"sdcardProcessor::startOverrideJob no SDcard\n");
        //return;
    //}
    log_print(_INFO,"sdcardProcessor::startOverrideJob.\n");
    
    mpOverrideJob->refresh();
    if(mpOverrideJob->isRunning() == false) {
        mpOverrideJob->start();
    } else {
        mpOverrideJob->resume();
    }
}

void sdcardProcessor::stopOverrideJob()
{
    if(mpOverrideJob == NULL)
        return;
    //log_print(_INFO,"sdcardProcessor::stopOverrideJob.\n");
    mpOverrideJob->pause();
}

void sdcardProcessor::turnOnOverride(bool onoff)
{
    if(mpOverrideJob == NULL)
        return;
    log_print(_INFO,"sdcardProcessor::turnOnOverride onoff %d\n",onoff);
    mpOverrideJob->setOverride(onoff);
    refreshSdcardStorage();
}
