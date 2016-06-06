#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

class Thread
{
public:
    Thread(int id = 0);
     ~Thread();

    void start();
    void stop();
    void join();
    int getThreadId() {return m_threadid;}
    bool isRunning();
    virtual void run() = 0;

private:
    //private copy-ctor; prevent copied
    Thread(const Thread&);
    //private assignment operator; prevent copied
    Thread& operator=(const Thread&);

    friend void *_threadFunc(void *);

protected:
    bool              m_running;

private:
    pthread_t         m_thread;
    pthread_attr_t    m_attr;
    int               m_threadid;
    bool              m_isJoined;

};

#endif

