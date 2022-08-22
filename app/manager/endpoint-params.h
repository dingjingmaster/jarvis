//
// Created by dingjing on 8/19/22.
//

#ifndef JARVIS_ENDPOINT_PARAMS_H
#define JARVIS_ENDPOINT_PARAMS_H

#include <stddef.h>

typedef struct _EndpointParams              EndpointParams;

enum TransportType
{
    TT_TCP,
    TT_UDP,
    TT_SCTP,
    TT_TCP_SSL,
    TT_SCTP_SSL,
};

struct _EndpointParams
{
    size_t                  maxConnections;
    int                     connectTimeout;
    int                     responseTimeout;
    int                     sslConnectTimeout;
    bool                    useTlsSni;
};

static constexpr EndpointParams ENDPOINT_PARAMS_DEFAULT =
        {
            .maxConnections             = 200,
            .connectTimeout             = 10 * 1000,
            .responseTimeout            = 10 * 1000,
            .sslConnectTimeout          = 10 * 1000,
            .useTlsSni                  = false,
        };


#endif //JARVIS_ENDPOINT_PARAMS_H
