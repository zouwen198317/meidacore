#include <fcntl.h>             // 提供open()函数  
#include <unistd.h>  
#include <stdio.h>  
#include <dirent.h>            // 提供目录流操作函数  
#include <string.h>  
#include <sys/stat.h>        // 提供属性操作函数  
#include <sys/statfs.h> 
#include <sys/types.h>         // 提供mode_t 类型  
#include <stdlib.h> 
#include <stdlib.h> 
#include <signal.h>
#include "overridejob.h"
#include "log/log.h"
#include "debug/errlog.h"
#include "recfilejob.h"
#include "mediaparms.h"
#include "mediacore.h"
#include "opccommand.h"
#include "ipc/ipcclient.h"
#include "parms_dump.h"
//#define REC_FILE_PATH "./"
extern MsgQueue *gpOverrideMsgQueue;
extern RecFileJob *gpRecFileJob;
extern IpcClient *gpMediaCoreIpc;
extern RecFileIniParm gRecIniParm;
extern int renameSuffix(char *srcName, char *suffix);

OverrideJob::OverrideJob(sdcardProcessor *sdcard)
{
    //buildQueueSortedbyTime((char*)REC_FILE_PATH);
    buildQueueSortedbyTime((char*)gRecIniParm.RecFile.path);
    mRunning = false;
    mOverride = gRecIniParm.RecFile.override ? true : false;
    mPause = false;
    mIntr = false;
    isSleeping = false;
    mpSdcard = sdcard;
}
OverrideJob::~OverrideJob(){}

bool OverrideJob::checkAvFileSubfix(char *name)
{
    if((strstr(name,".avi") || strstr(name,".mp4") || strstr(name,".mov") || strstr(name,".flv")) && strstr(name,"~") == NULL) {
        return true;
    } else {
        return false;
    }
}

void OverrideJob::refresh()
{
    if(mPause || mRunning == false) {
        log_print(_INFO, "OverrideJob::refresh()\n");
        while(!tobeDeletedQueue.empty()) {
            tobeDeletedQueue.pop();
        }
        if(mpSdcard->IsExist())
            buildQueueSortedbyTime((char*)gRecIniParm.RecFile.path);
    }
}

bool OverrideJob::isDirEmpty(const char* path)
{
    DIR *dp;
    struct dirent* entry;
    int count = 0;
    if((dp = opendir(path)) == NULL) {
        log_print(_ERROR,"OverrideJob::isDirEmpty: cannot open file path %s\n",path);
        return false;
    }
    while((entry = readdir(dp)) != NULL) {
        if(strcmp(".", entry->d_name)==0 || strcmp("..", entry->d_name)==0)
            continue;
        ++count;
    }
    closedir(dp);
    if(count)
        return false;
    return true;
}

bool OverrideJob::canOverride()
{
    if(mOverride == false)
        return false;
    if(tobeDeletedQueue.empty())
        refresh();
    return !tobeDeletedQueue.empty();
}


void OverrideJob::buildQueueSortedbyTime(char* rootpath)
{
    log_print(_INFO, "#########################buildQueueSortedbyTime\n");
    struct tm *pTm = NULL;
    struct tm fileTm;
    time_t nowTime = time(NULL);
    pTm = gmtime(&nowTime);
    memcpy(&fileTm, pTm, sizeof(struct tm));
    pTm = &fileTm;
    
    char expiryDate[MAX_PATH_NAME_LEN];//the date n months ago
    memset(expiryDate, 0, MAX_PATH_NAME_LEN);
    
    if(1+pTm->tm_mon > EMC_FILE_EXPIRY_DATE) {//calculate the date n months ago
        snprintf(expiryDate, 64, "%04d%02d%02d%02d%02d%02d_emc.avi",
            (1900+pTm->tm_year), (1+pTm->tm_mon-EMC_FILE_EXPIRY_DATE), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    } else {
        snprintf(expiryDate, 64, "%04d%02d%02d%02d%02d%02d_emc.avi",
            (1900+pTm->tm_year-1), (1+pTm->tm_mon-EMC_FILE_EXPIRY_DATE+12), pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    }
    log_print(_DEBUG, "OverrideJob buildCycleRecQueue() expiryDate %s\n", expiryDate);
    
    buildCycleRecQueue(rootpath, expiryDate);
}

void OverrideJob::buildCycleRecQueue(char* rootpath, char *expiryDate)
{
    //printf("#########################buildQueueSortedbyTime\n");
    char cur_path[MAX_PATH_NAME_LEN];
    getcwd(cur_path,MAX_PATH_NAME_LEN);
    DIR *dp;
    struct dirent* entry;
    struct stat statbuf;
    
    if((dp = opendir(rootpath)) == NULL) {
        log_print(_ERROR, "cannot open rec file path %s\n", rootpath);
        return;
    }
    chdir(rootpath);
    while((entry = readdir(dp)) != NULL) {
        lstat(entry->d_name, &statbuf);
        if(S_IFDIR & statbuf.st_mode) {//directory
            if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue; 
            char dir_path[MAX_PATH_NAME_LEN];
            sprintf(dir_path, "%s/%s/", rootpath, entry->d_name);
            if(isDirEmpty(dir_path))
                rmdir(dir_path);
            else
                buildCycleRecQueue(dir_path, expiryDate);
        } else {
            //if(strchr(entry->d_name,'_')==NULL && (checkAvFileSubfix(entry->d_name) == true))
            if(checkAvFileSubfix(entry->d_name) == true) {
                //if(strchr(entry->d_name,'_')!=NULL)
                if(strstr(entry->d_name, "_emc") != NULL) {
                    if(strcmp(entry->d_name, expiryDate) > 0) {//emc file will be delete if it was recorded n months ago.
                        log_print(_DEBUG, "OverrideJob buildCycleRecQueue %s is ok\n", entry->d_name);
                        continue;
                    }
                    log_print(_INFO, "OverrideJob buildCycleRecQueue() %s will be overrided\n", entry->d_name);
                }
                char file_path[MAX_PATH_NAME_LEN];
                sprintf(file_path, "%s/%s", rootpath, entry->d_name);
                PushPath(file_path);
            }
        }
    }
    closedir(dp);
    chdir(cur_path);
}

void OverrideJob::PushPath(char* path)
{
    tobeDeletedQueue.push(std::string(path));
    log_print(_DEBUG, "OverrideJob run(): add file @ %s\n", path);
}

void OverrideJob::PopPath()
{
    int ret;
    std::string path = tobeDeletedQueue.top();
    ret = remove(path.data());
    if(ret == 0) {
        log_print(_INFO, "OverrideJob run() rm file @ %s(deleted)\n", path.data());
    }
#if DUMP_ALG_FILE
    char dataFileName[MAX_PATH_NAME_LEN];
    strcpy(dataFileName, path.data());
    renameSuffix(dataFileName, ".dat");
    ret = remove(dataFileName);
    if(ret == 0) {
        log_print(_INFO, "OverrideJob run() rm file @ %s(deleted)\n", dataFileName);
    }
#endif
    tobeDeletedQueue.pop();
    sync();
}

void OverrideJob::exit()
{
    if(mRunning) {
        mRunning = false;
        wakeup();
        //pthread_kill(pid,SIGUSR1);
    }
}
void OverrideJob::pause()
{
    if(mPause==false) { 
        mPause = true;
        log_print(_INFO, "OverrideJob pause()\n");
    }
}
void OverrideJob::resume()
{
    if(mPause) {
        mPause = false;
        //clear all msg
        while(gpOverrideMsgQueue->msgNum() > 0) {
            msgbuf_t msg;
            gpOverrideMsgQueue->recv(&msg);
        }
        wakeup();
        log_print(_INFO, "OverrideJob resume()\n");
    }
}
void OverrideJob::setOverride(bool onoff)
{
    if(onoff != mOverride) {
        mOverride = onoff;
    }
}
//XXX
void OverrideJob::Sleep(int sec)
{
    if(sec <= 0) return;
    isSleeping = true;
    for(int cur=0; cur<sec; ++cur) {
        ::sleep(1);
        if(mIntr == true)
            break;
    }
    isSleeping = false;
    mIntr = false;
}
void OverrideJob::wakeup()
{
    if(isSleeping)
        mIntr = true;
}
//I want to wake up this thread from sleep at first,but actually it may cause some problems.
//void sigusr1_handler(int signo){log_print(_DEBUG,"Override recieved signal usr1.\n");}
void OverrideJob::run()
{
    mRunning = true;
    msgbuf_t msg;
    unsigned long long availableDisk;
    //register usr1 signal
    //in order to get out of sleep func
    //pid=pthread_self();
    //signal(SIGUSR1,sigusr1_handler);
    
    setErrLogPath((char*)ERROR_LOG_PATH);  
    setErrlogVersion((char*)VERSION); 
    Register_debug_signal();
    log_print(_DEBUG,"OverrideJob run()\n");
    
    //refresh(); //must have refresh in startOverrideJob()
    mPause = false;
    
    while(mRunning) {
        if(mPause) {
            //log_print(_INFO, "OverrideJob run() pause %d,mOverride %d\n", mPause, mOverride);
            Sleep(10);
            continue;
        }
        while(gpOverrideMsgQueue->msgNum() > 0) {//receive msg 
            gpOverrideMsgQueue->recv(&msg);
            log_print(_INFO, "OverrideJob run():new file @ %s\n", msg.data);
            PushPath(msg.data);
        }
        //check memory
        availableDisk = mpSdcard->getFreeSpace()>>10;//KB
        log_print(_INFO, "OverrideJob run() availableDisk %lld MB\n", availableDisk>>10);
        //if(availableDisk >= THRESHOLD+3*1024)//more than 303MB
        if(availableDisk >= THRESHOLD + MAX_SIZE_PER_MIN * 1024) {//more than 390MB
            //int sleepTime = (((availableDisk>>10)-300)/3)<<3;//3M/8 per sec 
            //int sleepTime = (((availableDisk-THRESHOLD)>>10))/3;
            int sleepTime = ((((availableDisk - THRESHOLD) >> 10)) / MAX_SIZE_PER_MIN) * 60;

            log_print(_INFO, "OverrideJob run() sleep time %d sec.\n", sleepTime);
            Sleep(sleepTime);
            continue;
        }
        if(availableDisk >= THRESHOLD) {
            //Sleep(8);
            //Sleep(1);
            log_print(_INFO, "OverrideJob run() sleep time 60 sec.\n");
            Sleep(60);
            continue;
        }
        //刚开机如果容量不足，也是会删的...
        if(availableDisk < THRESHOLD) {//less than 300MB
            //log_print(_INFO,"OverrideJob run(): current memory %lldKB\n",availableDisk);
        /**
            if we can override video,we pop item from queue which is not empty immediately.
            if the queue is empty or we can't override video,we should stop recording 
              and notify GUI.
        */
            if(mOverride && !tobeDeletedQueue.empty()) {
                PopPath();
            } else {
                if(mOverride) {
                    log_print(_ERROR, "OverrideJob run(): no more items to be deleted.\n");
                    sleep(1);
                } else {
                    log_print(_ERROR, "OverrideJob run(): storage full.\n");
                }
            }
            mpSdcard->refreshSdcardStorage();
        }       
    }
    log_print(_ERROR,"OverrideJob exit()\n");
}
    
