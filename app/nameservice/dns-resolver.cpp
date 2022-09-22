//
// Created by dingjing on 8/22/22.
//

#include "dns-resolver.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <utility>
#include <string>

#include "../algorithm/dns-routine.h"
#include "../manager/route-manager.h"
#include "../manager/endpoint-params.h"
#include "../manager/global.h"
#include "../factory/task-factory.h"
#include "../factory/resource-pool.h"
#include "name-service.h"
#include "../manager/dns-cache.h"
#include "../client/dns-client.h"
#include "../protocol/dns/dns-util.h"

#define HOSTS_LINEBUF_INIT_SIZE	128
#define PORT_STR_MAX			5

// Dns Thread task. For internal usage only.
using ThreadDnsTask = ThreadTask<DnsInput, DnsOutput>;
using thread_dns_callback_t = std::function<void (ThreadDnsTask *)>;

static constexpr struct addrinfo __ai_hints =
        {
#ifdef AI_ADDRCONFIG
                .ai_flags = AI_ADDRCONFIG,
#else
                .ai_flags = 0,
#endif
                .ai_family = AF_UNSPEC,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = 0,
                .ai_addrlen = 0,
                .ai_addr = NULL,
                .ai_canonname = NULL,
                .ai_next = NULL
        };

struct DnsContext
{
    int state;
    int error;
    int eai_error;
    unsigned short port;
    struct addrinfo *ai;
};

static int __default_family()
{
    struct addrinfo *res;
    struct addrinfo *cur;
    int family = AF_UNSPEC;
    bool v4 = false;
    bool v6 = false;

    if (getaddrinfo(NULL, "1", &__ai_hints, &res) == 0)
    {
        for (cur = res; cur; cur = cur->ai_next)
        {
            if (cur->ai_family == AF_INET)
                v4 = true;
            else if (cur->ai_family == AF_INET6)
                v6 = true;
        }

        freeaddrinfo(res);
        if (v4 ^ v6)
            family = v4 ? AF_INET : AF_INET6;
    }

    return family;
}

// hosts line format: IP canonical_name [aliases...] [# Comment]
static int __readaddrinfo_line(char *p, const char *name, const char *port,
                               const struct addrinfo *hints,
                               struct addrinfo **res)
{
    const char *ip = NULL;
    char *start;

    start = p;
    while (*start != '\0' && *start != '#')
        start++;
    *start = '\0';

    while (1)
    {
        while (isspace(*p))
            p++;

        start = p;
        while (*p != '\0' && !isspace(*p))
            p++;

        if (start == p)
            break;

        if (*p != '\0')
            *p++ = '\0';

        if (ip == NULL)
        {
            ip = start;
            continue;
        }

        if (strcasecmp(name, start) == 0)
        {
            if (getaddrinfo(ip, port, hints, res) == 0)
                return 0;
        }
    }

    return 1;
}

static int __readaddrinfo(const char *path,
                          const char *name, unsigned short port,
                          const struct addrinfo *hints,
                          struct addrinfo **res)
{
    char port_str[PORT_STR_MAX + 1];
    size_t bufsize = 0;
    char *line = NULL;
    int count = 0;
    struct addrinfo h;
    int errno_bak;
    FILE *fp;
    int ret;

    fp = fopen(path, "r");
    if (!fp)
        return EAI_SYSTEM;

    h = *hints;
    h.ai_flags |= AI_NUMERICSERV | AI_NUMERICHOST,
            snprintf(port_str, PORT_STR_MAX + 1, "%u", port);

    errno_bak = errno;
    while ((ret = getline(&line, &bufsize, fp)) > 0)
    {
        if (__readaddrinfo_line(line, name, port_str, &h, res) == 0)
        {
            count++;
            res = &(*res)->ai_next;
        }
    }

    ret = ferror(fp) ? EAI_SYSTEM : EAI_NONAME;
    free(line);
    fclose(fp);
    if (count != 0)
    {
        errno = errno_bak;
        return 0;
    }

    return ret;
}

// Add AI_PASSIVE to point that this addrinfo is alloced by getaddrinfo
static void __add_passive_flags(struct addrinfo *ai)
{
    while (ai)
    {
        ai->ai_flags |= AI_PASSIVE;
        ai = ai->ai_next;
    }
}

static ThreadDnsTask *__create_thread_dns_task(const std::string& host,
                                               unsigned short port,
                                               thread_dns_callback_t callback)
{
    auto *task = ThreadTaskFactory<DnsInput, DnsOutput>::
    createThreadTask(Global::getDnsQueue(),
                       Global::getDnsExecutor(),
                       DnsRoutine::run,
                       std::move(callback));

    task->getInput()->reset(host, port);
    return task;
}

void ResolverTask::dispatch()
{
    const ParsedURI& uri = ns_params_.mUri;
    host_ = uri.host ? uri.host : "";
    port_ = uri.port ? atoi(uri.port) : 0;

    DnsCache *dns_cache = Global::getDnsCache();
    const DnsCache::DnsHandle *addr_handle;
    std::string hostname = host_;

    if (ns_params_.mRetryTimes == 0)
        addr_handle = dns_cache->get_ttl(hostname, port_);
    else
        addr_handle = dns_cache->get_confident(hostname, port_);

    if (addr_handle)
    {
        RouteManager *route_manager = Global::getRouteManager();
        struct addrinfo *addrinfo = addr_handle->mValue.addrinfo;
        struct addrinfo first;

        if (ns_params_.mFixedAddr && addrinfo->ai_next)
        {
            first = *addrinfo;
            first.ai_next = NULL;
            addrinfo = &first;
        }

        if (route_manager->get(ns_params_.mType, addrinfo, ns_params_.mInfo, &ep_params_, hostname, this->mResult) < 0)
        {
            this->mState = TASK_STATE_SYS_ERROR;
            this->mError = errno;
        }
        else
            this->mState = TASK_STATE_SUCCESS;

        dns_cache->release(addr_handle);
        this->subTaskDone();
        return;
    }

    if (*host_)
    {
        char front = host_[0];
        char back = host_[hostname.size() - 1];
        struct in6_addr addr;
        int ret;

        if (strchr(host_, ':'))
            ret = inet_pton(AF_INET6, host_, &addr);
        else if (isdigit(back) && isdigit(front))
            ret = inet_pton(AF_INET, host_, &addr);
        else if (front == '/')
            ret = 1;
        else
            ret = 0;

        if (ret == 1)
        {
            DnsInput dns_in(hostname, port_, true); // 'true' means numeric host
            DnsOutput dns_out;

            DnsRoutine::run(&dns_in, &dns_out);
            __add_passive_flags((struct addrinfo *)dns_out.get_addrinfo());
            dns_callback_internal(&dns_out, (unsigned int)-1, (unsigned int)-1);
            this->subTaskDone();
            return;
        }
    }

    const char *hosts = Global::getGlobalSettings()->hosts_path;
    if (hosts)
    {
        struct addrinfo *ai;
        int ret = __readaddrinfo(hosts, host_, port_, &__ai_hints, &ai);

        if (ret == 0)
        {
            DnsOutput out;
            DnsRoutine::create(&out, ret, ai);
            __add_passive_flags((struct addrinfo *)out.get_addrinfo());
            dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
            this->subTaskDone();
            return;
        }
    }

    DnsClient *client = Global::getDnsClient();
    if (client)
    {
        static int family = __default_family();
        ResourcePool *respool = Global::getDnsRespool();

        if (family == AF_INET || family == AF_INET6)
        {
            auto&& cb = std::bind(&ResolverTask::dns_single_callback,
                                  this,
                                  std::placeholders::_1);
            DnsTask *dns_task = client->createDnsTask(hostname, std::move(cb), mUdata);

            if (family == AF_INET6)
                dns_task->getReq()->set_question_type(DNS_TYPE_AAAA);

            Conditional *cond = respool->get(dns_task);
            seriesOf(this)->pushFront(cond);
        }
        else
        {
            struct DnsContext *dctx = new struct DnsContext[2];
            DnsTask *task_v4;
            DnsTask *task_v6;
            ParallelWork *pwork;

            dctx[0].ai = NULL;
            dctx[1].ai = NULL;
            dctx[0].port = port_;
            dctx[1].port = port_;

            task_v4 = client->createDnsTask(hostname, dns_partial_callback, mUdata);
            task_v4->mUserData = dctx;

            task_v6 = client->createDnsTask(hostname, dns_partial_callback, mUdata);
            task_v6->getReq()->set_question_type(DNS_TYPE_AAAA);
            task_v6->mUserData = dctx + 1;

            auto&& cb = std::bind(&ResolverTask::dns_parallel_callback, this, std::placeholders::_1);

            pwork = Workflow::createParallelWork(std::move(cb));
            pwork->setContext(dctx);

            Conditional *cond_v4 = respool->get(task_v4);
            Conditional *cond_v6 = respool->get(task_v6);
            pwork->addSeries(Workflow::createSeriesWork(cond_v4, nullptr));
            pwork->addSeries(Workflow::createSeriesWork(cond_v6, nullptr));

            seriesOf(this)->pushFront(pwork);
        }
    } else {
        auto&& cb = std::bind(&ResolverTask::thread_dns_callback, this, std::placeholders::_1);
        ThreadDnsTask *dns_task = __create_thread_dns_task(hostname, port_, std::move(cb));
        seriesOf(this)->pushFront(dns_task);
    }

    has_next_ = true;
    this->subTaskDone();
}

SubTask* ResolverTask::done()
{
    SeriesWork *series = seriesOf(this);

    if (!has_next_) {
        if (this->mCallback) {
            this->mCallback(this);
        }
        delete this;
    } else {
        has_next_ = false;
    }

    return series->pop();
}

void ResolverTask::dns_callback_internal(void *thrd_dns_output,
                                           unsigned int ttl_default,
                                           unsigned int ttl_min)
{
    DnsOutput *dns_out = (DnsOutput *)thrd_dns_output;
    int dns_error = dns_out->get_error();

    if (dns_error)
    {
        if (dns_error == EAI_SYSTEM)
        {
            this->mState = TASK_STATE_SYS_ERROR;
            this->mError = errno;
        }
        else
        {
            this->mState = TASK_STATE_DNS_ERROR;
            this->mError = dns_error;
        }
    }
    else
    {
        RouteManager *route_manager = Global::getRouteManager();
        DnsCache *dns_cache = Global::getDnsCache();
        struct addrinfo *addrinfo = dns_out->move_addrinfo();
        const DnsCache::DnsHandle *addr_handle;
        std::string hostname = host_;

        addr_handle = dns_cache->put(hostname, port_, addrinfo,
                                     (unsigned int)ttl_default,
                                     (unsigned int)ttl_min);
        if (route_manager->get(ns_params_.mType, addrinfo, ns_params_.mInfo,
                               &ep_params_, hostname, this->mResult) < 0)
        {
            this->mState = TASK_STATE_SYS_ERROR;
            this->mError = errno;
        }
        else
            this->mState = TASK_STATE_SUCCESS;

        dns_cache->release(addr_handle);
    }
}

void ResolverTask::dns_single_callback(void *net_dns_task)
{
    DnsTask *dns_task = (DnsTask*)net_dns_task;
    Global::getDnsRespool()->post(NULL);

    if (dns_task->getState() == TASK_STATE_SUCCESS)
    {
        struct addrinfo *ai = NULL;
        int ret;

        ret = protocol::DnsUtil::getaddrinfo(dns_task->getResp(), port_, &ai);
        DnsOutput out;
        DnsRoutine::create(&out, ret, ai);
        dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
    } else {
        this->mState = dns_task->getState();
        this->mError = dns_task->getError();
    }

    if (this->mCallback)
        this->mCallback(this);

    delete this;
}

void ResolverTask::dns_partial_callback(void *net_dns_task, void*)
{
    DnsTask *dns_task = (DnsTask *)net_dns_task;
    Global::getDnsRespool()->post(NULL);

    struct DnsContext *ctx = (struct DnsContext *)dns_task->mUserData;
    ctx->ai = NULL;
    ctx->state = dns_task->getState();
    ctx->error = dns_task->getError();
    if (ctx->state == TASK_STATE_SUCCESS) {
        protocol::DnsResponse *resp = dns_task->getResp();
        ctx->eai_error = protocol::DnsUtil::getaddrinfo(resp, ctx->port, &ctx->ai);
    } else {
        ctx->eai_error = EAI_NONAME;
    }
}

void ResolverTask::dns_parallel_callback(const void *parallel)
{
    const ParallelWork *pwork = (const ParallelWork *)parallel;
    struct DnsContext *c4 = (struct DnsContext *)(pwork->getContext());
    struct DnsContext *c6 = c4 + 1;
    DnsOutput out;

    if (c4->state != TASK_STATE_SUCCESS && c6->state != TASK_STATE_SUCCESS)
    {
        this->mState = c4->state;
        this->mError = c4->error;
    }
    else if (c4->eai_error != 0 && c6->eai_error != 0)
    {
        DnsRoutine::create(&out, c4->eai_error, NULL);
        dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
    }
    else
    {
        struct addrinfo *ai = NULL;
        struct addrinfo **pai = &ai;

        if (c4->ai != NULL)
        {
            *pai = c4->ai;
            while (*pai)
                pai = &(*pai)->ai_next;
        }

        if (c6->ai != NULL)
            *pai = c6->ai;

        DnsRoutine::create(&out, 0, ai);
        dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
    }

    delete[] c4;

    if (this->mCallback)
        this->mCallback(this);

    delete this;
}

void ResolverTask::thread_dns_callback(void *thrd_dns_task)
{
    ThreadDnsTask *dns_task = (ThreadDnsTask *)thrd_dns_task;

    if (dns_task->getState() == TASK_STATE_SUCCESS)
    {
        DnsOutput *out = dns_task->getOutput();
        __add_passive_flags((struct addrinfo *)out->get_addrinfo());
        dns_callback_internal(out, dns_ttl_default_, dns_ttl_min_);
    }
    else
    {
        this->mState = dns_task->getState();
        this->mError = dns_task->getError();
    }

    if (this->mCallback)
        this->mCallback(this);

    delete this;
}

RouterTask* DnsResolver::createRouterTask(const NSParams *params, RouterCallback callback, void* udata)
{
    const GlobalSettings *settings = Global::getGlobalSettings();
    unsigned int dns_ttl_default = settings->dns_ttl_default;
    unsigned int dns_ttl_min = settings->dns_ttl_min;
    const EndpointParams *ep_params = &settings->endpointParams;
    return new ResolverTask(params, dns_ttl_default, dns_ttl_min, ep_params, std::move(callback), udata);
}
