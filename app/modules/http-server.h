//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_HTTP_SERVER_H
#define JARVIS_HTTP_SERVER_H

#include <utility>

#include "server.h"
#include "../factory/task-factory.h"
#include "../protocol/http/http-message.h"


using HttpProcess = std::function<void(HttpTask*)>;
using HttpServer = Server<protocol::HttpRequest, protocol::HttpResponse>;

static constexpr ServerParams HTTP_SERVER_PARAMS_DEFAULT =
        {
                .maxConnections         =   2000,
                .peerResponseTimeout    =   10 * 1000,
                .receiveTimeout         =   -1,
                .keepAliveTimeout       =   60 * 1000,
                .requestSizeLimit       =   (size_t)-1,
                .sslAcceptTimeout       =   10 * 1000,
        };

template<>
inline HttpServer::Server(HttpProcess proc)
    : ServerBase(&HTTP_SERVER_PARAMS_DEFAULT), mProcess(std::move(proc))
{
    logv("");
}

template<>
inline CommSession *HttpServer::newSession(long long seq, CommConnection *conn)
{
    HttpTask *task;

    logv("");

    task = ServerTaskFactory::createHttpTask(this, mProcess, nullptr);
    task->setKeepAlive(mParams.keepAliveTimeout);
    task->setReceiveTimeout(mParams.receiveTimeout);
    task->getReq()->setSizeLimit(mParams.requestSizeLimit);

    return task;
}


#endif //JARVIS_HTTP_SERVER_H
