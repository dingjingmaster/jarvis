//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_COMMUNICATOR_H
#define JARVIS_COMMUNICATOR_H

#include <time.h>
#include <stddef.h>
#include <sys/uio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

#include "c-list.h"
#include "c-poll.h"

typedef struct _CommConnEntry           CommConnEntry;

class CommConnection
{
    friend class CommSession;
    friend class Communicator;
protected:
    virtual ~CommConnection() { }
};

class CommTarget
{
    friend class CommSession;
    friend class Communicator;
public:
    void deInit();
    int init(const struct sockaddr *addr, socklen_t addrLen, int connectTimeout, int responseTimeout);

public:
    void getAddr(const struct sockaddr** addr, socklen_t* addrLen) const
    {
        *addr = mAddr;
        *addrLen = mAddrLen;
    }

protected:
    void setSSL(SSL_CTX* sslCtx, int sslConnectTimeout)
    {
        mSSLCtx = sslCtx;
        mSSLConnectTimeout = sslConnectTimeout;
    }

    SSL_CTX* getSSLCtx() const { return this->mSSLCtx; }

private:
    virtual int createConnectFd()
    {
        return socket(mAddr->sa_family, SOCK_STREAM, 0);
    }

    virtual CommConnection* newConnection(int connectFd)
    {
        return new CommConnection;
    }

    virtual int initSSL (SSL* ssl) { return 0; }

public:
    virtual void release(int keep_alive) { }

public:
    int                                 mResponseTimeout;
private:
    struct sockaddr*                    mAddr;
    socklen_t                           mAddrLen;
    int                                 mConnectTimeout;
    int                                 mSSLConnectTimeout;
    SSL_CTX*                            mSSLCtx;

private:
    pthread_mutex_t                     mMutex;
    struct list_head                    mIdleList;

public:
    virtual ~CommTarget() { }
};

class CommMessageOut
{
private:
    virtual int encode(struct iovec vectors[], int max) = 0;

public:
    virtual ~CommMessageOut() { }
    friend class Communicator;
};

class CommMessageIn : private CPollMessage
{
    friend class Communicator;
private:
    virtual int append(const void *buf, size_t *size) = 0;

protected:
    /* Send small packet while receiving. Call only in append(). */
    virtual int feedback(const void *buf, size_t size);

    /* In append(), reset the begin time of receiving to current time. */
    virtual void reNew();

private:
    CommConnEntry*                  mEntry;

public:
    virtual ~CommMessageIn() { }
};

#define CS_STATE_SUCCESS	0
#define CS_STATE_ERROR		1
#define CS_STATE_STOPPED	2
#define CS_STATE_TOREPLY	3	/* for service session only. */

class CommSession
{
    friend class Communicator;
    friend class CommMessageIn;
private:
    virtual CommMessageOut* messageOut() = 0;
    virtual CommMessageIn* messageIn() = 0;
    virtual int sendTimeout() { return -1; }
    virtual int receiveTimeout() { return -1; }
    virtual int keepAliveTimeout() { return 0; }
    virtual int firstTimeout() { return 0; }	/* for client session only. */
    virtual void handle(int state, int error) = 0;

protected:
    CommTarget* getTarget() const { return mTarget; }
    CommConnection* getConnection() const { return mConn; }
    CommMessageOut* getMessageOut() const { return mOut; }
    CommMessageIn* getMessageIn() const { return mIn; }
    long long getSeq() const { return mSeq; }

private:
    CommTarget*                     mTarget;
    CommConnection*                 mConn;
    CommMessageOut*                 mOut;
    CommMessageIn*                  mIn;
    long long                       mSeq;

private:
    struct timespec                 mBeginTime;
    int                             mTimeout;
    int                             mPassive;

public:
    CommSession() { mPassive = 0; }
    virtual ~CommSession();
};

class CommService
{
    friend class Communicator;
    friend class CommServiceTarget;
public:
    int init(const struct sockaddr *bind_addr, socklen_t addrLen, int listenTimeout, int responseTimeout);
    void deInit();

    int drain(int max);

public:
    void getAddr(const struct sockaddr **addr, socklen_t *addrLen) const
    {
        *addr = mBindAddr;
        *addrLen = mAddrLen;
    }

protected:
    void setSSL (SSL_CTX *sslCtx, int sslAcceptTimeout)
    {
        mSSLCtx = sslCtx;
        mSSLAcceptTimeout = sslAcceptTimeout;
    }

    SSL_CTX *getSSLCtx() const { return mSSLCtx; }

private:
    virtual CommSession *newSession(long long seq, CommConnection *conn) = 0;
    virtual void handleStop(int error) { }
    virtual void handleUnbound() = 0;

private:
    virtual int createListenFd()
    {
        return socket(mBindAddr->sa_family, SOCK_STREAM, 0);
    }

    virtual CommConnection* newConnection(int acceptFd)
    {
        return new CommConnection;
    }

    virtual int initSSL (SSL *ssl) { return 0; }

private:
    struct sockaddr*                mBindAddr;
    socklen_t                       mAddrLen;
    int                             mListenTimeout;
    int                             mResponseTimeout;
    int                             mSSLAcceptTimeout;
    SSL_CTX*                        mSSLCtx;

private:
    void incref();
    void decref();

private:
    int                             mListenFd;
    int                             mRef;

private:
    struct list_head                mAliveList;
    pthread_mutex_t                 mMutex;

public:
    virtual ~CommService() { }
};

#define SS_STATE_COMPLETE	0
#define SS_STATE_ERROR		1
#define SS_STATE_DISRUPTED	2

class SleepSession
{
private:
    virtual int duration(struct timespec *value) = 0;
    virtual void handle(int state, int error) = 0;

public:
    virtual ~SleepSession() { }
    friend class Communicator;
};

#include "m-poll.h"
#include "msg-queue.h"
#include "io-service.h"
#include "thread-pool.h"

class Communicator
{
    friend class CommTarget;
    friend class CommSession;
public:
    int init(size_t pollThreads, size_t handlerThreads);
    void deInit();

    int request(CommSession *session, CommTarget *target);
    int reply(CommSession *session);

    int push(const void *buf, size_t size, CommSession *session);

    int bind(CommService *service);
    void unbind(CommService *service);

    int sleep(SleepSession *session);

    int ioBind(IOService *service);
    void ioUnbind(IOService *service);

public:
    [[nodiscard]] int isHandlerThread() const;
    int increaseHandlerThread();

private:
    MPoll*                          mPoll;
    MsgQueue*                       mQueue;
    ThreadPool*                     mThreadPool;
    int                             mStopFlag;

private:
    int createPoll (size_t pollThreads);
    int createHandlerThreads(size_t handlerThreads);

    int nonblockConnect(CommTarget *target);
    int nonblockListen(CommService *service);

    CommConnEntry *launchConn(CommSession *session, CommTarget *target);
    CommConnEntry *acceptConn(class CommServiceTarget *target, CommService *service);

    void releaseConn(CommConnEntry *entry);

    void shutdownService(CommService *service);

    void shutdownIOService(IOService *service);

    int sendMessageSync(struct iovec vectors[], int cnt, CommConnEntry *entry);
    int sendMessageAsync(struct iovec vectors[], int cnt, CommConnEntry *entry);

    int sendMessage(CommConnEntry *entry);

    int replyIdleConn(CommSession *session, CommTarget *target);
    int requestIdleConn(CommSession *session, CommTarget *target);

    int requestNewConn(CommSession *session, CommTarget *target);

    void handleIncomingReply(CPollResult* res);
    void handleIncomingRequest(CPollResult* res);

    void handleReplyResult(CPollResult* res);
    void handleRequestResult(CPollResult* res);

    void handleReadResult(CPollResult* res);
    void handleWriteResult(CPollResult* res);

    void handleListenResult(CPollResult* res);
    void handleConnectResult(CPollResult* res);

    void handleSleepResult(CPollResult* res);
    void handleSSLAcceptResult(CPollResult* res);

    void handleAioResult(CPollResult* res);
    static void handlerThreadRoutine(void* context);

    static int nextTimeout(CommSession *session);
    static int firstTimeout(CommSession *session);

    static int firstTimeoutSend(CommSession *session);
    static int firstTimeoutRecv(CommSession *session);

    static int createServiceSession(CommConnEntry *entry);
    static int append(const void *buf, size_t *size, CPollMessage* msg);

    static CPollMessage* createMessage(void *context);

    static int partialWritten(size_t n, void *context);

    static void callback(CPollResult* res, void *context);

    static void *accept(const struct sockaddr *addr, socklen_t addrLen, int sockFd, void *context);

public:
    virtual ~Communicator() { }
};

#endif //JARVIS_COMMUNICATOR_H
