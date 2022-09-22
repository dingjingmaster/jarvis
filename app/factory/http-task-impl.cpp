//
// Created by dingjing on 8/23/22.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include "task-error.h"
#include "task-factory.h"
#include "../utils/string-util.h"
#include "../manager/global.h"
#include "../protocol/http/http-util.h"
#include "../protocol/ssl-wrapper.h"

using namespace protocol;

#define HTTP_KEEPALIVE_DEFAULT	(60 * 1000)
#define HTTP_KEEPALIVE_MAX		(300 * 1000)

/**********Client**********/

class ComplexHttpTask : public ComplexClientTask<HttpRequest, HttpResponse>
{
public:
    ComplexHttpTask(int redirect_max, int retry_max, HttpCallback && callback, void* udata)
        : ComplexClientTask(retry_max, std::move(callback), udata), redirect_max_(redirect_max), redirect_count_(0), mUdata(udata)
    {
        HttpRequest *clientReq = this->getReq();

        clientReq->setMethod(HTTP_METHOD_GET);
        clientReq->setHttpVersion("HTTP/1.1");
    }

protected:
    void initFailed() override ;
    bool finishOnce() override ;
    bool initSuccess() override ;
    int keepAliveTimeout() override ;
    CommMessageOut *messageOut() override;
    CommMessageIn *messageIn() override ;

protected:
    bool needRedirect(ParsedURI& uri);
    bool redirectUrl(HttpResponse *client_resp, ParsedURI& uri);
    void setEmptyRequest();
    void checkResponse();

private:
    int redirect_max_;
    int redirect_count_;
    void*                   mUdata;
};

CommMessageOut *ComplexHttpTask::messageOut()
{
    HttpRequest *req = this->getReq();
    HttpMessageHeader header;
    bool is_alive;

    if (!req->isChunked() && !req->hasContentLengthHeader()) {
        size_t body_size = req->getOutputBodySize();
        const char *method = req->getMethod();

        if (body_size != 0 || strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
            char buf[32];
            header.name = "Content-Length";
            header.nameLen = strlen("Content-Length");
            header.value = buf;
            header.valueLen = sprintf(buf, "%zu", body_size);
            req->addHeader(&header);
        }
    }

    if (req->hasConnectionHeader())
        is_alive = req->isKeepAlive();
    else {
        header.name = "Connection";
        header.nameLen = strlen("Connection");
        is_alive = (this->mKeepAliveTime != 0);
        if (is_alive) {
            header.value = "Keep-Alive";
            header.valueLen = strlen("Keep-Alive");
        } else {
            header.value = "close";
            header.valueLen = strlen("close");
        }

        req->addHeader(&header);
    }

    if (!is_alive)
        this->mKeepAliveTime = 0;
    else if (req->hasKeepAliveHeader()) {
        protocol::HttpHeaderCursor req_cursor(req);

        //req---Connection: Keep-Alive
        //req---Keep-Alive: timeout=0,max=100
        header.name = "Keep-Alive";
        header.nameLen = strlen("Keep-Alive");
        if (req_cursor.find(&header)) {
            std::string keep_alive((const char *)header.value, header.valueLen);
            std::vector<std::string> params = StringUtil::split(keep_alive, ',');

            for (const auto& kv : params) {
                std::vector<std::string> arr = StringUtil::split(kv, '=');
                if (arr.size() < 2)
                    arr.emplace_back("0");

                std::string key = StringUtil::strip(arr[0]);
                std::string val = StringUtil::strip(arr[1]);
                if (strcasecmp(key.c_str(), "timeout") == 0) {
                    this->mKeepAliveTime = 1000 * atoi(val.c_str());
                    break;
                }
            }
        }

        if ((unsigned int)this->mKeepAliveTime > HTTP_KEEPALIVE_MAX)
            this->mKeepAliveTime = HTTP_KEEPALIVE_MAX;
        //if (this->keep_alive_timeo < 0 || this->keep_alive_timeo > HTTP_KEEPALIVE_MAX)
    }

    //req->set_header_pair("Accept", "*/*");
    return this->ComplexClientTask::messageOut();
}

CommMessageIn *ComplexHttpTask::messageIn()
{
    HttpResponse *resp = this->getResp();

    if (strcmp(this->getReq()->getMethod(), HTTP_METHOD_HEAD) == 0)
        resp->parseZeroBody();

    return this->ComplexClientTask::messageIn();
}

int ComplexHttpTask::keepAliveTimeout()
{
    return this->mResp.isKeepAlive() ? this->keepAliveTimeout() : 0;
}

void ComplexHttpTask::setEmptyRequest()
{
    HttpRequest *client_req = this->getReq();
    client_req->setRequestUri("/");
    client_req->setHeaderPair("Host", "");
}

void ComplexHttpTask::initFailed()
{
    this->setEmptyRequest();
}

bool ComplexHttpTask::initSuccess()
{
    HttpRequest *client_req = this->getReq();
    std::string request_uri;
    std::string header_host;
    bool is_ssl;
    bool is_unix = false;

    if (mUri.scheme && strcasecmp(mUri.scheme, "http") == 0)
        is_ssl = false;
    else if (mUri.scheme && strcasecmp(mUri.scheme, "https") == 0)
        is_ssl = true;
    else {
        this->mState = TASK_STATE_TASK_ERROR;
        this->mError = TASK_ERROR_URI_SCHEME_INVALID;
        this->setEmptyRequest();
        return false;
    }

    //todo http+unix
    //https://stackoverflow.com/questions/26964595/whats-the-correct-way-to-use-a-unix-domain-socket-in-requests-framework
    //https://stackoverflow.com/questions/27037990/connecting-to-postgres-via-database-url-and-unix-socket-in-rails

    if (mUri.path && mUri.path[0])
        request_uri = mUri.path;
    else
        request_uri = "/";

    if (mUri.query && mUri.query[0]) {
        request_uri += "?";
        request_uri += mUri.query;
    }

    if (mUri.host && mUri.host[0]) {
        header_host = mUri.host;
        if (mUri.host[0] == '/')
            is_unix = true;
    }

    if (!is_unix && mUri.port && mUri.port[0]) {
        int port = atoi(mUri.port);

        if (is_ssl) {
            if (port != 443) {
                header_host += ":";
                header_host += mUri.port;
            }
        } else {
            if (port != 80) {
                header_host += ":";
                header_host += mUri.port;
            }
        }
    }

    this->ComplexClientTask::setTransportType(is_ssl ? TT_TCP_SSL : TT_TCP);
    client_req->setRequestUri(request_uri.c_str());
    client_req->setHeaderPair("Host", header_host.c_str());

    return true;
}

bool ComplexHttpTask::redirectUrl(HttpResponse *client_resp, ParsedURI& uri)
{
    if (redirect_count_ < redirect_max_) {
        redirect_count_++;
        std::string url;
        protocol::HttpHeaderCursor cursor(client_resp);

        if (!cursor.find("Location", url) || url.empty()) {
            this->mState = TASK_STATE_TASK_ERROR;
            this->mError = TASK_ERROR_HTTP_BAD_REDIRECT_HEADER;
            return false;
        }

        if (url[0] == '/') {
            if (url[1] != '/') {
                if (uri.port)
                    url = ':' + (uri.port + url);
                url = "//" + (uri.host + url);
            }

            url = uri.scheme + (':' + url);
        }

        URIParser::parse(url, uri);
        return true;
    }

    return false;
}

bool ComplexHttpTask::needRedirect(ParsedURI& uri)
{
    HttpRequest *client_req = this->getReq();
    HttpResponse *client_resp = this->getResp();
    const char *status_code_str = client_resp->getStatusCode();
    const char *method = client_req->getMethod();

    if (!status_code_str || !method)
        return false;

    int status_code = atoi(status_code_str);

    switch (status_code) {
        case 301:
        case 302:
        case 303:
            if (redirectUrl(client_resp, uri)) {
                if (strcasecmp(method, HTTP_METHOD_GET) != 0 && strcasecmp(method, HTTP_METHOD_HEAD) != 0) {
                    client_req->setMethod(HTTP_METHOD_GET);
                }

                return true;
            }
            else
                break;

        case 307:
        case 308:
            if (redirectUrl(client_resp, uri))
                return true;
            else
                break;

        default:
            break;
    }

    return false;
}

void ComplexHttpTask::checkResponse()
{
    HttpResponse *resp = this->getResp();

    resp->endParsing();
    if (this->mState == TASK_STATE_SYS_ERROR && this->mError == ECONNRESET) {
        /* Servers can end the message by closing the connection. */
        if (resp->isHeaderComplete() &&
            !resp->isKeepAlive() &&
            !resp->isChunked() &&
            !resp->hasContentLengthHeader())
        {
            this->mState = TASK_STATE_SUCCESS;
            this->mError = 0;
        }
    }
}

bool ComplexHttpTask::finishOnce()
{
    if (this->mState != TASK_STATE_SUCCESS)
        this->checkResponse();

    if (this->mState == TASK_STATE_SUCCESS) {
        if (this->needRedirect(mUri))
            this->setRedirect(mUri);
        else if (this->mState != TASK_STATE_SUCCESS)
            this->disableRetry();
    }

    return true;
}

/*******Proxy Client*******/

static int __encode_auth(const char *p, std::string& auth)
{
    size_t len = strlen(p);
    size_t base64_len = (len + 2) / 3 * 4;
    char *base64 = (char *)malloc(base64_len + 1);

    if (!base64)
        return -1;

    EVP_EncodeBlock((unsigned char *)base64, (const unsigned char *)p, len);
    auth.append("Basic ");
    auth.append(base64, base64_len);

    free(base64);
    return 0;
}

static SSL *__create_ssl(SSL_CTX *ssl_ctx)
{
    BIO *wbio;
    BIO *rbio;
    SSL *ssl;

    rbio = BIO_new(BIO_s_mem());
    if (rbio)
    {
        wbio = BIO_new(BIO_s_mem());
        if (wbio)
        {
            ssl = SSL_new(ssl_ctx);
            if (ssl)
            {
                SSL_set_bio(ssl, rbio, wbio);
                return ssl;
            }

            BIO_free(wbio);
        }

        BIO_free(rbio);
    }

    return NULL;
}

class ComplexHttpProxyTask : public ComplexHttpTask
{
public:
    ComplexHttpProxyTask(int redirect_max, int retry_max, HttpCallback && callback, void* udata)
        : ComplexHttpTask(redirect_max, retry_max, std::move(callback), udata), is_user_request_(true), mUdata(udata)
    { }

    void setUserUri(ParsedURI&& uri) { user_uri_ = std::move(uri); }
    void setUserUri(const ParsedURI& uri) { user_uri_ = uri; }

    virtual const ParsedURI *getCurrentUri() const { return &user_uri_; }

protected:
    virtual CommMessageOut *messageOut();
    virtual CommMessageIn *messageIn();
    virtual int keepAliveTimeout();
    virtual bool initSuccess();
    virtual bool finishOnce();

protected:
    virtual Connection *getConnection() const
    {
        Connection *conn = this->ComplexHttpTask::getConnection();

        if (conn && is_ssl_)
            return (SSLConnection *)conn->getContext();

        return conn;
    }

private:
    struct SSLConnection : public Connection
    {
        SSL *ssl_;
        SSLHandshaker handshaker_;
        SSLWrapper wrapper_;
        SSLConnection(SSL *ssl) : handshaker_(ssl), wrapper_(&wrapper_, ssl)
        {
            ssl_ = ssl;
        }
    };

    SSLHandshaker *getSslHandshaker() const
    {
        return &((SSLConnection *)this->getConnection())->handshaker_;
    }

    SSLWrapper *getSslWrapper(ProtocolMessage *msg) const
    {
        SSLConnection *conn = (SSLConnection *)this->getConnection();
        conn->wrapper_ = SSLWrapper(msg, conn->ssl_);
        return &conn->wrapper_;
    }

    int initSslConnection();

private:
    std::string proxy_auth_;
    ParsedURI user_uri_;
    bool is_ssl_;
    bool is_user_request_;
    short state_;
    int error_;

    void*           mUdata;
};

int ComplexHttpProxyTask::initSslConnection()
{
    SSL *ssl = __create_ssl(Global::getSslClientCtx());
    Connection *conn;

    if (!ssl)
        return -1;

    SSL_set_tlsext_host_name(ssl, user_uri_.host);
    SSL_set_connect_state(ssl);

    conn = this->ComplexHttpTask::getConnection();
    SSLConnection *ssl_conn = new SSLConnection(ssl);

    auto&& deleter = [] (void *ctx)
    {
        SSLConnection *ssl_conn = (SSLConnection *)ctx;
        SSL_free(ssl_conn->ssl_);
        delete ssl_conn;
    };
    conn->setContext(ssl_conn, std::move(deleter));
    return 0;
}

CommMessageOut *ComplexHttpProxyTask::messageOut()
{
    long long seqid = this->getSeq();

    if (seqid == 0) // CONNECT
    {
        HttpRequest *conn_req = new HttpRequest;
        std::string request_uri(user_uri_.host);

        request_uri += ":";
        if (user_uri_.port)
            request_uri += user_uri_.port;
        else
            request_uri += is_ssl_ ? "443" : "80";

        conn_req->setMethod("CONNECT");
        conn_req->setRequestUri(request_uri);
        conn_req->setHttpVersion("HTTP/1.1");
        conn_req->addHeaderPair("Host", request_uri.c_str());

        if (!proxy_auth_.empty())
            conn_req->addHeaderPair("Proxy-Authorization", proxy_auth_);

        is_user_request_ = false;
        return conn_req;
    }
    else if (seqid == 1 && is_ssl_) // HANDSHAKE
    {
        is_user_request_ = false;
        return getSslHandshaker();
    }

    auto *msg = (ProtocolMessage *)this->ComplexHttpTask::messageOut();
    return is_ssl_ ? getSslWrapper(msg) : msg;
}

CommMessageIn *ComplexHttpProxyTask::messageIn()
{
    long long seqid = this->getSeq();

    if (seqid == 0) {
        HttpResponse *conn_resp = new HttpResponse;
        conn_resp->parseZeroBody();
        return conn_resp;
    }
    else if (seqid == 1 && is_ssl_)
        return getSslHandshaker();

    auto *msg = (ProtocolMessage *)this->ComplexHttpTask::messageIn();
    if (is_ssl_)
        return getSslWrapper(msg);

    return msg;
}

int ComplexHttpProxyTask::keepAliveTimeout()
{
    long long seqid = this->getSeq();

    state_ = TASK_STATE_SUCCESS;
    error_ = 0;
    if (seqid == 0) {
        HttpResponse *resp = this->getResp();
        const char *code_str;
        int status_code;

        *resp = std::move(*(HttpResponse *)this->getMessageIn());
        code_str = resp->getStatusCode();
        status_code = code_str ? atoi(code_str) : 0;

        switch (status_code) {
            case 200:
                break;
            case 407:
                this->disableRetry();
            default:
                state_ = TASK_STATE_TASK_ERROR;
                error_ = TASK_ERROR_HTTP_PROXY_CONNECT_FAILED;
                return 0;
        }

        this->clearResp();

        if (is_ssl_ && initSslConnection() < 0) {
            state_ = TASK_STATE_SYS_ERROR;
            error_ = errno;
            return 0;
        }

        return HTTP_KEEPALIVE_DEFAULT;
    } else if (seqid == 1 && is_ssl_) {
        return HTTP_KEEPALIVE_DEFAULT;
    }

    return this->ComplexHttpTask::keepAliveTimeout();
}

bool ComplexHttpProxyTask::initSuccess()
{
    if (!mUri.scheme || strcasecmp(mUri.scheme, "http") != 0) {
        this->mState = TASK_STATE_TASK_ERROR;
        this->mError = TASK_ERROR_URI_SCHEME_INVALID;
        return false;
    }

    if (user_uri_.state == URI_STATE_ERROR) {
        this->mState = TASK_STATE_SYS_ERROR;
        this->mError = mUri.error;
        return false;
    }
    else if (user_uri_.state != URI_STATE_SUCCESS)
    {
        this->mState = TASK_STATE_TASK_ERROR;
        this->mError = TASK_ERROR_URI_PARSE_FAILED;
        return false;
    }

    if (user_uri_.scheme && strcasecmp(user_uri_.scheme, "http") == 0)
        is_ssl_ = false;
    else if (user_uri_.scheme && strcasecmp(user_uri_.scheme, "https") == 0)
        is_ssl_ = true;
    else
    {
        this->mState = TASK_STATE_TASK_ERROR;
        this->mError = TASK_ERROR_URI_SCHEME_INVALID;
        this->setEmptyRequest();
        return false;
    }

    int user_port;
    if (user_uri_.port)
    {
        user_port = atoi(user_uri_.port);
        if (user_port <= 0 || user_port > 65535)
        {
            this->mState = TASK_STATE_TASK_ERROR;
            this->mError = TASK_ERROR_URI_PORT_INVALID;
            return false;
        }
    }
    else
        user_port = is_ssl_ ? 443 : 80;

    if (mUri.userInfo && mUri.userInfo[0])
    {
        proxy_auth_.clear();
        if (__encode_auth(mUri.userInfo, proxy_auth_) < 0)
        {
            this->mState = TASK_STATE_SYS_ERROR;
            this->mError = errno;
            return false;
        }
    }

    std::string info("http-proxy|remote:");
    info += is_ssl_ ? "https://" : "http://";
    info += user_uri_.host;
    info += ":";
    if (user_uri_.port)
        info += user_uri_.port;
    else
        info += is_ssl_ ? "443" : "80";
    info += "|auth:";
    info += proxy_auth_;

    this->ComplexClientTask::setInfo(info);

    std::string request_uri;
    std::string header_host;

    if (user_uri_.path && user_uri_.path[0])
        request_uri = user_uri_.path;
    else
        request_uri = "/";

    if (user_uri_.query && user_uri_.query[0])
    {
        request_uri += "?";
        request_uri += user_uri_.query;
    }

    if (user_uri_.host && user_uri_.host[0])
        header_host = user_uri_.host;

    if ((is_ssl_ && user_port != 443) || (!is_ssl_ && user_port != 80))
    {
        header_host += ":";
        header_host += mUri.port;
    }

    HttpRequest *client_req = this->getReq();
    client_req->setRequestUri(request_uri.c_str());
    client_req->setHeaderPair("Host", header_host.c_str());
    this->ComplexClientTask::setTransportType(TT_TCP);
    return true;
}

bool ComplexHttpProxyTask::finishOnce()
{
    if (!is_user_request_) {
        if (this->mState == TASK_STATE_SUCCESS && state_ != TASK_STATE_SUCCESS) {
            this->mState = state_;
            this->mError = error_;
        }

        if (this->getSeq() == 0) {
            delete this->getMessageIn();
            delete this->getMessageOut();
        }

        is_user_request_ = true;
        return false;
    }

    if (this->mState != TASK_STATE_SUCCESS)
        this->checkResponse();

    if (this->mState == TASK_STATE_SUCCESS) {
        if (this->needRedirect(user_uri_))
            this->setRedirect(mUri);
        else if (this->mState != TASK_STATE_SUCCESS)
            this->disableRetry();
    }

    return true;
}

/**********Client Factory**********/

HttpTask *TaskFactory::createHttpTask(const std::string& url, int redirect_max, int retry_max, HttpCallback callback, void* udata)
{
    auto *task = new ComplexHttpTask(redirect_max, retry_max, std::move(callback), udata);
    ParsedURI uri;

    URIParser::parse(url, uri);
    task->init(std::move(uri));
    task->setKeepAlive(HTTP_KEEPALIVE_DEFAULT);
    return task;
}

HttpTask *TaskFactory::createHttpTask(const ParsedURI& uri, int redirect_max, int retry_max, HttpCallback callback, void* udata)
{
    auto *task = new ComplexHttpTask(redirect_max, retry_max, std::move(callback), udata);

    task->init(uri);
    task->setKeepAlive(HTTP_KEEPALIVE_DEFAULT);
    return task;
}

HttpTask *TaskFactory::createHttpTask(const std::string& url, const std::string& proxy_url, int redirect_max, int retry_max, HttpCallback callback, void* udata)
{
    auto *task = new ComplexHttpProxyTask(redirect_max, retry_max, std::move(callback), udata);

    ParsedURI uri, user_uri;
    URIParser::parse(url, user_uri);
    URIParser::parse(proxy_url, uri);

    task->setUserUri(std::move(user_uri));
    task->setKeepAlive(HTTP_KEEPALIVE_DEFAULT);
    task->init(std::move(uri));
    return task;
}

HttpTask *TaskFactory::createHttpTask(const ParsedURI& uri, const ParsedURI& proxy_uri, int redirect_max, int retry_max, HttpCallback callback, void* udata)
{
    auto *task = new ComplexHttpProxyTask(redirect_max, retry_max, std::move(callback), udata);

    task->setUserUri(uri);
    task->setKeepAlive(HTTP_KEEPALIVE_DEFAULT);
    task->init(proxy_uri);
    return task;
}

/**********Server**********/

class HttpServerTask : public ServerTask<HttpRequest, HttpResponse>
{
public:
    HttpServerTask(CommService *service, std::function<void (HttpTask *)>& process, void* udata)
        : ServerTask(service, Global::getScheduler(), process, udata), req_is_alive_(false), req_has_keep_alive_header_(false)
    {
        logv("");
    }

protected:
    virtual void handle(int state, int error)
    {
        if (state == TASK_STATE_TOREPLY) {
            req_is_alive_ = this->mReq.isKeepAlive();
            if (req_is_alive_ && this->mReq.hasKeepAliveHeader()) {
                protocol::HttpHeaderCursor req_cursor(&this->mReq);
                HttpMessageHeader header;

                header.name = "Keep-Alive";
                header.nameLen = strlen("Keep-Alive");
                req_has_keep_alive_header_ = req_cursor.find(&header);
                if (req_has_keep_alive_header_) {
                    req_keep_alive_.assign((const char *)header.value, header.valueLen);
                }
            }
        }

        this->ServerTask::handle(state, error);
    }

    virtual CommMessageOut *messageOut();

private:
    bool req_is_alive_;
    bool req_has_keep_alive_header_;
    std::string req_keep_alive_;
};

CommMessageOut *HttpServerTask::messageOut()
{
    HttpResponse *resp = this->getResp();
    HttpMessageHeader header;

    if (!resp->getHttpVersion())
        resp->setHttpVersion("HTTP/1.1");

    const char *status_code_str = resp->getStatusCode();
    if (!status_code_str || !resp->getReasonPhrase()) {
        int status_code;

        if (status_code_str)
            status_code = atoi(status_code_str);
        else
            status_code = HttpStatusOK;

        HttpUtil::setResponseStatus(resp, status_code);
    }

    if (!resp->isChunked() && !resp->hasContentLengthHeader()) {
        char buf[32];
        header.name = "Content-Length";
        header.nameLen = strlen("Content-Length");
        header.value = buf;
        header.valueLen = sprintf(buf, "%zu", resp->getOutputBodySize());
        resp->addHeader(&header);
    }

    bool is_alive;

    if (resp->hasConnectionHeader())
        is_alive = resp->isKeepAlive();
    else
        is_alive = req_is_alive_;

    if (!is_alive)
        this->mKeepAliveTime = 0;
    else {
        //req---Connection: Keep-Alive
        //req---Keep-Alive: timeout=5,max=100

        if (req_has_keep_alive_header_) {
            int flag = 0;
            std::vector<std::string> params = StringUtil::split(req_keep_alive_, ',');

            for (const auto& kv : params) {
                std::vector<std::string> arr = StringUtil::split(kv, '=');
                if (arr.size() < 2)
                    arr.emplace_back("0");

                std::string key = StringUtil::strip(arr[0]);
                std::string val = StringUtil::strip(arr[1]);
                if (!(flag & 1) && strcasecmp(key.c_str(), "timeout") == 0) {
                    flag |= 1;
                    // keep_alive_timeo = 5000ms when Keep-Alive: timeout=5
                    this->mKeepAliveTime = 1000 * atoi(val.c_str());
                    if (flag == 3)
                        break;
                } else if (!(flag & 2) && strcasecmp(key.c_str(), "max") == 0) {
                    flag |= 2;
                    if (this->getSeq() >= atoi(val.c_str())) {
                        this->mKeepAliveTime = 0;
                        break;
                    }

                    if (flag == 3)
                        break;
                }
            }
        }

        if ((unsigned int)this->mKeepAliveTime > HTTP_KEEPALIVE_MAX)
            this->mKeepAliveTime = HTTP_KEEPALIVE_MAX;
        //if (this->keep_alive_timeo < 0 || this->keep_alive_timeo > HTTP_KEEPALIVE_MAX)

    }

    if (!resp->hasConnectionHeader()) {
        header.name = "Connection";
        header.nameLen = 10;
        if (this->mKeepAliveTime == 0) {
            header.value = "close";
            header.valueLen = 5;
        } else {
            header.value = "Keep-Alive";
            header.valueLen = 10;
        }

        resp->addHeader(&header);
    }

    return this->ServerTask::messageOut();
}

/**********Server Factory**********/

HttpTask *ServerTaskFactory::createHttpTask(CommService *service, std::function<void (HttpTask*)>& process, void* udata)
{
    logv("");
    return new HttpServerTask(service, process, udata);
}

