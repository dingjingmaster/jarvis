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
    DnsClient() : mParams(NULL) { }
    virtual ~DnsClient() { }

    int init(const std::string& url);
    int init(const std::string& url, const std::string& search_list, int ndots, int attempts, bool rotate);
    void deInit();

    DnsTask *createDnsTask(const std::string& name, DnsCallback callback);

private:
    std::atomic<size_t>         mId;
    void*                       mParams;
};


#endif //JARVIS_DNS_CLIENT_H
