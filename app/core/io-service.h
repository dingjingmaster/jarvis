//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_IO_SERVICE_H
#define JARVIS_IO_SERVICE_H

#include <stddef.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/eventfd.h>

#include "c-list.h"

#define IOS_STATE_SUCCESS	0
#define IOS_STATE_ERROR		1

class IOSession
{
    friend class IOService;
    friend class Communicator;
private:
    virtual int prepare() = 0;
    virtual void handle(int state, int error) = 0;

protected:
    /* prepare() has to call one of the the prep_ functions. */
    void prepPRead(int fd, void *buf, size_t count, long long offset);
    void prepPWrite(int fd, void *buf, size_t count, long long offset);
    void prepPReadv(int fd, const struct iovec *iov, int iovcnt, long long offset);
    void prepPWritev(int fd, const struct iovec *iov, int iovcnt, long long offset);
    void prepFSync(int fd);
    void prepFdSync(int fd);

protected:
    long getRes() const { return this->mRes; }

private:
    char                mIoCbBuf[64];
    long                mRes;

private:
    struct list_head    mList;

public:
    virtual ~IOSession() { }
};

class IOService
{
    friend class Communicator;
public:
    void deInit();
    int init(int maxEvents);

    int request(IOSession *session);

private:
    virtual void handleUnbound() = 0;
    virtual void handleStop(int error) { }

private:
    virtual int createEventFd()
    {
        return eventfd(0, 0);
    }

private:
    struct io_context*      mIoCtx;

private:
    void decref();
    void incref();

private:
    int                     mEventFd;
    int                     mRef;

private:
    struct list_head        mSessionList;
    pthread_mutex_t         mMutex;

private:
    static void *aio_finish(void *context);

public:
    virtual ~IOService() { }
};



#endif //JARVIS_IO_SERVICE_H
