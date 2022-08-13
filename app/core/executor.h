//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_EXECUTOR_H
#define JARVIS_EXECUTOR_H
#include <pthread.h>

#include "c-list.h"
#include "thread-pool.h"

#define ES_STATE_FINISHED	    0
#define ES_STATE_ERROR		    1
#define ES_STATE_CANCELED	    2

class ExecQueue;
class ExecSession;

class ExecQueue
{
    friend class Executor;
public:
    int init ();
    void deInit ();

private:
    struct list_head            mTaskList;
    pthread_mutex_t             mMutex;

public:
    virtual ~ExecQueue() {}
};

class ExecSession
{
    friend class Executor;
private:
    virtual void execute () = 0;
    virtual void handle (int state, int error) = 0;

protected:
    ExecQueue *getQueue() { return this->mQueue; }

private:
    ExecQueue*                  mQueue;

public:
    virtual ~ExecSession() { }
};


class Executor
{
public:
    int init(size_t nThreads);
    void deInit();

    int request(ExecSession* session, ExecQueue* queue);

private:
    ThreadPool*                 mThreadPool;

private:
    static void executor_thread_routine(void *context);
    static void executor_cancel_tasks(const ThreadPoolTask* task);

public:
    virtual ~Executor() { }
};


#endif //JARVIS_EXECUTOR_H
