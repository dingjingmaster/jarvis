//
// Created by dingjing on 8/23/22.
//

#include <string>
#include "task-error.h"
#include "task-factory.h"
#include "../protocol/dns/dns-message.h"

using namespace protocol;

#define DNS_KEEPALIVE_DEFAULT	(60 * 1000)

/**********Client**********/

class ComplexDnsTask : public ComplexClientTask<DnsRequest, DnsResponse, std::function<void (DnsTask *)>>
{
    static struct addrinfo hints;

public:
    ComplexDnsTask(int retry_max, DnsCallback && cb, void* udata)
        : ComplexClientTask(retry_max, std::move(cb), udata)
    {
        this->setTransportType(TT_UDP);
    }

protected:
    virtual CommMessageOut *messageOut();
    virtual bool initSuccess();
    virtual bool finishOnce();

private:
    bool need_redirect();
};

struct addrinfo ComplexDnsTask::hints =
        {
                .ai_flags     = AI_NUMERICSERV | AI_NUMERICHOST,
                .ai_family    = AF_UNSPEC,
                .ai_socktype  = SOCK_STREAM,
                .ai_protocol  = 0,
                .ai_addrlen   = 0,
                .ai_addr      = NULL,
                .ai_canonname = NULL,
                .ai_next      = NULL
        };

CommMessageOut *ComplexDnsTask::messageOut()
{
    DnsRequest *req = this->getReq();
    DnsResponse *resp = this->getResp();
    TransportType type = this->getTransportType();

    /* Set these field every time, in case of reconstruct on redirect */
    resp->set_request_id(req->get_id());
    resp->set_request_name(req->get_question_name());
    req->set_single_packet(type == TT_UDP);
    resp->set_single_packet(type == TT_UDP);

    return this->ClientTask::messageOut();
}

bool ComplexDnsTask::initSuccess()
{
    if (mUri.scheme && strcasecmp(mUri.scheme, "dnss") == 0)
        this->ComplexClientTask::setTransportType(TT_TCP_SSL);
    else if (!mUri.scheme || strcasecmp(mUri.scheme, "dns") != 0)
    {
        this->mState = TASK_STATE_TASK_ERROR;
        this->mError = TASK_ERROR_URI_SCHEME_INVALID;
        return false;
    }

    if (!this->mRouteResult.mRequestObject)
    {
        TransportType type = this->getTransportType();
        struct addrinfo *addr;
        int ret;

        ret = getaddrinfo(mUri.host, mUri.port, &hints, &addr);
        if (ret != 0)
        {
            this->mState = TASK_STATE_TASK_ERROR;
            this->mError = TASK_ERROR_URI_PARSE_FAILED;
            return false;
        }

        auto *ep = &Global::getGlobalSettings()->dnsServerParams;
        ret = Global::getRouteManager()->get(type, addr, mInfo, ep, mUri.host, mRouteResult);
        freeaddrinfo(addr);
        if (ret < 0)
        {
            this->mState = TASK_STATE_SYS_ERROR;
            this->mError = errno;
            return false;
        }
    }

    return true;
}

bool ComplexDnsTask::finishOnce()
{
    if (this->mState == TASK_STATE_SUCCESS)
    {
        if (need_redirect())
            this->setRedirect(mUri);
        else if (this->mState != TASK_STATE_SUCCESS)
            this->disableRetry();
    }

    /* If retry times meet retry max and there is no redirect,
     * we ask the client for a retry or redirect.
     */
    if (mRetryTimes == mRetryMax && !mRedirect && *this->getMutableCtx())
    {
        /* Reset type to UDP before a client redirect. */
        this->setTransportType(TT_UDP);
        (*this->getMutableCtx())(this);
    }

    return true;
}

bool ComplexDnsTask::need_redirect()
{
    DnsResponse *client_resp = this->getResp();
    TransportType type = this->getTransportType();

    if (type == TT_UDP && client_resp->get_tc() == 1) {
        this->setTransportType(TT_TCP);
        return true;
    }

    return false;
}

/**********Client Factory**********/

DnsTask *TaskFactory::createDnsTask (const std::string& url, int retry_max, DnsCallback callback, void* udata)
{
    ParsedURI uri;

    URIParser::parse(url, uri);
    return TaskFactory::createDnsTask(uri, retry_max, std::move(callback), udata);
}

DnsTask *TaskFactory::createDnsTask(const ParsedURI& uri, int retry_max, DnsCallback callback, void* udata)
{
    ComplexDnsTask *task = new ComplexDnsTask(retry_max, std::move(callback), udata);
    const char *name;

    if (uri.path && uri.path[0] && uri.path[1])
        name = uri.path + 1;
    else
        name = ".";

    DnsRequest *req = task->getReq();
    req->set_question(name, DNS_TYPE_A, DNS_CLASS_IN);

    task->init(uri);
    task->setKeepAlive(DNS_KEEPALIVE_DEFAULT);

    return task;
}

