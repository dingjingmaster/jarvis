//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_GLOBAL_H
#define JARVIS_GLOBAL_H

#if __cplusplus < 201100
#error CPLUSPLUS VERSION required at least C++11. Please use "-std=c++11".
#include <C++11_REQUIRED>
#endif

#include <string>
#include <openssl/ssl.h>

#include "dns-cache.h"
#include "route-manager.h"
#include "../core/executor.h"
#include "../core/common-scheduler.h"
#include "endpoint-params.h"
#include "resource-pool.h"
#include "../nameservice/name-service.h"
#include "dns-resolver.h"
#include "endpoint-params.h"

typedef struct _GlobalSettings      GlobalSettings;

struct _GlobalSettings
{
    struct EndpointParams endpointParams;
    struct EndpointParams dnsServerParams;
    unsigned int dns_ttl_default;	///< in seconds, DNS TTL when network request success
    unsigned int dns_ttl_min;		///< in seconds, DNS TTL when network request fail
    int dns_threads;
    int poller_threads;
    int handler_threads;
    int compute_threads;			///< auto-set by system CPU number if value<=0
    const char *resolv_conf_path;
    const char *hosts_path;
};

static constexpr GlobalSettings GLOBAL_SETTINGS_DEFAULT =
        {
                .endpoint_params	=	ENDPOINT_PARAMS_DEFAULT,
                .dns_server_params	=	ENDPOINT_PARAMS_DEFAULT,
                .dns_ttl_default	=	12 * 3600,
                .dns_ttl_min		=	180,
                .dns_threads		=	4,
                .poller_threads		=	4,
                .handler_threads	=	20,
                .compute_threads	=	-1,
                .resolv_conf_path	=	"/etc/resolv.conf",
                .hosts_path			=	"/etc/hosts",
        };

extern void jarvis_library_init(const struct WFGlobalSettings *settings);

class Global
{
public:
    static void registerSchemePort(const std::string& scheme, unsigned short port);
    static const char* getDefaultPort(const std::string& scheme);
    static const GlobalSettings* getGlobalSettings()
    {
        return &gSettings;
    }

    static void setGlobalSettings(const GlobalSettings *settings)
    {
        gSettings = *settings;
    }

    static const char* getErrorString(int state, int error);

    // Internal usage only
public:
    static class CommScheduler *getScheduler();
    static SSL_CTX* getSslClientCtx();
    static SSL_CTX* newSslServerCtx();
    static class ExecQueue* getExecQueue(const std::string& queueName);
    static class Executor* getComputeExecutor();
    static class IOService* getIoService();
    static class ExecQueue* getDnsQueue();
    static class Executor* getDnsExecutor();
    static class DnsClient* getDnsClient();
    static class ResourcePool* getDnsRespool();

    static class RouteManager* getRouteManager()
    {
        return &gRouteManager;
    }

    static class DnsCache* getDnsCache()
    {
        return &gDnsCache;
    }

    static class DnsResolver *getDnsResolver()
    {
        return &gDnsResolver;
    }

    static class NameService *getNameService()
    {
        return &gNameService;
    }

public:
    static void syncOperationBegin();
    static void syncOperationEnd();

private:
    static GlobalSettings               gSettings;
    static RouteManager                 gRouteManager;
    static DnsCache                     gDnsCache;
    static DnsResolver                  gDnsResolver;
    static NameService                  gNameService;
};

#endif //JARVIS_GLOBAL_H
