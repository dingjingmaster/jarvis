//
// Created by dingjing on 8/22/22.
//

#include "dns-client.h"

#include <string>
#include <vector>
#include <atomic>

#include "../utils/uri-parser.h"
#include "../utils/string-util.h"

using namespace protocol;

using DnsCtx = std::function<void (DnsTask*)>;
using ComplexTask = ComplexClientTask<DnsRequest, DnsResponse, DnsCtx>;

class DnsParams
{
public:
    struct dns_params
    {
        std::vector<ParsedURI> uris;
        std::vector<std::string> search_list;
        int ndots;
        int attempts;
        bool rotate;
    };

public:
    DnsParams()
    {
        this->ref = new std::atomic<size_t>(1);
        this->params = new dns_params();
    }

    DnsParams(const DnsParams& p)
    {
        this->ref = p.ref;
        this->params = p.params;
        this->incref();
    }

    DnsParams& operator=(const DnsParams& p)
    {
        if (this != &p)
        {
            this->decref();
            this->ref = p.ref;
            this->params = p.params;
            this->incref();
        }
        return *this;
    }

    ~DnsParams() { this->decref(); }

    const dns_params *get_params() const { return this->params; }
    dns_params *get_params() { return this->params; }

private:
    void incref() { (*this->ref)++; }
    void decref()
    {
        if (--*this->ref == 0)
        {
            delete this->params;
            delete this->ref;
        }
    }

private:
    dns_params *params;
    std::atomic<size_t> *ref;
};

enum
{
    DNS_STATUS_TRY_ORIGIN_DONE = 0,
    DNS_STATUS_TRY_ORIGIN_FIRST = 1,
    DNS_STATUS_TRY_ORIGIN_LAST = 2
};

struct DnsStatus
{
    std::string origin_name;
    std::string current_name;
    size_t next_server;			// next server to try
    size_t last_server;			// last server to try
    size_t next_domain;			// next search domain to try
    int attempts_left;
    int try_origin_state;
};

static int __get_ndots(const std::string& s)
{
    int ndots = 0;
    for (size_t i = 0; i < s.size(); i++)
        ndots += s[i] == '.';
    return ndots;
}

static bool __has_next_name(const DnsParams::dns_params *p,
                            struct DnsStatus *s)
{
    if (s->try_origin_state == DNS_STATUS_TRY_ORIGIN_FIRST)
    {
        s->current_name = s->origin_name;
        s->try_origin_state = DNS_STATUS_TRY_ORIGIN_DONE;
        return true;
    }

    if (s->next_domain < p->search_list.size())
    {
        s->current_name = s->origin_name;
        s->current_name.push_back('.');
        s->current_name.append(p->search_list[s->next_domain]);

        s->next_domain++;
        return true;
    }

    if (s->try_origin_state == DNS_STATUS_TRY_ORIGIN_LAST)
    {
        s->current_name = s->origin_name;
        s->try_origin_state = DNS_STATUS_TRY_ORIGIN_DONE;
        return true;
    }

    return false;
}

static void __callback_internal(DnsTask *task, const DnsParams& params,
                                struct DnsStatus& s)
{
    ComplexTask *ctask = static_cast<ComplexTask *>(task);
    int state = task->getState();
    DnsRequest *req = task->getReq();
    DnsResponse *resp = task->getResp();
    const auto *p = params.get_params();
    int rcode = resp->get_rcode();

    bool try_next_server = state != TASK_STATE_SUCCESS ||
                           rcode == DNS_RCODE_SERVER_FAILURE ||
                           rcode == DNS_RCODE_NOT_IMPLEMENTED ||
                           rcode == DNS_RCODE_REFUSED;
    bool try_next_name = rcode == DNS_RCODE_FORMAT_ERROR ||
                         rcode == DNS_RCODE_NAME_ERROR ||
                         resp->get_ancount() == 0;

    if (try_next_server)
    {
        if (s.last_server == s.next_server)
            s.attempts_left--;
        if (s.attempts_left <= 0)
            return;

        s.next_server = (s.next_server + 1) % p->uris.size();
        ctask->setRedirect(p->uris[s.next_server]);
        return;
    }

    if (try_next_name && __has_next_name(p, &s))
    {
        req->set_question_name(s.current_name.c_str());
        ctask->setRedirect(p->uris[s.next_server]);
        return;
    }
}

int DnsClient::init(const std::string& url)
{
    return this->init(url, "", 1, 2, false);
}

int DnsClient::init(const std::string& url, const std::string& search_list,
                      int ndots, int attempts, bool rotate)
{
    std::vector<std::string> hosts;
    std::vector<ParsedURI> uris;
    std::string host;
    ParsedURI uri;

    this->mId = 0;
    hosts = StringUtil::splitFilterEmpty(url, ',');

    for (size_t i = 0; i < hosts.size(); i++)
    {
        host = hosts[i];
        if (strncasecmp(host.c_str(), "dns://", 6) != 0 &&
            strncasecmp(host.c_str(), "dnss://", 7) != 0)
        {
            host = "dns://" + host;
        }

        if (URIParser::parse(host, uri) != 0)
            return -1;

        uris.emplace_back(std::move(uri));
    }

    if (uris.empty() || ndots < 0 || attempts < 1)
    {
        errno = EINVAL;
        return -1;
    }

    this->mParams = new DnsParams;
    DnsParams::dns_params *q = ((DnsParams *)this->mParams)->get_params();
    q->uris = std::move(uris);
    q->search_list = StringUtil::splitFilterEmpty(search_list, ',');
    q->ndots = ndots > 15 ? 15 : ndots;
    q->attempts = attempts > 5 ? 5 : attempts;
    q->rotate = rotate;

    return 0;
}

void DnsClient::deInit()
{
    delete (DnsParams *)this->mParams;
    this->mParams = NULL;
}

DnsTask *DnsClient::createDnsTask(const std::string& name, DnsCallback callback)
{
    DnsParams::dns_params *p = ((DnsParams *)this->mParams)->get_params();
    struct DnsStatus status;
    size_t next_server;
    DnsTask *task;
    DnsRequest *req;

    next_server = p->rotate ? this->mId++ % p->uris.size() : 0;

    status.origin_name = name;
    status.next_domain = 0;
    status.attempts_left = p->attempts;
    status.try_origin_state = DNS_STATUS_TRY_ORIGIN_FIRST;

    if (!name.empty() && name.back() == '.')
        status.next_domain = p->search_list.size();
    else if (__get_ndots(name) < p->ndots)
        status.try_origin_state = DNS_STATUS_TRY_ORIGIN_LAST;

    __has_next_name(p, &status);

    task = TaskFactory::createDnsTask(p->uris[next_server], 0, std::move(callback));
    status.next_server = next_server;
    status.last_server = (next_server + p->uris.size() - 1) % p->uris.size();

    req = task->getReq();
    req->set_question(status.current_name.c_str(), DNS_TYPE_A, DNS_CLASS_IN);
    req->set_rd(1);

    ComplexTask *ctask = static_cast<ComplexTask *>(task);
    *ctask->getMutableCtx() = std::bind(__callback_internal, std::placeholders::_1, *(DnsParams *)mParams, status);

    return task;
}

