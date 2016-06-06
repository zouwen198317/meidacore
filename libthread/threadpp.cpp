#include "threadpp.h"
#include <stdio.h>
#include "log/log.h"
void *_threadFunc(void *obj)
{
    void *retval = 0;
    Thread *thread = static_cast<Thread *>(obj);
    thread->m_running = true;
    thread->run();
    thread->m_running = false;
    log_print(_INFO, "ThreadFunc#%d exit\n", thread->m_threadid);
    pthread_exit(NULL);
    return retval;
}


Thread::Thread(int id)
       :m_running(false),
	m_isJoined(false),
        m_threadid(id)
{
    size_t stacksize;
    pthread_attr_init(&m_attr);
    pthread_attr_getstacksize(&m_attr, &stacksize);
    //stacksize *= 2;
    //pthread_attr_setstacksize(&m_attr, stacksize);
    log_print(_DEBUG,"Thread#%d stack size %d\n", m_threadid, stacksize);

}

Thread::~Thread()
{
    
    if (m_running)
        pthread_cancel(m_thread);
    if(!m_isJoined)
    	join();
    pthread_attr_destroy(&m_attr);
    log_print(_INFO,"Thread#%d destory\n", m_threadid);

}

void Thread::start()
{

    log_print(_DEBUG, "Thread#%d start() running=%d\n", m_threadid, m_running);
    if (m_running)
        return;

    if (pthread_create(&m_thread, &m_attr, _threadFunc, static_cast<void *>(this)) == 0)
    {
    }

}

void Thread::stop()
{
    log_print(_DEBUG, "Thread#%d stop() running=%d\n", m_threadid, m_running);
    //pthread_cancel(m_thread);
    m_running = false;
}

void Thread::join()
{
    //wait for the running thread to exit
    void *retValue;
    m_isJoined = true;

    log_print(_DEBUG,"Thread#%d join() running=%d joined=%d\n", m_threadid, m_running, m_isJoined);
    //pthread_attr_destroy(&m_attr);
    stop();
    if (pthread_join(m_thread, &retValue) == 0)
    {
    }
}

bool Thread::isRunning()
{
    return m_running;
}

