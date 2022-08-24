//
// Created by dingjing on 8/13/22.
//

#include "communicator.h"

#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "c-poll.h"
#include "m-poll.h"
#include "c-list.h"
#include "msg-queue.h"
#include "thread-pool.h"
#include "c-log.h"

struct _CommConnEntry
{
public:
    struct list_head                list;
    CommConnection*                 conn;
    long long                       seq;
    int                             sockFd;
#define CONN_STATE_CONNECTING	0
#define CONN_STATE_CONNECTED	1
#define CONN_STATE_RECEIVING	2
#define CONN_STATE_SUCCESS		3
#define CONN_STATE_IDLE			4
#define CONN_STATE_KEEPALIVE	5
#define CONN_STATE_CLOSING		6
#define CONN_STATE_ERROR		7
    int                             state;
    int                             error;
    int                             ref;
    struct iovec*                   writeIov;
    SSL*                            ssl;
    CommSession*                    session;
    CommTarget*                     target;
    CommService*                    service;
    MPoll*                          poll;
    /* Connection entry's mutex is for client session only. */
    pthread_mutex_t                 mutex;
};

static inline int _set_fd_nonblock(int fd)
{
    logv("");
    int flags = fcntl(fd, F_GETFL);

    if (flags >= 0)
        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return flags;
}

static int _bind_and_listen(int sockFd, const struct sockaddr *addr, socklen_t addrLen)
{
    logv("");
    struct sockaddr_storage ss;
    socklen_t len;

    len = sizeof (struct sockaddr_storage);
    if (getsockname(sockFd, (struct sockaddr *)&ss, &len) < 0)
        return -1;

    ss.ss_family = 0;
    while (len != 0) {
        if (((char *)&ss)[--len] != 0)
            break;
    }

    if (len == 0) {
        if (bind(sockFd, addr, addrLen) < 0)
            return -1;
    }

    return listen(sockFd, SOMAXCONN);
}

static int _create_ssl(SSL_CTX* sslCtx, CommConnEntry *entry)
{
    logv("");
    BIO *bio = BIO_new_socket(entry->sockFd, BIO_NOCLOSE);

    if (bio) {
        entry->ssl = SSL_new(sslCtx);
        if (entry->ssl) {
            SSL_set_bio(entry->ssl, bio, bio);
            return 0;
        }

        BIO_free(bio);
    }

    return -1;
}

#define SSL_WRITE_BUFSIZE	8192

static int _ssl_writev(SSL *ssl, const struct iovec vectors[], int cnt)
{
    logv("");
    char buf[SSL_WRITE_BUFSIZE];
    size_t nLeft = SSL_WRITE_BUFSIZE;
    char *p = buf;
    size_t n;
    int i;

    if (vectors[0].iov_len >= SSL_WRITE_BUFSIZE || cnt == 1)
        return SSL_write(ssl, vectors[0].iov_base, vectors[0].iov_len);

    for (i = 0; i < cnt; i++) {
        if (vectors[i].iov_len <= nLeft)
            n = vectors[i].iov_len;
        else
            n = nLeft;

        memcpy(p, vectors[i].iov_base, n);
        p += n;
        nLeft -= n;
        if (nLeft == 0)
            break;
    }

    return SSL_write(ssl, buf, p - buf);
}

int CommTarget::init(const struct sockaddr *addr, socklen_t addrLen, int connectTimeout, int responseTimeout)
{
    logv("");
    int ret;

    mAddr = (struct sockaddr *)malloc(addrLen);
    if (mAddr) {
        ret = pthread_mutex_init(&mMutex, NULL);
        if (ret == 0) {
            memcpy(mAddr, addr, addrLen);
            mAddrLen = addrLen;
            mConnectTimeout = connectTimeout;
            mResponseTimeout = responseTimeout;
            INIT_LIST_HEAD(&mIdleList);

            mSSLCtx = NULL;
            mSSLConnectTimeout = 0;
            return 0;
        }

        errno = ret;
        free(mAddr);
    }

    return -1;
}

void CommTarget::deInit()
{
    logv("");
    pthread_mutex_destroy(&mMutex);
    free(mAddr);
}

int CommMessageIn::feedback(const void *buf, size_t size)
{
    logv("");
    CommConnEntry *entry = mEntry;
    int ret;

    if (!entry->ssl)
        return write(entry->sockFd, buf, size);

    if (size == 0)
        return 0;

    ret = SSL_write(entry->ssl, buf, size);
    if (ret <= 0) {
        ret = SSL_get_error(entry->ssl, ret);
        if (ret != SSL_ERROR_SYSCALL)
            errno = -ret;

        ret = -1;
    }

    return ret;
}

void CommMessageIn::reNew()
{
    logv("");
    CommSession *session = mEntry->session;
    session->mTimeout = -1;
    session->mBeginTime.tv_nsec = -1;
}

int CommService::init(const struct sockaddr* bindAddr, socklen_t addrLen, int listenTimeout, int responseTimeout)
{
    logv("");
    int ret;

    mBindAddr = (struct sockaddr *)malloc(addrLen);
    if (mBindAddr) {
        ret = pthread_mutex_init(&mMutex, nullptr);
        if (ret == 0) {
            memcpy(mBindAddr, bindAddr, addrLen);
            mAddrLen = addrLen;
            mListenTimeout= listenTimeout;
            mResponseTimeout = responseTimeout;
            INIT_LIST_HEAD(&mAliveList);

            mSSLCtx = nullptr;
            mSSLAcceptTimeout = 0;

            return 0;
        }

        errno = ret;
        free(mBindAddr);
    }

    return -1;
}

void CommService::deInit()
{
    logv("");
    pthread_mutex_destroy(&mMutex);
    free(mBindAddr);
}

int CommService::drain(int max)
{
    logv("");
    CommConnEntry *entry;
    struct list_head *pos;
    int errno_bak;
    int cnt = 0;

    errno_bak = errno;
    pthread_mutex_lock(&mMutex);
    while (cnt != max && !list_empty(&mAliveList)) {
        pos = mAliveList.next;
        entry = list_entry(pos, CommConnEntry, list);
        list_del(pos);
        cnt++;

        /* Cannot change the sequence of next two lines. */
        m_poll_del(entry->sockFd, entry->poll);
        entry->state = CONN_STATE_CLOSING;
    }

    pthread_mutex_unlock(&mMutex);
    errno = errno_bak;

    return cnt;
}

inline void CommService::incref()
{
    logv("");
    __sync_add_and_fetch(&mRef, 1);
}

inline void CommService::decref()
{
    logv("");
    if (__sync_sub_and_fetch(&mRef, 1) == 0)
        handleUnbound();
}

class CommServiceTarget : public CommTarget
{
public:
    void incref()
    {
        logv("");
        __sync_add_and_fetch(&mRef, 1);
    }

    void decref()
    {
        logv("");
        if (__sync_sub_and_fetch(&mRef, 1) == 0) {
            service->decref();
            deInit();
            delete this;
        }
    }

private:
    int                 sockFd;
    int                 mRef;

private:
    CommService *service;

private:
    virtual int createConnectFd()
    {
        logv("");
        errno = EPERM;
        return -1;
    }

    friend class Communicator;
};

CommSession::~CommSession()
{
    logv("");
    CommConnEntry *entry;
    struct list_head *pos;
    CommTarget *target;
    int errno_bak;

    if (!mPassive)
        return;

    target = mTarget;
    if (mPassive == 1) {
        pthread_mutex_lock(&target->mMutex);
        if (!list_empty(&target->mIdleList)) {
            pos = target->mIdleList.next;
            entry = list_entry(pos, CommConnEntry, list);
            errno_bak = errno;
            m_poll_del(entry->sockFd, entry->poll);
            errno = errno_bak;
        }
        pthread_mutex_unlock(&target->mMutex);
    }

    ((CommServiceTarget *)target)->decref();
}

inline int Communicator::firstTimeout(CommSession *session)
{
    logv("");
    int timeout = session->mTarget->mResponseTimeout;

    if (timeout < 0 || (unsigned int)session->mTimeout <= (unsigned int)timeout)
    {
        timeout = session->mTimeout;
        session->mTimeout = 0;
        session->mBeginTime.tv_nsec = 0;
    }
    else
        clock_gettime(CLOCK_MONOTONIC, &session->mBeginTime);

    return timeout;
}

int Communicator::nextTimeout(CommSession *session)
{
    logv("");
    int timeout = session->mTarget->mResponseTimeout;
    struct timespec cur_time;
    int time_used, time_left;

    if (session->mTimeout > 0) {
        clock_gettime(CLOCK_MONOTONIC, &cur_time);
        time_used = 1000 * (cur_time.tv_sec - session->mBeginTime.tv_sec) + (cur_time.tv_nsec - session->mBeginTime.tv_nsec) / 1000000;
        time_left = session->mTimeout - time_used;
        if (time_left <= timeout) /* here timeout >= 0 */ {
            timeout = time_left < 0 ? 0 : time_left;
            session->mTimeout = 0;
        }
    }

    return timeout;
}

int Communicator::firstTimeoutSend(CommSession *session)
{
    logv("");
    session->mTimeout = session->sendTimeout();

    return Communicator::firstTimeout(session);
}

int Communicator::firstTimeoutRecv(CommSession *session)
{
    logv("");
    session->mTimeout = session->receiveTimeout();

    return Communicator::firstTimeout(session);
}

void Communicator::releaseConn(CommConnEntry *entry)
{
    logv("");
    delete entry->conn;
    if (!entry->service)
        pthread_mutex_destroy(&entry->mutex);

    if (entry->ssl)
        SSL_free(entry->ssl);

    close(entry->sockFd);
    free(entry);
}

void Communicator::shutdownService(CommService *service)
{
    logv("");
    close(service->mListenFd);
    service->mListenFd = -1;
    service->drain(-1);
    service->decref();
}

#ifndef IOV_MAX
# ifdef UIO_MAXIOV
#  define IOV_MAX	UIO_MAXIOV
# else
#  define IOV_MAX	1024
# endif
#endif

int Communicator::sendMessageSync(struct iovec vectors[], int cnt, CommConnEntry *entry)
{
    logv("");
    CommSession *session = entry->session;
    CommService *service;
    int timeout;
    ssize_t n;
    int i;

    while (cnt > 0) {
        if (!entry->ssl) {
            n = writev(entry->sockFd, vectors, cnt <= IOV_MAX ? cnt : IOV_MAX);
            if (n < 0)
                return errno == EAGAIN ? cnt : -1;
        } else if (vectors->iov_len > 0) {
            n = _ssl_writev(entry->ssl, vectors, cnt);
            if (n <= 0)
                return cnt;
        } else {
            n = 0;
        }

        for (i = 0; i < cnt; i++) {
            if ((size_t)n >= vectors[i].iov_len) {
                n -= vectors[i].iov_len;
            } else {
                vectors[i].iov_base = (char *)vectors[i].iov_base + n;
                vectors[i].iov_len -= n;
                break;
            }
        }

        vectors += i;
        cnt -= i;
    }

    service = entry->service;
    if (service) {
        __sync_add_and_fetch(&entry->ref, 1);
        timeout = session->keepAliveTimeout();
        switch (timeout) {
            default: {
                m_poll_set_timeout(entry->sockFd, timeout, mPoll);
                pthread_mutex_lock(&service->mMutex);
                if (service->mListenFd >= 0) {
                    entry->state = CONN_STATE_KEEPALIVE;
                    list_add_tail(&entry->list, &service->mAliveList);
                    entry = NULL;
                }

                pthread_mutex_unlock(&service->mMutex);
                if (entry) {
                    case 0:
                        m_poll_del(entry->sockFd, mPoll);
                    entry->state = CONN_STATE_CLOSING;
                }
            }
        }
    } else {
        if (entry->state == CONN_STATE_IDLE) {
            timeout = session->firstTimeout();
            if (timeout == 0)
                timeout = Communicator::firstTimeoutRecv(session);
            else {
                session->mTimeout = -1;
                session->mBeginTime.tv_nsec = -1;
            }

            m_poll_set_timeout(entry->sockFd, timeout, mPoll);
        }

        entry->state = CONN_STATE_RECEIVING;
    }

    return 0;
}

int Communicator::sendMessageAsync(struct iovec vectors[], int cnt, CommConnEntry *entry)
{
    logv("");
    CPollData data;
    int timeout;
    int ret;
    int i;

    entry->writeIov = (struct iovec *)malloc(cnt * sizeof (struct iovec));
    if (entry->writeIov) {
        for (i = 0; i < cnt; i++)
            entry->writeIov[i] = vectors[i];
    } else {
        return -1;
    }

    data.operation = PD_OP_WRITE;
    data.fd = entry->sockFd;
    data.ssl = entry->ssl;
    data.context = entry;
    data.writeIov = entry->writeIov;
    data.iovec = cnt;
    timeout = Communicator::firstTimeoutSend(entry->session);
    if (entry->state == CONN_STATE_IDLE) {
        ret = m_poll_mod(&data, timeout, mPoll);
        if (ret < 0 && errno == ENOENT)
            entry->state = CONN_STATE_RECEIVING;
    } else {
        ret = m_poll_add(&data, timeout, mPoll);
        if (ret >= 0) {
            if (mStopFlag)
                m_poll_del(data.fd, mPoll);
        }
    }

    if (ret < 0) {
        free(entry->writeIov);
        if (entry->state != CONN_STATE_RECEIVING)
            return -1;
    }

    return 1;
}

#define ENCODE_IOV_MAX		8192

int Communicator::sendMessage(CommConnEntry *entry)
{
    logv("");
    struct iovec vectors[ENCODE_IOV_MAX];
    struct iovec *end;
    int cnt;

    cnt = entry->session->mOut->encode(vectors, ENCODE_IOV_MAX);
    if ((unsigned int)cnt > ENCODE_IOV_MAX) {
        if (cnt > ENCODE_IOV_MAX)
            errno = EOVERFLOW;
        return -1;
    }

    end = vectors + cnt;
    cnt = sendMessageSync(vectors, cnt, entry);
    if (cnt <= 0)
        return cnt;

    return this->sendMessageAsync(end - cnt, cnt, entry);
}

void Communicator::handleIncomingRequest(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;
    CommTarget *target = entry->target;
    CommSession *session = NULL;
    int state;

    switch (res->state) {
        case PR_ST_SUCCESS:
            session = entry->session;
            state = CS_STATE_TOREPLY;
            pthread_mutex_lock(&target->mMutex);
            if (entry->state == CONN_STATE_SUCCESS) {
                __sync_add_and_fetch(&entry->ref, 1);
                entry->state = CONN_STATE_IDLE;
                list_add(&entry->list, &target->mIdleList);
            }

            pthread_mutex_unlock(&target->mMutex);
            break;

        case PR_ST_FINISHED:
            res->error = ECONNRESET;
            if (1)
                case PR_ST_ERROR:
                    state = CS_STATE_ERROR;
            else
                case PR_ST_DELETED:
                case PR_ST_STOPPED:
                    state = CS_STATE_STOPPED;

            pthread_mutex_lock(&target->mMutex);
            switch (entry->state) {
                case CONN_STATE_KEEPALIVE:
                    pthread_mutex_lock(&entry->service->mMutex);
                    if (entry->state == CONN_STATE_KEEPALIVE)
                        list_del(&entry->list);
                    pthread_mutex_unlock(&entry->service->mMutex);
                    break;

                case CONN_STATE_IDLE:
                    list_del(&entry->list);
                    break;

                case CONN_STATE_ERROR:
                    res->error = entry->error;
                    state = CS_STATE_ERROR;
                case CONN_STATE_RECEIVING:
                    session = entry->session;
                    break;

                case CONN_STATE_SUCCESS:
                    /* This may happen only if handler_threads > 1. */
                    entry->state = CONN_STATE_CLOSING;
                    entry = NULL;
                    break;
            }

            pthread_mutex_unlock(&target->mMutex);
            break;
    }

    if (entry) {
        if (session)
            session->handle(state, res->error);

        if (__sync_sub_and_fetch(&entry->ref, 1) == 0) {
            this->releaseConn(entry);
            ((CommServiceTarget *)target)->decref();
        }
    }
}

void Communicator::handleIncomingReply(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry*)res->data.context;
    CommTarget *target = entry->target;
    CommSession *session = NULL;
    pthread_mutex_t *mutex;
    int state;

    switch (res->state) {
        case PR_ST_SUCCESS:
            session = entry->session;
            state = CS_STATE_SUCCESS;
            pthread_mutex_lock(&target->mMutex);
            if (entry->state == CONN_STATE_SUCCESS) {
                __sync_add_and_fetch(&entry->ref, 1);
                if (session->mTimeout != 0) {
                    entry->state = CONN_STATE_IDLE;
                    list_add(&entry->list, &target->mIdleList);
                } else {
                    entry->state = CONN_STATE_CLOSING;
                }
            }

            pthread_mutex_unlock(&target->mMutex);
            break;

        case PR_ST_FINISHED:
            res->error = ECONNRESET;
            if (1)
                case PR_ST_ERROR:
                    state = CS_STATE_ERROR;
            else
                case PR_ST_DELETED:
                case PR_ST_STOPPED:
                    state = CS_STATE_STOPPED;

            mutex = &entry->mutex;
            pthread_mutex_lock(&target->mMutex);
            pthread_mutex_lock(mutex);
            switch (entry->state) {
                case CONN_STATE_IDLE:
                    list_del(&entry->list);
                    break;

                case CONN_STATE_ERROR:
                    res->error = entry->error;
                    state = CS_STATE_ERROR;
                case CONN_STATE_RECEIVING:
                    session = entry->session;
                    break;

                case CONN_STATE_SUCCESS:
                    /* This may happen only if handler_threads > 1. */
                    entry->state = CONN_STATE_CLOSING;
                    entry = NULL;
                    break;
            }

            pthread_mutex_unlock(&target->mMutex);
            pthread_mutex_unlock(mutex);
            break;
    }

    if (entry) {
        if (session) {
            target->release(entry->state == CONN_STATE_IDLE);
            session->handle(state, res->error);
        }

        if (__sync_sub_and_fetch(&entry->ref, 1) == 0)
            releaseConn(entry);
    }
}

void Communicator::handleReadResult(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;

    if (res->state != PR_ST_MODIFIED) {
        if (entry->service) {
            handleIncomingRequest(res);
        } else {
            handleIncomingReply(res);
        }
    }
}

void Communicator::handleReplyResult(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;
    CommService *service = entry->service;
    CommSession *session = entry->session;
    CommTarget *target = entry->target;
    int timeout;
    int state;

    switch (res->state) {
        case PR_ST_FINISHED: {
            timeout = session->keepAliveTimeout();
            if (timeout != 0) {
                __sync_add_and_fetch(&entry->ref, 1);
                res->data.operation = PD_OP_READ;
                res->data.message = NULL;
                pthread_mutex_lock(&target->mMutex);
                if (m_poll_add(reinterpret_cast<const CPollData *>(&res->data), timeout, mPoll) >= 0) {
                    pthread_mutex_lock(&service->mMutex);
                    if (!this->mStopFlag && service->mListenFd >= 0) {
                        entry->state = CONN_STATE_KEEPALIVE;
                        list_add_tail(&entry->list, &service->mAliveList);
                    } else {
                        m_poll_del(res->data.fd, this->mPoll);
                        entry->state = CONN_STATE_CLOSING;
                    }
                    pthread_mutex_unlock(&service->mMutex);
                } else {
                    __sync_sub_and_fetch(&entry->ref, 1);
                }
                pthread_mutex_unlock(&target->mMutex);
            }

            if (1)
                state = CS_STATE_SUCCESS;
            else if (1)
                case PR_ST_ERROR:
                    state = CS_STATE_ERROR;
            else
                case PR_ST_DELETED:        /* DELETED seems not possible. */
                case PR_ST_STOPPED:
                    state = CS_STATE_STOPPED;

            session->handle(state, res->error);
            if (__sync_sub_and_fetch(&entry->ref, 1) == 0) {
                releaseConn(entry);
                ((CommServiceTarget *) target)->decref();
            }
            break;
        }
    }
}

void Communicator::handleRequestResult(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;
    CommSession *session = entry->session;
    int timeout;
    int state;

    switch (res->state) {
        case PR_ST_FINISHED: {
            entry->state = CONN_STATE_RECEIVING;
            res->data.operation = PD_OP_READ;
            res->data.message = NULL;
            timeout = session->firstTimeout();
            if (timeout == 0)
                timeout = Communicator::firstTimeoutRecv(session);
            else {
                session->mTimeout = -1;
                session->mBeginTime.tv_nsec = -1;
            }

            if (m_poll_add(reinterpret_cast<const CPollData *>(&res->data), timeout, mPoll) >= 0) {
                if (mStopFlag)
                    m_poll_del(res->data.fd, mPoll);
                break;
            }

            res->error = errno;
            if (1)
                case PR_ST_ERROR:
                    state = CS_STATE_ERROR;
            else
                case PR_ST_DELETED:
                case PR_ST_STOPPED:
                    state = CS_STATE_STOPPED;

            entry->target->release(0);
            session->handle(state, res->error);
            pthread_mutex_lock(&entry->mutex);
            /* do nothing */
            pthread_mutex_unlock(&entry->mutex);
            if (__sync_sub_and_fetch(&entry->ref, 1) == 0)
                this->releaseConn(entry);

            break;
        }
    }
}

void Communicator::handleWriteResult(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;

    free(entry->writeIov);
    if (entry->service)
        handleReplyResult(res);
    else
        handleRequestResult(res);
}

CommConnEntry* Communicator::acceptConn(CommServiceTarget *target, CommService *service)
{
    logv("");
    CommConnEntry* entry;
    size_t size;

    if (_set_fd_nonblock(target->sockFd) >= 0){
        size = offsetof(CommConnEntry, mutex);
        entry = (CommConnEntry *)malloc(size);
        if (entry) {
            entry->conn = service->newConnection(target->sockFd);
            if (entry->conn) {
                entry->seq = 0;
                entry->poll = mPoll;
                entry->service = service;
                entry->target = target;
                entry->ssl = NULL;
                entry->sockFd = target->sockFd;
                entry->state = CONN_STATE_CONNECTED;
                entry->ref = 1;
                return entry;
            }

            free(entry);
        }
    }

    return NULL;
}

void Communicator::handleListenResult(CPollResult* res)
{
    logv("");
    CommService *service = (CommService *)res->data.context;
    CommConnEntry *entry;
    CommServiceTarget *target;
    int timeout;

    switch (res->state) {
        case PR_ST_SUCCESS:
            target = (CommServiceTarget *)res->data.result;
            entry = acceptConn(target, service);
            if (entry) {
                if (service->mSSLCtx) {
                    if (_create_ssl(service->mSSLCtx, entry) >= 0 && service->initSSL(entry->ssl) >= 0) {
                        res->data.operation = PD_OP_SSL_ACCEPT;
                        timeout = service->mSSLAcceptTimeout;
                    }
                } else {
                    res->data.operation = PD_OP_READ;
                    res->data.message = NULL;
                    timeout = target->mResponseTimeout;
                }

                if (res->data.operation != PD_OP_LISTEN) {
                    res->data.fd = entry->sockFd;
                    res->data.ssl = entry->ssl;
                    res->data.context = entry;
                    if (m_poll_add(reinterpret_cast<const CPollData *>(&res->data), timeout, mPoll) >= 0) {
                        if (mStopFlag)
                            m_poll_del(res->data.fd, mPoll);
                        break;
                    }
                }

                releaseConn(entry);
            } else
                close(target->sockFd);

            target->decref();
            break;

        case PR_ST_DELETED:
            shutdownService(service);
            break;

        case PR_ST_ERROR:
        case PR_ST_STOPPED:
            service->handleStop(res->error);
            break;
    }
}

void Communicator::handleConnectResult(CPollResult* res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;
    CommSession *session = entry->session;
    CommTarget *target = entry->target;
    int timeout;
    int state;
    int ret;

    switch (res->state) {
        case PR_ST_FINISHED:
            if (target->mSSLCtx && !entry->ssl) {
                if (_create_ssl(target->mSSLCtx, entry) >= 0 && target->initSSL(entry->ssl) >= 0) {
                    ret = 0;
                    res->data.operation = PD_OP_SSL_CONNECT;
                    res->data.ssl = entry->ssl;
                    timeout = target->mSSLConnectTimeout;
                } else
                    ret = -1;
            } else if ((session->mOut = session->messageOut()) != NULL) {
                ret = this->sendMessage(entry);
                if (ret == 0) {
                    res->data.operation = PD_OP_READ;
                    res->data.message = NULL;
                    timeout = session->firstTimeout();
                    if (timeout == 0)
                        timeout = Communicator::firstTimeoutRecv(session);
                    else {
                        session->mTimeout = -1;
                        session->mBeginTime.tv_nsec = -1;
                    }
                } else if (ret > 0)
                    break;
            } else
                ret = -1;

            if (ret >= 0) {
                if (m_poll_add(reinterpret_cast<const CPollData *>(&res->data), timeout, mPoll) >= 0) {
                    if (mStopFlag)
                        m_poll_del(res->data.fd, mPoll);
                    break;
                }
            }

            res->error = errno;
            if (1)
                case PR_ST_ERROR:
                    state = CS_STATE_ERROR;
            else
                case PR_ST_DELETED:
                case PR_ST_STOPPED:
                    state = CS_STATE_STOPPED;

            target->release(0);
            session->handle(state, res->error);
            releaseConn(entry);
            break;
    }
}

void Communicator::handleSSLAcceptResult(CPollResult *res)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)res->data.context;
    CommTarget *target = entry->target;
    int timeout;

    switch (res->state) {
        case PR_ST_FINISHED:
            res->data.operation = PD_OP_READ;
            res->data.message = NULL;
            timeout = target->mResponseTimeout;
            if (m_poll_add(reinterpret_cast<const CPollData *>(&res->data), timeout, mPoll) >= 0) {
                if (mStopFlag)
                    m_poll_del(res->data.fd, mPoll);
                break;
            }

        case PR_ST_DELETED:
        case PR_ST_ERROR:
        case PR_ST_STOPPED:
            releaseConn(entry);
            ((CommServiceTarget *)target)->decref();
            break;
    }
}

void Communicator::handleSleepResult(CPollResult *res)
{
    logv("");
    SleepSession *session = (SleepSession *)res->data.context;
    int state;

    if (res->state == PR_ST_STOPPED)
        state = SS_STATE_DISRUPTED;
    else
        state = SS_STATE_COMPLETE;

    session->handle(state, 0);
}

void Communicator::handleAioResult(CPollResult *res)
{
    logv("");
    IOService *service = (IOService *)res->data.context;
    IOSession *session;
    int state, error;

    switch (res->state) {
        case PR_ST_SUCCESS:
            session = (IOSession *)res->data.result;
            pthread_mutex_lock(&service->mMutex);
            list_del(&session->mList);
            pthread_mutex_unlock(&service->mMutex);
            if (session->mRes >= 0) {
                state = IOS_STATE_SUCCESS;
                error = 0;
            } else {
                state = IOS_STATE_ERROR;
                error = -session->mRes;
            }

            session->handle(state, error);
            service->decref();
            break;

        case PR_ST_DELETED:
            shutdownIOService(service);
            break;

        case PR_ST_ERROR:
        case PR_ST_STOPPED:
            service->handleStop(res->error);
            break;
    }
}

void Communicator::handlerThreadRoutine(void *context)
{
    logv("");
    Communicator *comm = (Communicator*)context;
    CPollResult* res;

    while ((res = (CPollResult*)msg_queue_get(comm->mQueue)) != nullptr) {
        switch (res->data.operation) {
            case PD_OP_READ: {
                logv("handle READ");
                comm->handleReadResult(res);
                break;
            }
            case PD_OP_WRITE: {
                logv("handle write");
                comm->handleWriteResult(res);
                break;
            }
            case PD_OP_CONNECT:
            case PD_OP_SSL_CONNECT: {
                logv("handle connect");
                comm->handleConnectResult(res);
                break;
            }
            case PD_OP_LISTEN: {
                logv("handle listen");
                comm->handleListenResult(res);
                break;
            }
            case PD_OP_SSL_ACCEPT: {
                logv("handle accept");
                comm->handleSSLAcceptResult(res);
                break;
            }
            case PD_OP_EVENT:
            case PD_OP_NOTIFY: {
                logv("handle event/notify");
                comm->handleAioResult(res);
                break;
            }
            case PD_OP_TIMER: {
                logv("handle timer");
                comm->handleSleepResult(res);
                break;
            }
        }

        free(res);
    }
}

int Communicator::append(const void *buf, size_t *size, CPollMessage* msg)
{
    logv("");
    CommMessageIn *in = (CommMessageIn *)msg;
    CommConnEntry *entry = in->mEntry;
    CommSession *session = entry->session;
    int timeout;
    int ret;

    ret = in->append(buf, size);
    if (ret > 0) {
        entry->state = CONN_STATE_SUCCESS;
        if (entry->service)
            timeout = -1;
        else {
            timeout = session->keepAliveTimeout();
            session->mTimeout = timeout; /* Reuse session's timeout field. */
            if (timeout == 0) {
                m_poll_del(entry->sockFd, entry->poll);
                return ret;
            }
        }
    } else if (ret == 0 && session->mTimeout != 0) {
        if (session->mBeginTime.tv_nsec == -1)
            timeout = Communicator::firstTimeoutRecv(session);
        else
            timeout = Communicator::nextTimeout(session);
    } else {
        return ret;
    }

    /* This set_timeout() never fails, which is very important. */
    m_poll_set_timeout(entry->sockFd, timeout, entry->poll);
    return ret;
}

int Communicator::createServiceSession(CommConnEntry *entry)
{
    logv("");
    CommService *service = entry->service;
    CommTarget *target = entry->target;
    CommSession *session;
    int timeout;

    pthread_mutex_lock(&service->mMutex);
    if (entry->state == CONN_STATE_KEEPALIVE)
        list_del(&entry->list);
    else if (entry->state != CONN_STATE_CONNECTED)
        entry = nullptr;

    pthread_mutex_unlock(&service->mMutex);
    if (!entry) {
        errno = ENOENT;
        return -1;
    }

    session = service->newSession(entry->seq, entry->conn);
    if (session) {
        session->mPassive = 1;
        entry->session = session;
        session->mTarget = target;
        session->mConn = entry->conn;
        session->mSeq = entry->seq++;
        session->mOut = nullptr;
        session->mIn = NULL;

        timeout = Communicator::firstTimeoutRecv(session);
        m_poll_set_timeout(entry->sockFd, timeout, entry->poll);
        entry->state = CONN_STATE_RECEIVING;

        ((CommServiceTarget *)target)->incref();
        return 0;
    }

    return -1;
}

CPollMessage* Communicator::createMessage(void *context)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)context;
    CommSession *session;

    if (entry->state == CONN_STATE_IDLE) {
        pthread_mutex_t *mutex;

        if (entry->service)
            mutex = &entry->target->mMutex;
        else
            mutex = &entry->mutex;

        pthread_mutex_lock(mutex);
        /* do nothing */
        pthread_mutex_unlock(mutex);
    }

    if (entry->state == CONN_STATE_CONNECTED || entry->state == CONN_STATE_KEEPALIVE) {
        if (Communicator::createServiceSession(entry) < 0)
            return nullptr;
    } else if (entry->state != CONN_STATE_RECEIVING) {
        errno = EBADMSG;
        return nullptr;
    }

    session = entry->session;
    session->mIn = session->messageIn();
    if (session->mIn) {
        session->mIn->CPollMessage::append = Communicator::append;
        session->mIn->mEntry = entry;
    }

    return session->mIn;
}

int Communicator::partialWritten(size_t n, void *context)
{
    logv("");
    CommConnEntry *entry = (CommConnEntry *)context;
    CommSession *session = entry->session;
    int timeout;

    timeout = Communicator::nextTimeout(session);
    m_poll_set_timeout(entry->sockFd, timeout, entry->poll);

    return 0;
}

void Communicator::callback(CPollResult* res, void *context)
{
    logv("");
    Communicator *comm = (Communicator *)context;

    msg_queue_put(res, comm->mQueue);
}

void *Communicator::accept(const struct sockaddr *addr, socklen_t addrLen, int sockFd, void *context)
{
    logv("");
    CommService *service = (CommService *)context;
    CommServiceTarget *target = new CommServiceTarget;

    if (target) {
        if (target->init(addr, addrLen, 0, service->mResponseTimeout) >= 0) {
            service->incref();
            target->service = service;
            target->sockFd = sockFd;
            target->mRef = 1;
            return target;
        }

        delete target;
    }

    close(sockFd);

    return NULL;
}

int Communicator::createHandlerThreads(size_t handlerThreads)
{
    logv("handle thread number: %d", handlerThreads);
    ThreadPoolTask task = {
            .routine	=	Communicator::handlerThreadRoutine,
            .context	=	this
    };
    size_t i;

    mThreadPool = thread_pool_create(handlerThreads, 0);
    if (mThreadPool) {
        for (i = 0; i < handlerThreads; i++) {
            if (thread_pool_schedule(&task, mThreadPool) < 0) {
                break;
            }
        }

        if (i == handlerThreads) {
            return 0;
        }

        msg_queue_set_nonblock(mQueue);
        thread_pool_destroy(NULL, mThreadPool);
    }

    return -1;
}

int Communicator::createPoll(size_t pollThreads)
{
    logv("");
    CPollParams params = {
            .maxOpenFiles       =	(size_t)sysconf(_SC_OPEN_MAX),
            .createMessage      =	Communicator::createMessage,
            .partialWritten     =	Communicator::partialWritten,
            .callback			=	Communicator::callback,
            .context			=	this
    };

    if ((ssize_t)params.maxOpenFiles < 0) {
        return -1;
    }

    mQueue = msg_queue_create(4096, sizeof (CPollResult));
    if (mQueue) {
        mPoll = m_poll_create(&params, pollThreads);
        if (mPoll) {
            if (m_poll_start(mPoll) >= 0) {
                return 0;
            }

            m_poll_destroy(mPoll);
        }

        msg_queue_destroy(mQueue);
    }

    return -1;
}

int Communicator::init(size_t poller_threads, size_t handler_threads)
{
    logv("");
    if (poller_threads == 0) {
        errno = EINVAL;
        return -1;
    }

    if (this->createPoll(poller_threads) >= 0) {
        if (createHandlerThreads(handler_threads) >= 0) {
            mStopFlag = 0;
            return 0;
        }

        m_poll_stop(mPoll);
        m_poll_destroy(mPoll);
        msg_queue_destroy(mQueue);
    }

    return -1;
}

void Communicator::deInit()
{
    logv("");
    mStopFlag = 1;
    m_poll_stop(mPoll);
    msg_queue_set_nonblock(mQueue);
    thread_pool_destroy(NULL, mThreadPool);
    m_poll_destroy(mPoll);
    msg_queue_destroy(mQueue);
}

int Communicator::nonblockConnect(CommTarget *target)
{
    logv("");
    int sockfd = target->createConnectFd();

    if (sockfd >= 0) {
        if (_set_fd_nonblock(sockfd) >= 0) {
            if (connect(sockfd, target->mAddr, target->mAddrLen) >= 0 ||errno == EINPROGRESS) {
                return sockfd;
            }
        }

        close(sockfd);
    }

    return -1;
}

CommConnEntry *Communicator::launchConn(CommSession *session, CommTarget *target)
{
    logv("");
    CommConnEntry *entry;
    int sockfd;
    int ret;

    sockfd = this->nonblockConnect(target);
    if (sockfd >= 0) {
        entry = (CommConnEntry *)malloc(sizeof (CommConnEntry));
        if (entry) {
            ret = pthread_mutex_init(&entry->mutex, NULL);
            if (ret == 0) {
                entry->conn = target->newConnection(sockfd);
                if (entry->conn) {
                    entry->seq = 0;
                    entry->poll = mPoll;
                    entry->service = NULL;
                    entry->target = target;
                    entry->session = session;
                    entry->ssl = NULL;
                    entry->sockFd = sockfd;
                    entry->state = CONN_STATE_CONNECTING;
                    entry->ref = 1;
                    return entry;
                }
                pthread_mutex_destroy(&entry->mutex);
            } else {
                errno = ret;
            }
            free(entry);
        }
        close(sockfd);
    }

    return NULL;
}

int Communicator::requestIdleConn(CommSession *session, CommTarget *target)
{
    logv("");
    CommConnEntry *entry;
    struct list_head *pos;
    int ret = -1;

    while (1) {
        pthread_mutex_lock(&target->mMutex);
        if (!list_empty(&target->mIdleList)) {
            pos = target->mIdleList.next;
            entry = list_entry(pos, CommConnEntry, list);
            list_del(pos);
            pthread_mutex_lock(&entry->mutex);
        }
        else
            entry = NULL;

        pthread_mutex_unlock(&target->mMutex);
        if (!entry) {
            errno = ENOENT;
            return -1;
        }

        if (m_poll_set_timeout(entry->sockFd, -1, mPoll) >= 0) {
            break;
        }

        entry->state = CONN_STATE_CLOSING;
        pthread_mutex_unlock(&entry->mutex);
    }

    entry->session = session;
    session->mConn = entry->conn;
    session->mSeq = entry->seq++;
    session->mOut = session->messageOut();
    if (session->mOut) {
        ret = sendMessage(entry);
    }

    if (ret < 0) {
        entry->error = errno;
        m_poll_del(entry->sockFd, mPoll);
        entry->state = CONN_STATE_ERROR;
        ret = 1;
    }

    pthread_mutex_unlock(&entry->mutex);

    return ret;
}

int Communicator::requestNewConn(CommSession *session, CommTarget *target)
{
    logv("");
    CommConnEntry *entry;
    CPollData data;
    int timeout;

    entry = launchConn(session, target);
    if (entry) {
        session->mConn = entry->conn;
        session->mSeq = entry->seq++;
        data.operation = PD_OP_CONNECT;
        data.fd = entry->sockFd;
        data.ssl = NULL;
        data.context = entry;
        timeout = session->mTarget->mConnectTimeout;
        if (m_poll_add(&data, timeout, mPoll) >= 0) {
            return 0;
        }
        releaseConn(entry);
    }

    return -1;
}

int Communicator::request(CommSession *session, CommTarget *target)
{
    logv("");
    int errno_bak;

    if (session->mPassive) {
        errno = EINVAL;
        return -1;
    }

    errno_bak = errno;
    session->mTarget = target;
    session->mOut = NULL;
    session->mIn = NULL;
    if (requestIdleConn(session, target) < 0) {
        if (requestNewConn(session, target) < 0) {
            session->mConn = NULL;
            session->mSeq = 0;
            return -1;
        }
    }

    errno = errno_bak;

    return 0;
}

int Communicator::nonblockListen(CommService *service)
{
    logv("");
    int sockfd = service->createListenFd();

    if (sockfd >= 0) {
        if (_set_fd_nonblock(sockfd) >= 0) {
            if (_bind_and_listen(sockfd, service->mBindAddr, service->mAddrLen) >= 0) {
                return sockfd;
            }
        }
        close(sockfd);
    }

    return -1;
}

int Communicator::bind(CommService *service)
{
    logv("");
    CPollData data;
    int socketFd = -1;

    socketFd = nonblockListen(service);
    if (socketFd >= 0) {
        service->mListenFd = socketFd;
        service->mRef = 1;
        data.operation = PD_OP_LISTEN;
        data.fd = socketFd;
        data.accept = Communicator::accept;
        data.context = service;
        data.result = NULL;
        if (m_poll_add(&data, service->mListenTimeout, mPoll) >= 0) {
            logv("service socket fd '%d' add to poll!", socketFd);
            return 0;
        }
        close(socketFd);
    }

    logd("service socket fd add poll failed!");

    return -1;
}

void Communicator::unbind(CommService *service)
{
    logv("");
    int errno_bak = errno;

    if (m_poll_del(service->mListenFd, mPoll) < 0) {
        shutdownService(service);
        errno = errno_bak;
    }
}

int Communicator::replyIdleConn(CommSession *session, CommTarget *target)
{
    logv("");
    CommConnEntry *entry;
    struct list_head *pos;
    int ret = -1;

    pthread_mutex_lock(&target->mMutex);
    if (!list_empty(&target->mIdleList)) {
        pos = target->mIdleList.next;
        entry = list_entry(pos, CommConnEntry, list);
        list_del(pos);

        session->mOut = session->messageOut();
        if (session->mOut)
            ret = sendMessage(entry);

        if (ret < 0) {
            entry->error = errno;
            m_poll_del(entry->sockFd, mPoll);
            entry->state = CONN_STATE_ERROR;
            ret = 1;
        }
    } else {
        errno = ENOENT;
    }

    pthread_mutex_unlock(&target->mMutex);
    return ret;
}

int Communicator::reply(CommSession *session)
{
    logv("");
    CommConnEntry *entry;
    CommTarget *target;
    int errno_bak;
    int ret;

    if (session->mPassive != 1) {
        errno = session->mPassive ? ENOENT : EPERM;
        return -1;
    }

    errno_bak = errno;
    session->mPassive = 2;
    target = session->mTarget;
    ret = this->replyIdleConn(session, target);
    if (ret < 0)
        return -1;

    if (ret == 0) {
        entry = session->mIn->mEntry;
        session->handle(CS_STATE_SUCCESS, 0);
        if (__sync_sub_and_fetch(&entry->ref, 1) == 0) {
            releaseConn(entry);
            ((CommServiceTarget *)target)->decref();
        }
    }

    errno = errno_bak;
    return 0;
}

int Communicator::push(const void *buf, size_t size, CommSession *session)
{
    logv("");
    CommTarget *target = session->mTarget;
    CommConnEntry *entry;
    int ret;

    if (session->mPassive != 1) {
        errno = session->mPassive ? ENOENT : EPERM;
        return -1;
    }

    pthread_mutex_lock(&target->mMutex);
    if (!list_empty(&target->mIdleList)) {
        entry = list_entry(target->mIdleList.next, CommConnEntry, list);
        if (!entry->ssl)
            ret = write(entry->sockFd, buf, size);
        else if (size == 0)
            ret = 0;
        else {
            ret = SSL_write(entry->ssl, buf, size);
            if (ret <= 0) {
                ret = SSL_get_error(entry->ssl, ret);
                if (ret != SSL_ERROR_SYSCALL) {
                    errno = -ret;
                }
                ret = -1;
            }
        }
    } else {
        errno = ENOENT;
        ret = -1;
    }

    pthread_mutex_unlock(&target->mMutex);

    return ret;
}

int Communicator::sleep(SleepSession *session)
{
    logv("");
    struct timespec value;

    if (session->duration(&value) >= 0) {
        if (m_poll_add_timer(&value, session, mPoll) >= 0) {
            return 0;
        }
    }

    return -1;
}

int Communicator::isHandlerThread() const
{
    logv("");
    return thread_pool_in_pool(mThreadPool);
}

extern "C" void _thread_pool_schedule(ThreadPoolTask*, void *,ThreadPool*);

int Communicator::increaseHandlerThread()
{
    logv("");
    void *buf = malloc(4 * sizeof (void *));

    if (buf) {
        if (thread_pool_increase(mThreadPool) >= 0) {
            ThreadPoolTask task = {
                    .routine	=	Communicator::handlerThreadRoutine,
                    .context	=	this
            };
            _thread_pool_schedule(&task, buf, mThreadPool);
            return 0;
        }
        free(buf);
    }
    return -1;
}

void Communicator::shutdownIOService(IOService *service)
{
    logv("");
    pthread_mutex_lock(&service->mMutex);
    close(service->mEventFd);
    service->mEventFd = -1;
    pthread_mutex_unlock(&service->mMutex);
    service->decref();
}

int Communicator::ioBind(IOService *service)
{
    logv("");
    CPollData data;
    int eventFd = service->createEventFd();
    if (eventFd >= 0) {
        if (_set_fd_nonblock(eventFd) >= 0) {
            service->mRef = 1;
            data.operation = PD_OP_EVENT;
            data.fd = eventFd;
            data.event = IOService::aio_finish;
            data.context = service;
            data.result = NULL;
            if (m_poll_add(&data, -1, mPoll) >= 0) {
                service->mEventFd = eventFd;
                return 0;
            }
        }
        close(eventFd);
    }

    return -1;
}

void Communicator::ioUnbind(IOService *service)
{
    logv("");
    int errno_bak = errno;

    if (m_poll_del(service->mEventFd, mPoll) < 0) {
        shutdownIOService(service);
        errno = errno_bak;
    }
}

