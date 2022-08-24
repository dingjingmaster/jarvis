//
// Created by dingjing on 8/17/22.
//

#include "server.h"

#include <mutex>
#include <atomic>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <condition_variable>

#include "../manager/global.h"
#include "../factory/connection.h"
#include "../core/common-scheduler.h"

#define PORT_STR_MAX	5

class ServerConnection : public Connection
{
public:
    ServerConnection(std::atomic<size_t> *connCount)
    {
        logv("");
        mConnCount = connCount;
    }

    virtual ~ServerConnection()
    {
        logv("");
        (*mConnCount)--;
    }

private:
    std::atomic<size_t>*            mConnCount;
};

int ServerBase::sslCtxCallback(SSL *ssl, int *al, void *arg)
{
    logv("");
    ServerBase *server = (ServerBase*)arg;
    const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    SSL_CTX *ssl_ctx = server->get_server_ssl_ctx(servername);

    if (!ssl_ctx)
        return SSL_TLSEXT_ERR_NOACK;

    if (ssl_ctx != server->getSSLCtx())
        SSL_set_SSL_CTX(ssl, ssl_ctx);

    return SSL_TLSEXT_ERR_OK;
}

int ServerBase::initSslCtx(const char *certFile, const char *keyFile)
{
    logv("");
    SSL_CTX *ssl_ctx = Global::newSslServerCtx();

    if (!ssl_ctx)
        return -1;

    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
    if (SSL_CTX_use_certificate_file(ssl_ctx, certFile, SSL_FILETYPE_PEM) > 0
        && SSL_CTX_use_PrivateKey_file(ssl_ctx, keyFile, SSL_FILETYPE_PEM) > 0
        && SSL_CTX_set_tlsext_servername_callback(ssl_ctx, sslCtxCallback) > 0
        && SSL_CTX_set_tlsext_servername_arg(ssl_ctx, this) > 0) {
        setSSL(ssl_ctx, mParams.sslAcceptTimeout);
        return 0;
    }

    SSL_CTX_free(ssl_ctx);
    return -1;
}

int ServerBase::init(const struct sockaddr *bindAddr, socklen_t addrLen, const char *certFile, const char *keyFile)
{
    int timeout = mParams.peerResponseTimeout;

    if (mParams.receiveTimeout >= 0) {
        if ((unsigned int)timeout > (unsigned int)mParams.receiveTimeout) {
            timeout = mParams.receiveTimeout;
        }
    }

    logv("timeout: %d, keyFile: %s, certFile: %s", timeout, keyFile ? keyFile : "null", certFile ? certFile : "null");

    if (CommService::init(bindAddr, addrLen, -1, timeout) < 0) {
        logd("CommService::init failed!");
        return -1;
    }

    if (keyFile && certFile) {
        logv("init ssl context...");
        if (this->initSslCtx(certFile, keyFile) < 0) {
            this->deInit();
            logv("init ssl context failed!");
            return -1;
        }
    }

    mScheduler = Global::getScheduler();

    logv("get global scheduler: %p", mScheduler);

    return 0;
}

int ServerBase::createListenFd()
{
    logv("");
    if (mListenFd < 0) {
        const struct sockaddr *bind_addr;
        socklen_t addrLen;
        int reuse = 1;

        getAddr(&bind_addr, &addrLen);
        mListenFd = socket(bind_addr->sa_family, SOCK_STREAM, 0);
        if (this->mListenFd >= 0) {
            setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (int));
        }
    } else {
        mListenFd = dup(mListenFd);
    }

    return mListenFd;
}

Connection *ServerBase::newConnection(int accept_fd)
{
    logv("");
    if (++mConnCount <= mParams.maxConnections
        || drain(1) == 1) {
        int reuse = 1;
        setsockopt(accept_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (int));
        return new ServerConnection(&mConnCount);
    }

    --mConnCount;
    errno = EMFILE;

    return nullptr;
}

void ServerBase::deleteConnection(Connection *conn)
{
    logv("");
    delete (ServerConnection*) conn;
}

void ServerBase::handleUnbound()
{
    logv("");
    mMutex.lock();
    mUnbindFinish = true;
    mCond.notify_one();
    mMutex.unlock();
}

int ServerBase::start(const struct sockaddr *bindAddr, socklen_t addrLen, const char *certFile, const char *keyFile)
{
    SSL_CTX*        sslCtx;

    logv("");

    if (init(bindAddr, addrLen, certFile, keyFile) >= 0) {
        if (mScheduler->bind(this) >= 0) {
            return 0;
        }

        sslCtx = getSSLCtx();
        deInit();
        if (sslCtx) {
            SSL_CTX_free(sslCtx);
        }
    }

    mListenFd = -1;

    return -1;
}

int ServerBase::start(int family, const char *host, unsigned short port, const char *certFile, const char *keyFile)
{
    logv("");
    struct addrinfo hints = {
            .ai_flags		=	AI_PASSIVE,
            .ai_family		=	family,
            .ai_socktype	=	SOCK_STREAM,
    };
    struct addrinfo *addrInfo;
    char portStr[PORT_STR_MAX + 1];
    int ret;

    snprintf(portStr, PORT_STR_MAX + 1, "%d", port);
    ret = getaddrinfo(host, portStr, &hints, &addrInfo);

    logv("host: %s, port: %s, getaddrinfo(): %d", host, portStr, ret);

    if (ret == 0) {
        ret = start(addrInfo->ai_addr, (socklen_t)addrInfo->ai_addrlen, certFile, keyFile);
        freeaddrinfo(addrInfo);
    } else {
        if (ret != EAI_SYSTEM) {
            errno = EINVAL;
        }
        ret = -1;
    }

    return ret;
}

int ServerBase::serve(int listenFd, const char *certFile, const char *keyFile)
{
    logv("");
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;

    if (getsockname(listenFd, (struct sockaddr *)&ss, &len) < 0) {
        return -1;
    }

    mListenFd = listenFd;
    return start((struct sockaddr *)&ss, len, certFile, keyFile);
}

void ServerBase::shutdown()
{
    logv("");
    mListenFd = -1;
    mScheduler->unbind(this);
}

void ServerBase::waitFinish()
{
    logv("");
    SSL_CTX *sslCtx = getSSLCtx();
    std::unique_lock<std::mutex> lock(mMutex);

    while (!mUnbindFinish) {
        mCond.wait(lock);
    }

    logv("");
    deInit();
    mUnbindFinish = false;
    lock.unlock();
    if (sslCtx) {
        logv("ssl ctx free");
        SSL_CTX_free(sslCtx);
    }
}
