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
        mConnCount = connCount;
    }

    virtual ~ServerConnection()
    {
        (*mConnCount)--;
    }

private:
    std::atomic<size_t>*            mConnCount;
};

int ServerBase::sslCtxCallback(SSL *ssl, int *al, void *arg)
{
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

int ServerBase::init(const struct sockaddr *bind_addr, socklen_t addrlen, const char *cert_file, const char *key_file)
{
    int timeout = mParams.peerResponseTimeout;

    if (mParams.receiveTimeout >= 0) {
        if ((unsigned int)timeout > (unsigned int)mParams.receiveTimeout)
            timeout = mParams.receiveTimeout;
    }

    if (CommService::init(bind_addr, addrlen, -1, timeout) < 0)
        return -1;

    if (key_file && cert_file) {
        if (this->initSslCtx(cert_file, key_file) < 0) {
            this->deInit();
            return -1;
        }
    }

    mScheduler = Global::getScheduler();

    return 0;
}

int ServerBase::createListenFd()
{
    if (mListenFd < 0) {
        const struct sockaddr *bind_addr;
        socklen_t addrlen;
        int reuse = 1;

        getAddr(&bind_addr, &addrlen);
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
    delete (ServerConnection*) conn;
}

void ServerBase::handleUnbound()
{
    mMutex.lock();
    mUnbindFinish = true;
    mCond.notify_one();
    mMutex.unlock();
}

int ServerBase::start(const struct sockaddr *bind_addr, socklen_t addrLen, const char *cert_file, const char *key_file)
{
    SSL_CTX *ssl_ctx;

    if (init(bind_addr, addrLen, cert_file, key_file) >= 0) {
        if (mScheduler->bind(this) >= 0)
            return 0;

        ssl_ctx = get_server_ssl_ctx();
        deInit();
        if (ssl_ctx)
            SSL_CTX_free(ssl_ctx);
    }

    mListenFd = -1;

    return -1;
}

int ServerBase::start(int family, const char *host, unsigned short port, const char *cert_file, const char *key_file)
{
    struct addrinfo hints = {
            .ai_flags		=	AI_PASSIVE,
            .ai_family		=	family,
            .ai_socktype	=	SOCK_STREAM,
    };
    struct addrinfo *addrinfo;
    char port_str[PORT_STR_MAX + 1];
    int ret;

    snprintf(port_str, PORT_STR_MAX + 1, "%d", port);
    ret = getaddrinfo(host, port_str, &hints, &addrinfo);
    if (ret == 0) {
        ret = start(addrinfo->ai_addr, (socklen_t)addrinfo->ai_addrlen, cert_file, key_file);
        freeaddrinfo(addrinfo);
    } else {
        if (ret != EAI_SYSTEM)
            errno = EINVAL;
        ret = -1;
    }

    return ret;
}

int ServerBase::serve(int listen_fd, const char *cert_file, const char *key_file)
{
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;

    if (getsockname(listen_fd, (struct sockaddr *)&ss, &len) < 0)
        return -1;

    mListenFd = listen_fd;
    return start((struct sockaddr *)&ss, len, cert_file, key_file);
}

void ServerBase::shutdown()
{
    mListenFd = -1;
    mScheduler->unbind(this);
}

void ServerBase::waitFinish()
{
    SSL_CTX *sslCtx = getSSLCtx();
    std::unique_lock<std::mutex> lock(mMutex);

    while (!mUnbindFinish) {
        mCond.wait(lock);
    }

    deInit();
    mUnbindFinish = false;
    lock.unlock();
    if (sslCtx) {
        SSL_CTX_free(sslCtx);
    }
}
