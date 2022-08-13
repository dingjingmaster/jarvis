//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_COMMON_SCHEDULER_H
#define JARVIS_COMMON_SCHEDULER_H

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

#include "communicator.h"

class CommSchedGroup;

class CommSchedObject
{
    friend class CommScheduler;
public:
    size_t getMaxLoad() { return this->mMaxLoad; }
    size_t getCurLoad() { return this->mCurLoad; }

private:
    virtual CommTarget* acquire(int waitTimeout) = 0;

protected:
    size_t                      mMaxLoad;
    size_t                      mCurLoad;

public:
    virtual ~CommSchedObject() { }
};


class CommSchedTarget : public CommSchedObject, public CommTarget
{
    friend class CommSchedGroup;
public:
    int init(const struct sockaddr *addr, socklen_t addrLen,int connectTimeout, int responseTimeout, size_t maxConnections);
    void deInit();

public:
    int init(const struct sockaddr *addr, socklen_t addrLen, SSL_CTX *sslCtx, int connectTimeout, int sslConnectTimeout, int responseTimeout, size_t maxConnections)
    {
        int ret = this->init(addr, addrLen, connectTimeout, responseTimeout, maxConnections);

        if (ret >= 0) {
            setSSL(sslCtx, sslConnectTimeout);
        }

        return ret;
    }

private:
    virtual void release(int keepAlive); /* final */
    virtual CommTarget *acquire(int waitTimeout); /* final */

private:
    CommSchedGroup*                 mGroup;
    int                             mIndex;
    int                             mWaitCnt;
    pthread_mutex_t                 mMutex;
    pthread_cond_t                  mCond;
};

class CommSchedGroup : public CommSchedObject
{
    friend class CommSchedTarget;
public:
    int init();
    void deInit();
    int add(CommSchedTarget *target);
    int remove(CommSchedTarget *target);

private:
    virtual CommTarget *acquire(int waitTimeout); /* final */

private:
    CommSchedTarget**                   mTgHeap;
    int                                 mHeapSize;
    int                                 mHeapBufSize;
    int                                 mWaitCnt;
    pthread_mutex_t                     mMutex;
    pthread_cond_t                      mCond;

private:
    void heapify(int top);
    void heap_remove(int index);
    int heap_insert(CommSchedTarget *target);
    void heap_adjust(int index, int swapOnEqual);
    static int target_cmp(CommSchedTarget *target1, CommSchedTarget *target2);
};

class CommScheduler
{
public:
    int init(size_t pollThreads, size_t handlerThreads)
    {
        return this->comm.init(pollThreads, handlerThreads);
    }

    void deInit()
    {
        this->comm.deInit();
    }

    /* wait_timeout in milliseconds, -1 for no timeout. */
    int request(CommSession *session, CommSchedObject *object, int wait_timeout, CommTarget **target)
    {
        int ret = -1;

        *target = object->acquire(wait_timeout);
        if (*target) {
            ret = this->comm.request(session, *target);
            if (ret < 0) {
                (*target)->release(0);
            }
        }

        return ret;
    }

    /* for services. */
    int reply(CommSession *session)
    {
        return this->comm.reply(session);
    }

    int push(const void *buf, size_t size, CommSession *session)
    {
        return this->comm.push(buf, size, session);
    }

    int bind(CommService *service)
    {
        return this->comm.bind(service);
    }

    void unbind(CommService *service)
    {
        this->comm.unbind(service);
    }

    /* for sleepers. */
    int sleep(SleepSession *session)
    {
        return this->comm.sleep(session);
    }

    /* for file aio services. */
    int ioBind(IOService *service)
    {
        return this->comm.ioBind(service);
    }

    void ioUnbind(IOService *service)
    {
        this->comm.ioUnbind(service);
    }

public:
    int isHandlerThread() const
    {
        return this->comm.isHandlerThread();
    }

    int increaseHandlerThread()
    {
        return this->comm.increaseHandlerThread();
    }

private:
    Communicator comm;

public:
    virtual ~CommScheduler() { }
};

#endif //JARVIS_COMMON_SCHEDULER_H
