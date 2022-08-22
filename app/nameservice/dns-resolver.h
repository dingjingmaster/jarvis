//
// Created by dingjing on 8/22/22.
//

#ifndef JARVIS_DNS_RESOLVER_H
#define JARVIS_DNS_RESOLVER_H
#include <string>
#include <functional>

#include "name-service.h"
#include "../manager/endpoint-params.h"

class ResolverTask : public RouterTask
{
public:
    ResolverTask(const NSParams *ns_params, unsigned int dns_ttl_default, unsigned int dns_ttl_min, const EndpointParams *ep_params, RouterCallback && cb)
        : RouterTask(std::move(cb)), ns_params_(*ns_params), ep_params_(*ep_params)
    {
        dns_ttl_default_ = dns_ttl_default;
        dns_ttl_min_ = dns_ttl_min;
        has_next_ = false;
    }

    ResolverTask(const NSParams *ns_params, RouterCallback && cb)
            : RouterTask(std::move(cb)), ns_params_(*ns_params)
    {
        has_next_ = false;
    }

protected:
    virtual void dispatch();
    virtual SubTask *done();
    void set_has_next() { has_next_ = true; }

private:
    void thread_dns_callback(void *thrd_dns_task);
    void dns_single_callback(void *net_dns_task);
    static void dns_partial_callback(void *net_dns_task);
    void dns_parallel_callback(const void *parallel);
    void dns_callback_internal(void *thrd_dns_output, unsigned int ttl_default, unsigned int ttl_min);

protected:
    NSParams ns_params_;
    unsigned int dns_ttl_default_;
    unsigned int dns_ttl_min_;
    EndpointParams ep_params_;

private:
    const char *host_;
    unsigned short port_;
    bool has_next_;
};

class DnsResolver : public NSPolicy
{
public:
    virtual RouterTask* createRouterTask(const NSParams *params, RouterCallback callback);
};


#endif //JARVIS_DNSRE_SOLVER_H
