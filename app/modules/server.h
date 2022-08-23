//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_SERVER_H
#define JARVIS_SERVER_H

#include <mutex>
#include <atomic>
#include <errno.h>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <condition_variable>

#include "../factory/task-factory.h"

typedef struct _ServerParams                ServerParams;

struct _ServerParams
{
    size_t                      maxConnections;
    int                         peerResponseTimeout;
    int                         receiveTimeout;
    int                         keepAliveTimeout;
    size_t                      requestSizeLimit;
    int                         sslAcceptTimeout;
};

static constexpr ServerParams SERVER_PARAMS_DEFAULT =
        {
            .maxConnections         = 2000,
            .peerResponseTimeout    = 10 * 1000,
            .receiveTimeout         = -1,
            .keepAliveTimeout       = 60 * 1000,
            .requestSizeLimit       = (size_t) -1,
            .sslAcceptTimeout       = 10 * 1000,
        };

class ServerBase : protected CommService
{
public:
    ServerBase (const ServerParams* params)
        : mConnCount(0)
    {
        mParams = *params;
        mUnbindFinish = false;
        mListenFd = -1;
    }

public:
    /* Start on port with IPv4. */
    int start(unsigned short port)
    {
        return start(AF_INET, nullptr, port, nullptr, nullptr);
    }

    /* Start with family. AF_INET or AF_INET6. */
    int start(int family, unsigned short port)
    {
        return start(family, nullptr, port, nullptr, nullptr);
    }

    /* Start with hostname and port. */
    int start(const char *host, unsigned short port)
    {
        return start(AF_INET, host, port, nullptr, nullptr);
    }

    /* Start with family, hostname and port. */
    int start(int family, const char *host, unsigned short port)
    {
        return start(family, host, port, nullptr, nullptr);
    }

    /* Start with binding address. */
    int start(const struct sockaddr *bindAddr, socklen_t addrLen)
    {
        return start(bindAddr, addrLen, nullptr, nullptr);
    }

    /* To start an SSL server. */
    int start(unsigned short port, const char *certFile, const char *keyFile)
    {
        return start(AF_INET, nullptr, port, certFile, keyFile);
    }

    int start(int family, unsigned short port, const char *certFile, const char *keyFile)
    {
        return start(family, nullptr, port, certFile, keyFile);
    }

    int start(const char *host, unsigned short port, const char *certFile, const char *keyFile)
    {
        return start(AF_INET, host, port, certFile, keyFile);
    }

    int start(int family, const char *host, unsigned short port, const char *certFile, const char *keyFile);

    /* This is the only necessary start function. */
    int start(const struct sockAddr *bindAddr, socklen_t addrLen, const char *certFile, const char *keyFile);

    /* To start with a specified fd. For graceful restart or SCTP server. */
    int serve(int listen_fd)
    {
        return serve(listen_fd, nullptr, nullptr);
    }

    int serve(int listenFd, const char *certFile, const char *keyFile);

    /* stop() is a blocking operation. */
    void stop()
    {
        this->shutdown();
        this->waitFinish();
    }

    /* Nonblocking terminating the server. For stopping multiple servers.
     * Typically, call shutdown() and then wait_finish().
     * But indeed wait_finish() can be called before shutdown(), even before
     * start() in another thread. */
    void shutdown();
    void waitFinish();

    size_t get_conn_count() const { return mConnCount; }

    /* Get the listening address. This is often used after starting
     * server on a random port (start() with port == 0). */
    int get_listen_addr(struct sockaddr *addr, socklen_t *addrLen) const
    {
        if (mListenFd >= 0) {
            return getsockname(mListenFd, addr, addrLen);
        }

        errno = ENOTCONN;
        return -1;
    }

protected:
    /* Override this function to implement server that supports TLS SNI.
	 * "servername" will be NULL if client does not set a host name.
	 * Returning NULL to indicate that servername is not supported. */
    virtual SSL_CTX *get_server_ssl_ctx(const char *servername)
    {
        return this->getSSLCtx();
    }

    virtual Connection* newConnection(int acceptFd);
    void deleteConnection(Connection *conn);

private:
    int init(const struct sockaddr *bindAddr, socklen_t addrLen, const char *certFile, const char *keyFile);
    int initSslCtx(const char *certFile, const char *keyFile);
    static int sslCtxCallback(SSL *ssl, int *al, void *arg);
    virtual int createListenFd();
    virtual void handleUnbound();

protected:
    std::atomic<size_t>                     mConnCount;
    ServerParams                            mParams;

private:
    int                                     mListenFd;
    bool                                    mUnbindFinish;

    std::mutex                              mMutex;
    std::condition_variable                 mCond;

    class CommScheduler*                    mScheduler;

    int start(const sockaddr *bind_addr, socklen_t addrLen, const char *cert_file, const char *key_file);
};

template<class REQ, class RESP>
class Server : public ServerBase
{
public:
    Server(const ServerParams *params, std::function<void (NetworkTask<REQ, RESP> *)> proc)
        : ServerBase(params), mProcess(std::move(proc))
    {
    }

    Server(std::function<void (NetworkTask<REQ, RESP> *)> proc)
        : ServerBase(&SERVER_PARAMS_DEFAULT), mProcess(std::move(proc))
    {
    }

protected:
    virtual CommSession *newSession(long long seq, CommConnection *conn) override ;

protected:
    std::function<void (NetworkTask<REQ, RESP> *)>              mProcess;
};

template<class REQ, class RESP>
CommSession* Server<REQ, RESP>::newSession(long long seq, CommConnection *conn)
{
    using factory = NetworkTaskFactory<REQ, RESP>;
    NetworkTask<REQ, RESP> *task;

    task = factory::createServerTask(this, mProcess);
    task->setKeepAlive(mParams.keepAliveTimeout);
    task->setReceiveTimeout(mParams.receiveTimeout);
    task->getReq()->setSizeLimit(mParams.requestSizeLimit);

    return task;
}

#endif //JARVIS_SERVER_H
