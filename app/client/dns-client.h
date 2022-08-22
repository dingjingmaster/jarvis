//
// Created by dingjing on 8/22/22.
//

#ifndef JARVIS_DNS_CLIENT_H
#define JARVIS_DNS_CLIENT_H

#include <string>
#include <atomic>
#include "../factory/task-factory.h"
#include "../protocol/dns/dns-message.h"

class DnsClient
{
public:
    DnsClient() : params(NULL) { }
    virtual ~DnsClient() { }

    int init(const std::string& url);
    int init(const std::string& url, const std::string& search_list, int ndots, int attempts, bool rotate);
    void deinit();

    WFDnsTask *create_dns_task(const std::string& name, dns_callback_t callback);

private:
    void *params;
    std::atomic<size_t> id;
};


#endif //JARVIS_DNS_CLIENT_H
